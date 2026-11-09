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