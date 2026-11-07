#include "menu_multiplayer.h"
#include "neutron.h"
#include "stage_interface.h"
#include "net_match.h"
#include "net_session.h"
#include "custom_stage.h"

#include <GL/gl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace menu_multiplayer {

namespace {

enum class Mode { ChooseRole, EnterName, HostLobby, JoinSetup, JoinLobby };

Mode s_mode = Mode::ChooseRole;
int s_role_cursor = 0; // 0 = Host, 1 = Join
std::string s_name_buf;
std::string s_ip_buf;
int s_lan_cursor = -1; // -1 = manual IP field has focus, >=0 = that discovered-game index
int s_discovery_scan_cooldown = 0;
std::string s_status_line;
int s_host_map_idx = -1; // host's map choice: -1 = random, else a real stage_display_name() index
                           // (0..total_stage_count()-1, including the custom map.txt slot if loaded)

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

// Shared by the name/IP text fields -- letters, digits, '.', backspace.
// Deliberately small: this isn't a general text widget, just enough for a
// player name and an IPv4 address.
bool edit_text_field(std::string& field, SDL_Keycode sym, size_t max_len) {
    if (sym == SDLK_BACKSPACE) {
        if (!field.empty()) field.pop_back();
        return true;
    }
    if (field.size() >= max_len) return false;
    if (sym >= SDLK_a && sym <= SDLK_z) { field += (char)('A' + (sym - SDLK_a)); return true; }
    if (sym >= SDLK_0 && sym <= SDLK_9) { field += (char)('0' + (sym - SDLK_0)); return true; }
    if (sym == SDLK_PERIOD || sym == SDLK_KP_PERIOD) { field += '.'; return true; }
    if (sym == SDLK_SPACE) { field += ' '; return true; }
    if (sym == SDLK_MINUS) { field += '-'; return true; }
    return false;
}

void return_to_main_menu(neutron* game) {
    net_session::leave_session();
    game->game_status = game->change_stage(1000, true);
}

// Generous vertical-band hit test, same spirit as touch_input.cpp's own
// hit_row() -- these rows are short single lines centered in a narrow
// logical screen, precision isn't needed.
bool hit_band(float ly, float row_y, float half_height = 10.0f) {
    return ly >= row_y - half_height && ly <= row_y + half_height;
}

// Same fixed 480-wide logical space print()'s begin_2d_fixed() sets up
// (glOrtho(0,480,0,272,-1,1)) -- matches touch_input.cpp's own LOGICAL_W.
const float LOGICAL_W = 480.0f;

} // namespace

void reset(neutron* game) {
    (void)game;
    s_mode = Mode::ChooseRole;
    s_role_cursor = 0;
    s_name_buf.clear();
    s_ip_buf.clear();
    s_lan_cursor = -1;
    s_discovery_scan_cooldown = 0;
    s_status_line.clear();
    s_host_map_idx = -1;
    net_session::init();
}

void update(neutron* game) {
    net_session::update();

    if (s_mode == Mode::JoinSetup) {
        if (s_discovery_scan_cooldown <= 0) {
            net_session::start_discovery_scan();
            s_discovery_scan_cooldown = 90; // re-scan roughly every ~4-5s at this engine's tick rates
        } else {
            s_discovery_scan_cooldown--;
        }
    }

    if (s_mode == Mode::JoinLobby && net_session::match_starting()) {
        net_match::start(game, net_session::match_map_idx(), net_session::match_seed());
        return;
    }
    if (s_mode == Mode::JoinLobby && net_session::role() == net_session::Role::None) {
        // Lost the connection (host closed/crashed) -- bounce back to setup.
        s_mode = Mode::JoinSetup;
        s_status_line = "DISCONNECTED FROM HOST";
    }
}

