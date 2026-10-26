#include "stage_interface.h"
#include "game_data.h"
#include <cstring>
#include <cstdio>

static void set_btn(button* b, int left, int top, int right, int bottom,
                    int key, int action, int action_type, const char* text, int alpha = 255) {
    b->left = left;
    b->top = top;
    b->right = right;
    b->bottom = bottom;
    b->key = key;
    b->action = action;
    b->action_type = action_type;
    b->color[0] = 255;
    b->color[1] = 255;
    b->color[2] = 255;
    b->color[3] = alpha;
    b->size = 0.2f;
    b->visible = false;
    b->enabled = false;
    b->load_text(text, -1);
}

stage_interface::stage_interface(int color, int difficulty, int stage, bool sound)
    : locator(new glinterface()),
      actual_option(0),
      choosen_color(color),
      max_stages(25),
      actual_staff(0) {

    whats[0] = "         original idea           ";
    whats[1] = "      designed and coded by      ";
    whats[2] = "            music by             ";
    whats[3] = "           tested by             ";
    whats[4] = "           thanks to             ";
    whats[5] = "          dedicated to           ";
    whats[6] = "  decompilation and recoding by  ";

    whos[0] = "          josep florit           ";
    whos[1] = "          josep florit           ";
    whos[2] = "          moish vexler           ";
    whos[3] = "  benoit duchesne, daniel sensenig ";
    whos[4] = "         pili and maria          ";
    whos[5] = "        laura miyashiro          ";
    whos[6] = "          everythingdigital1     ";

    std::memcpy(intro_positions, INTRO_DATA, sizeof(intro_positions));

    for (int i = 0; i < 40; i++) {
        buttons[i] = new button();
    }

    set_btn(buttons[0],  0, 0, 0, 0, 0, -1, -1, "  score:");
    set_btn(buttons[1],  0, 0, 0, 0, 0, -1, -1, "000000");
    set_btn(buttons[2],  0, 0, 0, 0, 0, -1, -1, "enemies:");
    set_btn(buttons[3],  0, 0, 0, 0, 0, -1, -1, "00");
    set_btn(buttons[4],  0, 0x110, 0xf0, 0xc, 3, 0, 0, "<", 0);
    set_btn(buttons[5],  0xf0, 0x110, 0x1e0, 0xc, 4, 1, 0, ">", 0);
    set_btn(buttons[6],  0, 0, 0, 0, 0, -1, -1, "");
    set_btn(buttons[7],  0x6e, 0xc0, 0x172, 0x16, 2, 2, 1, "game", 255);
    set_btn(buttons[8],  0x6e, 0xc0, 0x172, 0x16, 2, 3, 1, "setup", 25);
    set_btn(buttons[9],  0x6e, 0xc0, 0x172, 0x16, 2, 0x1b, 1, "staff", 25);
    set_btn(buttons[10], 0x6e, 0xc0, 0x172, 0x16, 2, 5, 1, "train", 25);
    set_btn(buttons[11], 0, 0x110, 0x1e0, 0, 3, 6, 2, "");
    set_btn(buttons[12], 0, 0x110, 0x1e0, 0, 4, 7, 3, "");
    set_btn(buttons[13], 0x195, 0x20, 0x1d1, -0x1c, 7, 8, 1, "exit");
    set_btn(buttons[14], 0xf, 0x20, 0x96, -0x1c, 2, 9, 1, "load enemy");
    set_btn(buttons[15], 0x6e, 0xc0, 0x172, 0x16, 2, 10, 1, "new", 25);
    set_btn(buttons[16], 0x6e, 0xc0, 0x172, 0x16, 2, 0xb, 1, "next", 25);
    set_btn(buttons[17], 0x6e, 0xc0, 0x172, 0x16, 2, 0xc, 1, "play", 25);
    set_btn(buttons[18], 0x6e, 0xc0, 0x172, 0x16, 2, 0xd, 1, "back", 25);
    set_btn(buttons[19], 0, 0x110, 0x1e0, 0, 0, 1, 2, "");
    set_btn(buttons[20], 0, 0x110, 0x1e0, 0, 0, 0, 3, "");
    set_btn(buttons[21], 0x6e, 0xc0, 0x172, 0x16, 2, 0x11, 1, "sound", 25);
    set_btn(buttons[22], 0x6e, 0xc0, 0x172, 0x16, 2, 0xf, 1, "color", 25);
    set_btn(buttons[23], 0x6e, 0xc0, 0x172, 0x16, 2, 0x10, 1, difficulty ? "hard" : "easy", 25);
    set_btn(buttons[24], 0x6e, 0xc0, 0x172, 0x16, 2, 0xe, 1, "back", 25);
    set_btn(buttons[25], 0, 0, 0, 0, 0, -1, -1, sound ? "on" : "off", 25);
    set_btn(buttons[26], 0x6e, 0xc0, 0x172, 0x16, 2, 0x12, 1, "try again", 25);
    set_btn(buttons[27], 0, 0x110, 0x1e0, 0, 3, 0x13, 2, "");
    set_btn(buttons[28], 0, 0x110, 0x1e0, 0, 4, 0x14, 3, "");
    set_btn(buttons[29], 0xf, 0x20, 0x82, -0x1c, 2, 0x15, 1, "play");
    set_btn(buttons[30], 0xf, 0x20, 300, -0x1c, 2, 0x16, 1, "tap to select or drag to move", 150);
    set_btn(buttons[31], 0, 0, 0, 0, 0, -1, -1, "stage", 25);
    set_btn(buttons[32], 0, 0, 0, 0, 0, -1, -1, "game", 25);
    set_btn(buttons[33], 0, 0, 0, 0, 0, -1, -1, "one", 25);
    set_btn(buttons[34], 0, 0x110, 0x1e0, 0, 6, 0x17, 4, "");
    set_btn(buttons[35], 0, 0x110, 0x1e0, 0, 5, 0x18, 5, "");
    set_btn(buttons[36], 0x5f, 0x20, 0xbe, -0x1c, 2, 0x19, 1, " yes ");
    set_btn(buttons[37], 0xf, 0x20, 0x55, -0x1c, 7, 0x1a, 1, " no ");
    set_btn(buttons[38], 0, 0, 0, 0, 0, -1, -1, whats[0], 200);
    set_btn(buttons[39], 0, 0, 0, 0, 0, -1, -1, whos[0], 200);

    menu_option[0] = 0;
    menu_option[1] = 2;
    menu_option[2] = 3;
    menu_option[3] = 5;
    menu_option[4] = 10;
    menu_option[5] = 0xb;
    menu_option[6] = 0xc;
    menu_option[7] = 0xd;
    menu_option[8] = 6;
    menu_option[9] = 7;
    menu_option[10] = 8;
    menu_option[11] = 9;
}

