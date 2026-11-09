#pragma once

#include <SDL2/SDL.h>

class neutron;

// Local split-screen setup flow: join screen + map select. Isolated the
// same way menu_battle.cpp/controller_input.cpp/touch_input.cpp are --
// its own file, its own state, driving the game only through existing
// public neutron members/methods. Confirming a map hands off to
// split_screen_match, which owns the actual multiplayer match/rendering.
//
// Reuses two existing-but-otherwise-unused game_status values, on
// actual_stage 1000 (join screen, keeps the animated menu_battle
// background) and 3000 (map select, reuses Stage Select's existing
// camera/miniature-rendering machinery in print_stage()):
//   game_status 20 -- join screen
//   game_status 21 -- map select
//
// Only keyboard and controllers can join a slot (up to 4 total) -- touch
// is a single-finger, single-player input by nature and doesn't fit a
// multi-controller local setup screen, so it isn't wired in here.
namespace split_screen_menu {

// Call when the main menu's new "SPLIT SCREEN" entry is selected, before
// setting game_status = 20, so a previous visit's joined slots don't
// linger.
void reset(neutron* game);

// Per-tick, called from main_loop()'s game_status 20/21 cases. 20 currently
// does nothing beyond letting menu_battle/the 3D background keep animating
// (print_stage() already handles that); 21 mirrors show_miniatures()'s
// per-tick rotation spin + camera tween.
void update_join_screen(neutron* game);
void update_map_select(neutron* game);

// Rendering, called from neutron::print()'s existing dispatch.
void print_join_screen(neutron* game);
void print_map_select(neutron* game);

// Keyboard, routed from main.cpp only while game_status is 20/21.
void handle_key_down(neutron* game, SDL_Keycode sym);

// Controller buttons, routed from main.cpp only while game_status is
// 20/21 (instead of controller_input::handle_button_down(), which has no
// concept of these screens).
void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e);

// Read-only queries for split_screen_match to pick up the join screen's
// final slot assignments when a match actually starts. source encoding:
// -2 = empty, -1 = keyboard, 0..3 = controller_input index.
int slot_count();
int slot_input_source(int slot);

} // namespace split_screen_menu
