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
// for what's supposed to be the exact same match. difficulty is the
// HOST's difficulty (0 easy, 1 hard, from net_session::match_difficulty())
// for THIS match's shared pacing clock -- deliberately NOT written into
// game->user_difficulty, which stays each player's own personal setting
// (also gates easy-mode guides; see split_screen_match.cpp's
// print_match()) so a player who prefers easy keeps their guides even in
// a host-hard match, while drive_tick() below still paces everyone to
// the host's chosen speed.
void start(neutron* game, int map_idx, unsigned seed, int difficulty);

// Tears the networked match down (net_session::leave_session() +
// split_screen_match::leave_match()) and clears is_active(). Called on
// ESC/disconnect from handle_key_down()/handle_controller_button_down()
// below, or by menu_multiplayer if the connection drops while still on a
// lobby screen (before a match even started).
void end(neutron* game);

// Lockstep gate -- applies every remote slot's direction change via
// split_screen_match::apply_direction() and calls split_screen_match::
// tick_local_only() to actually advance the simulation, but ONLY once
// EVERY connected player's action for this exact tick has arrived AND
// this function's own internal pacing clock says enough real time has
// passed since the last real advance (same 55/75ms cadence local play
// uses via the difficulty slider) -- see this function's own body for
// why that second condition exists. Safe to call more than once in
// quick succession (both pump_network() below and the still-existing
// gated split_screen_match::tick() dispatch call into this) -- whichever
// call lands after both conditions are true does the real work, every
// other call this same tick-window is a no-op.
void drive_tick(neutron* game);

// Called from split_screen_match::pump_network() every real render frame
// (main.cpp's own outer loop, NOT gated by the ~55-75ms sim-tick cadence
// main_loop() normally runs behind) while a networked match or its
// match-over screen is showing.
//
// Real bug this fixes, in two parts, both found from real multiplayer
// testing:
//
// Part 1 (match ran at roughly half speed, even on a real two-machine
// LAN): submit_local_action()/net_session::update() used to only run as
// often as main_loop() was invoked -- gated by the SAME ~55-75ms
// outer-loop check used for the actual simulation step. Two peers' outer
// loops are two independent while() loops on two different machines,
// NOT phase-aligned with each other at all -- a given tick could easily
// need to wait for the OTHER peer's own next ~55-75ms-gated pass before
// its input even got sent or received, adding up to nearly one full
// extra tick interval of latency PER tick on average. Fixed by this
// function existing at all: pumping sockets every real frame (~16ms at
// 60fps) instead of ~55-75ms shrinks that phase-misalignment window
// roughly 3-5x.
//
// Part 2 (speed was fixed, but input STILL felt noticeably delayed): the
// first fix above only made tick_ready() flip true SOONER -- it didn't
// change how often the match actually CHECKED/consumed a now-ready tick,
// since drive_tick() (the only thing that does) was still only reached
// via the SAME gated main_loop() path. A tick resolving early (thanks to
// part 1) still had to sit "ready" doing nothing until this peer's own
// next ~55-75ms-gated slot came around to actually apply it -- a real,
// separate source of latency layered on top of the inherent one-tick
// lockstep delay this design always has by nature. Fixed by calling the
// FULL drive_tick() (not just a raw socket pump) from here for the
// live-match screen specifically, so a resolved tick can advance the
// instant its own pacing window opens, checked every real frame instead
// of once per outer-gate cycle.
void pump_network(neutron* game);

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

// Ends the current match early and returns to the multiplayer LOBBY
// (menu_multiplayer::enter_lobby()), not the main menu -- and, unlike
// end() below, never calls net_session::leave_session(), so the
// session/roster survive. On the host this also sends CANCEL to every
// client (net_session::cancel_match()) so they follow along to their own
// lobby screen instead of being left connected to a host that's silently
// sitting on a different screen. A client calls this same function
// itself once it notices the host's CANCEL arrived (see
// net_session::cancel_token(), checked every frame by drive_tick()/
// update_match_over() below) -- net_session::cancel_match()'s own
// host-only guard makes that call a no-op for the client, so this one
// function covers both sides.
//
// Real bug this (and the cancel_token()/role()==None checks below) fixes:
// ESC during a live match used to always call end() -- for the HOST, that
// tore the whole session down via leave_session(), closing every client's
// TCP socket out from under them with no warning. Clients had no code
// path anywhere that noticed a connection had been closed mid-match (only
// menu_multiplayer's own lobby-screen update() checked for that, and a
// live match never runs that code), so drive_tick() just kept returning
// early forever (tick_ready() can never become true again) -- the match
// visibly froze for every client, each needing a MANUAL Escape to get out
// on their own, which still just dumped them to the main menu like a real
// disconnect, i.e. the exact "the host leaves, everyone else freezes until
// they also escape" behavior reported from real testing.
void cancel(neutron* game);

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

// True if WE are the host of the active (or just-ended) networked match --
// split_screen_match::print_match_over() uses this to show the right
// prompt (only the host's Enter actually does anything on the match-over
// screen; a client pressing Enter there was silently doing nothing, which
// read as "retry doesn't work" when reported from real testing). Keeps
// split_screen_match.cpp from needing to know net_session exists at all,
// same reasoning as winner_display_name() above.
bool local_is_host();

} // namespace net_match
