Neutron -- LAN Multiplayer fork
====================================
Based on /workspace/Neutron-Port (the pure desktop SDL2 + OpenGL1/GLU
variant), copied into its own folder per the user's request rather than
modifying the original in place. Not a console port -- this is a desktop
feature addition.

Built and compiles clean (including -Wall -Wextra on every new file), but
the actual networking has NEVER been tested against a real second machine
or even a real LAN -- this sandbox has no way to run two instances and
have them talk to each other over a network, let alone a display to drive
the GUI at all. Everything here is a careful, honest first pass: the
logic has been read through and reasoned about directly, not verified by
actually running it. Real testing (packet loss, real multi-machine
timing, router/firewall quirks, actually playing a match with someone)
still needs to happen before trusting this.


What's here
--------------
1. LAN multiplayer: a new "MULTIPLAYER" menu entry, ABOVE "OPTIONS" (main
   menu is now 8 entries instead of 7 -- see src/stage_interface.cpp's
   opts[8] and the matching %7->%8 change in both src/main.cpp and
   src/controller_input.cpp's main-menu navigation, which had its own
   separate, duplicate copy of the same option-count logic).

2. net_session.h/.cpp (new files): UDP broadcast LAN discovery + TCP for
   everything else (roster, start-of-match, lockstep input exchange).
   Host-authoritative star topology -- clients only ever talk to the
   host, never to each other. See net_session.h's own long comment for
   the full design reasoning, especially why lockstep fits this engine's
   existing discrete (not continuous-analog) steering input so well.

3. menu_multiplayer.h/.cpp (new files): the actual menu screen. Host/Join
   choice -> name entry -> host lobby (shows roster, host presses Enter
   to start once >=2 players) or join setup (manual IP field + a
   LAN-discovered-games list, re-scanned every ~4-5s). Keyboard-only for
   now -- the text-entry fields (name/IP) have no on-screen-keyboard
   equivalent for controllers, deliberately out of scope this pass (see
   main.cpp's SDL_CONTROLLERBUTTONDOWN dispatch, which explicitly
   swallows controller input on game_status==30 rather than letting it
   misfire as main-menu navigation).

