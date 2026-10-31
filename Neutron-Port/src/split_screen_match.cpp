#include "split_screen_match.h"
#include "split_screen_menu.h"
#include "neutron.h"
#include "controller_input.h"
#include "custom_stage.h"
#include "miniature.h"
#include "terrain.h"
#include "plane.h"
#include "player.h"
#include "camera.h"
#include "sound_mixer.h"
#include "game_data.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstdlib>

// Implemented in main.cpp -- see controller_input.cpp's identical forward
// declaration for why this lives there instead of here (this module has no
// access to, and shouldn't own, the SDL window handle).
void app_get_window_size(int* w, int* h);

namespace split_screen_match {

namespace {

// Same duplicated-on-purpose 2D text helpers as split_screen_menu.cpp (see
// its own comment for why: stage_interface.cpp's versions are file-local).
void begin_2d_fixed() {
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

const int MAX_PLAYERS = 4;
player* s_players[MAX_PLAYERS] = {nullptr, nullptr, nullptr, nullptr};
int s_colision[MAX_PLAYERS] = {0, 0, 0, 0}; // 0 alive, 1 eliminated
int s_input_source[MAX_PLAYERS] = {-2, -2, -2, -2}; // -2 empty, -1 keyboard, 0..3 controller index
int s_winner_slot = -1; // -1 = draw/none
int s_last_map_idx = -1; // resolved (post-RANDOM-roll) map, reused by retry

struct Color { int r, g, b; };
const Color SLOT_COLORS[MAX_PLAYERS] = {
    {0, 255, 255},   // cyan
    {255, 0, 255},   // magenta
    {0, 255, 0},     // green
    {255, 140, 0},   // orange
};

void cleanup_players() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        delete s_players[i];
        s_players[i] = nullptr;
    }
}

void spawn_player(neutron* game, int slot) {
    terrain* ground = game->ground;
    if (!ground || ground->N_PLANES < 1) return;

    player* p = new player();
    p->ident = slot + 1; // 1..4 -- never 0, which stays reserved for neutron::proton
    const Color& c = SLOT_COLORS[slot];
    p->material[0] = c.r;
    p->material[1] = c.g;
    p->material[2] = c.b;
    p->material[3] = 200;

    // Distinct plane per slot whenever there are enough planes to give one
    // (true for any real stage -- tiny/degenerate ones just wrap and share).
    // Within the plane, offset from center by a different amount per axis
    // per slot (not just always-dead-center) so two players sharing a plane
    // on a small stage still don't spawn on the exact same piece.
    int pl = (ground->start_on_plane + slot) % ground->N_PLANES;
    plane* pp = ground->map[pl];
    if (!pp) {
        for (int k = 0; k < ground->N_PLANES; k++) {
            if (ground->map[k]) { pl = k; pp = ground->map[k]; break; }
        }
    }
    if (!pp) { delete p; return; }

    // Keep offsets within an inner margin, not the full 0..side-1 range --
    // the plain mod-side version let slot 3's multiplier wrap all the way
    // onto the last valid index (a literal corner piece, with little to no
    // room to move), which is why the 4th player was dying immediately.
    int side = pp->side_size > 0 ? pp->side_size : 1;
    int margin = side / 4;
    int safe_width = side - margin * 2;
    if (safe_width < 1) { margin = 0; safe_width = side; }
    int off_a = margin + (slot * 3) % safe_width;
    int off_b = margin + (slot * 5) % safe_width;

    p->start_plane = pl;
    p->actual_plane = pl;
    p->start_actual_face = pp->face_number;
    // load_proton()'s "face_number - 2" is only ever fed start_on_plane, a
    // single curated starting plane that apparently never has face_number
    // 1 or 2 on any real stage -- fine there, but split-screen's other
    // slots land on arbitrary planes and CAN have face_number 1 or 2,
    // where plain "- 2" goes to 0 or -1. ship::do_step()'s face-transition
    // switch doesn't recognize those as a valid previous face, so the ship
    // never actually advances -- it re-claims its own spawn tile every
    // tick and crashes into itself one tick later. Wrap within the valid
    // 1..6 face range instead (identical to the plain "-2" for every
    // face_number where that already worked).
    p->start_previous_face = ((pp->face_number - 2 - 1 + 6) % 6) + 1;

    if (pp->dead_axis == 1) {
        p->start_x = pp->dead_offset;
        p->start_y = pp->i + off_a;
        p->start_z = pp->j + off_b;
    } else if (pp->dead_axis == 2) {
        p->start_x = pp->i + off_a;
        p->start_y = pp->dead_offset;
        p->start_z = pp->j + off_b;
    } else {
        p->start_x = pp->i + off_a;
        p->start_y = pp->j + off_b;
        p->start_z = pp->dead_offset;
    }

    p->restart();
    p->init(ground->map);
    s_players[slot] = p;
}

int find_slot_for_keyboard() {
    for (int i = 0; i < MAX_PLAYERS; i++) if (s_input_source[i] == -1) return i;
    return -1;
}

int find_slot_for_controller(SDL_JoystickID id) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (s_input_source[i] < 0) continue;
        SDL_GameController* c = controller_input::get_controller(s_input_source[i]);
        if (c && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c)) == id) return i;
    }
    return -1;
}

