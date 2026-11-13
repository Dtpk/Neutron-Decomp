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

// True once a tap has focused the name or IP field and SDL_StartTextInput()
// has been called -- on Android/iOS this is what actually shows the
// on-screen keyboard (nothing in this codebase called it before, so the
// keyboard never appeared no matter what was tapped). Physical-keyboard/
// controller input keeps working identically regardless of this flag --
// see edit_text_field()'s own SDLK_* path below, untouched -- this only
// adds a SECOND input path (real SDL_TEXTINPUT text events, the ones an
// on-screen/IME keyboard actually sends) on top of it.
bool s_text_input_focused = false;

void focus_text_field() {
    if (!s_text_input_focused) {
        SDL_StartTextInput();
        s_text_input_focused = true;
    }
}

void unfocus_text_field() {
    if (s_text_input_focused) {
        SDL_StopTextInput();
        s_text_input_focused = false;
    }
}

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
    // Real bug this avoids: once a tap has focused the field (see
    // s_text_input_focused above), SDL_TEXTINPUT is active -- and a
    // PHYSICAL keyboard's printable keys reliably send both a matching
    // SDL_KEYDOWN and SDL_TEXTINPUT for the same press (this function's
    // own header comment already notes that), not just the on-screen
    // keyboard's. With both this function and edit_text_field_text()
    // (below) wired up, that meant every character got inserted twice
    // ("test" -> "tteesstt") the moment a field was focused, regardless
    // of which keyboard actually did the typing. Backspace above is
    // unaffected -- it has no SDL_TEXTINPUT counterpart at all, so it
    // only ever arrives here either way. Letters/digits/etc. below defer
    // to edit_text_field_text() instead while focused; unfocused (no tap
    // yet -- pure keyboard/controller navigation, SDL_TEXTINPUT never
    // active) this function alone still handles them, unchanged.
    if (s_text_input_focused) return false;
    if (field.size() >= max_len) return false;
    if (sym >= SDLK_a && sym <= SDLK_z) { field += (char)('A' + (sym - SDLK_a)); return true; }
    if (sym >= SDLK_0 && sym <= SDLK_9) { field += (char)('0' + (sym - SDLK_0)); return true; }
    if (sym == SDLK_PERIOD || sym == SDLK_KP_PERIOD) { field += '.'; return true; }
    if (sym == SDLK_SPACE) { field += ' '; return true; }
    if (sym == SDLK_MINUS) { field += '-'; return true; }
    return false;
}

// Same allowed character set as edit_text_field() above, but fed from a
// real SDL_TEXTINPUT event's UTF-8 text instead of a single SDLK_*
// keycode -- what an on-screen/IME keyboard actually sends (most physical-
// keyboard presses generate BOTH an SDL_KEYDOWN and a matching
// SDL_TEXTINPUT, but an IME's own on-screen keys are only guaranteed to
// send the latter). Backspace/Enter/Escape still arrive as real SDLK_*
// values even from Android's own keyboard, so those stay on
// edit_text_field()'s existing path untouched; this one only ever
// inserts printable characters. Non-ASCII/multi-byte UTF-8 bytes (an IME
// autocomplete inserting something outside this font's A-Z/0-9 set)
// are silently dropped one byte at a time, same as any other
// unsupported character already was.
void edit_text_field_text(std::string& field, const char* text, size_t max_len) {
    for (const char* p = text; *p != '\0'; p++) {
        if (field.size() >= max_len) return;
        char c = *p;
        if (c >= 'a' && c <= 'z') field += (char)(c - 'a' + 'A');
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) field += c;
        else if (c == '.' || c == ' ' || c == '-') field += c;
        // else: dropped -- not in this font's charset.
    }
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