stage_interface::~stage_interface() {
    delete locator;
    for (int i = 0; i < 40; i++) {
        delete buttons[i];
        buttons[i] = nullptr;
    }
}

int stage_interface::who_is(int screen_x, int screen_y) {
    for (int i = 0; i < 40; i++) {
        button* b = buttons[i];
        if (b && b->enabled && b->visible) {
            if (screen_x >= b->left && screen_x <= b->right &&
                screen_y >= b->bottom && screen_y <= b->top) {
                return i;
            }
        }
    }
    return -1;
}

int stage_interface::catch_key(int key) {
    for (int i = 0; i < 40; i++) {
        button* b = buttons[i];
        if (b && b->enabled && b->key == key) {
            if (buttons[4]) buttons[4]->color[3] = 0;
            if (buttons[5]) buttons[5]->color[3] = 0;
            return b->action;
        }
    }
    if (buttons[4]) buttons[4]->color[3] = 0;
    if (buttons[5]) buttons[5]->color[3] = 0;
    return -1;
}

int stage_interface::catch_tap(int screen_x, int screen_y, int action_type) {
    for (int i = 0; i < 40; i++) {
        button* b = buttons[i];
        if (b && b->enabled) {
            if (screen_x >= b->left && screen_x <= b->right &&
                screen_y >= b->bottom && screen_y <= b->top) {
                if (b->action_type == action_type) {
                    if (buttons[4]) buttons[4]->color[3] = 0;
                    if (buttons[5]) buttons[5]->color[3] = 0;
                    return b->action;
                }
            }
        }
    }
    return -1;
}

void stage_interface::activate_buttons(const int* id_enableds, int count) {
    for (int i = 0; i < 40; i++) {
        if (buttons[i]->action != -1) {
            buttons[i]->enabled = false;
            buttons[i]->visible = false;
            buttons[i]->color[3] = 40;
        }
    }
    if (buttons[30]) buttons[30]->color[3] = 150;

    for (int j = 0; j < count; j++) {
        int idx = id_enableds[j];
        if (idx >= 0 && idx < 40) {
            buttons[idx]->enabled = true;
            buttons[idx]->visible = true;
            buttons[idx]->color[3] = 255;
            if (idx == 29) {
                if (buttons[30]) buttons[30]->color[3] = 255;
            }
        }
    }
}

