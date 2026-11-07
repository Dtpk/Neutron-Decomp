#pragma once

#include <SDL2/SDL.h>

class neutron;

// Owns ALL the networked-match-specific orchestration: lockstep tick
// gating, which slot is "mine," single-viewport-per-client rendering,
// staging local input instead of applying it immediately. Drives
// split_screen_match purely through its generic public hooks
// (start_match_with_sources/apply_direction/tick_local_only/print_match's
// forced_single_slot/leave_match) and net_session purely through ITS
// public API -- this file is the one place those two modules' concerns
// actually meet, so neither split_screen_match.cpp nor net_session.cpp has
// to know the other exists.
//
// split_screen_match::tick()/print_match()/handle_key_down()/
// handle_controller_button_down() each check is_active() first and
// delegate to drive_tick()/drive_print()/handle_key_down()/
// handle_controller_button_down() below when a networked match is live --
// see split_screen_match.cpp for exactly where.
namespace net_match {

// True once start() has set up a networked match and it hasn't ended yet
// (leave_match()'s caller -- see end() below -- resets this).
bool is_active();

// Called from menu_multiplayer once net_session reports a match starting
// (either the host triggering it, or a client receiving the host's
// START). Reads the player roster from net_session (one ship per
// connected slot, in net_session's own slot order, matching
// net_session::local_slot() for "which one is mine") and sets up
// split_screen_match accordingly via start_match_with_sources(), marking
// every connected slot -3 ("externally driven" -- see that function's own
// comment).
//
// map_idx MUST already be a concrete, resolved stage index, never
// -1/"random" -- see net_session::start_match()'s own comment for why
// (each peer would otherwise independently re-roll a DIFFERENT random
// map). seed is srand()'d here, identically on every peer, right before
// calling start_match_with_sources() -- a real desync bug this fixes:
// spawn-plane shuffling and offsets (split_screen_match.cpp's
// roll_spawn_planes()) use std::rand(), and with no shared seed, each
// peer's own process-local call history (which diverges the moment two
// peers run different code paths, e.g. different menu navigation before
// the match even starts) would make them compute DIFFERENT spawn layouts
// for what's supposed to be the exact same match.
void start(neutron* game, int map_idx, unsigned seed);

// Tears the networked match down (net_session::leave_session() +
// split_screen_match::leave_match()) and clears is_active(). Called on
// ESC/disconnect from handle_key_down()/handle_controller_button_down()
// below, or by menu_multiplayer if the connection drops while still on a
// lobby screen (before a match even started).
void end(neutron* game);

// Lockstep gate, called from split_screen_match::tick() once per
// simulation-tick attempt while a networked match is active. Submits the
// local player's staged action (set by handle_key_down/
// handle_controller_button_down below), pumps net_session, and -- only
// once EVERY connected player's action for this exact tick has arrived --
// applies every remote slot's direction change via
// split_screen_match::apply_direction() and calls
// split_screen_match::tick_local_only() to actually advance the
// simulation. If the full set hasn't arrived yet, does nothing this call
// (see this function's own body for the real, disclosed latency caveat).
void drive_tick(neutron* game);

// Rendering, called from split_screen_match::print_match() once per
// frame while a networked match is active -- just forwards to
// print_match(game, <local slot>) so only the local player's view is ever
// shown, never local split-screen's multi-viewport layout.
void drive_print(neutron* game);

// Host-only: starts another match with the same still-connected roster
// and the same map just played, WITHOUT tearing the session down (no
// disconnect, no re-joining) -- called from handle_key_down()/
// handle_controller_button_down() below when the host confirms on the
// match-over screen. No-op if called on a client.
void retry(neutron* game);

// Called every frame from split_screen_match::update_match_over() (see
// its own delegation) while a networked match-over screen is showing --
// keeps pumping net_session so a client can actually notice the host's
// retry() signal (or any other message, e.g. a disconnect) while sitting
// on this screen.
void update_match_over(neutron* game);

// Input, called from split_screen_match::handle_key_down()/
// handle_controller_button_down() while a networked match is active.
// Stages the local player's action (consumed once per tick by
// drive_tick() above, NOT applied immediately -- see drive_tick()'s own
// comment on why local input has to wait for the same tick boundary
// every remote peer's copy of it is applied at) or handles ESC/leave.
void handle_key_down(neutron* game, SDL_Keycode sym);
void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e);

// Winner's chosen display name for the match-over screen, or empty if
// not active / no winner yet / not connected to a roster. split_screen_match
// doesn't track names at all (that's net_session's job) -- print_match_over()
// calls this itself when net_match::is_active() so the networked match-over
// screen shows "DANIEL WINS!" instead of "PLAYER N WINS!", without
// split_screen_match.cpp needing to know net_session exists.
const char* winner_display_name();

} // namespace net_match