// Heuristic cap on how many players a map's cube count can reasonably
// spread spawns/trails across without the whole thing turning into an
// immediate pileup -- 6 players per cube, so e.g. "cube" (1 cube) only
// supports up to 6, while "death star" (12 cubes, the biggest built-in
// map) supports up to 72. Tuned for "feels playable," not derived from
// anything exact; adjust this one constant if that balance feels wrong
// in practice. Only applies to the multiplayer host's map picker below --
// local split-screen is untouched and stays capped at 4 players regardless
// of map, same as before this feature existed.
const int PLAYERS_PER_CUBE = 6;

bool stage_fits_player_count(int idx, int player_count) {
    return stage_cube_count(idx) * PLAYERS_PER_CUBE >= player_count;
}

// Cycles s_host_map_idx by `direction` (-1 or +1), skipping over any
// concrete map that doesn't fit the CURRENT connected player count --
// RANDOM (-1) is always kept as a valid stop since it resolves to a
// fitting map at start-match time (see resolve_random_map_idx() below),
// never to a fixed one. Bounded to at most one full lap of every stage so
// a degenerate "nothing fits" case (shouldn't happen -- even "cube" fits
// up to 6, and this feature's own cap tops out at 12) can't spin forever.
void step_host_map(int direction) {
    int player_count = net_session::connected_player_count();
    int n = total_stage_count();
    for (int tries = 0; tries <= n; tries++) {
        s_host_map_idx += direction;
        if (s_host_map_idx < -1) s_host_map_idx = n - 1;
        else if (s_host_map_idx >= n) s_host_map_idx = -1;
        if (s_host_map_idx < 0 || stage_fits_player_count(s_host_map_idx, player_count)) break;
    }
}

// Resolves "RANDOM" to a concrete index, picking only among maps that fit
// player_count -- same reasoning as start_match_with_sources()'s own
// comment on resolving RANDOM once on the host: a map too small for the
// actual roster should never be the result, random or not.
int resolve_random_map_idx(int player_count) {
    int n = total_stage_count();
    int candidates[26]; // n is at most 26 (25 built-in + 1 optional custom)
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (stage_fits_player_count(i, player_count)) candidates[count++] = i;
    }
    if (count == 0) return 0; // degenerate fallback, shouldn't happen -- see step_host_map()'s comment
    return candidates[std::rand() % count];
}

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

void enter_lobby(neutron* game) {
    game->game_status = 30;
    s_mode = (net_session::role() == net_session::Role::Host) ? Mode::HostLobby : Mode::JoinLobby;
}

int validate_map_choice(int map_idx, int player_count) {
    // RANDOM is always valid as-is -- it resolves to a fitting map fresh
    // at start-match time (resolve_random_map_idx() above) regardless of
    // how the roster has changed since it was picked.
    if (map_idx < 0 || stage_fits_player_count(map_idx, player_count)) return map_idx;
    // Real gap this fixes: net_match.cpp's retry() used to replay
    // s_last_map_idx completely unchecked -- a roster that grew past what
    // that map could handle (mid-match joiners promoted in between, see
    // net_session::notify_match_ended()) would silently replay an
    // oversized match on the SAME too-small map, even though the
    // HostLobby screen's own map picker would never have let that
    // combination be chosen fresh. Same wrap-forward search as
    // step_host_map(1), but starting from map_idx itself rather than
    // s_host_map_idx, and never landing on RANDOM (-1) -- retry() always
    // wants a concrete replacement, not a fresh roll.
    int n = total_stage_count();
    for (int tries = 0; tries < n; tries++) {
        map_idx = (map_idx + 1) % n;
        if (stage_fits_player_count(map_idx, player_count)) return map_idx;
    }
    return 0; // degenerate fallback, same reasoning as resolve_random_map_idx()
}

