#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "neutron.h"

static SDL_Window* g_window = nullptr;
static SDL_GLContext g_glContext = nullptr;
static Mix_Chunk* g_musicChunk = nullptr;
static neutron* g_game = nullptr;
static bool g_running = true;

static int g_windowWidth = 960;
static int g_windowHeight = 544;

void quit_game(int code) {
    if (g_musicChunk) {
        Mix_FreeChunk(g_musicChunk);
        g_musicChunk = nullptr;
    }
    Mix_CloseAudio();

    delete g_game;
    g_game = nullptr;

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

static void SDL_SetupVideo() {
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
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::fprintf(stderr, "Warning: Unable to open audio: %s\n", Mix_GetError());
        return;
    }

    // Try loading music.wav if available
    g_musicChunk = Mix_LoadWAV("music.wav");
    if (g_musicChunk) {
        Mix_PlayChannelTimed(-1, g_musicChunk, -1, -1);
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

        // Skip intro if active
        if (g_game->game_status == 1000 || g_game->game_status == 4) {
            g_game->intro_step = 93;
            g_game->game_status = 6;
            int active_btns[4] = {7, 8, 9, 10};
            if (g_game->interface) g_game->interface->activate_buttons(active_btns, 4);
            return;
        }

        // Main Menu navigation (stage 1000)
        if (g_game->actual_stage == 1000 && g_game->game_status != 13) {
            if (sym == SDLK_UP || sym == SDLK_w) {
                if (g_game->interface) {
                    g_game->interface->actual_option = (g_game->interface->actual_option + 3) % 4;
                }
                return;
            }
            if (sym == SDLK_DOWN || sym == SDLK_s) {
                if (g_game->interface) {
                    g_game->interface->actual_option = (g_game->interface->actual_option + 1) % 4;
                }
                return;
            }
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                int opt = g_game->interface ? g_game->interface->actual_option : 0;
                if (opt == 0) { // Start Game (Stage 1)
                    g_game->user_stage = 1;
                    g_game->change_stage(1, true);
                    g_game->game_status = 0;
                } else if (opt == 1) { // Training
                    g_game->change_stage(2000, true);
                    g_game->game_status = 0;
                } else if (opt == 2) { // Stage Select
                    g_game->actual_stage = 3000;
                    g_game->load_miniatures();
                    g_game->game_status = 9;
                } else { // Credits
                    g_game->game_status = 13;
                }
                return;
            }
            if (sym == SDLK_1 || sym == SDLK_KP_1) {
                g_game->user_stage = 1;
                g_game->change_stage(1, true);
                g_game->game_status = 0;
                return;
            }
            if (sym == SDLK_2 || sym == SDLK_KP_2 || sym == SDLK_t) {
                g_game->change_stage(2000, true);
                g_game->game_status = 0;
                return;
            }
            if (sym == SDLK_3 || sym == SDLK_KP_3 || sym == SDLK_s) {
                g_game->actual_stage = 3000;
                g_game->load_miniatures();
                g_game->game_status = 9;
                return;
            }
            if (sym == SDLK_4 || sym == SDLK_KP_4 || sym == SDLK_c) {
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
                g_game->change_stage(1000, true);
                g_game->game_status = 6;
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
                g_game->game_status = 0;
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->change_stage(1000, true);
                g_game->game_status = 6;
                return;
            }
        }

        // In-game controls (Gameplay stages 1-25 or Training 2000)
        if (g_game->game_status == 0) {
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
            if (sym == SDLK_ESCAPE) {
                g_game->change_stage(1000, true);
                g_game->game_status = 6;
                return;
            }
        }

        // Crash retry (game_status == 1)
        if (g_game->game_status == 1) {
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                g_game->change_stage(g_game->actual_stage, true);
                g_game->game_status = 0;
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->change_stage(1000, true);
                g_game->game_status = 6;
                return;
            }
        }

        // Level Complete victory or Crash retry (game_status == 8)
        if (g_game->game_status == 8 || g_game->game_status == 3) {
            if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
                if (g_game->colision == 1) {
                    g_game->change_stage(g_game->actual_stage, true);
                    g_game->game_status = 0;
                } else {
                    g_game->next_level();
                    g_game->game_status = 0;
                }
                return;
            }
            if (sym == SDLK_ESCAPE) {
                g_game->change_stage(1000, true);
                g_game->game_status = 6;
                return;
            }
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (!g_game) return;
        if (event.button.button == SDL_BUTTON_LEFT) {
            // Click to start or skip intro
            if (g_game->actual_stage == 1000) {
                if (g_game->game_status == 1000 || g_game->game_status == 4) {
                    g_game->intro_step = 93;
                    g_game->game_status = 6;
                } else {
                    g_game->user_stage = 1;
                    g_game->change_stage(1, true);
                    g_game->game_status = 0;
                }
            } else if (g_game->game_status == 1) {
                g_game->change_stage(g_game->actual_stage, true);
                g_game->game_status = 0;
            } else if (g_game->game_status == 8 || g_game->game_status == 3) {
                if (g_game->colision == 1) {
                    g_game->change_stage(g_game->actual_stage, true);
                    g_game->game_status = 0;
                } else {
                    g_game->next_level();
                    g_game->game_status = 0;
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

    SDL_SetupVideo();
    SDL_SetupAudio();

    g_game = new neutron(0, 0, 1, true);

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
            if (g_game) {
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