void eliminate_player(neutron* game, int slot) {
    player* p = s_players[slot];
    if (!p) return;
    p->status = 6;
    p->effect_size = p->size;
    p->effect_alpha = (float)p->material[3];
    if (game->ground) game->ground->fire_up_cue(p->ident);
    s_colision[slot] = 1;
    if (game->mixer) game->mixer->play_effect(6);
}

void end_match(neutron* game) {
    int alive = 0, last = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (s_players[i] && s_colision[i] == 0) { alive++; last = i; }
    }
    s_winner_slot = (alive == 1) ? last : -1;
    game->game_status = 24;
}

void return_to_main_menu(neutron* game) {
    cleanup_players();
    game->game_status = game->change_stage(1000, true); // fully rebuilds ground/proton/menu_battle
}

} // namespace

void start_match(neutron* game, int map_idx) {
    cleanup_players();
    // Remember who was joined (slot_input_source) across a retry -- only
    // re-read it from split_screen_menu on a genuinely fresh start (map_idx
    // == -2 is retry's own sentinel, see retry_match() below).
    if (map_idx != -2) {
        for (int i = 0; i < MAX_PLAYERS; i++) s_input_source[i] = split_screen_menu::slot_input_source(i);
    }
    for (int i = 0; i < MAX_PLAYERS; i++) s_colision[i] = 0;
    s_winner_slot = -1;

    int count = total_stage_count();
    if (map_idx == -2) map_idx = s_last_map_idx; // retry: same map as last time
    if (map_idx < 0 || map_idx >= count) map_idx = std::rand() % count;
    s_last_map_idx = map_idx;

    // Mirrors load_stage()'s real-stage branch -- terrain/miniature
    // construction is identical regardless of player count; only the enemy-
    // stocking tail (autoload_eletrons()) is skipped, since split-screen is
    // pure PvP (no AI), and the single proton is never touched.
    game->unload_stage();
    game->create_miniatures();
    game->ground = new terrain();
    game->ground->create_from_miniature(game->miniatures[map_idx]);
    game->ground->init(0, 0, 0, 20);
    game->ground->status = 4;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (s_input_source[i] != -2) spawn_player(game, i);
    }

    game->actual_stage = 4000;
    game->game_status = 23;
}

void tick(neutron* game) {
    if (!game->ground) return;

    // Same difficulty slider single-player uses, and the same two values
    // (neutron::main_loop() resets `speed` to 30.0f before dispatching here
    // every tick, so this has to be reasserted every call, not just once at
    // match start -- mirrors combat()'s own case 0 in main_loop()'s switch).
    game->speed = (game->user_difficulty == 1) ? 55.0f : 75.0f;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        player* p = s_players[i];
        if (!p || s_colision[i] != 0) continue;
        if (p->step_counter < p->step_speed) {
            p->step_counter++;
            continue;
        }
        p->step_counter = 0;
        int res = p->move(game->ground->map, 1); // mode 1: combat/persistent trail
        if (res != 0) eliminate_player(game, i);
    }

    int alive = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (s_players[i] && s_colision[i] == 0) alive++;
    }
    if (alive <= 1) end_match(game);
}

void update_match_over(neutron* game) {
    (void)game;
}

