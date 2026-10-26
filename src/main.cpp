#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "neutron.h"
#include "save_data.h"

static SDL_Window* g_window = nullptr;
static SDL_GLContext g_glContext = nullptr;
static neutron* g_game = nullptr;
static bool g_running = true;

static int g_windowWidth = 960;
static int g_windowHeight = 544;

// Hidden main-menu cheat code -- Up, Up, Down, Down, Left, Right, Enter --
// toggles the same unlock_all_stages flag the old visible "Unlock All
// Stages (TEST)" options entry did. Tracked in parallel with normal menu
// navigation: every arrow/enter press on the main menu both does its usual
// job AND is checked against this sequence.
static const SDL_Keycode CHEAT_SEQUENCE[7] = {
    SDLK_UP, SDLK_UP, SDLK_DOWN, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_RETURN
};
static int g_cheatProgress = 0;

static void check_cheat_code(SDL_Keycode sym) {
    if (sym == CHEAT_SEQUENCE[g_cheatProgress]) {
        g_cheatProgress++;
        if (g_cheatProgress == 7) {
            g_cheatProgress = 0;
            if (g_game) g_game->execute_action(22); // toggle unlock_all_stages
        }
    } else {
        // A wrong key breaks the match, but it might itself be the start of
        // a fresh attempt (e.g. mashing Up when the sequence wants Down).
        g_cheatProgress = (sym == CHEAT_SEQUENCE[0]) ? 1 : 0;
    }
}

void quit_game(int code) {
    if (g_game) g_game->save_progress(); // safety net alongside the per-event saves

    delete g_game;
    g_game = nullptr;

    Mix_CloseAudio();

    if (g_glContext) {
        SDL_GL_DeleteContext(g_glContext);
        g_glContext = nullptr;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }
    SDL_Quit();
    std::exit(code);
}

static void toggle_fullscreen() {
    if (!g_game || !g_window) return;
    g_game->fullscreen = !g_game->fullscreen;
    SDL_SetWindowFullscreen(g_window, g_game->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    g_game->save_progress();
}

static bool try_init_gl(int depth, int alpha) {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if (depth > 0) {
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth);
    }
    if (alpha > 0) {
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, alpha);
    }

    g_window = SDL_CreateWindow(
        "Neutron - 3D Lightcycle Grid",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        g_windowWidth, g_windowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!g_window) return false;

    g_glContext = SDL_GL_CreateContext(g_window);
    if (!g_glContext) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
        return false;
    }
    return true;
}

static void SDL_SetupVideo(bool start_fullscreen) {
    // Try visual configurations in order of compatibility:
    // 1. 24-bit depth (standard on Linux X11/Mesa/NVIDIA)
    // 2. 16-bit depth
    // 3. Default SDL attributes (no strict depth/alpha constraints)
    // 4. 24-bit depth with 8-bit alpha
    // 5. 16-bit depth with 8-bit alpha
    bool ok = try_init_gl(24, 0);
    if (!ok) ok = try_init_gl(16, 0);
    if (!ok) ok = try_init_gl(0, 0);
    if (!ok) ok = try_init_gl(24, 8);
    if (!ok) ok = try_init_gl(16, 8);

    if (!ok) {
        std::fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
        std::fprintf(stderr, "Tip: If running in a VM or headless session, try: LIBGL_ALWAYS_SOFTWARE=1 ./run.sh\n");
        std::fprintf(stderr, "Tip: If on Wayland, try: SDL_VIDEODRIVER=wayland ./run.sh or SDL_VIDEODRIVER=x11 ./run.sh\n");
        std::exit(1);
    }

    if (start_fullscreen) {
        SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnableClientState(GL_VERTEX_ARRAY);
}

static void SDL_SetupAudio() {
    // Just opens the device here. Loading/playing travel.wav, explosion.wav,
    // and music.wav is sound_mixer's job (constructed below, inside
    // neutron) -- if this fails, sound_mixer detects no device is open and
    // every playback call becomes a silent no-op, never a crash.
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::fprintf(stderr, "Warning: Unable to open audio: %s\n", Mix_GetError());
    }
}