void print(neutron* game) {
    (void)game;
    begin_2d_fixed();

    draw_text_hud("MULTIPLAYER", 150.0f, 250.0f, 2.2f, 255, 255, 255, 255);

    if (s_mode == Mode::ChooseRole) {
        draw_text_hud(s_role_cursor == 0 ? ">" : " ", 120.0f, 190.0f, 1.6f, 255, 255, 0, 255);
        draw_text_hud("HOST A GAME", 140.0f, 190.0f, 1.6f, 255, 255, 255, 255);
        draw_text_hud(s_role_cursor == 1 ? ">" : " ", 120.0f, 165.0f, 1.6f, 255, 255, 0, 255);
        draw_text_hud("JOIN A GAME", 140.0f, 165.0f, 1.6f, 255, 255, 255, 255);
        draw_text_hud("[UP/DOWN] CHOOSE   [ENTER] CONFIRM   [ESC] BACK", 60.0f, 20.0f, 1.1f, 180, 180, 180, 200);

    } else if (s_mode == Mode::EnterName) {
        draw_text_hud("YOUR NAME (ENTER FOR DEFAULT):", 90.0f, 190.0f, 1.3f, 200, 200, 200, 220);
        std::string shown = s_name_buf + "_";
        draw_text_hud(shown.c_str(), 100.0f, 165.0f, 1.8f, 0, 255, 255, 255);
        draw_text_hud("[ENTER] CONTINUE   [ESC] BACK", 110.0f, 20.0f, 1.1f, 180, 180, 180, 200);

    } else if (s_mode == Mode::HostLobby) {
        char line[64];
        std::snprintf(line, sizeof(line), "HOSTING -- %d / %d PLAYERS",
                      net_session::connected_player_count(), net_session::max_player_count());
        draw_text_hud(line, 90.0f, 200.0f, 1.5f, 255, 255, 0, 255);
        for (int i = 0; i < net_session::connected_player_count(); i++) {
            std::string nm = net_session::player_name(i);
            if (nm.empty()) continue;
            char row[80];
            std::snprintf(row, sizeof(row), "%d. %s", i + 1, nm.c_str());
            draw_text_hud(row, 110.0f, 175.0f - i * 16.0f, 1.3f, 255, 255, 255, 230);
        }
        // Simple map picker -- not as elaborate as local split-screen's own
        // 3D miniature-preview map select, just the name (stage_display_name()
        // already resolves the custom map.txt slot's real name when that's
        // what's picked, same as everywhere else in this codebase shows it).
        // Whatever the host picks here is exactly what gets sent to clients
        // in the START message (see net_match::start()'s map_idx param) --
        // a joining player always gets the same map the host chose,
        // including a custom one.
        {
            char map_line[48];
            const char* map_name = (s_host_map_idx < 0) ? "RANDOM" : stage_display_name(s_host_map_idx);
            std::snprintf(map_line, sizeof(map_line), "< MAP: %s >", map_name);
            draw_text_hud(map_line, 100.0f, 108.0f, 1.3f, 255, 200, 0, 255);
        }

        bool can_start = net_session::connected_player_count() >= 2;
        draw_text_hud(can_start ? "[ENTER] START MATCH" : "NEED AT LEAST 2 PLAYERS",
                      90.0f, 85.0f, 1.4f, can_start ? 0 : 200, can_start ? 255 : 100, can_start ? 0 : 100, 255);
        draw_text_hud("[ESC] CANCEL HOSTING", 120.0f, 20.0f, 1.1f, 180, 180, 180, 200);

    } else if (s_mode == Mode::JoinSetup) {
        draw_text_hud("IP ADDRESS:", 90.0f, 215.0f, 1.3f, 200, 200, 200, 220);
        std::string ip_shown = s_ip_buf + (s_lan_cursor < 0 ? "_" : "");
        int ip_a = (s_lan_cursor < 0) ? 0 : 150, ip_b = (s_lan_cursor < 0) ? 255 : 150, ip_c = (s_lan_cursor < 0) ? 255 : 150;
        draw_text_hud(ip_shown.c_str(), 100.0f, 195.0f, 1.6f, ip_a, ip_b, ip_c, 255);

        draw_text_hud("LAN GAMES:", 90.0f, 165.0f, 1.3f, 200, 200, 200, 220);
        const auto& games = net_session::discovered_games();
        if (games.empty()) {
            draw_text_hud("(SEARCHING...)", 110.0f, 145.0f, 1.2f, 150, 150, 150, 200);
        }
        for (size_t i = 0; i < games.size() && i < 6; i++) {
            bool sel = (s_lan_cursor == (int)i);
            char row[96];
            std::snprintf(row, sizeof(row), "%s%s (%d/%d)", sel ? "> " : "  ",
                          games[i].host_name.c_str(), games[i].player_count, games[i].max_players);
            draw_text_hud(row, 100.0f, 145.0f - (float)i * 15.0f, 1.2f, sel ? 255 : 220, 255, sel ? 0 : 220, 255);
        }
        if (!s_status_line.empty()) draw_text_hud(s_status_line.c_str(), 90.0f, 45.0f, 1.2f, 255, 80, 80, 255);
        draw_text_hud("[UP/DOWN] PICK LAN GAME   [ENTER] CONNECT   [ESC] BACK", 20.0f, 20.0f, 1.0f, 180, 180, 180, 200);

    } else if (s_mode == Mode::JoinLobby) {
        draw_text_hud("CONNECTED -- WAITING FOR HOST TO START", 60.0f, 200.0f, 1.4f, 255, 255, 0, 255);
        for (int i = 0; i < net_session::connected_player_count(); i++) {
            std::string nm = net_session::player_name(i);
            if (nm.empty()) continue;
            char row[80];
            std::snprintf(row, sizeof(row), "%d. %s%s", i + 1, nm.c_str(),
                          (i == net_session::local_slot()) ? " (YOU)" : "");
            draw_text_hud(row, 110.0f, 175.0f - i * 16.0f, 1.3f, 255, 255, 255, 230);
        }
        draw_text_hud("[ESC] LEAVE", 150.0f, 20.0f, 1.1f, 180, 180, 180, 200);
    }

    end_2d();
}