4. split_screen_match.cpp gets a new start_match_networked() entry point
   and tick() gains a lockstep gate when network mode is active: nobody's
   direction change (including the LOCAL player's) is applied until every
   connected player's action for that exact tick has arrived over the
   network -- see tick()'s own comment for the real, disclosed limitation
   here (if a tick's full input set doesn't arrive before main_loop()'s
   own delta>=target_frame_time check fires again, that tick is skipped
   entirely rather than retried sooner -- fine on a real LAN, not
   something that would hold up well over higher-latency connections
   without a real latency-smoothing pass added later).

5. Win screen now shows the actual chosen player name for networked
   matches ("DANIEL WINS!") instead of a bare slot number, with a default
   ("Player"/"Host") if left blank -- see net_session.cpp's
   DEFAULT_CLIENT_NAME/DEFAULT_HOST_NAME. Local split-screen's win screen
   is unchanged ("PLAYER 2 WINS!" etc.), since there's no name concept to
   show there.

6. Spawn randomization (also benefits local split-screen, not just
   networked play): spawn_player()'s plane choice and in-plane offset
   used to be fully deterministic every single match
   ((start_on_plane+slot)%N_PLANES, offsets of slot*3/slot*5) -- flagged
   as "not super great." Replaced with roll_spawn_planes(), a real
   Fisher-Yates shuffle of plane indices computed once per match start,
   still preserving the original's "distinct plane per player when
   possible, share/wrap gracefully when not" behavior and its margin-
   clamping fix (a corner piece has no room to maneuver -- this is why
   the 4th local player used to die immediately before that fix existed).

7. draw_text_hud() exposed from stage_interface.h/.cpp (a thin wrapper
   over the existing file-local draw_hud_text()) so menu_multiplayer.cpp
   can render simple text without duplicating the 6x6 bitmap FONT table a
   third time (split_screen_match.cpp and split_screen_menu.cpp already
   each keep their own duplicate copy, per those files' own comments on
   why -- stage_interface.cpp's version is file-local). Also fixed the
   same one-glBegin-per-pixel performance bug in split_screen_match.cpp's
   copy of this font renderer that the Xbox port diagnosed and fixed in
   stage_interface.cpp's copy -- batches one glBegin/glEnd per glyph
   instead of per lit pixel. Harmless-but-free win here regardless of
   platform.


Known scope cuts / not done this pass
------------------------------------------
- No map-select UI for networked matches yet -- the host always starts on
  a random map (net_session::start_match(-1) / start_match_networked's
  own map_idx<0 fallback). The local split-screen flow's existing map-
  select screen (split_screen_menu::print_map_select) could be reused for
  this as a real follow-up.
- No retry flow for a finished networked match (ESC to leave is the only
  option on the match-over screen in network mode) -- retrying would need
  real re-coordination through net_session (everyone agreeing to go
  again), not simply replaying start_match(-2) like local play does.
- No reconnect-after-drop handling beyond "you get bounced back to the
  join screen with a DISCONNECTED message" -- no attempt to resume a
  match in progress.
- Internet play without port forwarding (UPnP/NAT-PMP, UDP hole
  punching, or a relay server) was discussed but deliberately NOT
  implemented this pass -- see the chat history for the real tradeoffs of
  each option. LAN + manual-IP/port-forwarded play is what's built here.
- Packet loss / jitter handling: none. TCP's own retransmission is the
  only reliability mechanism in play; a real internet connection with
  meaningful loss would likely stall ticks noticeably (see tick()'s own
  comment on this).


Real-world test #1 -- results and fixes (2026-11-06)
----------------------------------------------------------
User got it running (after fixing the missing bundled libSDL2_net, see
above) and found real bugs:

1. Networked matches were rendering as local split-screen (window divided
   between every connected player) instead of each player getting their
   own full screen. Real bug: print_match()'s viewport-count logic was
   driven by "how many player ships exist," which in network mode equals
   "how many networked players," not "how many local screens to split."
   Fixed: in network mode, only the LOCAL player's viewport is ever used
   (forced to fullscreen), regardless of how many others are connected --
   the inner per-viewport loop still draws every player's ship into that
   one view, so you still see your opponents, just without the window
   being divided.

2. LAN discovery never found a host (manual IP connect worked fine). REAL
   ROOT CAUSE, not a network/router issue: start_discovery_scan() sent
   the broadcast correctly, but nothing anywhere in net_session.cpp ever
   called SDLNet_UDP_Recv on the discovery socket to collect replies --
   g_discovered could never have been populated, period, regardless of
   network conditions. Added poll_discovery_replies(), called every
   update() regardless of role. Also added logging (previously silent)
   if the HOST's own discovery-responder UDP bind fails, in case that's
   a separate factor on some setup.

3. A match froze mid-game once, and possibly desynced another time (one
   side showed a winner, the other side appeared frozen). Two real bugs
   found and fixed in the lockstep code:
     - No timeout at all: if any connected player's tick input was ever
       lost/delayed, the host would wait forever with no recovery,
       freezing the match for everyone. Added a 5s stall timeout -- the
       host now drops an unresponsive client and continues the match
       without them instead of hanging forever.
     - Ordering bug: tick() called net_session::update() BEFORE
       submit_local_action(), so the host's own tick-readiness check
       always ran against the PREVIOUS tick's local action instead of
       the current one -- not a deadlock by itself, but it structurally
       doubled the effective tick interval for networked play every
       single tick. Fixed the call order (submit, then update).
     - Also fixed g_local_action's initial value (was 0, a valid action,
       instead of -1's "not yet submitted" sentinel used everywhere else
       -- a real correctness inconsistency, not confirmed to be the
       cause of either specific freeze report, but wrong regardless).
   Not independently re-tested yet -- these are real, reasoned fixes for
   real bugs found by reading the code, not confirmed against a repro.

Also, a real architecture fix the user flagged directly: networking
orchestration had been added straight into split_screen_match.cpp (the
LOCAL split-screen engine's own file) -- net_session calls, network-mode
state, lockstep gating, all mixed into functions that used to be purely
local. That broke this codebase's own consistent pattern (every feature
file owns its state and drives neutron only through public APIs, never
reaching into another feature file's internals). Pulled all of it out
into a new net_match.h/.cpp, which is the ONLY place that knows both
split_screen_match AND net_session exist. split_screen_match.cpp is back
to knowing nothing about networking -- it gained a few new GENERIC public
hooks instead (start_match_with_sources, apply_direction, tick_local_only,
print_match's optional forced_single_slot, print_match_over's optional
winner_name_override, leave_match) that net_match.cpp drives it through,
the same way a local caller already does. tick()/print_match()/
handle_key_down()/handle_controller_button_down() remain the exact same
entry points main.cpp/neutron.cpp already call either way -- each one's
first move is checking net_match::is_active() and handing off, so no
call site outside these two files needed to change.

5. Retry on win/loss without disconnecting everyone: previously ESC was
   the only option on the match-over screen in network mode, tearing the
   whole session down. Added real support: host presses Enter/A (or taps
   the left half of the screen) to start another match with the SAME
   still-connected roster and the SAME map, without any disconnect/
   rejoin. Needed a new net_session::match_generation() counter (bumped
   on every real START, initial or retry) so a client -- which has to
   keep pumping net_session::update() while sitting on the match-over
   screen, a real gap that didn't exist before this (nothing was calling
   update() there at all) -- can tell "a fresh retry signal just
   arrived" apart from match_starting()'s permanently-stuck-true state
   since the very first match. See net_match.cpp's retry()/
   update_match_over().

6. Host-side map picker: a simple one, not the local split-screen flow's
   3D miniature-preview map select -- just the name (stage_display_name(),
   already resolves the custom map.txt slot's real name), cycled with
   Left/Right in the host lobby. Whatever the host picks is exactly what
   gets sent to every client in the START message, including the custom
   map if that's what's selected -- joining players always get the same
   map the host chose.

7. Touch input: the main-menu tap handler still had the OLD 7-row
   layout/row-spacing from before Multiplayer was added (8 rows now,
   17px spacing) -- taps on the lower menu items (Options, Credits) were
   hitting the wrong rows or missing entirely, and there was no way to
   reach or confirm anything on the Multiplayer screen by touch at all
   (it fell through to the root main-menu tap handler, since nothing
   excluded game_status==30 from that check). Fixed: updated the row
   count/spacing to match, added a real case for selecting Multiplayer
   from the main menu, and added a game_status==30 branch where tapping
   anywhere acts as Enter (confirms whatever default/selection is
   already showing) -- same "tap anywhere" convention the Credits screen
   already uses, so a touch user never needs to type a name/IP (the
   default-name fallback handles that, as already intended) but can
   still host/join/confirm through the whole flow.


Real-world test #2 -- results and fixes (2026-11-06)
----------------------------------------------------------
8. Touch in the multiplayer sub-menu, round 2: "tap anywhere = Enter"
   (test #1's fix) wasn't enough -- there was no way to actually CHOOSE
   Host vs. Join, or pick a specific LAN game, by touch at all (only
   Up/Down + Enter could). Added real per-row navigation
   (menu_multiplayer::handle_touch(), routed from touch_input.cpp instead
   of a blind Enter): tapping "HOST A GAME"/"JOIN A GAME" selects AND
   confirms in one tap (no separate confirm step -- there's no reason for
   one on touch), tapping a specific LAN game in the list selects AND
   connects immediately (same reasoning -- manual IP entry needs typing
   either way, so a LAN game is the only real touch-only path to Join).
   Also added touch support where there was NONE before: left/right half
   of the screen steers during a live networked match (same convention
   single-player gameplay already uses), and left/right half retries
   (host only) or leaves on the match-over screen.

9. Map-selector arrows now touch-cycleable too (tapping the map-name row's
   left/right half cycles the choice), same left/right-half convention as
   everything else added this round.

10. Real desync bug found and fixed, NOT a same-machine-testing artifact:
   there is no srand() ANYWHERE in this codebase, meaning every process
   starts std::rand() from the same default state, but then diverges
   based on however much each one actually CALLS it before a match
   starts (different menu navigation on host vs. client, different
   timing, etc.). roll_spawn_planes() (spawn-plane shuffle + offsets) and
   "RANDOM" map resolution both called std::rand() independently ON EACH
   PEER -- meaning host and client could easily compute DIFFERENT spawn
   layouts, or even pick DIFFERENT maps outright for the exact same
   "random" match, with nothing over the network keeping them in sync.
   This fully explains stale-looking trails/apparent desync on retry (or
   even on a first match) -- it was never specific to same-machine
   testing.
   Real fix: net_session's START message now carries an explicit shared
   seed alongside the map index (never -1/"random" either -- the host
   resolves that to a concrete index once, locally, before sending, so
   every peer loads the SAME map by construction, not by chance).
   net_match::start() calls srand(seed) identically on every peer right
   before split_screen_match::start_match_with_sources() runs, so
   roll_spawn_planes()'s std::rand() calls produce the exact same
   sequence everywhere regardless of each peer's prior call history.
   Applies to both the initial match start and every retry (a fresh seed
   each time, via SDL_GetTicks(), so a retry's layout isn't identical to
   the match that just ended either). Not independently re-tested yet.