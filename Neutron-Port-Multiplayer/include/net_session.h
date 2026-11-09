#pragma once

#include <string>
#include <vector>

// LAN (and manual-IP) multiplayer session management: discovery, connection
// handshake, and lockstep tick exchange.
//
// Why lockstep: this engine's own steering input is already discrete (turn
// left / turn right / nothing per simulation tick -- see
// controller_input.cpp's execute_action(0)/execute_action(1) calls), not a
// continuous analog stream. That makes lockstep a natural fit: each tick,
// every peer only needs to exchange one tiny integer with the others, and
// every peer runs the EXACT same deterministic simulation (this codebase
// already cares a lot about exact, reproducible sim behavior -- see
// main.cpp's --test harness) from the same inputs, so nobody needs to send
// or receive actual game state. The real cost is latency: nobody advances to
// tick N until EVERY peer's input for tick N has arrived, so perceived input
// delay is roughly one network round-trip. Fine for a grid-based, non-twitch
// game; would NOT be the right architecture for something reflex-heavy.
//
// Topology: simple star, host-authoritative tick assembly. Clients only
// ever talk to the host (never to each other directly) -- they send their
// own tick's action to the host, and the host broadcasts back the FULL
// assembled action set for that tick once every connected player's input
// for it has arrived (including the host's own). This keeps clients simple
// (no peer discovery among themselves) and avoids needing a full mesh of
// connections for what's at most a handful of players.
//
// Transport split: UDP broadcast for LAN discovery (fire-and-forget,
// perfect for "who's out there"), TCP for everything else (roster,
// start-of-match, and per-tick input exchange) -- small, infrequent
// messages where simple reliable delivery matters more than raw speed, and
// not having to hand-roll reliability/ordering on top of UDP is worth the
// small latency cost for this kind of game.
namespace net_session {

// Shared by net_match.cpp and menu_multiplayer.cpp (both already include
// this header) so the player-count cap is ONE real constant across those
// three files instead of three independent literals that happen to
// agree. split_screen_match.cpp keeps its OWN separate MAX_PLAYERS by
// design (deliberately net_session-agnostic) -- keep that one in sync
// with this value by hand; see its own comment for why it isn't just
// included from here instead.
constexpr int MAX_PLAYERS = 12;

enum class Role { None, Host, Client };

struct DiscoveredGame {
    std::string host_name;
    std::string ip;
    int player_count;
    int max_players;
};

// Must be called once before anything else here (initializes SDL_net).
bool init();
void quit();

// Call every frame regardless of state -- pumps all sockets (UDP discovery
// replies, TCP roster/control messages, and tick-exchange messages once a
// match is live). Nothing else in this header does any blocking I/O; this
// is the only place socket data actually moves.
void update();

// --- Discovery (client side) ---
// Broadcasts a discovery request on the LAN and collects replies. Call this
// once to kick off a scan, then read discovered_games() each frame (update()
// keeps appending replies as they arrive) -- the multiplayer menu's LAN list
// is meant to re-call this periodically (e.g. once every second or so) to
// stay current as games come and go.
void start_discovery_scan();
const std::vector<DiscoveredGame>& discovered_games();

// --- Hosting ---
// Opens the UDP discovery responder + a TCP listen socket. my_name is what
// shows up in other players' LAN lists and the in-match roster; empty
// becomes "Host" (see DEFAULT_NAME in the .cpp).
bool host_game(const std::string& my_name, int max_players);

// --- Joining ---
// Connects to a host by IP (from discovered_games()[i].ip, or typed in
// manually for a port-forwarded remote host). Uses the fixed TCP port this
// module defines (see net_session.cpp) -- no separate port field needed.
bool join_game(const std::string& my_name, const std::string& host_ip);

void leave_session(); // client: disconnects. host: tears down the session for everyone.

// Client-only: true once the host has parked US because a match was
// already live when we joined (see host_handle_message()'s pending_join
// handling in net_session.cpp) -- false again once the real WELCOME
// finally arrives (promoted into the roster for real). menu_multiplayer's
// JoinLobby screen uses this to show "match in progress, waiting" instead
// of the generic "waiting for host to start" text, which used to be
// shown with no indication at all that a match was already running.
bool join_pending();

Role role();
bool is_connected();          // true once a client's join handshake completes, or once hosting is active
int local_slot();             // which player slot WE are (0..3), -1 if not connected
int connected_player_count();
int max_player_count();
std::string player_name(int slot); // empty if that slot isn't filled

// Host only: begins the match once ready (sends START to every connected
// client, including implicitly "starting" itself). map_idx must already
// be a concrete, resolved stage index -- NEVER -1/"random" (see the real
// desync bug this avoids, below). seed is a shared spawn-layout seed the
// caller should srand() with, identically, on every peer right before
// calling split_screen_match::start_match_with_sources() -- see
// net_match::start()'s own comment for why this exists at all.
bool start_match(int map_idx, unsigned seed);

// True once a START has been received (client) or sent (host) -- the menu
// should transition to the match screen when this flips true.
bool match_starting();
int match_map_idx();    // valid once match_starting() is true
unsigned match_seed();  // valid once match_starting() is true -- see start_match()'s own comment

// Increments on EVERY real START event -- the initial match start AND any
// later retry (the host can call start_match() again from the match-over
// screen without tearing the session down; see net_match.cpp). A caller
// that needs to tell "a fresh retry signal just arrived" apart from "the
// same match_starting()==true I already consumed" should remember the
// generation it last acted on and only react when this value changes.
int match_generation();

// Host-only: called once, right when a live match ends NATURALLY (a
// win/draw actually detected -- see net_match.cpp's drive_tick(), the
// only caller, which notices the game_status 23 -> 24 transition). Folds
// in anyone who tried to JOIN while that match was still running (see
// the real lockup bug below -- same reasoning as cancel_match(), which
// does the same promotion for the OTHER way a match can end). No-op if
// called on a client.
void notify_match_ended();

// Host-only: ends the CURRENT match early (sends CANCEL to every connected
// client) and returns everyone to their lobby screen WITHOUT touching the
// session/roster at all -- unlike leave_session(), nobody's TCP connection
// is closed, so the host can immediately pick a new map and start again
// with the same roster. Real bug this exists to fix: before this, the
// host's only way to back out of a live match was leave_session() (torn
// down for EVERYONE), and clients had no way to notice their connection
// had been closed mid-match at all -- see net_match.cpp's cancel_token()
// handling for why they'd otherwise just sit there forever. Also folds
// in any pending mid-match joiner, same as notify_match_ended() above
// (this is simply the OTHER way a live match can end). No-op if called
// on a client (same guard style as start_match() above).
void cancel_match();

// Increments on EVERY real CANCEL event, same "remember the last one you
// acted on" pattern as match_generation() above -- lets a client's
// net_match.cpp tell "a fresh cancel just arrived" apart from one it
// already handled.
int cancel_token();

// --- Lockstep tick exchange, used once a match is live ---
// Actions submitted here match this engine's existing discrete steering
// model: 0 = none, 1 = turn left, 2 = turn right. tick_action() below can
// also return 3 for a slot, meaning "this player disconnected mid-match,
// eliminate them" -- the host injects this itself for a slot that was
// ever_welcomed but no longer is (see net_session.cpp's host_update()),
// never something a caller submits; see net_match.cpp's drive_tick() for
// the only place that interprets it.
void submit_local_action(int action);

// True once every connected player's action for the CURRENT tick has
// arrived. The caller should keep calling update() every frame and only
// advance its own simulation once this is true.
bool tick_ready();
int tick_action(int slot); // valid once tick_ready() is true
void advance_tick();       // call after consuming the current tick's actions

} // namespace net_session
