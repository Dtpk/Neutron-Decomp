#pragma once

#include <SDL2/SDL.h>

class neutron;
class player;

// The actual local PvP match engine: N independent player ships sharing one
// terrain, N-way multi-viewport rendering, last-ship-standing. Isolated the
// same way every other feature file in this codebase is -- its own state,
// driving neutron only through existing public members/methods, and (as of
// this pass) knowing NOTHING about networking at all. net_match.h/.cpp owns
// all of that instead, driving THIS module purely through the generic public
// hooks below (start_match_with_sources/apply_direction/tick_local_only/
// print_match's forced_single_slot/leave_match) -- the same hooks a THIRD
// kind of caller could reuse later without this file needing to change
// again. tick()/print_match()/handle_key_down()/handle_controller_button_down()
// remain the single entry points main.cpp/neutron.cpp already call
// regardless of local-vs-networked; each one's first move is checking
// net_match::is_active() and handing off if so -- see split_screen_match.cpp.
//
// Picks up where split_screen_menu's map-select screen leaves off (for the
// local flow) or menu_multiplayer (for the networked flow, via net_match).
//
// Reuses two more game_status values, on a new, dedicated actual_stage
// (4000) that nothing else in the codebase checks for, so none of
// neutron.cpp's many existing `actual_stage ==`-gated branches can
// accidentally fire during a match:
//   game_status 23, actual_stage 4000 -- live match
//   game_status 24, actual_stage 4000 -- match over (results screen)
//
// Player ships here are NOT neutron::proton -- they're a separate, parallel
// array this module owns outright, with idents 1..4 (proton's is always 0,
// so these never collide with it or with each other).
namespace split_screen_match {

// Call from split_screen_menu once a map is confirmed. map_idx is a real
// 0-based stage index, or -1 for "pick one at random" (the RANDOM
// selection). Reads which input source (keyboard / controller index) is in
// each join-screen slot via split_screen_menu's own accessors, then defers
// to start_match_with_sources() below.
void start_match(neutron* game, int map_idx);

// Generic spawn/setup entry point start_match() itself is a thin wrapper
// around. input_sources must point to an array of at least this file's
// own MAX_PLAYERS (currently 12) elements -- this function reads exactly
// that many, unchecked (a raw pointer, no length passed in), so a caller
// providing fewer is a real out-of-bounds read, not something caught at
// compile time. net_match.cpp's own array is sized from
// net_session::MAX_PLAYERS specifically because of this -- the two
// constants are independent by design but MUST be kept numerically equal;
// see net_session.h's own comment on its copy.
// Encoding: -2 empty, -1 keyboard, 0..3 controller index, -3 "externally
// driven" (a real player slot that exists and should be rendered/
// simulated, but whose direction changes come from outside this module --
// apply_direction() below, not local keyboard/controller polling).
// net_match.cpp uses -3 for every networked slot.
void start_match_with_sources(neutron* game, int map_idx, const int* input_sources);

// Applies a direction change (0 = turn left, 1 = turn right) to whichever
// player occupies `slot`, if any and if still alive. Safe no-op otherwise.
// The generic hook any external driver (net_match.cpp today) uses instead
// of reaching into this module's own player array, which stays private.
void apply_direction(int slot, int direction);

// Eliminates whichever player occupies `slot`, same as a real in-match
// crash would (status/effects set, fire_up_cue, play_effect) -- the
// public hook net_match.cpp uses for a disconnected player's ship
// (otherwise it would just sit on the field forever, never eliminated,
// still counting toward "alive," so a match could never correctly
// conclude -- a real bug this hook fixes). Safe no-op if slot is empty
// or already eliminated, so a driver can call this every tick for as
// long as a disconnect signal keeps arriving without re-triggering the
// elimination effects repeatedly.
void eliminate_slot(neutron* game, int slot);

// The actual local simulation step for one tick (speed assignment, each
// player's move()/elimination check, win detection) -- NO network
// awareness of any kind. tick() below calls this directly for a local
// match, or net_match::drive_tick() calls it once a networked tick's full
// input set has actually arrived.
void tick_local_only(neutron* game);

// Per-tick, called from main_loop()'s game_status 23/24 cases. Delegates
// to net_match::drive_tick() if a networked match is active, else calls
// tick_local_only() directly.
void tick(neutron* game);
void update_match_over(neutron* game);

// Called from main.cpp's OWN outer loop, every real frame, UNCONDITIONALLY
// (not gated by the ~55-75ms delta>=target_frame_time check main_loop()
// itself only runs behind) -- a real no-op for local play or when no
// networked match is active at all. Delegates to net_match::pump_network()
// otherwise; see that function's own comment (net_match.h) for the two
// real bugs this fixes -- the match running at roughly half speed (two
// peers' independently-phased outer loops), and, once that was fixed,
// input still feeling noticeably delayed (tick CONSUMPTION, not just
// socket I/O, was still bottlenecked to the same gated cadence).
void pump_network(neutron* game);

// Rendering, called from neutron::print()'s existing dispatch.
// forced_single_slot: -1 (default) renders the normal local multi-viewport
// split across every active player. >=0 forces a single fullscreen
// viewport for exactly that slot regardless of how many other players
// exist -- what a networked match needs (each player is on their own
// machine with their own full screen), used by net_match::drive_print().
void print_match(neutron* game, int forced_single_slot = -1);

// winner_name_override: nullptr (default) shows "PLAYER N WINS!" as local
// split-screen always has. A networked match passes the winner's actual
// chosen name instead (net_match::drive_print() does this, reading it
// from net_session -- the generic hook keeps that coupling in net_match
// only, not here).
void print_match_over(neutron* game, const char* winner_name_override = nullptr);

// Public wrapper for leaving a match back to the main menu (cleans up
// players/state, rebuilds the menu). net_match.cpp calls this on
// disconnect/ESC instead of reaching into this module's own internals.
void leave_match(neutron* game);

// Same player/state cleanup and 3D background rebuild as leave_match()
// above, but deliberately does NOT touch game->game_status itself --
// net_match.cpp's cancel() calls this and then sets game_status = 30
// itself (via menu_multiplayer::enter_lobby()) instead of landing on
// whatever change_stage(1000, true) would have returned (the main menu's
// own status), since cancelling a match should return to the multiplayer
// lobby, not all the way out to the main menu.
void leave_match_to_lobby(neutron* game);

// Keyboard/controller, routed from main.cpp only while game_status is
// 23/24. Delegate to net_match's own handlers if a networked match is
// active, else handle local keyboard/controller input directly.
void handle_key_down(neutron* game, SDL_Keycode sym);
void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e);

// Read-only queries -- used by the automated test harness to verify the
// simulation/elimination/win logic directly, and a reasonable seam for any
// future HUD polish. slot is 0..3.
int colision_for_slot(int slot); // 0 alive, 1 eliminated, -1 no player in that slot
int winner_slot();               // -1 = no winner yet / draw
player* player_at_slot(int slot); // nullptr if empty

} // namespace split_screen_match
