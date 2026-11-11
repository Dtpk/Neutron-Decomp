#pragma once

#include <SDL2/SDL.h>

class neutron;

// The Multiplayer menu flow: Host/Join choice -> name entry -> either a
// host lobby (shows roster, host starts the match once >=2 players are in)
// or a join screen (manual IP entry + a periodically-refreshed LAN
// discovery list). Isolated the same way every other menu/feature file in
// this codebase is -- its own state, driving neutron only through existing
// public members/methods, talking to the actual network plumbing only
// through net_session's own public API (see net_session.h).
//
// Reuses one new game_status value on the existing actual_stage 1000 (so
// menu_battle's animated background keeps running behind it, same as every
// other main-menu submenu):
//   game_status 30 -- multiplayer menu (all its own sub-modes below)
//
// Hands off to net_match::start() once a networked match actually starts
// (either the host triggering it, or a client receiving the host's
// START) -- see net_match.h for how that handoff drives split_screen_match
// without this file needing to know split_screen_match exists at all.
namespace menu_multiplayer {

// Call when the main menu's "MULTIPLAYER" entry is selected, before setting
// game_status = 30, so a previous visit's state doesn't linger.
void reset(neutron* game);

// Called by net_match.cpp's cancel() when a live match is cancelled back
// to the lobby (host-initiated, or a client reacting to the host's
// CANCEL) -- unlike reset() above, this does NOT touch s_host_map_idx,
// s_name_buf, or re-init the session: the whole point of cancelling
// (rather than leave_session()-ing) is that the roster/connection survive,
// so the lobby should come back exactly as it was, not blanked out. Sets
// game_status = 30 itself and picks HostLobby or JoinLobby from
// net_session::role() -- the caller doesn't need to know which.
void enter_lobby(neutron* game);

// Re-validates a map choice against a player count, auto-advancing
// (same heuristic as the HostLobby map picker's own step_host_map()) to
// the next map that fits if it no longer does. map_idx may be -1
// (RANDOM), always returned unchanged -- it resolves fresh at
// start-match time regardless. net_match.cpp's retry() calls this before
// replaying, since a roster that grew since the map was first chosen
// (mid-match joiners promoted in between) could otherwise silently
// replay an oversized match on a too-small map -- only the HostLobby
// screen itself used to re-check this.
int validate_map_choice(int map_idx, int player_count);

// Per-tick, called from main_loop()'s game_status 30 case. Pumps
// net_session::update() and, on the join screen, re-triggers a LAN
// discovery scan periodically so the list stays current.
void update(neutron* game);

// Rendering, called from neutron::print()'s existing dispatch.
void print(neutron* game);

// Keyboard, routed from main.cpp only while game_status is 30. Handles
// both menu navigation and the text-entry fields (name/IP) -- there's no
// general-purpose text widget elsewhere in this codebase, so this owns a
// small one itself, scoped to just what these two fields need (letters,
// digits, '.', backspace, enter, escape).
void handle_key_down(neutron* game, SDL_Keycode sym);

// Touch, routed from touch_input.cpp only while game_status is 30.
// (lx, ly) are in the same fixed 480x272 logical space print()'s own
// begin_2d_fixed() sets up, already converted from raw SDL touch
// coordinates (see touch_input.cpp's own LOGICAL_W/LOGICAL_H and y-flip).
// Real row-by-row navigation (not just "tap anywhere = Enter"), since
// Host/Join and picking a LAN game both need to be choosable by touch
// without a keyboard: tapping a specific row selects (and, where that's
// the only sensible action -- a LAN game, which can't be "typed" either
// way -- also confirms) rather than just moving a cursor. Tapping the
// name or IP text itself focuses it for the on-screen keyboard (see
// handle_text_input() below) instead of needing a physical one.
void handle_touch(neutron* game, float lx, float ly);

// Routed from main.cpp's SDL_TEXTINPUT handling only while game_status is
// 30 -- real on-screen/IME keyboard text (a UTF-8 string, usually one
// character but not guaranteed to be) for whichever field a tap most
// recently focused (see handle_touch()'s own comment and
// focus_text_field()'s, menu_multiplayer.cpp). No-op if nothing is
// currently focused, so routing this unconditionally from main.cpp is
// safe even outside EnterName/JoinSetup.
void handle_text_input(neutron* game, const char* text);

// Controller, routed from main.cpp only while game_status is 30. Real
// gap this fixes: main.cpp used to deliberately SWALLOW every controller
// button here outright (reported: no controller input registers at all
// on this menu, Steam Deck being the most obvious case where that
// matters) -- reasonable back when there was no on-screen keyboard at
// all and the whole screen was genuinely keyboard-only, but that
// justification is stale now (see handle_text_input() above). Maps
// standard buttons onto the exact same keycodes handle_key_down() above
// already handles -- DPAD up/down/left/right -> arrow keys, A -> Enter,
// B/Start -> Escape -- so every navigation/confirm/cancel action already
// implemented for keyboard works for a controller too, with zero new
// logic duplicated. Typing a name/IP still has no controller equivalent
// (same as it has none for touch) -- a controller player is expected to
// use the default name, same reasoning as touch.
void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e);

} // namespace menu_multiplayer
