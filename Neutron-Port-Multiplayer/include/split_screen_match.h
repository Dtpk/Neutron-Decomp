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
// around. input_sources[MAX_PLAYERS(=4)]: -2 empty, -1 keyboard, 0..3
// controller index, -3 "externally driven" (a real player slot that exists
// and should be rendered/simulated, but whose direction changes come from
// outside this module -- apply_direction() below, not local keyboard/
// controller polling). net_match.cpp uses -3 for every networked slot.
void start_match_with_sources(neutron* game, int map_idx, const int* input_sources);

// Applies a direction change (0 = turn left, 1 = turn right) to whichever
// player occupies `slot`, if any and if still alive. Safe no-op otherwise.
// The generic hook any external driver (net_match.cpp today) uses instead
// of reaching into this module's own player array, which stays private.
void apply_direction(int slot, int direction);

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
