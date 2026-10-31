#pragma once

#include <SDL2/SDL.h>

class neutron;

// Gamepad support, isolated from the core game the same way menu_battle.cpp
// isolates its own feature: this module owns all SDL_GameController
// bookkeeping and its own copy of main.cpp's input-dispatch branching, and
// only ever drives the game through the exact same seams the keyboard
// handler in main.cpp already uses (execute_action()/direct field
// mutation). It never touches or reorders main.cpp's existing keyboard
// `if` chain, so existing keyboard behavior carries zero risk from this
// being added.
//
// Phase 1 is still single-player: only the first-connected controller
// actually drives the game (see handle_button_down()/poll_axes()). Later
// phases (local split-screen) are expected to read num_connected()/
// get_controller() to enumerate join-screen candidates instead of adding
// their own SDL_GameController bookkeeping.
namespace controller_input {

void init();
void shutdown();

// Call once per SDL_CONTROLLERDEVICEADDED / SDL_CONTROLLERDEVICEREMOVED.
void handle_device_added(const SDL_ControllerDeviceEvent& e);
void handle_device_removed(const SDL_ControllerDeviceEvent& e);

// Call once per SDL_CONTROLLERBUTTONDOWN.
void handle_button_down(neutron* game, const SDL_ControllerButtonEvent& e);

// Call once per frame (not event-driven) so a held D-pad/stick repeats
// menu navigation or steering at a controlled rate, the way a human
// repeatedly tapping a key would.
void poll_axes(neutron* game, Uint32 now_ms);

int num_connected();
SDL_GameController* get_controller(int index); // index in [0, 4)

} // namespace controller_input