void print_match(neutron* game) {
    terrain* ground = game->ground;
    if (!ground) return;

    int window_w = 960, window_h = 544;
    app_get_window_size(&window_w, &window_h);

    int active_slots[MAX_PLAYERS];
    int active_count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (s_players[i]) active_slots[active_count++] = i;
    }
    if (active_count == 0) return;

    struct Rect { int x, y, w, h; };
    Rect rects[MAX_PLAYERS];
    if (active_count <= 1) {
        rects[0] = {0, 0, window_w, window_h};
    } else if (active_count == 2) {
        rects[0] = {0, 0, window_w / 2, window_h};
        rects[1] = {window_w / 2, 0, window_w - window_w / 2, window_h};
    } else {
        int hw = window_w / 2, hh = window_h / 2;
        rects[0] = {0, hh, hw, window_h - hh};
        rects[1] = {hw, hh, window_w - hw, window_h - hh};
        rects[2] = {0, 0, hw, hh};
        rects[3] = {hw, 0, window_w - hw, hh};
    }

    for (int q = 0; q < active_count; q++) {
        int slot = active_slots[q];
        player* p = s_players[slot];
        const Rect& r = rects[q];
        if (r.w <= 0 || r.h <= 0) continue;

        glViewport(r.x, r.y, r.w, r.h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)r.w / (double)r.h, 1.0, 500.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        if (p->cam_ship) p->cam_ship->locate();

        ground->print(true, p->actual_plane, game->low_quality);
        for (int j = 0; j < MAX_PLAYERS; j++) {
            if (s_players[j]) s_players[j]->print(0, game->low_quality);
        }

        // Same difficulty-gated guide markers single-player shows next to
        // its own ship (print_stage()'s "user_difficulty == 0" check) --
        // each viewport shows its own player's guides, easy only.
        if (game->user_difficulty == 0 && s_colision[slot] == 0 && p->status == 1) {
            plane* pp = ground->map[p->actual_plane];
            if (pp) p->print_guides(pp->face_number, pp->dead_axis);
        }
    }

    // Divider lines + per-quadrant status, drawn once over the whole window.
    glViewport(0, 0, window_w, window_h);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)window_w, 0.0, (double)window_h, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    glColor4ub(255, 255, 255, 180);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    if (active_count == 2) {
        glVertex2f((float)(window_w / 2), 0.0f);
        glVertex2f((float)(window_w / 2), (float)window_h);
    } else if (active_count >= 3) {
        glVertex2f(0.0f, (float)(window_h / 2));
        glVertex2f((float)window_w, (float)(window_h / 2));
        glVertex2f((float)(window_w / 2), 0.0f);
        glVertex2f((float)(window_w / 2), (float)window_h);
    }
    glEnd();

    for (int q = 0; q < active_count; q++) {
        int slot = active_slots[q];
        const Rect& r = rects[q];
        const Color& c = SLOT_COLORS[slot];
        char label[24];
        std::snprintf(label, sizeof(label), "P%d %s", slot + 1, s_colision[slot] == 0 ? "" : "OUT");
        // Window-pixel-space text, independent of the fixed 480x272 HUD
        // space used elsewhere -- this overlay is positioned by real
        // quadrant rects, which only make sense in real window pixels.
        draw_text(label, (float)r.x + 10.0f, (float)(r.y + r.h) - 20.0f, 1.3f, c.r, c.g, c.b, 230);
    }

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void print_match_over(neutron* game) {
    (void)game;
    begin_2d_fixed();
    if (s_winner_slot >= 0) {
        const Color& c = SLOT_COLORS[s_winner_slot];
        char label[24];
        std::snprintf(label, sizeof(label), "PLAYER %d WINS!", s_winner_slot + 1);
        draw_text(label, 130.0f, 160.0f, 2.0f, c.r, c.g, c.b, 255);
    } else {
        draw_text("DRAW!", 190.0f, 160.0f, 2.0f, 255, 255, 255, 255);
    }
    draw_text("PRESS ENTER OR ESC FOR MENU", 100.0f, 110.0f, 1.3f, 200, 200, 200, 220);
    end_2d();
}

void handle_key_down(neutron* game, SDL_Keycode sym) {
    if (game->game_status == 23) {
        int slot = find_slot_for_keyboard();
        if (slot >= 0 && s_colision[slot] == 0 && s_players[slot]) {
            if (sym == SDLK_LEFT || sym == SDLK_a || sym == SDLK_q) { s_players[slot]->change_direction(0); return; }
            if (sym == SDLK_RIGHT || sym == SDLK_d || sym == SDLK_e) { s_players[slot]->change_direction(1); return; }
        }
        if (sym == SDLK_ESCAPE) { return_to_main_menu(game); return; }
        return;
    }
    if (game->game_status == 24) {
        if (sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) { start_match(game, -2); return; }
        if (sym == SDLK_ESCAPE) { return_to_main_menu(game); return; }
        return;
    }
}

void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e) {
    if (game->game_status == 23) {
        int slot = find_slot_for_controller(e.which);
        if (slot >= 0 && s_colision[slot] == 0 && s_players[slot]) {
            if (e.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || e.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                s_players[slot]->change_direction(0);
                return;
            }
            if (e.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || e.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                s_players[slot]->change_direction(1);
                return;
            }
        }
        if (e.button == SDL_CONTROLLER_BUTTON_START || e.button == SDL_CONTROLLER_BUTTON_B) {
            return_to_main_menu(game);
        }
        return;
    }
    if (game->game_status == 24) {
        if (e.button == SDL_CONTROLLER_BUTTON_A) { start_match(game, -2); return; }
        if (e.button == SDL_CONTROLLER_BUTTON_B || e.button == SDL_CONTROLLER_BUTTON_START) {
            return_to_main_menu(game);
        }
        return;
    }
}

int colision_for_slot(int slot) {
    if (slot < 0 || slot >= MAX_PLAYERS || !s_players[slot]) return -1;
    return s_colision[slot];
}

int winner_slot() { return s_winner_slot; }

player* player_at_slot(int slot) {
    if (slot < 0 || slot >= MAX_PLAYERS) return nullptr;
    return s_players[slot];
}

} // namespace split_screen_match
