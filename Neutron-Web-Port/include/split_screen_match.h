#pragma once

#include <SDL2/SDL.h>

class neutron;
class player;

// The actual split-screen PvP match: N independent player ships sharing one
// terrain, N-way multi-viewport rendering, and last-ship-standing. Isolated
// the same way every other new-feature file this session is -- its own
// state, driving neutron only through existing public members/methods.
// Picks up where split_screen_menu's map-select screen leaves off.
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
// each join-screen slot via split_screen_menu's own accessors.
void start_match(neutron* game, int map_idx);

// Per-tick, called from main_loop()'s game_status 23/24 cases.
void tick(neutron* game);
void update_match_over(neutron* game);

// Rendering, called from neutron::print()'s existing dispatch.
void print_match(neutron* game);
void print_match_over(neutron* game);

// Keyboard/controller, routed from main.cpp only while game_status is 23/24.
void handle_key_down(neutron* game, SDL_Keycode sym);
void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e);

// Read-only queries -- used by the automated test harness to verify the
// simulation/elimination/win logic directly, and a reasonable seam for any
// future HUD polish. slot is 0..3.
int colision_for_slot(int slot); // 0 alive, 1 eliminated, -1 no player in that slot
int winner_slot();               // -1 = no winner yet / draw
player* player_at_slot(int slot); // nullptr if empty

} // namespace split_screen_match
