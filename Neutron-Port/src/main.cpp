#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "neutron.h"
#include "save_data.h"
#include "custom_stage.h"
#include "controller_input.h"
#include "touch_input.h"
#include "split_screen_menu.h"
#include "split_screen_match.h"

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

    controller_input::shutdown();
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

// controller_input.cpp needs this for its Graphics & Sound submenu parity
// (option 32, Fullscreen) but has no access to -- and shouldn't own -- the
// SDL window handle itself.
void app_toggle_fullscreen() {
    toggle_fullscreen();
}

// split_screen_match.cpp needs actual window pixel dimensions to lay out
// its own multi-viewport glViewport() rects -- g_windowWidth/Height are
// private to this file (and must stay that way, since main.cpp's own
// per-frame render loop is what keeps them current on resize).
void app_get_window_size(int* w, int* h) {
    if (w) *w = g_windowWidth;
    if (h) *h = g_windowHeight;
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

        // Split-screen join/map-select/match screens -- intercepted here,
        // before the generic actual_stage==1000/3000 blocks below, which would
        // otherwise also fire for these game_status values (join screen keeps
        // actual_stage==1000 for the animated background; map select reuses
        // actual_stage==3000 for Stage Select's existing camera/miniature
        // rendering) and corrupt this flow with single-player menu/stage-select
        // behavior.
        if (g_game->game_status == 20 || g_game->game_status == 21) {
            split_screen_menu::handle_key_down(g_game, sym);
            return;
        }
        if (g_game->game_status == 23 || g_game->game_status == 24) {
            split_screen_match::handle_key_down(g_game, sym);
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
                    g_game->interface->actual_option = (g_game->interface->actual_option + 6) % 7;
                }
                return;
            }
            if (sym == SDLK_DOWN || sym == SDLK_s) {
                if (g_game->interface) {
                    g_game->interface->actual_option = (g_game->interface->actual_option + 1) % 7;
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
                } else if (opt == 4) { // Split Screen
                    split_screen_menu::reset(g_game);
                    g_game->game_status = 20;
                } else if (opt == 5) { // Options
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
            if (sym == SDLK_5 || sym == SDLK_KP_5) {
                split_screen_menu::reset(g_game);
                g_game->game_status = 20;
                return;
            }
            if (sym == SDLK_6 || sym == SDLK_KP_6 || sym == SDLK_o) {
                if (g_game->interface) g_game->interface->actual_option = 10;
                return;
            }
            if (sym == SDLK_7 || sym == SDLK_KP_7 || sym == SDLK_c) {
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

        // Scripted tutorial (New Game, game_status == 10 -- see neutron::tutorial())
        if (g_game->game_status == 10) {
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
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                // Steps 10 (watch the enemy) and 16 (the real fight) advance on
                // their own -- [[ENTER] NEXT] is only shown/accepted otherwise.
                if (g_game->tutorial_step != 10 && g_game->tutorial_step != 16) {
                    g_game->step_tutorial();
                }
                return;
            }
            if (sym == SDLK_ESCAPE) {
                // The "exit" option is live throughout the whole scripted
                // tutorial in the original (no pause step here, unlike normal
                // gameplay) -- execute_action(8) also clears isTutorial/step.
                g_game->execute_action(8);
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

    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        controller_input::handle_device_added(event.cdevice);
        return;
    }
    if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        controller_input::handle_device_removed(event.cdevice);
        return;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (g_game) {
            if (g_game->game_status == 20 || g_game->game_status == 21) {
                split_screen_menu::handle_controller_button_down(g_game, event.cbutton);
            } else if (g_game->game_status == 23 || g_game->game_status == 24) {
                split_screen_match::handle_controller_button_down(g_game, event.cbutton);
            } else {
                controller_input::handle_button_down(g_game, event.cbutton);
            }
        }
        return;
    }

    if (event.type == SDL_FINGERDOWN) {
        if (g_game) touch_input::handle_finger_down(g_game, event.tfinger);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (!g_game) return;
        // SDL synthesizes a matching mouse event for every touch by default
        // (SDL_HINT_TOUCH_MOUSE_EVENTS is "1" unless we say otherwise -- see
        // main()), so a single tap would otherwise be handled TWICE: once
        // correctly by touch_input's row-aware dispatch above, then again
        // here by this much older, coarser "any click" handler (which, on
        // the main menu, unconditionally does New Game regardless of which
        // row is selected) -- skip anything that didn't come from a real
        // mouse.
        if (event.button.which == SDL_TOUCH_MOUSEID) return;
        if (g_game->game_status == 20 || g_game->game_status == 21 ||
            g_game->game_status == 23 || g_game->game_status == 24) return;
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

    // Touch and mouse are handled as two deliberately separate paths here
    // (touch_input.cpp's row-aware dispatch vs. the older, coarser
    // SDL_MOUSEBUTTONDOWN handler below) -- without this hint SDL also
    // synthesizes a mouse click for every touch, double-handling one tap.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    controller_input::init();

    load_custom_stage();
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

        // 6. Gamepad dispatch (virtual controller, no physical hardware needed) --
        // exercises controller_input's own button-dispatch table, not just that it
        // compiles. Real hot-plug/axis-feel/physical-layout correctness still needs
        // the user's own hardware (see controller_input.h's header comment).
        {
            int dev_idx = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1);
            if (dev_idx < 0) {
                std::fprintf(stderr, "[TEST FAIL] Could not attach virtual controller: %s\n", SDL_GetError());
                return 1;
            }
            SDL_ControllerDeviceEvent added_ev{};
            added_ev.which = dev_idx;
            controller_input::handle_device_added(added_ev);
            if (controller_input::num_connected() < 1) {
                std::fprintf(stderr, "[TEST FAIL] Virtual controller did not register!\n");
                return 1;
            }
            SDL_GameController* vc = controller_input::get_controller(0);
            SDL_Joystick* vj = SDL_GameControllerGetJoystick(vc);
            SDL_JoystickID vid = SDL_JoystickInstanceID(vj);

            // Back to main-menu idle, then DPAD Down/Up should move actual_option.
            g_game->actual_stage = 1000;
            g_game->game_status = 6;
            g_game->interface->actual_option = 0;

            auto press = [&](SDL_GameControllerButton btn) {
                SDL_ControllerButtonEvent ev{};
                ev.which = vid;
                ev.button = (Uint8)btn;
                controller_input::handle_button_down(g_game, ev);
            };

            press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            if (g_game->interface->actual_option != 1) {
                std::fprintf(stderr, "[TEST FAIL] Controller DPAD_DOWN didn't advance actual_option (got %d)!\n",
                              g_game->interface->actual_option);
                return 1;
            }
            press(SDL_CONTROLLER_BUTTON_DPAD_UP);
            if (g_game->interface->actual_option != 0) {
                std::fprintf(stderr, "[TEST FAIL] Controller DPAD_UP didn't move actual_option back (got %d)!\n",
                              g_game->interface->actual_option);
                return 1;
            }

            // A on "New Game" (option 0) should reach execute_action(2), same as keyboard Enter.
            press(SDL_CONTROLLER_BUTTON_A);
            if (g_game->actual_stage != 1 || !g_game->proton) {
                std::fprintf(stderr, "[TEST FAIL] Controller A on New Game didn't start stage 1!\n");
                return 1;
            }
            g_game->game_status = 0; // in-game run, matching the keyboard section above

            press(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            g_game->main_loop();
            press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            g_game->main_loop();

            // Bumpers double as Left/Right steering too.
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            g_game->main_loop();
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            g_game->main_loop();

            // Regression check: controller A on the main menu's "SPLIT SCREEN"
            // row (position 4 since the Phase 3 reorder) must open the join
            // screen, not fall through to a stale pre-reorder dispatch (this
            // exact bug shipped once already -- controller_input.cpp had its
            // own copy of the menu dispatch table that didn't get updated
            // when the menu order changed, so A on Split Screen opened
            // Options instead).
            g_game->actual_stage = 1000;
            g_game->game_status = 6;
            g_game->interface->actual_option = 4;
            press(SDL_CONTROLLER_BUTTON_A);
            if (g_game->game_status != 20) {
                std::fprintf(stderr, "[TEST FAIL] Controller A on Split Screen opened something else (status=%d)!\n",
                              g_game->game_status);
                return 1;
            }
            g_game->actual_stage = 1000;
            g_game->game_status = 6;
            g_game->interface->actual_option = 0;

            std::printf("[TEST] Gamepad dispatch (virtual controller) executed successfully\n");
        }

        // 7. Touch dispatch -- synthesize SDL_FINGERDOWN events directly (SDL
        // reports finger coords pre-normalized 0..1, so no real touch device
        // is needed to exercise the coordinate transform + row hit-testing).
        {
            auto tap = [&](float logical_x, float logical_y) {
                SDL_TouchFingerEvent ev{};
                ev.x = logical_x / 480.0f;
                ev.y = 1.0f - (logical_y / 272.0f);
                touch_input::handle_finger_down(g_game, ev);
            };

            g_game->actual_stage = 1000;
            g_game->game_status = 6;
            g_game->interface->actual_option = 0;
            g_game->isTutorial = false; // reset from the earlier New Game test run

            // Regression check: tapping row 1 ("2. CONTINUE GAME", y=188-22=166)
            // must reach execute_action(11), not silently fall through to
            // execute_action(2) (New Game) via a duplicate synthetic mouse
            // click -- execute_action(2) is the only one of the two that sets
            // isTutorial, so this is a direct, reliable regression check for
            // SDL_HINT_TOUCH_MOUSE_EVENTS / the SDL_TOUCH_MOUSEID guard above.
            tap(200.0f, 166.0f); // row 1: Continue Game
            if (g_game->isTutorial) {
                std::fprintf(stderr, "[TEST FAIL] Touch tap on 'Continue Game' triggered New Game instead!\n");
                return 1;
            }
            if (!g_game->proton) {
                std::fprintf(stderr, "[TEST FAIL] Touch tap on 'Continue Game' didn't start a stage!\n");
                return 1;
            }
            g_game->game_status = 0; // in-game run

            tap(100.0f, 100.0f); // left half of screen -> turn left
            g_game->main_loop();
            tap(400.0f, 100.0f); // right half of screen -> turn right
            g_game->main_loop();

            // The real on-screen "[ESC] MENU" hint text -> pause.
            tap(260.0f, 18.0f);
            if (!g_game->paused) {
                std::fprintf(stderr, "[TEST FAIL] Touch tap on the ESC hint didn't pause!\n");
                return 1;
            }
            // Tapping the paused ESC hint a second time quits to the menu...
            tap(290.0f, 110.0f);
            if (g_game->paused || g_game->actual_stage != 1000) {
                std::fprintf(stderr, "[TEST FAIL] Touch tap on paused ESC hint didn't quit to menu!\n");
                return 1;
            }

            std::printf("[TEST] Touch dispatch (synthesized finger events) executed successfully\n");
        }

        // 8. Split-screen join/map-select flow -- keyboard as one player,
        // the virtual controller attached in section 6 as a second.
        {
            SDL_GameController* vc = controller_input::get_controller(0);
            SDL_JoystickID vid = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(vc));
            auto press_ctrl = [&](SDL_GameControllerButton btn) {
                SDL_ControllerButtonEvent ev{};
                ev.which = vid;
                ev.button = (Uint8)btn;
                split_screen_menu::handle_controller_button_down(g_game, ev);
            };

            g_game->actual_stage = 1000;
            g_game->game_status = 6;
            split_screen_menu::reset(g_game);
            g_game->game_status = 20; // join screen

            split_screen_menu::handle_key_down(g_game, SDLK_RETURN); // keyboard joins
            split_screen_menu::handle_key_down(g_game, SDLK_s);      // too few players -- must not start
            if (g_game->game_status != 20) {
                std::fprintf(stderr, "[TEST FAIL] Split-screen started with only 1 player joined!\n");
                return 1;
            }

            press_ctrl(SDL_CONTROLLER_BUTTON_A); // controller joins (2nd player)
            split_screen_menu::handle_key_down(g_game, SDLK_s); // keyboard, already joined, starts the match
            if (g_game->game_status != 21 || g_game->actual_stage != 3000) {
                std::fprintf(stderr, "[TEST FAIL] Split-screen didn't reach map select with 2 players!\n");
                return 1;
            }
            if (g_game->actual_miniature != -1) {
                std::fprintf(stderr, "[TEST FAIL] Map select didn't default to RANDOM!\n");
                return 1;
            }

            split_screen_menu::handle_key_down(g_game, SDLK_RIGHT); // browse off RANDOM onto a real map
            if (g_game->actual_miniature != 0) {
                std::fprintf(stderr, "[TEST FAIL] Map select RIGHT didn't move off RANDOM!\n");
                return 1;
            }

            split_screen_menu::handle_key_down(g_game, SDLK_RETURN); // confirm map -> starts the real match
            if (g_game->game_status != 23 || g_game->actual_stage != 4000) {
                std::fprintf(stderr, "[TEST FAIL] Confirming a map didn't start the match!\n");
                return 1;
            }

            player* p0 = split_screen_match::player_at_slot(0); // keyboard, joined first
            player* p1 = split_screen_match::player_at_slot(1); // controller, joined second
            if (!p0 || !p1) {
                std::fprintf(stderr, "[TEST FAIL] Match started without both players spawned!\n");
                return 1;
            }
            if (split_screen_match::colision_for_slot(0) != 0 || split_screen_match::colision_for_slot(1) != 0) {
                std::fprintf(stderr, "[TEST FAIL] A player started already eliminated!\n");
                return 1;
            }

            for (int frame = 0; frame < 5; frame++) g_game->main_loop();
            if (split_screen_match::colision_for_slot(0) != 0 || split_screen_match::colision_for_slot(1) != 0) {
                std::fprintf(stderr, "[TEST FAIL] A player crashed on their own during normal ticking!\n");
                return 1;
            }

            std::printf("[TEST] Split-screen join/map-select/match-start flow executed successfully\n");

            // Force a real collision: park player 1 on top of player 0's
            // current tile (already claimed, and persistent -- mode==1 trail
            // never ages out) and tick once more -- player 1 should crash,
            // player 0 should be declared the winner.
            p1->x = p0->x; p1->y = p0->y; p1->z = p0->z;
            p1->actual_plane = p0->actual_plane;
            p1->step_counter = p1->step_speed;
            split_screen_match::tick(g_game);
            if (split_screen_match::colision_for_slot(1) != 1) {
                std::fprintf(stderr, "[TEST FAIL] Forced collision didn't eliminate player 1!\n");
                return 1;
            }
            if (split_screen_match::colision_for_slot(0) != 0) {
                std::fprintf(stderr, "[TEST FAIL] Player 0 was incorrectly eliminated too!\n");
                return 1;
            }
            if (g_game->game_status != 24 || split_screen_match::winner_slot() != 0) {
                std::fprintf(stderr, "[TEST FAIL] Last-ship-standing win detection didn't declare player 0 the winner!\n");
                return 1;
            }

            split_screen_match::handle_key_down(g_game, SDLK_RETURN); // match over -> Enter retries
            if (g_game->game_status != 23 || g_game->actual_stage != 4000) {
                std::fprintf(stderr, "[TEST FAIL] Enter on the match-over screen didn't retry!\n");
                return 1;
            }
            if (split_screen_match::colision_for_slot(0) != 0 || split_screen_match::colision_for_slot(1) != 0) {
                std::fprintf(stderr, "[TEST FAIL] Retry didn't reset both players back to alive!\n");
                return 1;
            }

            split_screen_match::handle_key_down(g_game, SDLK_ESCAPE); // now Escape quits
            // change_stage(1000, true) returns 11 (change_option's idle state),
            // the same return value execute_action(8) itself assigns to
            // game_status when leaving any other screen back to the menu.
            if ((g_game->game_status != 6 && g_game->game_status != 11) || g_game->actual_stage != 1000) {
                std::fprintf(stderr, "[TEST FAIL] Escape on the live match didn't return to the main menu!\n");
                return 1;
            }

            std::printf("[TEST] Split-screen collision/elimination/win-detection/retry executed successfully\n");
        }

        // 9. Full 4-player spawn validity -- regression check for a bug where
        // a player landing on face_number 1 or 2 got an invalid
        // start_previous_face (plain "face_number - 2" going to -1/0), so
        // ship::do_step() never advanced them and they crashed into their
        // own spawn tile one tick later. Checked across several maps since
        // which face number each slot lands on depends on the stage.
        {
            int dev2 = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1);
            int dev3 = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1);
            SDL_ControllerDeviceEvent ev2{}; ev2.which = dev2; controller_input::handle_device_added(ev2);
            SDL_ControllerDeviceEvent ev3{}; ev3.which = dev3; controller_input::handle_device_added(ev3);

            for (int map_idx = 0; map_idx < 5 && map_idx < total_stage_count(); map_idx++) {
                split_screen_menu::reset(g_game);
                g_game->actual_stage = 1000;
                g_game->game_status = 20;
                split_screen_menu::handle_key_down(g_game, SDLK_RETURN); // keyboard -> slot 0
                for (int ci = 0; ci < 3; ci++) {
                    SDL_GameController* c = controller_input::get_controller(ci);
                    SDL_ControllerButtonEvent be{};
                    be.which = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
                    be.button = SDL_CONTROLLER_BUTTON_A;
                    split_screen_menu::handle_controller_button_down(g_game, be);
                }
                split_screen_match::start_match(g_game, map_idx);
                for (int s = 0; s < 4; s++) {
                    if (!split_screen_match::player_at_slot(s)) {
                        std::fprintf(stderr, "[TEST FAIL] 4-player match on map %d: slot %d didn't spawn!\n", map_idx, s);
                        return 1;
                    }
                }
                for (int t = 0; t < 3; t++) split_screen_match::tick(g_game);
                for (int s = 0; s < 4; s++) {
                    if (split_screen_match::colision_for_slot(s) != 0) {
                        std::fprintf(stderr,
                            "[TEST FAIL] 4-player match on map %d: slot %d crashed with no steering input!\n", map_idx, s);
                        return 1;
                    }
                }
            }

            std::printf("[TEST] 4-player spawn validity (all slots, several maps) executed successfully\n");
        }

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
        // Split-screen's join/map-select/match screens handle their own
        // controller buttons (see SDL_CONTROLLERBUTTONDOWN above) and don't
        // support held-stick-repeat navigation -- skip controller_input's own
        // axis dispatch here too, or a held stick would leak into it and fire
        // the single-player main-menu/stage-select navigation underneath.
        if (g_game && g_game->game_status != 20 && g_game->game_status != 21 &&
            g_game->game_status != 23 && g_game->game_status != 24) {
            controller_input::poll_axes(g_game, SDL_GetTicks());
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
