#include "net_match.h"
#include "neutron.h"
#include "split_screen_match.h"
#include "net_session.h"

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

} // namespace

bool is_active() { return s_active; }

void start(neutron* game, int map_idx, unsigned seed) {
    s_active = true;
    s_local_slot = net_session::local_slot();
    s_pending_local_action = 0;
    s_last_map_idx = map_idx;
    s_seen_generation = net_session::match_generation();

    // See this function's own header comment for why this matters: every
    // peer must srand() identically, right here, before anything that
    // calls std::rand() for this match's setup (spawn-plane shuffling,
    // in split_screen_match.cpp's roll_spawn_planes()).
    std::srand(seed);

    // input_sources[MAX_PLAYERS]: -3 marks a slot as "externally driven"
    // (see split_screen_match.h's own comment on start_match_with_sources)
    // for every slot net_session reports as connected, -2 (empty) for
    // everything else.
    int sources[4];
    for (int i = 0; i < 4; i++) {
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
    unsigned seed = (unsigned)SDL_GetTicks();
    net_session::start_match(s_last_map_idx, seed);
    start(game, s_last_map_idx, seed);
}

// Called every frame while on the match-over screen (see
// split_screen_match::update_match_over()'s delegation) so a client keeps
// pumping sockets and can actually notice a retry. Hosts don't need the
// generation check for themselves (retry() above calls start() directly),
// but still need net_session::update() pumped here regardless of role --
// otherwise nothing would process incoming messages (a client
// disconnecting, etc.) while sitting on this screen at all.
void update_match_over(neutron* game) {
    net_session::update();
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
    // directly). If the full set hasn't arrived yet, this tick is skipped
    // entirely: main_loop()'s own dispatch already decided "enough real
    // time has passed for a tick" via its delta >= target_frame_time
    // check, and doesn't retry any sooner than a full target_frame_time
    // later if we bail out here -- fine on a real LAN (round-trip time is
    // far smaller than the 55-75ms tick interval), but a real latency-
    // smoothing pass would be needed before this could handle
    // meaningfully higher-latency connections well.
    //
    // Order matters: submit BEFORE update(). update() is what actually
    // assembles the host's tick-ready check (see net_session.cpp's
    // host_update()) -- calling it first would make that check run
    // against LAST tick's leftover local action instead of this one,
    // silently doubling the effective tick interval for networked play
    // every single tick (each tick would need two outer-loop passes to
    // resolve instead of one) without ever technically deadlocking.
    net_session::submit_local_action(s_pending_local_action);
    net_session::update();
    if (!net_session::tick_ready()) return;

    for (int slot = 0; slot < 4; slot++) {
        int action = net_session::tick_action(slot);
        if (action == 1) split_screen_match::apply_direction(slot, 0); // turn left
        else if (action == 2) split_screen_match::apply_direction(slot, 1); // turn right
    }
    net_session::advance_tick();
    s_pending_local_action = 0;

    split_screen_match::tick_local_only(game);
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
        if (sym == SDLK_ESCAPE) { end(game); return; }
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
        if (sym == SDLK_ESCAPE) { end(game); return; }
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
        if (e.button == SDL_CONTROLLER_BUTTON_START || e.button == SDL_CONTROLLER_BUTTON_B) { end(game); }
        return;
    }
    if (game->game_status == 24) {
        if (e.button == SDL_CONTROLLER_BUTTON_A && net_session::role() == net_session::Role::Host) {
            retry(game);
            return;
        }
        if (e.button == SDL_CONTROLLER_BUTTON_B || e.button == SDL_CONTROLLER_BUTTON_START) { end(game); }
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

} // namespace net_match
