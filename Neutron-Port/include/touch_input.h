#pragma once

#include <SDL2/SDL.h>

class neutron;

// Touch-screen support, isolated the same way controller_input.cpp isolates
// gamepad support: its own module, driving the game only through the same
// seams keyboard/controller already use (execute_action()/direct field
// mutation).
//
// IMPORTANT: stage_interface.cpp already carries old PSP-decomp button-rect
// data (button::left/top/right/bottom, who_is(), catch_tap(), catch_key())
// that looks tailor-made for touch at first glance -- but it's stale. This
// port's actual menu/HUD rendering (print_menu_interface(), print_game_
// interface(), etc.) draws plain text at hardcoded positions and never
// consults those rects, so they don't correspond to what's really on
// screen (some aren't drawn at all; others are drawn at a different
// position than their static rect claims). Do NOT wire touch through who_is
// ()/catch_tap() -- this module computes its own tap zones directly from
// the same row-position formulas stage_interface.cpp's renderers use, kept
// in lockstep with them deliberately (see the comments in touch_input.cpp).
namespace touch_input {

// Call once per SDL_FINGERDOWN. SDL already reports e.x/e.y normalized to
// 0..1 regardless of actual window size, so no window-dimension parameter
// is needed here.
void handle_finger_down(neutron* game, const SDL_TouchFingerEvent& e);

} // namespace touch_input