static void handle_input(SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        g_running = false;
        return;
    }

    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            g_windowWidth = event.window.data1;
            g_windowHeight = event.window.data2;
        }
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        if (!g_game) return;
        SDL_Keycode sym = event.key.keysym.sym;

        // Skip intro if active. game_status == 4 is the generic "delay" state --
        // it's the main menu's into-skip target state ONLY when still on the menu
        // (actual_stage == 1000); mid-level it's the brief post-flythrough pause
        // (see show_stage()/change_stage()), which must NOT be treated as "skip to
        // main menu idle" or it corrupts actual_stage/game_status into a combo
        // nothing else in this handler recognizes, hard-locking input.
        if (g_game->actual_stage == 1000 &&
            (g_game->game_status == 1000 || g_game->game_status == 4)) {
            g_game->intro_step = 93;
            g_game->game_status = 6;
            int active_btns[4] = {7, 8, 9, 10};
            if (g_game->interface) g_game->interface->activate_buttons(active_btns, 4);
            return;
        }

        // Graphics & Sound submenu (stage 1000, actual_option 30-33: Sound on/off, Volume, Fullscreen, Back)
        if (g_game->actual_stage == 1000 && g_game->game_status != 13 &&
            g_game->interface && g_game->interface->actual_option >= 30 && g_game->interface->actual_option < 34) {
            if (sym == SDLK_UP || sym == SDLK_w) {
                int& opt = g_game->interface->actual_option;
                opt = (opt > 30) ? opt - 1 : 33;
                return;
            }
            if (sym == SDLK_DOWN || sym == SDLK_s) {
                int& opt = g_game->interface->actual_option;
                opt = (opt < 33) ? opt + 1 : 30;
                return;
            }
            if (sym == SDLK_LEFT || sym == SDLK_a) {
                if (g_game->interface->actual_option == 31) g_game->execute_action(29); // Volume down
                return;
            }
            if (sym == SDLK_RIGHT || sym == SDLK_d) {
                if (g_game->interface->actual_option == 31) g_game->execute_action(28); // Volume up
                return;
            }
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                switch (g_game->interface->actual_option) {
                    case 30: g_game->execute_action(17); break; // Sound on/off
                    case 32: toggle_fullscreen(); break; // Fullscreen on/off
                    case 33: g_game->interface->actual_option = 10; break; // Back to Options
                }
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->interface->actual_option = 10;
                return;
            }
        }

        // Options submenu (stage 1000, actual_option 10-13: Sound/Color/Difficulty/Back)
        if (g_game->actual_stage == 1000 && g_game->game_status != 13 &&
            g_game->interface && g_game->interface->actual_option >= 10 && g_game->interface->actual_option < 14) {
            if (sym == SDLK_UP || sym == SDLK_w) {
                int& opt = g_game->interface->actual_option;
                opt = (opt > 10) ? opt - 1 : 13;
                return;
            }
            if (sym == SDLK_DOWN || sym == SDLK_s) {
                int& opt = g_game->interface->actual_option;
                opt = (opt < 13) ? opt + 1 : 10;
                return;
            }
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                switch (g_game->interface->actual_option) {
                    case 10: g_game->interface->actual_option = 30; break; // Open Graphics & Sound
                    case 11: g_game->execute_action(15); break; // Player color
                    case 12: g_game->execute_action(16); break; // Difficulty easy/hard
                    case 13: g_game->interface->actual_option = 0; break; // Back to main menu
                }
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->interface->actual_option = 0;
                return;
            }
        }

        // Main Menu navigation (stage 1000)
        if (g_game->actual_stage == 1000 && g_game->game_status != 13) {
            check_cheat_code(sym);
            if (sym == SDLK_UP || sym == SDLK_w) {
                if (g_game->interface) {
                    g_game->interface->actual_option = (g_game->interface->actual_option + 5) % 6;
                }
                return;
            }
            if (sym == SDLK_DOWN || sym == SDLK_s) {
                if (g_game->interface) {
                    g_game->interface->actual_option = (g_game->interface->actual_option + 1) % 6;
                }
                return;
            }
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                int opt = g_game->interface ? g_game->interface->actual_option : 0;
                if (opt == 0) { // New Game (always starts at stage 1)
                    g_game->execute_action(2);
                } else if (opt == 1) { // Continue Game (resume at furthest unlocked stage)
                    g_game->execute_action(11);
                } else if (opt == 2) { // Training
                    g_game->execute_action(5);
                } else if (opt == 3) { // Stage Select
                    g_game->execute_action(12);
                } else if (opt == 4) { // Options
                    if (g_game->interface) g_game->interface->actual_option = 10;
                } else { // Credits
                    g_game->game_status = 13;
                }
                return;
            }
            if (sym == SDLK_1 || sym == SDLK_KP_1) {
                g_game->execute_action(2);
                return;
            }
            if (sym == SDLK_2 || sym == SDLK_KP_2) {
                g_game->execute_action(11);
                return;
            }
            if (sym == SDLK_3 || sym == SDLK_KP_3 || sym == SDLK_t) {
                g_game->execute_action(5);
                return;
            }
            if (sym == SDLK_4 || sym == SDLK_KP_4) {
                g_game->execute_action(12);
                return;
            }
            if (sym == SDLK_5 || sym == SDLK_KP_5 || sym == SDLK_o) {
                if (g_game->interface) g_game->interface->actual_option = 10;
                return;
            }
            if (sym == SDLK_6 || sym == SDLK_KP_6 || sym == SDLK_c) {
                g_game->game_status = 13;
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_running = false;
                return;
            }
        }

        // Credits screen
        if (g_game->game_status == 13) {
            if (sym == SDLK_ESCAPE || sym == SDLK_RETURN || sym == SDLK_SPACE) {
                g_game->execute_action(8);
                return;
            }
        }

        // Stage Select mode
        if (g_game->actual_stage == 3000) {
            if (sym == SDLK_LEFT || sym == SDLK_a) {
                g_game->execute_action(19); // Prev stage
                return;
            }
            if (sym == SDLK_RIGHT || sym == SDLK_d) {
                g_game->execute_action(20); // Next stage
                return;
            }
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                g_game->execute_action(21); // Play selected stage
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->execute_action(8);
                return;
            }
        }

        // In-game controls (Gameplay stages 1-25 or Training 2000)
        if (g_game->game_status == 0) {
            if (g_game->paused) {
                // First Escape paused us; Enter/Space resumes, a second Escape
                // falls through to the normal quit-to-menu behavior below.
                if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                    g_game->paused = false;
                    return;
                }
                if (sym == SDLK_ESCAPE) {
                    g_game->paused = false;
                    g_game->execute_action(8);
                    return;
                }
                return; // swallow steering/view input while paused
            }
            if (sym == SDLK_LEFT || sym == SDLK_a || sym == SDLK_q) {
                g_game->execute_action(0); // Turn Left
                return;
            }
            if (sym == SDLK_RIGHT || sym == SDLK_d || sym == SDLK_e) {
                g_game->execute_action(1); // Turn Right
                return;
            }
            if (sym == SDLK_v) {
                g_game->vision = 1 - g_game->vision; // Toggle Camera
                return;
            }
            if (g_game->actual_stage == 2000 &&
                (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z)) {
                g_game->execute_action(9); // Spawn enemy (Training mode only)
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->paused = true;
                if (g_game->mixer) g_game->mixer->stop_travel();
                return;
            }
        }

        // Crash retry (game_status == 1)
        if (g_game->game_status == 1) {
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                g_game->execute_action(18);
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->execute_action(8);
                return;
            }
        }

        // Level Complete victory or Crash retry (game_status == 8)
        if (g_game->game_status == 8 || g_game->game_status == 3) {
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                if (g_game->colision == 1) {
                    g_game->execute_action(18); // Try again
                } else {
                    g_game->next_level();
                }
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->execute_action(8);
                return;
            }
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (!g_game) return;
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (g_game->paused) {
                g_game->paused = false;
                return;
            }
            // Click to start or skip intro
            if (g_game->actual_stage == 1000) {
                if (g_game->game_status == 1000 || g_game->game_status == 4) {
                    g_game->intro_step = 93;
                    g_game->game_status = 6;
                } else {
                    g_game->execute_action(2);
                }
            } else if (g_game->game_status == 1) {
                g_game->execute_action(18);
            } else if (g_game->game_status == 8 || g_game->game_status == 3) {
                if (g_game->colision == 1) {
                    g_game->execute_action(18);
                } else {
                    g_game->next_level();
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    bool test_mode = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--test") == 0) {
            test_mode = true;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SaveData save = load_save_data();

    SDL_SetupVideo(save.fullscreen);
    SDL_SetupAudio();

    g_game = new neutron(save.user_color, save.user_difficulty, save.user_stage, save.user_sound, save.user_volume, save.fullscreen);
    g_game->apply_save_data(save);
    if (g_game->mixer) g_game->mixer->start_music();

    if (!test_mode) {
        // Start directly in interactive menu ready to play!
        g_game->actual_stage = 1000;
        g_game->game_status = 6;
        int active_btns[4] = {7, 8, 9, 10};
        if (g_game->interface) g_game->interface->activate_buttons(active_btns, 4);
    }

    if (test_mode) {
        std::printf("[TEST] Running automated test verification...\n");
        // 1. Initial state
        std::printf("[TEST] Initial state OK (status: %d)\n", g_game->game_status);

        // 2. Start game (stage 1)
        g_game->execute_action(2); // Start Game
        std::printf("[TEST] Loaded stage: %d, status: %d\n", g_game->actual_stage, g_game->game_status);
        if (g_game->actual_stage != 1 || !g_game->ground || !g_game->proton) {
            std::fprintf(stderr, "[TEST FAIL] Stage 1 load failed!\n");
            return 1;
        }

        // 3. Simulate gameplay frames
        g_game->game_status = 0; // In-game run
        for (int frame = 0; frame < 30; frame++) {
            g_game->main_loop();
            g_game->print();
        }
        std::printf("[TEST] 30 gameplay frames executed successfully\n");

        // 4. Direction changes
        g_game->execute_action(0); // Left
        g_game->main_loop();
        g_game->execute_action(1); // Right
        g_game->main_loop();
        std::printf("[TEST] Controls left/right change executed\n");

        // 5. Stage selection (miniatures)
        g_game->execute_action(12);
        std::printf("[TEST] Stage select mode: actual_stage=%d, status=%d\n", g_game->actual_stage, g_game->game_status);
        if (g_game->actual_stage != 3000 || g_game->game_status != 9) {
            std::fprintf(stderr, "[TEST FAIL] Stage select failed!\n");
            return 1;
        }

        for (int frame = 0; frame < 15; frame++) {
            g_game->main_loop();
            g_game->print();
        }
        std::printf("[TEST] 15 stage select frames executed successfully\n");

        std::printf("[TEST SUCCESS] All 1-to-1 port verification checks passed!\n");
        quit_game(0);
        return 0;
    }

    Uint32 last_tick = SDL_GetTicks();

    while (g_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handle_input(event);
        }

        Uint32 current_tick = SDL_GetTicks();
        float delta = (float)(current_tick - last_tick);

        float target_frame_time = (g_game ? g_game->speed : 50.0f);
        if (target_frame_time < 10.0f) target_frame_time = 10.0f;

        if (delta >= target_frame_time) {
            last_tick = current_tick;
            if (g_game && !g_game->paused) {
                g_game->main_loop();
            }
        }

        glViewport(0, 0, g_windowWidth, g_windowHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double aspect = (double)g_windowWidth / (double)g_windowHeight;
        gluPerspective(45.0, aspect, 1.0, 500.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        if (g_game) {
            g_game->print();
        }

        SDL_GL_SwapWindow(g_window);
        SDL_Delay(1);
    }

    quit_game(0);
    return 0;
}
