#include "split_screen_menu.h"
#include "split_screen_match.h"
#include "neutron.h"
#include "stage_interface.h"
#include "controller_input.h"
#include "custom_stage.h"
#include "miniature.h"
#include "game_data.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>

namespace split_screen_menu {

namespace {

// stage_interface.cpp's begin_2d()/end_2d()/draw_hud_text() do exactly what
// this screen needs, but they're file-local `static` functions there --
// duplicated here rather than exposing them, to keep this new screen from
// requiring any change to that existing, working rendering code.
void begin_2d() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 480.0, 0.0, 272.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
}

void end_2d() {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void draw_text(const char* text, float x, float y, float char_size, int r, int g, int b, int a) {
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

const int MAX_SLOTS = 4;
// -2 = empty, -1 = keyboard, 0..3 = controller_input index.
int s_slot_source[MAX_SLOTS] = {-2, -2, -2, -2};

struct Color { int r, g, b; };
const Color SLOT_COLORS[MAX_SLOTS] = {
    {0, 255, 255},   // cyan
    {255, 0, 255},   // magenta
    {0, 255, 0},     // green
    {255, 140, 0},   // orange
};

int count_joined() {
    int n = 0;
    for (int i = 0; i < MAX_SLOTS; i++) if (s_slot_source[i] != -2) n++;
    return n;
}

int find_slot_for_source(int source) {
    for (int i = 0; i < MAX_SLOTS; i++) if (s_slot_source[i] == source) return i;
    return -1;
}

int find_empty_slot() {
    for (int i = 0; i < MAX_SLOTS; i++) if (s_slot_source[i] == -2) return i;
    return -1;
}

// Join if this source doesn't already hold a slot, unjoin if it does --
// shared by keyboard and every controller.
void toggle_join(int source) {
    int existing = find_slot_for_source(source);
    if (existing >= 0) {
        s_slot_source[existing] = -2;
        return;
    }
    int empty = find_empty_slot();
    if (empty >= 0) s_slot_source[empty] = source;
}

void try_start_match(neutron* game, int requesting_source) {
    if (find_slot_for_source(requesting_source) < 0) return; // only a joined device can start
    if (count_joined() < 2) return; // need at least 2 players for split-screen
    game->game_status = 21; // map select
    game->actual_stage = 3000; // reuses Stage Select's camera/miniature rendering
    game->create_miniatures();
    // Frame on miniature[0]'s waypoint, same as load_miniatures() does for
    // regular Stage Select -- a real, composed camera shot to start from,
    // even though the default *selection* is RANDOM (actual_miniature only
    // becomes a valid index once the player actually browses to one).
    if (game->game_cam && game->miniatures[0]) {
        miniature* m0 = game->miniatures[0];
        game->game_cam->update_pos(m0->cam[0], m0->cam[1], m0->cam[2],
                                    (float)m0->cam_center[0], (float)m0->cam_center[1],
                                    (float)m0->cam_center[2]);
    }
    game->actual_miniature = -1; // -1 is the "RANDOM" sentinel, and the default selection
}

void back_to_main_menu(neutron* game) {
    game->actual_stage = 1000;
    game->game_status = 6;
}

int controller_source_from_event(const SDL_ControllerButtonEvent& e) {
    for (int i = 0; i < 4; i++) {
        SDL_GameController* c = controller_input::get_controller(i);
        if (c && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c)) == e.which) return i;
    }
    return -1;
}

} // namespace

void reset(neutron* game) {
    (void)game;
    for (int i = 0; i < MAX_SLOTS; i++) s_slot_source[i] = -2;
}

void update_join_screen(neutron* game) {
    (void)game; // nothing to do per-tick; print_stage() already animates the background
}

void update_map_select(neutron* game) {
    game->speed = 30.0f;
    for (int i = 0; i < total_stage_count(); i++) {
        if (game->miniatures[i]) game->miniatures[i]->rotation = (game->miniatures[i]->rotation + 1) % 360;
    }
    game->on_miniature_change(); // safely no-ops when actual_miniature == -1 (RANDOM)
}

int slot_count() { return MAX_SLOTS; }
int slot_input_source(int slot) {
    if (slot < 0 || slot >= MAX_SLOTS) return -2;
    return s_slot_source[slot];
}