void handle_key_down(neutron* game, SDL_Keycode sym) {
    if (s_mode == Mode::ChooseRole) {
        if (sym == SDLK_UP || sym == SDLK_DOWN || sym == SDLK_w || sym == SDLK_s) {
            s_role_cursor = 1 - s_role_cursor;
            return;
        }
        if (sym == SDLK_RETURN || sym == SDLK_SPACE) {
            s_mode = Mode::EnterName;
            return;
        }
        if (sym == SDLK_ESCAPE) { return_to_main_menu(game); return; }
        return;
    }

    if (s_mode == Mode::EnterName) {
        if (sym == SDLK_RETURN) {
            if (s_role_cursor == 0) {
                if (net_session::host_game(s_name_buf, 4)) {
                    s_mode = Mode::HostLobby;
                } else {
                    s_status_line = "COULD NOT START HOSTING";
                    s_mode = Mode::ChooseRole;
                }
            } else {
                s_mode = Mode::JoinSetup;
                net_session::start_discovery_scan();
            }
            return;
        }
        if (sym == SDLK_ESCAPE) { s_mode = Mode::ChooseRole; return; }
        edit_text_field(s_name_buf, sym, 15);
        return;
    }

    if (s_mode == Mode::HostLobby) {
        if (sym == SDLK_LEFT || sym == SDLK_a) {
            s_host_map_idx--;
            if (s_host_map_idx < -1) s_host_map_idx = total_stage_count() - 1;
            return;
        }
        if (sym == SDLK_RIGHT || sym == SDLK_d) {
            s_host_map_idx++;
            if (s_host_map_idx >= total_stage_count()) s_host_map_idx = -1;
            return;
        }
        if (sym == SDLK_RETURN && net_session::connected_player_count() >= 2) {
            // Resolve "RANDOM" to a CONCRETE index here, once, before
            // sending -- never transmit -1 itself. If every peer
            // independently re-rolled "random" on their own, they could
            // each land on a DIFFERENT map for what's supposed to be the
            // same match (see net_match.h's own comment on the parallel
            // spawn-seed issue this same pattern would cause).
            int resolved_map_idx = (s_host_map_idx < 0) ? (std::rand() % total_stage_count()) : s_host_map_idx;
            unsigned seed = (unsigned)SDL_GetTicks();
            net_session::start_match(resolved_map_idx, seed);
            net_match::start(game, resolved_map_idx, seed);
            return;
        }
        if (sym == SDLK_ESCAPE) { net_session::leave_session(); s_mode = Mode::ChooseRole; return; }
        return;
    }

    if (s_mode == Mode::JoinSetup) {
        const auto& games = net_session::discovered_games();
        if (sym == SDLK_UP) {
            if (s_lan_cursor < 0) s_lan_cursor = (int)games.size() - 1;
            else s_lan_cursor--;
            return;
        }
        if (sym == SDLK_DOWN) {
            s_lan_cursor++;
            if (s_lan_cursor >= (int)games.size()) s_lan_cursor = -1;
            return;
        }
        if (sym == SDLK_RETURN) {
            std::string target_ip = (s_lan_cursor >= 0 && s_lan_cursor < (int)games.size())
                                         ? games[(size_t)s_lan_cursor].ip
                                         : s_ip_buf;
            if (target_ip.empty()) { s_status_line = "ENTER AN IP OR PICK A LAN GAME"; return; }
            if (net_session::join_game(s_name_buf, target_ip)) {
                s_mode = Mode::JoinLobby;
                s_status_line.clear();
            } else {
                s_status_line = "COULD NOT CONNECT";
            }
            return;
        }
        if (sym == SDLK_ESCAPE) { s_mode = Mode::EnterName; return; }
        if (s_lan_cursor < 0) edit_text_field(s_ip_buf, sym, 15);
        return;
    }

    if (s_mode == Mode::JoinLobby) {
        if (sym == SDLK_ESCAPE) {
            net_session::leave_session();
            s_mode = Mode::JoinSetup;
            return;
        }
        return;
    }
}

