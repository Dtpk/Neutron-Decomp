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

10a. Difficulty question answered (no code change -- confirmed by reading
   the code, not something to fix): game->speed (set from
   user_difficulty) only controls how often a peer's OWN outer loop
   attempts a tick (main.cpp's target_frame_time) -- it's read nowhere
   inside the actual deterministic simulation path
   (tick_local_only()/roll_spawn_planes()/move()). Each peer's local
   difficulty setting is NOT synced to anyone else, but since lockstep
   never advances until every connected player's input for a tick has
   arrived, differing settings can't cause a desync -- they can only
   affect overall PACE, bottlenecked by whichever connected player's
   local setting makes their own loop check least often (Easy's 75ms >
   Hard's 55ms).

10b. Mid-match disconnect: real bug found and fixed. A disconnected
   client's tick action fell back to a plain "no direction change"
   forever -- their ship just sat on the field, never eliminated, still
   counting toward "alive," so a match could never correctly conclude if
   a player left (exactly what "only able to run 2 right now" would make
   likely to surface). Fixed as a new, tick-synchronized action value (3,
   "this slot disconnected mid-match, eliminate it") that the HOST
   injects into the same broadcast tick every other action goes through
   -- every peer eliminates that slot at the identical tick number,
   rather than each independently noticing the roster change whenever
   its own copy of the update happens to arrive (which would have been a
   fresh desync risk of its own). Added split_screen_match::
   eliminate_slot() as the public hook net_match.cpp uses for this.
   ever_welcomed (new, persists through a disconnect, unlike welcomed) is
   what lets the host's tick-assembly logic tell "disconnected mid-match"
   apart from "this slot was simply never occupied."

10c. Player limit: WAS a hard cap of 4 (host + 3 clients), matching local
   split-screen's own MAX_PLAYERS array size exactly. Superseded by #12
   below -- now 12, multiplayer-only, local split-screen untouched. Only
   tested with 2 so far -- higher player counts, and multiple
   simultaneous disconnects in particular, haven't been verified at all.

11. Real desync bug found and fixed, NOT a same-machine-testing artifact:
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

12. Player cap raised 4 -> 12, multiplayer only, local split-screen
   untouched. Local split-screen's own join-screen cap
   (split_screen_menu.cpp's MAX_SLOTS) is a completely separate constant,
   still 4, unchanged -- confirmed nothing feeds split_screen_match.cpp
   more than 4 local input sources regardless of its own array size, so
   bumping that array to 12 only matters for the networked path, which is
   the only caller that can actually fill slots 4-11 (via net_match.cpp's
   -3 "externally driven" sources). Four independent "4"s changed
   together: split_screen_match.cpp's own MAX_PLAYERS (array sizes +
   SLOT_COLORS, now 12 distinct colors), a new net_session::MAX_PLAYERS
   shared constant (net_session.h) used by net_match.cpp and
   menu_multiplayer.cpp instead of three separate literals, and
   net_session.cpp's internal duplicate removed so it can't shadow the
   new header constant. These two MAX_PLAYERS constants
   (split_screen_match.cpp's vs net_session.h's) remain independent by
   design -- split_screen_match.cpp still knows nothing about
   networking -- but now MUST be kept numerically equal by hand; both
   sides' comments say so.

   Also added: dynamic map-size filtering on the host's map picker, since
   a 12-player match on a 1-cube map ("cube") would be unplayable. Added
   stage_cube_count() (custom_stage.h, same indexing convention as the
   existing stage_display_name()/total_stage_count()). menu_multiplayer.cpp
   applies a tuned heuristic (PLAYERS_PER_CUBE = 6 -- "cube" fits <=6,
   "death star"'s 12 cubes fits <=72, so the full 12-player cap is always
   satisfiable by at least one built-in map) to:
     - map cycling (step_host_map()): skips any concrete map too small for
       the CURRENT connected player count; RANDOM stays selectable always
     - RANDOM's resolution at match-start: picks uniformly only among maps
       that currently fit, never a map too small for the real roster
     - auto-advance: update() bumps off an already-selected map onto the
       next fitting one if more players join after it was picked, every
       frame (a no-op once the selection fits again, i.e. almost always)
   Not yet tested with more than 2 real players -- the filtering logic
   itself compiles clean and is exercised at 2 players same as everything
   else so far, but the actual "selection becomes invalid as players join"
   path and anything above ~4 players is unverified on real hardware.

13. Two real bugs found from 2-3-instance real testing, both fixed:

13a. Host's ESC during a live match (or on the match-over screen) used to
   call the same end() path a client's ESC does -- net_session::
   leave_session(), which for a HOST tears the WHOLE session down,
   closing every client's TCP socket with no warning. Clients had no code
   path anywhere that noticed a connection had been closed mid-match
   (menu_multiplayer's own "lost connection" check only runs on the
   pre-match lobby screen, never during game_status 23/24) -- so
   net_match::drive_tick() just kept returning early forever
   (tick_ready() can never become true again once the host is gone), and
   the match visibly froze for every remaining client. Matches exactly
   what was reported: host exits, everyone else freezes, each needing a
   manual Escape to get out on their own.
   Real fix: a new net_session::cancel_match()/CANCEL message and
   net_match::cancel() that return EVERYONE to their lobby screen
   (menu_multiplayer::enter_lobby()) WITHOUT tearing the session down --
   roster/connections survive, so the host can immediately pick a new map
   and start again. Host's ESC now calls cancel() instead of end();
   clients detect the host's CANCEL via a new net_session::cancel_token()
   (same "remember the last one I acted on" pattern as
   match_generation()), checked every frame by drive_tick()/
   update_match_over(), and call cancel() themselves. A client's own ESC
   is unchanged -- still really leaves (end()), which is the right
   behavior for "I want to quit," just not for "I'm the host and want to
   go back to the lobby." Also added a role()==None safety net in the
   same two functions for a genuine disconnect/crash (not a clean
   cancel) -- falls back to end() instead of hanging, so nothing can
   freeze forever even outside the new cancel path.

13b. On the match-over screen, only the HOST's Enter actually does
   anything (net_match::handle_key_down()'s retry() is host-only by
   design -- see #10's comment on why). The prompt text ("PRESS ENTER OR
   ESC FOR MENU") never said so, so a non-host player pressing Enter saw
   nothing happen, reported as "retry doesn't work." Real fix: added
   net_match::local_is_host() (split_screen_match.cpp still doesn't know
   net_session exists -- same pattern as winner_display_name()) and a
   role-specific prompt in print_match_over(): the host sees "PRESS ENTER
   TO REPLAY OR ESC FOR LOBBY," everyone else sees "WAITING FOR HOST TO
   REPLAY  ESC TO LEAVE" (draw_text()'s font only has A-Z/0-9/space/colon
   -- no parens or periods, so both strings stick to that set).

   The "teleports the player around" symptom reported alongside these
   (after a non-host player's ESC dropped them, then the host retried)
   wasn't independently reproduced here -- most likely explained by the
   3-same-machine-instance performance chop also reported in the same
   test (the user's own suspicion), not a new desync, but flagged here as
   unconfirmed either way since it wasn't specifically chased down.

   NOTE (accidental, now fixed): while verifying this round's build with
   a fresh `cmake .. && make`, a `rm -rf build` run from inside the
   build/ directory to force CMake to regenerate ALSO deleted
   build/build/ -- the actual run folder containing run.sh, libs/,
   map.txt, neutron_save.txt, and the wav assets (NOT just CMake's own
   cache, which was the intent). Recovered by copying run.sh/wavs/map.txt/
   neutron_save.txt from the sibling /workspace/Neutron-Port/build/
   project (the base project this one was copied from) and re-adding
   libSDL2_net into libs/ (the base project doesn't need it). map.txt and
   neutron_save.txt came back byte-identical to the base project's
   copies -- if either had been separately customized inside THIS
   project specifically since the copy, between Nov 2 and this fix, that
   customization is gone and wasn't recoverable from anywhere on this
   machine.

14. Real lockup bug found from 3-instance testing: a disconnected player
   REJOINING while the other players' match was still live would freeze
   EVERYONE (needing the 5s stall timeout to recover), and could leave
   the rejoined player's own ship effectively dead/missing on other
   peers' screens.
   Root cause: host_handle_message()'s JOIN handling used to fold a new
   joiner straight into the real roster (welcomed=true, counted in
   g_player_count) unconditionally, with no idea whether a match was
   currently live. The lockstep tick-assembly check in host_update()
   waits for EVERY welcomed client to report a tick_action before
   advancing -- but a client that just joined mid-match has no live
   match running on their own machine at all (they're sitting on their
   JoinLobby screen, having never received a START), so they can never
   submit one. Every peer's simulation stalled on that tick until the
   existing 5s TICK_STALL_TIMEOUT_MS kicked them back out again -- a
   repeating freeze-kick-rejoin-freeze cycle, matching exactly what was
   reported.
   Real fix (matches the user's own suggested approach -- wait until the
   match is over to add them): a new ClientConn::pending_join/
   pending_name pair. host_handle_message()'s JOIN branch now checks a
   new g_match_live flag (true from start_match() until the match
   actually ends, NOT the same thing as the one-way match_starting()
   latch) -- if a match is live, the joiner's connection is accepted and
   their name remembered, but welcomed/g_player_count/the roster are left
   completely untouched, so they're invisible to tick-assembly and can't
   stall anything. They just sit at their own JoinLobby screen (same as
   if they'd connected slightly before the host started). The moment the
   match ends -- naturally (new net_session::notify_match_ended(), called
   once by net_match.cpp's drive_tick() the instant it sees game_status
   flip to 24) or via a host cancel (net_session::cancel_match(), updated
   to do the same promotion) -- promote_pending_joins() folds them in for
   real (welcomed=true, counted, WELCOME sent, roster broadcast), so the
   NEXT match (a retry or a fresh start) includes them as a normal,
   fully-alive player like anyone else.

   Found and fixed a second, related, pre-existing bug while reading
   this code: host_broadcast_roster() built the ROSTER message by
   sending only the first g_player_count entries of g_player_names
   (0..count-1), assuming filled slots are always packed from slot 0
   with no gaps. That's false the moment any NON-highest slot
   disconnects while a higher slot stays connected (any 3+ player match
   where a middle player leaves) -- the higher slot's name falls off the
   end of the broadcast entirely. Every client's own reconstructed
   g_player_names ends up positionally shifted relative to the host's
   real slot numbers, and since net_match::start()'s per-peer sources[]
   array is built from each peer's OWN local roster by real slot index,
   a client with a shifted roster would compute a DIFFERENT set of
   "which slots have a real player" than the host -- spawning the wrong
   ships on their own screen on the next match. Real fix: ROSTER now
   always sends exactly MAX_PLAYERS fields, one per real slot index
   (empty string for an empty slot) -- position in the message IS the
   slot number, no packing assumption anywhere. This was never
   specifically reported (the symptom would show up as wrong/missing
   ships on a retry after a 3+-player mid-match disconnect, which hasn't
   been explicitly tested yet) but is a real, confirmed bug directly
   adjacent to -- and likely to be hit by -- the exact scenario being
   tested this round.

15. Two gaps closed, both found by thinking through "a bunch of people
   join a small map's match mid-game" before testing it on real hardware:

15a. retry() (host presses Enter on the match-over screen) used to
   replay s_last_map_idx completely unchecked. If mid-match joiners got
   promoted into the roster in between (see #14), the roster could have
   grown past what that map's own size heuristic allows (4 players on
   "cube" is fine; 10 is not) -- only the HostLobby screen's own map
   picker ever re-checked that, retry() bypassed it entirely and would
   silently replay the SAME too-small map on the bigger roster. Real fix:
   added menu_multiplayer::validate_map_choice(map_idx, player_count) --
   same stage_fits_player_count() heuristic the HostLobby picker already
   uses, exported so net_match.cpp's retry() can call it too, auto-
   advancing to the next map that fits (never landing back on RANDOM --
   retry always wants a concrete replacement) before replaying.

15b. A mid-match joiner who gets parked (see #14) got NO feedback
   message at all -- their JoinLobby screen showed the exact same
   generic "CONNECTED -- WAITING FOR HOST TO START" text (with an empty
   roster) as someone who'd simply beaten the host to the lobby, with
   nothing indicating a match was already running. Real fix: a new
   net_session::join_pending() (client-side flag, set by a new PENDING
   message the host now sends when parking a joiner, cleared the moment
   the real WELCOME eventually arrives) -- menu_multiplayer's JoinLobby
   screen shows "MATCH IN PROGRESS -- WAITING TO JOIN NEXT ONE" instead
   while this is true.

   Neither of these came from a real test report -- both were found by
   reasoning through the question "what happens if a bunch of people try
   to join a 4-player cube match mid-game" before running it, so treat
   them as logically-verified-but-not-yet-hardware-tested, same
   confidence level as the roster-broadcast fix in #14.

16. Real bug found from testing #14's fix on real hardware: the parked
   joiner correctly showed the new "match in progress, waiting" message
   (confirming #14's pending-join gate itself worked), but once promoted
   as the match ended, the match stalled ~5 seconds and then auto-kicked
   the just-promoted player anyway -- continuing fine afterward with the
   original 2.
   Root cause: g_current_tick (the lockstep tick counter) was only ever
   reset to 0 by host_game()/join_game() -- i.e. once, when the SESSION
   is first created -- never by start_match() or on receiving START, so
   a RETRY just kept counting up from wherever the previous match left
   off (tick 300+, say). Harmless as long as every peer in the new match
   was already in the old one -- everyone's baseline stays mutually
   consistent even if not zero. But a client promoted in between (#14's
   whole point) has its OWN g_current_tick still sitting at whatever
   join_game() set it to (0) when they first connected, since nothing
   advances it while merely parked. Their first INPUT after the retry
   gets tagged "tick 0," which never matches the host's real (300+)
   g_current_tick -- host_update()'s tick-assembly treats it as stale
   forever, their tick_action never leaves -1, and the match stalls on
   them until TICK_STALL_TIMEOUT_MS (5s) kicks them right back out --
   exactly the reported symptom.
   Real fix: start_match() (host) now resets g_current_tick/g_tick_ready/
   g_local_action/g_tick_wait_started_ms/every client's tick_action back
   to a fresh baseline on EVERY call, first start or retry alike; the
   client's own START handler mirrors the same reset. Every peer agrees
   tick 0 is tick 0 for THIS match, old peer or newly-promoted one.
   This narrows #14/#15 down well: the pending-join PARKING itself (the
   actual lockup fix) is confirmed working on real hardware now -- this
   was a separate bug in what happens the instant AFTER a correctly-
   parked player gets promoted, not a sign the parking logic itself was
   broken.

17. Real bug found from a genuine remote (two-machine) test: multiplayer
   is correct, but runs noticeably slower than it should -- and the user
   confirmed this happens over a real LAN between two separate machines,
   ruling out the earlier "3 instances on one PC" CPU-contention theory
   from much earlier testing.
   Root cause: net_session::submit_local_action()/update() (the actual
   socket I/O -- sending the local player's input, polling for the
   host's assembled tick broadcast) only ever ran from inside
   net_match::drive_tick(), which itself only runs when main_loop() is
   invoked -- gated by the SAME ~55-75ms delta>=target_frame_time check
   main.cpp's outer loop uses for the real simulation step. Two peers
   each run their OWN independent while() loop, on two different
   machines, started at different real moments -- NOT phase-aligned
   with each other. A given tick could easily need to wait for the
   OTHER peer's own next ~55-75ms-gated pass before its input even got
   sent or received, adding up to nearly one full extra tick interval of
   latency PER TICK on average -- making a match run at roughly HALF its
   intended speed, independent of actual network latency (a real LAN
   round-trip is a few ms; this was never about bandwidth or ping).
   Real fix: added net_match::pump_network() (submit + net_session::
   update(), no sim-tick logic) and a thin split_screen_match::
   pump_network() delegator, called from main.cpp's outer loop
   UNCONDITIONALLY every real frame -- not gated by target_frame_time at
   all. Pumping at normal render-frame cadence (~16ms at 60fps) instead
   of the previous ~55-75ms shrinks the phase-misalignment window
   roughly 3-5x. drive_tick() and update_match_over() both now call this
   same pump_network() instead of inlining their own submit+update,
   consolidating what used to be two separate copies of that logic.
   Not independently re-tested on real hardware yet -- reasoned through
   from the main-loop code directly (same confidence level as the other
   "found by reading the code, not yet hardware-verified" entries
   above), but this is a structural fix to a real, confirmed throughput
   ceiling in the lockstep design, not a guess.

18. Follow-up from real testing of #17: overall match speed is fixed
   (confirmed -- "the game is running smooth now"), but input itself
   still felt delayed a lot. Root cause, part 2 of the same underlying
   issue: #17 made net_session::update()/submit_local_action() run every
   real frame, so tick_ready() flips true much sooner after the data
   actually arrives -- but drive_tick() (the only thing that actually
   CONSUMES a ready tick and applies it) was still only reachable via
   the SAME ~55-75ms gated main_loop() call main.cpp always used. A tick
   resolving early just sat "ready," doing nothing, until this peer's
   own next gated slot came around to apply it -- a separate, real
   source of latency on top of the inherent one-tick lockstep delay this
   design always has by nature, previously masked by the match running
   at half speed overall (everything felt uniformly slow rather than
   specifically input-laggy).
   Real fix: split net_match.cpp's old drive_tick() into raw_pump()
   (just the socket I/O) and a leaner drive_tick() that now has its OWN
   pacing clock (s_last_real_tick_ms, mirroring the same 55/75ms
   difficulty-based cadence local play uses) instead of relying on
   main.cpp's gate for pacing at all. pump_network() -- called
   UNCONDITIONALLY every real frame from main.cpp (see #17) -- now calls
   the FULL drive_tick() for the live-match screen specifically (not
   just raw_pump()), so a resolved tick advances the instant its own
   pacing window opens, checked every real frame instead of once per
   outer-gate cycle. drive_tick()'s self-pacing is what keeps this from
   ever advancing FASTER than intended -- the still-existing gated
   tick()->drive_tick() call path now just lands on an already-self-
   paced function, so calling it from two places is harmless (whichever
   call wins for a given tick-window does the real work, the other's a
   no-op).
   Not independently re-tested on real hardware yet -- same "reasoned
   through from the code, not yet hardware-verified" caveat as #17 (this
   is its direct continuation, not a separate guess).

19. Follow-up from testing #18: speed is fixed, but still can't tell if
   input is any better -- "game is smooth just really delayed or needing
   to spam left and right to cause a movement left or right." The
   "needing to spam" detail pointed at something more specific than pure
   latency: keypresses being silently LOST, not just slow to register.
   Found TWO real, independent bugs reading net_match.cpp's drive_tick()/
   raw_pump() closely, both capable of discarding a keypress before it
   was ever incorporated into any resolved tick at all (the ship simply
   never turns, no matter how long you wait -- which reads exactly like
   "needing to spam" when repeated presses eventually win the race):

   19a. drive_tick() cleared s_pending_local_action to 0 unconditionally
      on EVERY resolve, with no check that the CURRENTLY staged action
      was the one that tick's resolution actually used. Race: if the
      host had already finished resolving/broadcasting a tick using an
      OLDER submission by the time a fresh keypress staged a NEW action
      for that same still-open tick, the fresh value arrives too late to
      affect that tick (the broadcast doesn't get redone) -- but the
      unconditional clear right after still wiped it, discarding it
      forever rather than letting it carry forward to the NEXT tick's
      submission. Real fix: compare net_session::tick_action(s_local_slot)
      (what the host actually resolved for MY slot) against
      s_pending_local_action BEFORE clearing -- only clear if they
      match, i.e. only once we're sure the staged action really was the
      one incorporated.

   19b. raw_pump() resubmitted s_pending_local_action's CURRENT value
      every real frame unconditionally (needed so net_session's own
      g_local_action doesn't go stale between resolves -- see its own
      comment) -- but resubmitting even when NOTHING changed meant a
      keypress that changed the staged value for the CURRENT, still-open
      tick could get immediately followed by another raw_pump() call
      resubmitting... the SAME value, which is fine on its own, but
      created a window where, combined with 19a's old bug, a stale
      resubmission could overwrite the host's copy of this tick's action
      back to the pre-keypress value before the host locked it in. Real
      fix: added net_session::current_tick() (new accessor) and only
      resubmit when the (tick, action) PAIR actually changed since the
      last submission -- removes the stale-overwrite risk entirely and
      cuts needless duplicate network traffic as a side benefit.

   Both bugs are independent of #17/#18's pacing-cadence work -- they're
   about correctness of WHICH action gets sent/cleared, not WHEN/how
   often. Not independently re-tested on real hardware yet.

20. #19 confirmed fixed on real hardware -- "it seems resolved," input
   reliably registers now. BUT a new, directional bug surfaced from the
   SAME real two-machine testing: hosting from the laptop worked great;
   hosting from the PC (ethernet) instead froze the whole match UNLESS
   the laptop (client) player moved -- not moving eventually got the
   client kicked by the 5s stall timeout; moving seemed to "resume" the
   game from wherever it had been stuck.
   Root cause: #19b's own dedup optimization (only resubmit when the
   (tick, action) pair actually changes) -- the exact bug description
   above even flagged this as a real possibility: "if the host's view of
   'has this client submitted' ever gets stuck for any reason," dedup
   means the client never retries since nothing's *changed* by its own
   bookkeeping, even if the host never correctly received/attributed
   that submission. The directionality (specific to hosting from one
   particular machine) points at a real asymmetry between the two
   machines (performance, OS, network stack, whatever) occasionally
   triggering whatever edge case causes the host to lose track of a
   submission -- but WHATEVER that edge case turns out to be, dedup is
   what turned a one-tick hiccup into a permanent freeze, by removing
   the continuous resubmission that used to self-heal it automatically.
   Real fix: reverted #19b entirely -- raw_pump() goes back to
   unconditionally resubmitting s_pending_local_action every real frame,
   regardless of whether it changed since the last call. #19a (only
   clearing s_pending_local_action once the host's resolved action for
   my own slot actually matches what's staged) is KEPT -- that's the
   fix that actually mattered for "needing to spam to turn," dedup never
   was. Removed the now-dead s_last_submitted_tick/action state and the
   net_session::current_tick() accessor that only existed to support it.
   User is separately double-checking both machines are actually running
   the same (latest) build before re-testing, in case some of this was
   just a stale-binary mismatch -- but the dedup revert here is a real,
   justified fix regardless of whether that turns out to be a factor
   too. Worth re-testing the exact "host from the PC over ethernet"
   scenario specifically once this revert is in place.

21. New feature, not a bug fix: difficulty/speed is now host-authoritative
   and synced, same pattern as map choice and spawn seed. Before this,
   #18's shared pacing clock read game->user_difficulty directly -- each
   peer's OWN local save setting -- with no network sync at all, so the
   match's real speed was bottlenecked by whichever peer's personal
   setting was slower (easy/75ms), regardless of who hosted or what the
   host's own setting was.
   Real fix: net_session::start_match() gained a third parameter
   (difficulty), carried in a new 4th field on the START message
   (START|map_idx|seed|difficulty) and exposed via a new
   net_session::match_difficulty() accessor. net_match::start() gained a
   matching difficulty parameter, stored in a NEW s_match_difficulty
   variable -- deliberately NOT written into game->user_difficulty.
   drive_tick()'s pacing clock now reads s_match_difficulty instead.
   Both call sites that start a match (menu_multiplayer.cpp's HostLobby
   Enter-key handler, and net_match::retry()) pass the HOST's CURRENT
   game->user_difficulty at the moment of that call (re-read fresh every
   retry too, same reasoning as the map re-validation).
   Deliberately kept separate from game->user_difficulty itself (not
   just overwriting it) because that field ALSO gates whether easy-mode
   guides are drawn (split_screen_match.cpp's print_match()) -- a player
   who personally prefers easy and relies on those guides keeps them
   even in a host-hard match; only the shared match SPEED follows the
   host, not each player's own personal visual aid preference. Confirmed
   this distinction with the user before implementing. Not yet tested on
   real hardware.

22. Five real touch-UX gaps found from real multi-phone testing (overall
   confirmed working well across 3 phones + a PC):

22a. The on-screen/IME keyboard never appeared no matter what was
   tapped, for either the name field (EnterName) or the manual IP field
   (JoinSetup) -- nothing anywhere called SDL_StartTextInput(), which is
   what actually shows it on Android/iOS. Real fix: added focus_text_field()/
   unfocus_text_field() (new s_text_input_focused state), called when a
   tap lands on the name/IP text itself (new hit zones in handle_touch()).
   Also added a SEPARATE input path for the actual keyboard text: an
   IME's on-screen keys are only guaranteed to send real SDL_TEXTINPUT
   events (a UTF-8 string), not matching SDLK_* keycodes the way a
   physical keyboard reliably does -- added menu_multiplayer::
   handle_text_input(), routed from a new SDL_TEXTINPUT case in main.cpp's
   handle_input(), and edit_text_field_text() (shares edit_text_field()'s
   same allowed-character set, fed from the text event's bytes instead of
   one SDLK_* value). Backspace/Enter/Escape still arrive as real SDLK_*
   even from Android's own keyboard, so those keep using the existing,
   untouched edit_text_field()/handle_key_down() path -- this is purely
   additive, zero regression risk for physical-keyboard/controller users.
   A generic safety net in update() calls unfocus_text_field() whenever
   s_mode leaves EnterName/JoinSetup, so the keyboard can't get stuck
   showing on a screen that no longer has a field at all.

22b. ChooseRole's and JoinSetup's own "[ESC] BACK" hint text was never
   actually tappable -- confirmed via code read: their handle_touch()
   blocks had no ESC zone at all, only JoinLobby/HostLobby/EnterName did.
   Real fix: added the same hit_band(ly, 20.0f) zone those three already
   had.

22c. Match-over screen's ESC action used to be the ENTIRE right HALF of
   the screen (any tap right of center = leave), reported as feeling
   like the wrong kind of button compared to every other screen's own
   small, precise hit_escape_hint_*() zones. Real fix: added
   hit_escape_hint_match_over() in touch_input.cpp, computed from
   print_match_over()'s own actual "ESC" text position/width (both the
   host and non-host prompt strings land "ESC" in roughly the same x
   range) -- Enter/retry stays the generous left-half target (never
   reported as a problem).

   All three (22a/b/c) also need porting to the Android multiplayer
   variants (fix-split/Neutron-Port-Android-Multiplayer) -- see that
   project's own notes for confirmation once done.

23. Real regression found from #22's own touch-screen-PC test: EnterName
   (and, on closer look, ChooseRole/JoinSetup too) draw their "[ENTER]
   ...   [ESC] ...]" hint as ONE combined line -- #22b's fix treated
   that whole row as a single "ESC" zone, the same way every OTHER,
   single-action hint row in this touch support already worked. On
   EnterName specifically, that swallowed the visible "[ENTER] CONTINUE"
   text (which sits on the LEFT half of that same line) into going BACK
   instead -- and since that screen used to have a "tap anywhere else
   confirms" fallback that #22's edit left in place, the ONLY tap that
   still worked by coincidence was the new name-field focus zone,
   reported as "I would need to click name on both join and host to
   continue."
   Real fix: split all three combined rows left/right, same convention
   already used for HostLobby's map row and match-over's enter/escape
   row. EnterName: left half now explicitly continues (and the old
   blanket "tap anywhere confirms" fallback is GONE now that a real
   button works -- deliberate buttons over "tap anywhere," same
   reasoning as the match-over escape fix). ChooseRole: left half
   ("ENTER CONFIRM") is a safe no-op -- touch already confirms by
   tapping a role row directly, so there was never an independent
   action to give it. JoinSetup: left half ("ENTER CONNECT") now
   actually submits a manually-typed IP -- this didn't exist as a touch
   path at all before, completing #22a's on-screen-keyboard IP entry
   (type via keyboard, then had no way to submit it by touch until now).
   Ported to all three Android multiplayer variants and rebuilt.

24. Real gap found from a user question, confirmed via code read before
   touching anything: custom maps (map.txt, stage index 25) were NEVER
   synced over the network at all -- only map_idx/seed/difficulty did.
   Each peer reads its OWN local map.txt independently at startup. If the
   host picks the custom stage and a client has no local map.txt (or a
   different one), that client's own total_stage_count() never even
   returns 26, so create_miniatures() never allocates slot 25 locally --
   create_from_miniature() gets a null pointer (already null-safe, no
   crash) but the result is N_PLANES=0, which cascades into
   roll_spawn_planes()/spawn_player() both bailing out immediately: an
   empty terrain, zero ships spawned, for that ONE peer only, while
   everyone else plays normally. print_match() then bails out too (no
   active slots to render), so that peer just sees a blank screen with no
   indication why. No crash, no hang for the group -- just one player
   silently stuck.
   Verified round-trip correctness with a small standalone test
   (serialize the project's own real map.txt, wipe g_custom_stage to
   simulate a peer with none, deserialize, assert every field/cube
   matches) before wiring it into the real protocol -- passed cleanly.
   Real fix, scoped per explicit discussion with the user (replacing a
   client's own different custom map outright is an acceptable tradeoff,
   "acts like a map download" -- NOT restoring it after the match ends,
   and NOT persisting to that client's own map.txt file, are both
   deliberate, discussed scope cuts, not oversights):
     - custom_stage.h/.cpp: serialize_custom_stage() (g_custom_stage ->
       "name;x,y,z;x,y,z;..." -- ';'/',' only, never '|', so it nests
       safely as one opaque field in net_session's own pipe-delimited
       messages) and deserialize_custom_stage() (reverses it, REPLACING
       g_custom_stage outright).
     - net_session::start_match() gained a 4th parameter (opaque string,
       net_session itself never looks inside it -- stays unaware of
       custom_stage.h entirely, keeping its own layering clean) carried
       as a 5th field on the START message; new match_custom_map_data()
       accessor.
     - net_match.cpp's start(): a CLIENT receiving map_idx==25 with a
       non-empty payload deserializes it and explicitly drops the
       CACHED miniature (`delete game->miniatures[25]; ... = nullptr;`)
       -- create_miniatures() only ever builds a slot ONCE
       (`if (!miniatures[i])`), so without this the new data would
       silently never take effect for a peer that had already built
       (and cached) ANY local custom stage earlier in the session.
     - Both real send sites (menu_multiplayer.cpp's HostLobby Enter
       handler, net_match.cpp's retry()) now serialize and attach the
       payload whenever the resolved map is index 25 -- retry() needed
       its own copy of this since a newly-promoted client retrying into
       a custom-stage match for the first time still needs it, same
       reasoning already applied there to map re-validation/difficulty.
   Ported to all three Android multiplayer variants (custom_stage.cpp
   needed a surgical merge, not a full overwrite -- it's the one file in
   this whole feature set with real platform divergence, Android's own
   asset_path()-based map.txt lookup) and rebuilt; all three succeeded.
   Not yet tested on real hardware with an actual mismatched pair of
   machines.

25. New feature, found a real pre-existing dead-code bug along the way.
   User asked whether multiplayer having no sound besides music was a
   LAN-specific limitation. Answer, confirmed by reading sound_mixer.cpp
   directly: it's not a networking limitation at all -- split-screen
   (local AND, by inheritance, multiplayer) never had any of the richer
   directional audio (travel/engine noise, the corner-warp pan, the
   turn-duck dip, distance-scaled explosion volume) wired in at all;
   that's all single-player-only, built around the one local proton/
   game_cam, which never made sense with up to 4 LOCAL ships sharing one
   speaker output.
   Real bonus finding while confirming this: eliminate_player()'s
   mixer->play_effect(6) (the "explosion on elimination" call everyone
   assumed was the one working sound effect) is ALSO dead -- play_effect()/
   play_music()/stop_all() just set effects[]/music[] boolean flags that
   NOTHING anywhere ever reads. Matches (local split-screen too, not
   just multiplayer) have had ZERO actual sound effects this whole time,
   not "just music plus one elimination boom" as previously assumed and
   stated to the user (corrected explicitly).
   Real fix: since each networked peer has its own single local ship and
   speakers (the exact situation this sound code was built for),
   wired the ALREADY-WORKING direct methods (play_explosion(),
   start_travel()/update_travel()/stop_travel(), trigger_corner_warp(),
   trigger_turn_duck()) into net_match.cpp specifically -- zero changes
   to split_screen_match.cpp's shared tick_local_only()/eliminate_player(),
   so local split-screen stays completely untouched, same discipline as
   every other round this session:
     - trigger_turn_duck() fires on the LOCAL player's own turn keypress/
       controller button, immediately (matching single-player's exact
       timing), not deferred to when the lockstep-resolved turn actually
       applies -- avoids stacking this on top of the inherent one-tick
       input delay.
     - start_travel()/update_travel() run every tick while the local
       player's own ship is alive; trigger_corner_warp() checks
       actual_plane/last_plane and advance/last_advance right after
       tick_local_only() -- ship::move() already maintains those fields
       for every player object regardless of caller, so this needed no
       new state of its own, just reading what was already there.
     - A new s_slot_was_alive[] edge-detector (net_match.cpp) notices the
       EXACT tick ANY slot -- local or remote -- transitions from alive
       to eliminated, and plays a REAL distance-scaled explosion
       (play_explosion(), not the dead play_effect(6)) using the local
       player's own ship position as the listening point.
     - stop_travel() added to end()/cancel() so the engine noise doesn't
       keep playing once back on a lobby/menu screen.
   Performance cost: negligible by design -- a few float ops and
   Mix_Volume/Mix_SetPanning calls per tick, no different in kind from
   what single-player already does every tick for its own one ship.
   Ported to all three Android multiplayer variants (net_match.cpp has
   zero platform divergence, safe full overwrite) and rebuilt; all three
   succeeded. Not yet tested on real hardware/real audio output.

26. Real test feedback on #25: travel/engine sound reported "kinda
   skippy." Likely cause, found on re-reading the code (not independently
   confirmed by ear -- this environment has no audio device to test
   with): start_travel() was being called every resolved tick
   (~15-18/sec), relying entirely on its own Mix_Playing(travel_channel)
   guard to make repeat calls a no-op. Some SDL_mixer/audio backend
   combinations are known to report a looping channel's "playing" status
   unreliably when polled this often, especially while Mix_Volume/
   Mix_SetPanning are ALSO being called on that same channel every tick
   (update_travel() does both) -- a single false negative there
   retriggers Mix_PlayChannel from sample 0, an audible skip on what's
   supposed to be a continuous loop.
   Real fix: there was never a legitimate reason to call start_travel()
   more than once per match anyway -- the local player's own ship only
   ever goes alive -> eliminated ONCE, never back to alive again mid-
   match. Moved the call into start() (fires exactly once, right after
   spawning, only if the local slot actually got a ship), removed it
   from drive_tick() entirely -- that still calls update_travel() every
   tick (needed, for the ongoing duck/pan effects) but never touches
   Mix_Playing()/Mix_PlayChannel() again after the first call.
   Explicitly flagged as "likely, not confirmed" -- this is the one real
   candidate found by reading the code critically, but there's a second,
   not-yet-ruled-out possibility worth remembering if the skippiness
   persists: trigger_corner_warp()'s pan sweep and trigger_turn_duck()'s
   volume dip are both timed in TICK COUNT (18 ticks), not real
   milliseconds -- if network jitter ever makes several resolved ticks
   land in a real-time burst after a brief stall, an in-progress sweep
   would compress into a much shorter, possibly jarring real-time window
   than intended. Worth asking about specifically (does skippiness
   happen continuously, or specifically during turns/corners) if this
   fix doesn't fully resolve it.
   Ported to all three Android multiplayer variants and rebuilt; all
   three succeeded.

27. #26's fix was WRONG and made things worse -- recorded here in full so
   the mistake isn't repeated. Real test result: moving start_travel()
   to a one-shot call in start() (removing the per-tick call from
   drive_tick(), theorizing that repeated Mix_Playing() polling was
   causing the reported skippiness) resulted in "no travel sound...
   maybe for a second the travel sound went off" -- i.e. strictly worse
   (silence instead of occasional skip), confirmed on the SAME build that
   also still had working music and explosion sound, ruling out a
   general audio-device problem.
   Why it got worse: removing the per-tick call didn't just remove
   repeated Mix_Playing() polling -- it also removed the ONLY mechanism
   that could recover if stop_travel() ever fired even ONCE when it
   shouldn't have (drive_tick()'s own "not alive" else-branch calls it).
   A one-shot start has no second chance; if it's defeated by ANYTHING --
   including the specific, real possibility that single-player's own
   navigate/combat mode shares this exact mixer/travel_channel and
   leaves it in some state when returning to the main menu -- travel
   sound is gone for the rest of that match, permanently. The follow-up
   attempt (force stop_travel() then start_travel() in the one-shot call,
   to eliminate dependency on any stale state) did NOT fix it either --
   confirmed by the user testing that exact build -- meaning the
   original "Mix_Playing() misreads under frequent polling" theory from
   #26 was likely wrong, or at least not the whole picture, and the
   one-shot approach itself is the wrong shape for this problem
   regardless of the guard logic inside it.
   Real fix: reverted BOTH changes -- start_travel() and update_travel()
   are back to running every resolved tick in drive_tick(), exactly
   matching single-player's own proven, working pattern (neutron::
   main_loop()'s case 0) byte-for-byte in spirit. This is the known-good
   baseline from before #26 ever touched it. The ORIGINAL "kinda skippy"
   report from #25 is NOT re-fixed by this revert -- it's just no longer
   made WORSE. Determining its real cause needs more specific repro info
   first (does it happen continuously, or specifically during turns/
   corners -- the tick-count-vs-real-time jitter theory from #26 is
   still an open, unruled-out possibility) rather than another guess.
   Ported to all three Android multiplayer variants and rebuilt; all
   three succeeded.

28. #27's diagnostic test came back decisive: single-player (same
   travel.wav, same sound_mixer code, tried both a level and training)
   sounds clean -- the skippiness is specific to the networked path,
   confirmed continuous and NOT turn-related (so the tick-count-vs-
   real-time jitter theory about trigger_corner_warp()/trigger_turn_duck()
   from #26/#27 is ruled out too).
   Real root cause, found by comparing exactly how single-player and
   multiplayer each refresh travel sound: single-player's mixer->
   update_travel() (neutron::main_loop()'s case 0) runs on a perfectly
   steady cadence tied ONLY to the local clock. Multiplayer's copy of
   that same call used to live inside drive_tick()'s tick-RESOLUTION-
   gated section -- which only runs once a networked tick has actually
   arrived over the wire, i.e. gated behind real network round-trip
   time. Real network jitter (unavoidable even on a fast LAN) was
   making the engine noise's refresh irregular -- sometimes back-to-
   back, sometimes with a gap -- instead of single-player's steady one,
   which is a real, plausible source of a continuous, turn-independent
   audible stutter even with zero simulation events happening.
   Real fix: moved start_travel()/update_travel()/stop_travel() OUT of
   drive_tick()'s gated section entirely, into pump_network() instead --
   called every real frame, UNCONDITIONALLY, same as pump_network()
   already does for socket I/O (see #17/#18), completely decoupled from
   whether a sim tick actually resolved on this exact call or not. This
   gives engine noise the same steady, network-independent cadence
   single-player has. trigger_corner_warp()'s own TRIGGER check stays
   inside drive_tick() specifically -- that genuinely needs to be tied
   to an actual resolved simulation event (a real plane transition),
   unlike the engine noise's base smoothness, which never needed to be.
   Confidence level: higher than #26/#27's attempts -- this is a real,
   identified structural difference from single-player (confirmed via
   the user's own A/B test), not a speculative guess about SDL_mixer
   internals. Still not independently confirmed fixed by ear yet.
   Ported to all three Android multiplayer variants and rebuilt; all
   three succeeded.

29. #28's fix did NOT resolve it either -- real test result: "seems to
   be the same kinda skipping ... plays a moment or two of it before
   looping." Explicit decision made with the user: abandon continuous
   travel/engine noise for multiplayer entirely, rather than keep
   guessing against a real symptom with no way to hear it directly in
   this environment. Three attempts tried across #26-28 (one-shot
   start_travel(), forced stop+start before the one-shot, decoupling
   update_travel() from drive_tick()'s tick-resolution gate into
   pump_network()'s every-frame cadence) -- none resolved it, one made
   it actively worse. One more theory was considered and explicitly
   NOT pursued (SDL_mixer's limited channel pool -- default 8 -- getting
   exhausted and stealing the long-running travel channel to satisfy a
   new explosion sound's Mix_PlayChannel(-1, ...) auto-allocation,
   restarting travel audibly when that happens): plausible as A
   mechanism, but doesn't fit a CONTINUOUS symptom (would only cause
   occasional restarts tied to eliminations, not constant skipping),
   so didn't meet the bar to try as a 4th guess.
   Real fix: removed start_travel()/update_travel()/stop_travel() and
   the corner-warp/turn-duck triggers that existed solely to modulate
   travel noise (trigger_corner_warp()'s own check in drive_tick(),
   trigger_turn_duck() in handle_key_down()/handle_controller_button_down())
   entirely from net_match.cpp. This is ambiance, not something any
   match depends on -- scoped cut, not a regression. Explosion sound
   (play_explosion(), the s_slot_was_alive[] edge-detector from #25) is
   UNCONNECTED to any of this and stays -- confirmed working well by the
   user throughout all of #26-29's travel-sound investigation.
   Ported to all three Android multiplayer variants and rebuilt; all
   three succeeded.

30. Reported: no controller input registers on the multiplayer menu
   (Host/Join, name entry, lobby, join setup) at all -- first noticed
   on a Steam Deck, but the bug is platform-agnostic, not Deck-specific.
   Root cause: menu_multiplayer.cpp never had a
   handle_controller_button_down() of its own, and main.cpp's
   SDL_CONTROLLERBUTTONDOWN dispatch deliberately SWALLOWED every
   controller button while game_status == 30, with a comment noting
   this was "V1 scope" because the menu's name/IP text fields were
   keyboard-only at the time that was written. That justification went
   stale once #22 added real on-screen-keyboard support (handle_text_input()) --
   navigation/confirm/cancel never needed a keyboard at all, so
   swallowing those too was an unnecessary regression left over from
   before the keyboard existed.
   Real fix: added menu_multiplayer::handle_controller_button_down(),
   mapping DPAD up/down/left/right -> arrow keys, A -> Enter, B/Start ->
   Escape, onto the exact same keycodes handle_key_down() already
   implements -- zero new logic, same pattern split_screen_menu.cpp and
   split_screen_match.cpp already use for their own controller support.
   main.cpp's game_status == 30 branch now calls it instead of
   swallowing. Typing a name/IP still has no controller equivalent (same
   as it has none for touch) -- a controller player is expected to use
   the default name, same reasoning as touch.
   Related bug fixed alongside it: game_status == 30 was missing from
   main.cpp's poll_axes() held-stick-repeat exclusion list (20/21/23/24
   were already excluded for the identical reason -- a held stick
   leaking into the single-player main-menu navigation underneath,
   since they all share actual_stage == 1000). Added it.
   Verified on desktop: compiles clean with zero warnings
   (g++ -Wall -Wextra), smoke-tested headless (no crash, no missing
   symbols). Ported to all three Android multiplayer variants (same
   main.cpp edit applied once to neutron-port-universal, confirmed
   byte-identical to neutron-port-android/neutron-port-legacy before
   the edit, then copied across) and rebuilt; all three succeeded.
   Not yet confirmed fixed on real controller hardware (Steam Deck or
   otherwise) -- pending user test.