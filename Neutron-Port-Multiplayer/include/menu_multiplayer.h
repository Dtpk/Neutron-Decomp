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
// way -- also confirms) rather than just moving a cursor. Typing a name
// or manual IP is still keyboard-only (no on-screen keyboard); a touch
// player is expected to use the default name and either host or pick a
// LAN game from the list, same reasoning as before.
void handle_touch(neutron* game, float lx, float ly);

} // namespace menu_multiplayer