void handle_touch(neutron* game, float lx, float ly) {
    if (s_mode == Mode::ChooseRole) {
        // Tapping a row both selects AND confirms -- a touch player has
        // no reason to select one role then separately confirm it, unlike
        // keyboard/controller where Up/Down and Enter are naturally
        // separate actions.
        if (hit_band(ly, 190.0f)) { s_role_cursor = 0; s_mode = Mode::EnterName; return; }
        if (hit_band(ly, 165.0f)) { s_role_cursor = 1; s_mode = Mode::EnterName; return; }
        return;
    }

    if (s_mode == Mode::EnterName) {
        // No on-screen keyboard -- tapping anywhere just confirms with
        // whatever's in the field (typically empty, which is exactly the
        // default-name scenario this whole field exists to make optional).
        handle_key_down(game, SDLK_RETURN);
        return;
    }

    if (s_mode == Mode::HostLobby) {
        // Map row ("< MAP: NAME >") -- tapping the left half cycles back,
        // right half cycles forward, same left/right-half convention used
        // for steering/retry/leave elsewhere in this touch support,
        // rather than needing precise taps on the literal "<"/">"
        // characters.
        if (hit_band(ly, 108.0f)) {
            handle_key_down(game, lx < LOGICAL_W * 0.5f ? SDLK_LEFT : SDLK_RIGHT);
            return;
        }
        if (hit_band(ly, 85.0f) && net_session::connected_player_count() >= 2) {
            handle_key_down(game, SDLK_RETURN);
            return;
        }
        if (hit_band(ly, 20.0f)) { handle_key_down(game, SDLK_ESCAPE); return; }
        return;
    }

    if (s_mode == Mode::JoinSetup) {
        // Manual IP entry needs typing either way, so touch's only real
        // path through this screen is tapping a LAN game directly --
        // tapping a row both selects AND connects immediately, same
        // reasoning as ChooseRole above.
        const auto& games = net_session::discovered_games();
        for (size_t i = 0; i < games.size() && i < 6; i++) {
            float row_y = 145.0f - (float)i * 15.0f;
            if (hit_band(ly, row_y, 7.5f)) {
                s_lan_cursor = (int)i;
                handle_key_down(game, SDLK_RETURN);
                return;
            }
        }
        return;
    }

    if (s_mode == Mode::JoinLobby) {
        if (hit_band(ly, 20.0f)) { handle_key_down(game, SDLK_ESCAPE); return; }
        return;
    }
}

} // namespace menu_multiplayer