void print_join_screen(neutron* game) {
    begin_2d();
    draw_text("SPLIT SCREEN", 160.0f, 250.0f, 2.0f, 255, 255, 255, 255);
    draw_text("UP TO 4 PLAYERS - JOIN WITH KEYBOARD OR A CONTROLLER", 25.0f, 222.0f, 1.0f, 200, 200, 200, 220);

    for (int i = 0; i < MAX_SLOTS; i++) {
        float y = 185.0f - (float)i * 30.0f;
        char label[48];
        const Color& c = SLOT_COLORS[i];
        if (s_slot_source[i] == -2) {
            std::snprintf(label, sizeof(label), "SLOT %d: EMPTY", i + 1);
            draw_text(label, 90.0f, y, 1.5f, 140, 140, 140, 200);
        } else if (s_slot_source[i] == -1) {
            std::snprintf(label, sizeof(label), "SLOT %d: KEYBOARD", i + 1);
            draw_text(label, 90.0f, y, 1.5f, c.r, c.g, c.b, 255);
        } else {
            std::snprintf(label, sizeof(label), "SLOT %d: CONTROLLER %d", i + 1, s_slot_source[i] + 1);
            draw_text(label, 90.0f, y, 1.5f, c.r, c.g, c.b, 255);
        }
    }

    draw_text("[ENTER] JOIN/LEAVE (KEYBOARD)", 60.0f, 55.0f, 1.2f, 255, 255, 0, 220);
    draw_text("[A] JOIN/LEAVE   [B] BACK   (CONTROLLER)", 60.0f, 40.0f, 1.2f, 255, 255, 0, 220);
    if (count_joined() >= 2) {
        draw_text("[S] OR [START] BEGIN", 150.0f, 22.0f, 1.3f, 0, 255, 0, 240);
    } else {
        draw_text("NEED AT LEAST 2 PLAYERS TO BEGIN", 110.0f, 22.0f, 1.2f, 255, 120, 120, 220);
    }
    end_2d();
}

void print_map_select(neutron* game) {
    begin_2d();
    draw_text("SPLIT SCREEN - CHOOSE A MAP", 110.0f, 255.0f, 1.6f, 0, 255, 255, 255);

    const char* name = (game->actual_miniature == -1) ? "RANDOM" : stage_display_name(game->actual_miniature);
    draw_text("MAP:", 150.0f, 225.0f, 1.4f, 255, 255, 255, 220);
    draw_text(name, 230.0f, 225.0f, 1.4f, 255, 255, 0, 255);

    draw_text("[LEFT:RIGHT] BROWSE MAPS", 120.0f, 45.0f, 1.4f, 255, 255, 0, 255);
    draw_text("[ENTER] CONFIRM   [ESC] BACK", 110.0f, 22.0f, 1.4f, 200, 200, 200, 220);
    end_2d();
}

void handle_key_down(neutron* game, SDL_Keycode sym) {
    if (game->game_status == 20) {
        if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) { toggle_join(-1); return; }
        if (sym == SDLK_s) { try_start_match(game, -1); return; }
        if (sym == SDLK_ESCAPE) { reset(game); back_to_main_menu(game); return; }
        return;
    }
    if (game->game_status == 21) {
        int& sel = game->actual_miniature;
        int count = total_stage_count();
        if (sym == SDLK_LEFT || sym == SDLK_a) {
            if (sel <= -1) sel = count - 1; else sel--;
            return;
        }
        if (sym == SDLK_RIGHT || sym == SDLK_d) {
            if (sel >= count - 1) sel = -1; else sel++;
            return;
        }
        if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) {
            split_screen_match::start_match(game, sel);
            return;
        }
        if (sym == SDLK_ESCAPE) { game->game_status = 20; return; }
        return;
    }
}

void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e) {
    int source = controller_source_from_event(e);
    if (source < 0) return;

    if (game->game_status == 20) {
        if (e.button == SDL_CONTROLLER_BUTTON_A) { toggle_join(source); return; }
        if (e.button == SDL_CONTROLLER_BUTTON_START) { try_start_match(game, source); return; }
        if (e.button == SDL_CONTROLLER_BUTTON_B) { reset(game); back_to_main_menu(game); return; }
        return;
    }
    if (game->game_status == 21) {
        int& sel = game->actual_miniature;
        int count = total_stage_count();
        if (e.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || e.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
            if (sel <= -1) sel = count - 1; else sel--;
            return;
        }
        if (e.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || e.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
            if (sel >= count - 1) sel = -1; else sel++;
            return;
        }
        if (e.button == SDL_CONTROLLER_BUTTON_A) { split_screen_match::start_match(game, sel); return; }
        if (e.button == SDL_CONTROLLER_BUTTON_B) { game->game_status = 20; return; }
        return;
    }
}

} // namespace split_screen_menu