void update(neutron* game) {
    net_session::update();

    // Safety net: whichever specific code path moved us off the one
    // screen a focused text field could exist on, make sure the
    // on-screen keyboard (if up) comes back down with it, rather than
    // needing every single mode-transition call site to separately
    // remember to call unfocus_text_field() itself.
    if (s_text_input_focused && s_mode != Mode::EnterName && s_mode != Mode::JoinSetup) {
        unfocus_text_field();
    }

    // A map picked while the roster was small can stop fitting once more
    // players join (see stage_fits_player_count()) -- bump off it onto the
    // next map that still fits rather than leaving an unplayable selection
    // sitting there silently. No-op whenever the current selection (or
    // RANDOM, always valid) still fits, so this is safe to check every
    // frame.
    if (s_mode == Mode::HostLobby && s_host_map_idx >= 0 &&
        !stage_fits_player_count(s_host_map_idx, net_session::connected_player_count())) {
        step_host_map(1);
    }

    if (s_mode == Mode::JoinSetup) {
        if (s_discovery_scan_cooldown <= 0) {
            net_session::start_discovery_scan();
            s_discovery_scan_cooldown = 90; // re-scan roughly every ~4-5s at this engine's tick rates
        } else {
            s_discovery_scan_cooldown--;
        }
    }

    if (s_mode == Mode::JoinLobby && net_session::match_starting()) {
        net_match::start(game, net_session::match_map_idx(), net_session::match_seed(), net_session::match_difficulty());
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
        // Real gap fixed here: a connection that arrives while a match is
        // already live gets parked by the host (net_session::
        // join_pending()) rather than folded in -- it used to show this
        // EXACT same text with no roster rows either way, giving no
        // indication a match was already running vs. just not started
        // yet.
        draw_text_hud(net_session::join_pending() ? "MATCH IN PROGRESS -- WAITING TO JOIN NEXT ONE"
                                                    : "CONNECTED -- WAITING FOR HOST TO START",
                      net_session::join_pending() ? 40.0f : 60.0f, 200.0f, 1.4f, 255, 255, 0, 255);
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
                if (net_session::host_game(s_name_buf, net_session::MAX_PLAYERS)) {
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
            step_host_map(-1);
            return;
        }
        if (sym == SDLK_RIGHT || sym == SDLK_d) {
            step_host_map(1);
            return;
        }
        if (sym == SDLK_RETURN && net_session::connected_player_count() >= 2) {
            // Resolve "RANDOM" to a CONCRETE index here, once, before
            // sending -- never transmit -1 itself. If every peer
            // independently re-rolled "random" on their own, they could
            // each land on a DIFFERENT map for what's supposed to be the
            // same match (see net_match.h's own comment on the parallel
            // spawn-seed issue this same pattern would cause).
            int resolved_map_idx = (s_host_map_idx < 0)
                ? resolve_random_map_idx(net_session::connected_player_count())
                : s_host_map_idx;
            unsigned seed = (unsigned)SDL_GetTicks();
            // Host's own current difficulty becomes the shared pacing
            // speed for every peer in this match -- see net_match::
            // start()'s own header comment for why this is kept separate
            // from each player's own game->user_difficulty (which stays
            // tied to their personal easy-mode guides, unaffected).
            // Custom map data only actually needs sending when THIS is
            // the custom stage -- serialize_custom_stage() itself already
            // returns "" if g_custom_stage isn't loaded, so this is safe
            // (and a harmless no-op send) even if resolved_map_idx isn't
            // 25 for some reason.
            std::string custom_map_data = (resolved_map_idx == 25) ? serialize_custom_stage() : "";
            net_session::start_match(resolved_map_idx, seed, game->user_difficulty, custom_map_data);
            net_match::start(game, resolved_map_idx, seed, game->user_difficulty);
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
        // The hint drawn here is ONE line, "[UP/DOWN] CHOOSE   [ENTER]
        // CONFIRM   [ESC] BACK" -- real gap this fixes: treating the
        // whole row as "ESC" (like every single-action row elsewhere in
        // this touch support does) meant tapping the visible "[ENTER]
        // CONFIRM" text on the LEFT actually went BACK to the main menu
        // too. Split left/right instead, same convention as HostLobby's
        // map row / match-over's enter-escape row -- "ENTER CONFIRM" has
        // no independent touch meaning here (tapping a row above already
        // both selects AND confirms), so the left half is just a safe
        // no-op rather than inventing one.
        if (hit_band(ly, 20.0f)) {
            if (lx >= LOGICAL_W * 0.5f) return_to_main_menu(game);
            return;
        }
        return;
    }

    if (s_mode == Mode::EnterName) {
        // Tapping the name text itself focuses it for the on-screen
        // keyboard (see focus_text_field()'s own comment) -- real gap
        // this fixes: nothing anywhere called SDL_StartTextInput(), so
        // the keyboard never appeared no matter what was tapped, leaving
        // touch players stuck with whatever default name applied.
        if (hit_band(ly, 165.0f)) { focus_text_field(); return; }
        // Real gap this fixes: the hint here is ONE line too,
        // "[ENTER] CONTINUE   [ESC] BACK" -- the same blind "whole row =
        // ESC" bug as ChooseRole above, but THIS screen used to also
        // have a "tap anywhere else confirms" fallback, so the visible
        // "[ENTER] CONTINUE" text landing on "ESC" instead was a real,
        // reported regression (read as "I have to tap the name field
        // just to continue" -- that field's own focus zone was the only
        // tap left that didn't accidentally go back). Split left/right
        // and drop the old blanket fallback now that the actual CONTINUE
        // button works correctly -- a deliberate button beats "tap
        // anywhere," same reasoning as match-over's own escape fix.
        if (hit_band(ly, 20.0f)) {
            if (lx < LOGICAL_W * 0.5f) handle_key_down(game, SDLK_RETURN);
            else s_mode = Mode::ChooseRole;
            return;
        }
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
        // Real gap this fixes: manual IP entry used to be keyboard-only
        // in practice -- tapping the IP text itself now focuses it for
        // the on-screen keyboard (see focus_text_field()), same as
        // EnterName's name field above. Switches to manual-entry mode
        // (s_lan_cursor = -1) if a LAN game row happened to be selected.
        if (hit_band(ly, 195.0f)) { s_lan_cursor = -1; focus_text_field(); return; }
        // Hint here is ONE line too, "[UP/DOWN] PICK LAN GAME   [ENTER]
        // CONNECT   [ESC] BACK" -- same left/right split as ChooseRole/
        // EnterName above, but LEFT gets a real action here: there was
        // previously no touch path at all to submit a manually-typed IP
        // (only tapping a discovered LAN game row could connect) --
        // tapping "[ENTER] CONNECT" now actually does, completing the
        // on-screen-keyboard IP entry flow added above.
        if (hit_band(ly, 20.0f)) {
            if (lx < LOGICAL_W * 0.5f) handle_key_down(game, SDLK_RETURN);
            else handle_key_down(game, SDLK_ESCAPE);
            return;
        }
        // Tapping a LAN game row both selects AND connects immediately,
        // same reasoning as ChooseRole above.
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

void handle_text_input(neutron* game, const char* text) {
    (void)game;
    if (!s_text_input_focused) return;
    if (s_mode == Mode::EnterName) {
        edit_text_field_text(s_name_buf, text, 15);
    } else if (s_mode == Mode::JoinSetup && s_lan_cursor < 0) {
        edit_text_field_text(s_ip_buf, text, 15);
    }
}

void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e) {
    switch (e.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    handle_key_down(game, SDLK_UP); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  handle_key_down(game, SDLK_DOWN); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  handle_key_down(game, SDLK_LEFT); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: handle_key_down(game, SDLK_RIGHT); break;
        case SDL_CONTROLLER_BUTTON_A:          handle_key_down(game, SDLK_RETURN); break;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_START:      handle_key_down(game, SDLK_ESCAPE); break;
        default: break;
    }
}

} // namespace menu_multiplayer
