#include "net_match.h"
#include "neutron.h"
#include "split_screen_match.h"
#include "net_session.h"
#include "menu_multiplayer.h"

#include <string>
#include <cstdlib>

namespace net_match {

namespace {

bool s_active = false;
int s_local_slot = -1;
int s_pending_local_action = 0; // staged by handle_key_down/handle_controller_button_down, consumed once per drive_tick()
int s_last_map_idx = -1;        // remembered so a retry replays the same map, same convention
                                 // local split-screen's own start_match(game, -2) sentinel uses
int s_seen_generation = -1;     // last net_session::match_generation() this module has actually
                                 // started from -- lets poll_start_signal() tell "a brand new
                                 // retry signal just arrived" apart from the stale true
                                 // match_starting() has held since the very first match start
int s_seen_cancel_token = -1;   // same pattern as s_seen_generation, for net_session::cancel_token()
Uint32 s_last_real_tick_ms = 0; // drive_tick()'s OWN pacing clock for actually ADVANCING a
                                 // resolved tick, independent of main.cpp's outer ~55-75ms
                                 // gate -- see pump_network()'s own comment for why main.cpp's
                                 // gate alone isn't enough once drive_tick() can be reached
                                 // every real frame instead of only once per gated main_loop()
// The actual socket I/O -- submit (status 23 only) + net_session::update().
// No tick-consumption logic at all; see drive_tick() for that. Deliberately
// resubmits EVERY real frame, even when s_pending_local_action hasn't
// changed since the last call -- a real regression was found (and
// reverted) here: a prior pass only resubmitted when the (tick, action)
// pair actually changed, reasoning that resending an unchanged value was
// pure waste. That's true in isolation, but it quietly removed the only
// thing making a single submission self-healing. If the host's view of
// "has this client submitted for tick N" ever gets stuck for ANY reason
// (observed on real hardware: hosting from one specific machine over
// ethernet would freeze the match entirely unless the OTHER player moved
// -- moving changed the action value, which is what let the dedup logic
// resubmit at all), the client was never going to resend the SAME value
// again, since nothing had "changed" by its own bookkeeping -- the host
// would then wait forever for a submission that was either never
// received correctly or got attributed to the wrong tick, with no way to
// recover short of the player moving or the 5s stall-timeout kicking
// them. Continuous resubmission is what used to self-heal exactly that
// class of problem; the small bandwidth cost of resending an unchanged
// tiny message every ~16ms on a LAN is not worth trading away that
// resilience for.
void raw_pump(neutron* game) {
    if (game->game_status == 23) {
        net_session::submit_local_action(s_pending_local_action);
    }
    net_session::update();
}

// Checked every frame by drive_tick()/update_match_over() below while a
// networked match is active: true if either a fresh CANCEL just arrived
// (host cancelled, go to the lobby) or the connection genuinely dropped
// out from under us (role() flipped to None -- a real disconnect/crash,
// not a clean cancel; see cancel()'s own header comment for the freeze
// this is the safety net for). Returns the local cleanup it performed, if
// any, so the caller can bail out of whatever it was about to do this
// frame instead of touching state cancel()/end() just tore down.
bool poll_disconnection_signals(neutron* game) {
    if (net_session::role() == net_session::Role::None) {
        end(game);
        return true;
    }
    if (net_session::role() == net_session::Role::Client &&
        net_session::cancel_token() != s_seen_cancel_token) {
        cancel(game);
        return true;
    }
    return false;
}

} // namespace

bool is_active() { return s_active; }

void start(neutron* game, int map_idx, unsigned seed) {
    s_active = true;
    s_local_slot = net_session::local_slot();
    s_pending_local_action = 0;
    s_last_map_idx = map_idx;
    s_seen_generation = net_session::match_generation();
    s_seen_cancel_token = net_session::cancel_token();
    s_last_real_tick_ms = SDL_GetTicks(); // fresh pacing baseline for this match/retry

    // See this function's own header comment for why this matters: every
    // peer must srand() identically, right here, before anything that
    // calls std::rand() for this match's setup (spawn-plane shuffling,
    // in split_screen_match.cpp's roll_spawn_planes()).
    std::srand(seed);

    // input_sources[net_session::MAX_PLAYERS]: -3 marks a slot as
    // "externally driven" (see split_screen_match.h's own comment on
    // start_match_with_sources) for every slot net_session reports as
    // connected, -2 (empty) for everything else. Must be sized to
    // net_session::MAX_PLAYERS, not split_screen_match's own (separate,
    // but currently equal) constant -- start_match_with_sources() reads
    // exactly that many entries internally, so a mismatch here would be
    // a real out-of-bounds read, not just a silently-ignored one.
    int sources[net_session::MAX_PLAYERS];
    for (int i = 0; i < net_session::MAX_PLAYERS; i++) {
        sources[i] = net_session::player_name(i).empty() ? -2 : -3;
    }
    split_screen_match::start_match_with_sources(game, map_idx, sources);
}

void retry(neutron* game) {
    // Host-only: re-coordinates another match with the SAME still-
    // connected roster, without tearing the session down (net_session's
    // TCP connections/roster stay exactly as they are -- only a fresh
    // START goes out, bumping match_generation() so update_match_over()
    // below can tell this apart from the original match's now-stale
    // signal). Same map as just played, matching local split-screen's own
    // retry convention. Fresh seed every time (SDL_GetTicks() is good
    // enough -- this just needs to differ run to run, not be
    // cryptographically anything) so a retry's spawn layout isn't
    // identical to the match that just ended.
    if (net_session::role() != net_session::Role::Host) return;
    // Re-validate the map against the CURRENT roster before replaying --
    // see menu_multiplayer::validate_map_choice()'s own comment for the
    // gap this closes (mid-match joiners promoted since this map was
    // chosen could have outgrown it; only the HostLobby screen used to
    // catch that).
    s_last_map_idx = menu_multiplayer::validate_map_choice(s_last_map_idx, net_session::connected_player_count());
    unsigned seed = (unsigned)SDL_GetTicks();
    net_session::start_match(s_last_map_idx, seed);
    start(game, s_last_map_idx, seed);
}

void cancel(neutron* game) {
    // See this function's own header comment (net_match.h) for the real
    // bug this replaces (ESC-during-match tearing the whole session down
    // and freezing every client). net_session::cancel_match()'s own
    // host-only guard makes this call a no-op when WE are a client
    // reacting to the host's CANCEL, so this one function is correct for
    // both sides without needing its own role check here.
    net_session::cancel_match();
    s_seen_cancel_token = net_session::cancel_token();
    split_screen_match::leave_match_to_lobby(game);
    menu_multiplayer::enter_lobby(game);
    s_active = false;
    s_local_slot = -1;
    s_pending_local_action = 0;
}

// Called every frame while on the match-over screen (see
// split_screen_match::update_match_over()'s delegation) so a client keeps
// pumping sockets and can actually notice a retry. Hosts don't need the
// generation check for themselves (retry() above calls start() directly),
// but still need net_session::update() pumped here regardless of role --
// otherwise nothing would process incoming messages (a client
// disconnecting, etc.) while sitting on this screen at all.
void update_match_over(neutron* game) {
    pump_network(game);
    if (poll_disconnection_signals(game)) return;
    if (net_session::role() == net_session::Role::Client &&
        net_session::match_starting() &&
        net_session::match_generation() != s_seen_generation) {
        start(game, net_session::match_map_idx(), net_session::match_seed());
    }
}

void end(neutron* game) {
    net_session::leave_session();
    split_screen_match::leave_match(game);
    s_active = false;
    s_local_slot = -1;
    s_pending_local_action = 0;
}

void drive_tick(neutron* game) {
    // Lockstep gate: nobody's direction change is applied until EVERY
    // connected player's action for this exact tick has arrived (see
    // net_session.h's own design comment for why) -- including the LOCAL
    // player's, applied here at the same tick boundary everyone else
    // applies it, rather than instantly on keypress (handle_key_down only
    // stages s_pending_local_action; it never applies a direction change
    // directly).
    raw_pump(game);
    if (poll_disconnection_signals(game)) return;
    if (!net_session::tick_ready()) return;

    // Own pacing, independent of main.cpp's outer gate -- this function
    // can now be reached every real frame (via pump_network() below),
    // not just once per ~55-75ms gated main_loop() call, so a resolved
    // tick must still only actually ADVANCE at the game's intended
    // cadence (same 55/75ms local play uses via the difficulty slider),
    // not the instant it happens to resolve network-wise -- otherwise
    // two machines with near-zero latency between them would run the
    // match faster than its own difficulty pacing intends.
    Uint32 now = SDL_GetTicks();
    float target_ms = (game->user_difficulty == 1) ? 55.0f : 75.0f;
    if (now - s_last_real_tick_ms < (Uint32)target_ms) return;
    s_last_real_tick_ms = now;

    for (int slot = 0; slot < net_session::MAX_PLAYERS; slot++) {
        int action = net_session::tick_action(slot);
        if (action == 1) split_screen_match::apply_direction(slot, 0); // turn left
        else if (action == 2) split_screen_match::apply_direction(slot, 1); // turn right
        else if (action == 3) split_screen_match::eliminate_slot(game, slot); // disconnected mid-match
    }

    // Real bug this avoids -- the actual cause of "needing to spam
    // left/right to get one turn to register": tick_action(s_local_slot)
    // is what the HOST actually resolved/broadcast for MY OWN slot this
    // tick, which can differ from whatever's CURRENTLY staged in
    // s_pending_local_action if a keypress landed in the race window
    // between "the host already finished resolving/broadcasting this
    // tick using an OLDER submission" and "raw_pump() just sent a FRESH
    // submission reflecting the new keypress, too late to affect this
    // tick." The old code cleared s_pending_local_action unconditionally
    // on every resolve -- discarding that keypress forever, having never
    // been incorporated into ANY tick's real resolution at all (the ship
    // never turned). Comparing against what was actually resolved before
    // clearing lets an un-incorporated keypress survive to be freshly
    // resubmitted for the NEXT tick instead.
    int my_resolved_action = net_session::tick_action(s_local_slot);
    net_session::advance_tick();
    if (my_resolved_action == s_pending_local_action) {
        s_pending_local_action = 0;
    }

    split_screen_match::tick_local_only(game);

    // tick_local_only() just detected the win/draw condition and flipped
    // game_status to 24 (match-over) -- fires exactly once, on the single
    // tick where that transition happens (drive_tick() is never called
    // again afterwards; the main loop dispatches update_match_over()
    // instead once game_status is 24). Host-only call (no-op for a
    // client, same guard style as every other host-only net_session
    // function) -- folds in anyone who tried to JOIN while this match was
    // still running; see net_session::notify_match_ended()'s own comment
    // for the lockup bug that made this necessary.
    if (game->game_status == 24) net_session::notify_match_ended();
}

void pump_network(neutron* game) {
    if (!s_active) return;
    // Real bug this fixes: an earlier pass only pumped sockets here,
    // relying on main.cpp's own ~55-75ms GATED main_loop() call to
    // actually attempt/consume a tick. That fixed I/O FREQUENCY (data
    // arrives sooner) but tick CONSUMPTION was still bottlenecked to at
    // most once per ~55-75ms of THIS peer's own gate, regardless of how
    // much earlier the data had actually arrived -- adding up to a full
    // extra ~55-75ms of pure "waiting for my own next scheduled slot" on
    // top of the inherent one-tick lockstep delay. The match's overall
    // SPEED looked fixed (ticks stopped silently doubling in length) but
    // INPUT RESPONSIVENESS still felt laggy, because consuming a
    // resolved tick was still gated the same way.
    //
    // Calling the full drive_tick() here (not just raw_pump()) for the
    // live-match screen lets a tick advance as soon as it's BOTH ready
    // AND the match's own pacing clock allows it, checked every real
    // frame instead of once per outer-gate cycle. drive_tick()'s own
    // s_last_real_tick_ms check is what keeps this from ever advancing
    // FASTER than the intended cadence. The still-existing GATED
    // tick()->drive_tick() call path (split_screen_match::tick()'s own
    // dispatch, reached via main_loop()) calling into this same,
    // self-paced drive_tick() too is harmless -- whichever call happens
    // to land after a resolved tick's pacing window opens does the real
    // work; the other is a no-op.
    if (game->game_status == 23) {
        drive_tick(game);
        return;
    }
    raw_pump(game); // match-over screen: just keep sockets pumped, no tick logic needed here
}

void drive_print(neutron* game) {
    if (s_local_slot < 0) return;
    split_screen_match::print_match(game, s_local_slot);
}

void handle_key_down(neutron* game, SDL_Keycode sym) {
    if (game->game_status == 23) {
        // Local input is STAGED, not applied immediately -- drive_tick()
        // applies it at the same tick boundary every remote peer's copy
        // of it is applied (see drive_tick()'s own comment on why).
        if (s_local_slot >= 0 && split_screen_match::colision_for_slot(s_local_slot) == 0) {
            if (sym == SDLK_LEFT || sym == SDLK_a || sym == SDLK_q) { s_pending_local_action = 1; return; }
            if (sym == SDLK_RIGHT || sym == SDLK_d || sym == SDLK_e) { s_pending_local_action = 2; return; }
        }
        // Host's ESC cancels back to the lobby for everyone (roster/
        // connection survive); a client's ESC really does leave (see
        // cancel()'s own header comment for why these differ).
        if (sym == SDLK_ESCAPE) {
            if (net_session::role() == net_session::Role::Host) cancel(game); else end(game);
            return;
        }
        return;
    }
    if (game->game_status == 24) {
        // Host confirms to play again (same roster, same map, no
        // disconnect) -- a client sees the result via
        // update_match_over()'s generation check instead of anything
        // triggered by its own input here.
        if ((sym == SDLK_RETURN || sym == SDLK_SPACE || sym == SDLK_z) &&
            net_session::role() == net_session::Role::Host) {
            retry(game);
            return;
        }
        if (sym == SDLK_ESCAPE) {
            if (net_session::role() == net_session::Role::Host) cancel(game); else end(game);
            return;
        }
        return;
    }
}

void handle_controller_button_down(neutron* game, const SDL_ControllerButtonEvent& e) {
    if (game->game_status == 23) {
        if (s_local_slot >= 0 && split_screen_match::colision_for_slot(s_local_slot) == 0) {
            if (e.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT || e.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                s_pending_local_action = 1;
                return;
            }
            if (e.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || e.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                s_pending_local_action = 2;
                return;
            }
        }
        if (e.button == SDL_CONTROLLER_BUTTON_START || e.button == SDL_CONTROLLER_BUTTON_B) {
            if (net_session::role() == net_session::Role::Host) cancel(game); else end(game);
        }
        return;
    }
    if (game->game_status == 24) {
        if (e.button == SDL_CONTROLLER_BUTTON_A && net_session::role() == net_session::Role::Host) {
            retry(game);
            return;
        }
        if (e.button == SDL_CONTROLLER_BUTTON_B || e.button == SDL_CONTROLLER_BUTTON_START) {
            if (net_session::role() == net_session::Role::Host) cancel(game); else end(game);
        }
        return;
    }
}

const char* winner_display_name() {
    if (!s_active) return "";
    int slot = split_screen_match::winner_slot();
    if (slot < 0) return "";
    static std::string s_name; // lifetime needs to outlive the caller's snprintf -- static is fine, single-threaded
    s_name = net_session::player_name(slot);
    return s_name.c_str();
}

bool local_is_host() { return net_session::role() == net_session::Role::Host; }

} // namespace net_match