static void begin_2d() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 480.0, 0.0, 272.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
}

static void end_2d() {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static void draw_hud_text(const char* text, float x, float y, float char_size, int r, int g, int b, int a) {
    if (!text) return;
    float cur_x = x;
    float step = char_size * 5.5f;
    float pixel_size = char_size * 0.45f;

    for (size_t i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        int glyph_idx = -1;

        if (ch >= 'A' && ch <= 'Z') glyph_idx = ch - 'A';
        else if (ch >= '0' && ch <= '9') glyph_idx = 26 + (ch - '0');
        else if (ch == ' ') glyph_idx = 36;
        else if (ch == ':') glyph_idx = 37;
        else if (ch == '<') glyph_idx = 38;
        else if (ch == '>') glyph_idx = 39;
        else if (ch == '.') glyph_idx = 40;
        else if (ch == ',') glyph_idx = 41;
        else if (ch == ';') glyph_idx = 42;
        else if (ch == '=') glyph_idx = 43;
        else if (ch == '#') glyph_idx = 44;
        else if (ch == '/') glyph_idx = 45;
        else if (ch == '[') glyph_idx = 38;
        else if (ch == ']') glyph_idx = 39;
        else if (ch == '-') glyph_idx = 43;

        if (glyph_idx >= 0 && glyph_idx < 49) {
            for (int row = 0; row < 6; row++) {
                for (int col = 0; col < 6; col++) {
                    if (FONT[glyph_idx][row][col] != 0) {
                        float px = cur_x + col * char_size;
                        float py = y - row * char_size;
                        glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
                        glBegin(GL_QUADS);
                        glVertex2f(px - pixel_size, py - pixel_size);
                        glVertex2f(px + pixel_size, py - pixel_size);
                        glVertex2f(px + pixel_size, py + pixel_size);
                        glVertex2f(px - pixel_size, py + pixel_size);
                        glEnd();
                    }
                }
            }
        }
        cur_x += step;
    }
}

void stage_interface::print_staff(int actual_stage) {
    begin_2d();
    draw_hud_text("NEUTRON CREDITS", 160.0f, 240.0f, 1.8f, 0, 255, 255, 255);
    if (actual_staff >= 0 && actual_staff < 6) {
        draw_hud_text(whats[actual_staff], 80.0f, 160.0f, 1.5f, 255, 255, 100, 240);
        draw_hud_text(whos[actual_staff], 80.0f, 120.0f, 1.6f, 100, 255, 255, 255);
    } else if (actual_staff == 6) {
        draw_hud_text("Decompilation And Recoding By", 70.0f, 182.0f, 1.4f, 255, 255, 100, 240);
        draw_hud_text("Everythingdigital1", 100.0f, 157.0f, 1.6f, 100, 255, 255, 255);
        draw_hud_text("AKA Dtpk, Daniel Sensenig", 85.0f, 134.0f, 1.3f, 100, 255, 255, 255);
        draw_hud_text("Started 2023 \"For The Gamers\"", 75.0f, 112.0f, 1.2f, 100, 255, 255, 255);
        draw_hud_text("Preserve your games", 100.0f, 86.0f, 1.4f, 255, 255, 100, 240);
    }
    draw_hud_text("PRESS [ESC] OR [ENTER] FOR MENU", 100.0f, 30.0f, 1.3f, 200, 200, 200, 220);
    end_2d();
}

void stage_interface::print_solo_interface(int actual_stage) {
    begin_2d();
    draw_hud_text("SELECT STAGE (25 LEVELS)", 115.0f, 255.0f, 1.6f, 0, 255, 255, 255);
    if (buttons[6]) {
        draw_hud_text("STAGE NAME:", 130.0f, 225.0f, 1.4f, 255, 255, 255, 220);
        buttons[6]->size = 1.4f;
        buttons[6]->print_2D(240.0f, 225.0f, 0.0f);
    }
    draw_hud_text("[LEFT:RIGHT] BROWSE STAGES", 110.0f, 45.0f, 1.4f, 255, 255, 0, 255);
    draw_hud_text("[ENTER] PLAY STAGE   [ESC] BACK", 100.0f, 22.0f, 1.4f, 200, 200, 200, 220);
    end_2d();
}

void stage_interface::print_tutorial_interface(int actual_stage, bool isTutorial) {
    begin_2d();
    draw_hud_text("TRAINING MODE", 175.0f, 255.0f, 1.6f, 0, 255, 255, 255);
    draw_hud_text("[A:D] STEER   [V] VIEW   [ESC] MENU", 75.0f, 25.0f, 1.3f, 200, 200, 200, 220);
    end_2d();
}

void stage_interface::print_train_interface(int actual_stage, bool paused) {
    begin_2d();
    draw_hud_text("TRAINING ARENA", 165.0f, 255.0f, 1.6f, 0, 255, 255, 255);
    if (paused) {
        draw_hud_text("PAUSED", 195.0f, 160.0f, 2.5f, 255, 255, 255, 255);
        draw_hud_text("[ENTER:CLICK] RESUME   [ESC] MENU", 90.0f, 110.0f, 1.4f, 200, 200, 200, 220);
    }
    draw_hud_text("[ENTER] SPAWN ENEMY   [A:D] STEER   [ESC] MENU", 40.0f, 25.0f, 1.3f, 255, 255, 0, 220);
    end_2d();
}

void stage_interface::print_game_interface(int actual_stage, int game_status, int colision, bool new_high_score, bool paused) {
    begin_2d();

    char buf[64];
    const char* stg_name = (actual_stage >= 1 && actual_stage <= 25) ? LEVEL_TABLE[actual_stage - 1].name : "GRID";
    std::snprintf(buf, sizeof(buf), "STAGE %02d: %s", actual_stage, stg_name);
    draw_hud_text(buf, 15.0f, 260.0f, 1.3f, 0, 255, 255, 240);

    draw_hud_text("SCORE:", 210.0f, 260.0f, 1.3f, 255, 255, 255, 240);
    if (buttons[1]) {
        buttons[1]->size = 1.3f;
        buttons[1]->print_2D(275.0f, 260.0f, 0.0f);
    }

    draw_hud_text("ENEMIES:", 370.0f, 260.0f, 1.3f, 255, 80, 80, 240);
    if (buttons[3]) {
        buttons[3]->size = 1.3f;
        buttons[3]->print_2D(445.0f, 260.0f, 0.0f);
    }

    if (paused) {
        draw_hud_text("PAUSED", 195.0f, 160.0f, 2.5f, 255, 255, 255, 255);
        draw_hud_text("[ENTER:CLICK] RESUME   [ESC] MENU", 90.0f, 110.0f, 1.4f, 200, 200, 200, 220);
    } else if (colision == 1) {
        draw_hud_text("YOU LOSE", 180.0f, 160.0f, 2.5f, 255, 50, 50, 255);
        if (buttons[26]) {
            buttons[26]->size = 1.6f;
            buttons[26]->print_2D(240.0f, 125.0f, 0.0f);
        }
        draw_hud_text("PRESS [ESC] FOR MENU", 135.0f, 90.0f, 1.5f, 200, 200, 200, 220);
    } else if (colision != 1 && (game_status == 8 || game_status == 3)) {
        if (new_high_score) {
            draw_hud_text("NEW HIGH SCORE!", 150.0f, 165.0f, 2.5f, 255, 255, 50, 255);
        } else {
            draw_hud_text("STAGE CLEAR!", 160.0f, 165.0f, 2.5f, 50, 255, 50, 255);
        }
        draw_hud_text("PRESS [ENTER] FOR NEXT STAGE", 100.0f, 100.0f, 1.6f, 0, 255, 255, 255);
    }

    draw_hud_text("[A:D] STEER   [V] VIEW   [ESC] MENU", 85.0f, 18.0f, 1.2f, 140, 180, 200, 180);
    end_2d();
}

void stage_interface::print_menu_interface(int actual_stage, int difficulty, bool sound, int color, int volume, bool fullscreen) {
    begin_2d();

    draw_hud_text("NEUTRON", 175.0f, 255.0f, 2.5f, 255, 255, 255, 255);
    draw_hud_text("3D LIGHTCYCLE COMBAT", 125.0f, 230.0f, 1.4f, 255, 255, 255, 220);

    if (actual_option >= 30 && actual_option < 34) {
        // Graphics & Sound Submenu (opened from Options -> Graphics & Sound)
        char sound_line[32];
        char volume_line[32];
        char fullscreen_line[32];
        std::snprintf(sound_line, sizeof(sound_line), "1. SOUND: %s", sound ? "ON" : "OFF");
        std::snprintf(volume_line, sizeof(volume_line), "2. VOLUME: %d%%", volume);
        std::snprintf(fullscreen_line, sizeof(fullscreen_line), "3. FULLSCREEN: %s", fullscreen ? "ON" : "OFF");

        draw_hud_text("GRAPHICS,SOUND", 150.0f, 205.0f, 1.6f, 255, 255, 255, 255);
        const char* sound_opts[4] = {
            sound_line,
            volume_line,
            fullscreen_line,
            "4. BACK"
        };
        for (int i = 0; i < 4; i++) {
            int opt_idx = 30 + i;
            bool sel = (actual_option == opt_idx);
            int r = 255, g = 255;
            int b = sel ? 0 : 255;
            int a = sel ? 255 : 220;
            float y = 170.0f - i * 24.0f;
            if (sel) {
                draw_hud_text(">", 115.0f, y, 1.8f, 255, 255, 0, 255);
            }
            draw_hud_text(sound_opts[i], 135.0f, y, 1.5f, r, g, b, a);
        }
        draw_hud_text("[LEFT:RIGHT] ADJUST VOLUME", 110.0f, 65.0f, 1.3f, 200, 200, 200, 200);
    } else if (actual_option >= 10 && actual_option < 14) {
        // Setup / Options Submenu
        static const char* color_names[3] = { "CYAN", "MAGENTA", "GREEN" };
        const char* color_name = (color >= 0 && color < 3) ? color_names[color] : "?";
        char color_line[32];
        char difficulty_line[32];
        std::snprintf(color_line, sizeof(color_line), "2. PLAYER COLOR: %s", color_name);
        std::snprintf(difficulty_line, sizeof(difficulty_line), "3. DIFFICULTY: %s", difficulty ? "HARD" : "EASY");

        draw_hud_text("SETUP & OPTIONS", 155.0f, 205.0f, 1.6f, 255, 255, 255, 255);
        const char* setup_opts[4] = {
            "1. GRAPHICS,SOUND",
            color_line,
            difficulty_line,
            "4. BACK TO MAIN MENU"
        };
        for (int i = 0; i < 4; i++) {
            int opt_idx = 10 + i;
            bool sel = (actual_option == opt_idx);
            int r = sel ? 255 : 255;
            int g = sel ? 255 : 255;
            int b = sel ? (sel ? 0 : 255) : 255;
            int a = sel ? 255 : 220;
            float y = 170.0f - i * 24.0f;
            if (sel) {
                draw_hud_text(">", 115.0f, y, 1.8f, 255, 255, 0, 255);
            }
            draw_hud_text(setup_opts[i], 135.0f, y, 1.5f, r, g, b, a);
        }
    } else {
        // Main Menu
        const char* opts[6] = {
            "1. NEW GAME",
            "2. CONTINUE GAME",
            "3. TRAINING MODE",
            "4. SELECT STAGE",
            "5. OPTIONS",
            "6. CREDITS"
        };
        for (int i = 0; i < 6; i++) {
            bool sel = (actual_option == i);
            int r = sel ? 255 : 255;
            int g = sel ? 255 : 255;
            int b = sel ? (sel ? 0 : 255) : 255;
            int a = sel ? 255 : 220;
            float y = 188.0f - i * 22.0f;
            if (sel) {
                draw_hud_text(">", 115.0f, y, 1.6f, 255, 255, 0, 255);
            }
            draw_hud_text(opts[i], 135.0f, y, 1.6f, r, g, b, a);
        }
    }

    draw_hud_text("[ENTER : SPACE] SELECT   [UP:DOWN] NAVIGATE", 55.0f, 48.0f, 1.3f, 255, 255, 255, 220);
    draw_hud_text("IN GAME: [A:D] STEER   [V] VIEW   [ESC] MENU", 40.0f, 25.0f, 1.2f, 200, 200, 200, 200);

    end_2d();
}

void stage_interface::print_intro(int intro_step) {
    if (!locator) return;
    for (int i = 0; i < 94; i++) {
        if (i <= intro_step) {
            float ix = (float)intro_positions[i][0];
            float iy = (float)intro_positions[i][1];
            float iz = 0.0f;
            if (i == intro_step || i == 93) {
                locator->square_3D(ix, iy, iz, 0, 255, 0, 150, 1.0f, 3);
            } else {
                locator->square_3D(ix, iy, iz, 255, 255, 255, 255, 0.5f, 3);
            }
        }
    }
}
