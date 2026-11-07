#include "net_session.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <array>
#include <sstream>

// See net_session.h for the overall design (lockstep over a host-
// authoritative star topology, UDP discovery + TCP for everything else).
// Untested against a real second machine/LAN -- this sandbox has no way to
// run two networked instances against each other. Compiles and the logic
// has been read through carefully, but real-world testing (packet loss,
// actual multi-machine timing, router/firewall behavior) is still needed.

namespace net_session {

namespace {

constexpr Uint16 DISCOVERY_PORT = 27771; // UDP
constexpr Uint16 GAME_PORT = 27772;      // TCP
constexpr int MAX_PLAYERS = 4;           // matches split_screen_match's own player-slot count
constexpr Uint32 TICK_STALL_TIMEOUT_MS = 5000; // see host_update()'s tick-assembly comment
constexpr const char* DEFAULT_HOST_NAME = "Host";
constexpr const char* DEFAULT_CLIENT_NAME = "Player";

// --- Small per-socket line-buffer helper -----------------------------------
// TCP is a byte stream, not a message stream -- a single send() on one end
// can arrive as multiple recv()s on the other, or multiple sends can arrive
// coalesced into one recv(). Every message this protocol uses is a single
// '\n'-terminated text line, so each socket gets its own accumulating
// buffer; extract_line() pulls one complete line out when one's available.
struct LineBuffer {
    std::string buf;

    void feed(const char* data, int len) { buf.append(data, len); }

    bool extract_line(std::string& out) {
        size_t pos = buf.find('\n');
        if (pos == std::string::npos) return false;
        out = buf.substr(0, pos);
        buf.erase(0, pos + 1);
        return true;
    }
};

std::vector<std::string> split_pipe(const std::string& s) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = s.find('|', start);
        if (pos == std::string::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

bool send_line(TCPsocket sock, const std::string& line) {
    if (!sock) return false;
    std::string msg = line + "\n";
    int sent = SDLNet_TCP_Send(sock, msg.c_str(), (int)msg.size());
    return sent == (int)msg.size();
}

// --- Session state -----------------------------------------------------
Role g_role = Role::None;
bool g_inited = false;

// Discovery (client side)
UDPsocket g_discovery_sock = nullptr;
UDPpacket* g_discovery_packet = nullptr;
std::vector<DiscoveredGame> g_discovered;

// Hosting
UDPsocket g_host_discovery_responder = nullptr; // bound to DISCOVERY_PORT, replies to scans
TCPsocket g_listen_sock = nullptr;
SDLNet_SocketSet g_client_set = nullptr;
struct ClientConn {
    TCPsocket sock = nullptr;
    LineBuffer rx;
    bool welcomed = false;
    int tick_action = -1; // -1 = no input received yet for the current tick
};
std::array<ClientConn, MAX_PLAYERS - 1> g_clients; // slots 1..MAX_PLAYERS-1 (slot 0 is the host)

// Client-side connection to a host
TCPsocket g_server_sock = nullptr;
SDLNet_SocketSet g_server_set = nullptr;
LineBuffer g_server_rx;

// Roster (both roles keep this -- host fills it as players join and
// broadcasts it; clients just receive it from the host)
std::array<std::string, MAX_PLAYERS> g_player_names;
int g_player_count = 0;
int g_max_players = MAX_PLAYERS;
int g_local_slot = -1;
std::string g_my_name;

// Match lifecycle
bool g_match_starting = false;
int g_match_map_idx = 0;
unsigned g_match_seed = 0;
int g_match_generation = 0; // see net_session.h's own comment on match_generation()

// Lockstep tick state
int g_current_tick = 0;
Uint32 g_tick_wait_started_ms = 0; // 0 = not currently stalled; see host_update()
int g_local_action = -1; // -1 = not yet submitted for the current tick (see advance_tick())
std::array<int, MAX_PLAYERS> g_tick_actions{};
bool g_tick_ready = false;

void reset_roster() {
    for (auto& n : g_player_names) n.clear();
    g_player_count = 0;
}

std::string broadcast_address_string() {
    // 255.255.255.255 reaches every host on the local subnet from almost
    // any consumer router/OS without needing the actual subnet mask --
    // the standard, simplest choice for LAN discovery.
    return "255.255.255.255";
}

} // namespace

bool init() {
    if (g_inited) return true;
    if (SDLNet_Init() < 0) {
        std::fprintf(stderr, "net_session: SDLNet_Init failed: %s\n", SDLNet_GetError());
        return false;
    }
    g_inited = true;
    return true;
}

void quit() {
    leave_session();
    if (g_inited) {
        SDLNet_Quit();
        g_inited = false;
    }
}

void start_discovery_scan() {
    if (!g_inited) return;
    g_discovered.clear();

    if (!g_discovery_sock) {
        g_discovery_sock = SDLNet_UDP_Open(0); // ephemeral local port, we only send+recv replies
    }
    if (!g_discovery_packet) {
        g_discovery_packet = SDLNet_AllocPacket(512);
    }
    if (!g_discovery_sock || !g_discovery_packet) return;

    IPaddress dest;
    SDLNet_ResolveHost(&dest, broadcast_address_string().c_str(), DISCOVERY_PORT);
    const char* msg = "NEUTRON_DISCOVER";
    std::memcpy(g_discovery_packet->data, msg, std::strlen(msg) + 1);
    g_discovery_packet->len = (int)std::strlen(msg) + 1;
    g_discovery_packet->address = dest;
    SDLNet_UDP_Send(g_discovery_sock, -1, g_discovery_packet);
}

const std::vector<DiscoveredGame>& discovered_games() {
    return g_discovered;
}

bool host_game(const std::string& my_name, int max_players) {
    if (!g_inited) return false;
    leave_session();

    g_my_name = my_name.empty() ? DEFAULT_HOST_NAME : my_name;
    g_max_players = (max_players < 2) ? 2 : (max_players > MAX_PLAYERS ? MAX_PLAYERS : max_players);

    IPaddress listen_addr;
    SDLNet_ResolveHost(&listen_addr, nullptr, GAME_PORT);
    g_listen_sock = SDLNet_TCP_Open(&listen_addr);
    if (!g_listen_sock) {
        std::fprintf(stderr, "net_session: host_game: TCP listen failed: %s\n", SDLNet_GetError());
        return false;
    }

    IPaddress discovery_addr;
    SDLNet_ResolveHost(&discovery_addr, nullptr, DISCOVERY_PORT);
    g_host_discovery_responder = SDLNet_UDP_Open(DISCOVERY_PORT);
    // Not fatal if this fails (e.g. port already bound by another local
    // instance, or blocked by a firewall) -- hosting still works, it just
    // won't show up in LAN scans, and a player can still join via manual
    // IP. Reported: LAN discovery never found a host over manual-IP-only
    // testing. Logged here (previously silent) so a future repro actually
    // shows whether THIS is the cause, rather than guessing -- check
    // stderr/the console next time this happens.
    if (!g_host_discovery_responder) {
        std::fprintf(stderr, "net_session: host_game: WARNING: discovery UDP bind on port %d failed (%s) -- "
                              "this host will NOT show up in LAN scans, manual IP entry still works.\n",
                      (int)DISCOVERY_PORT, SDLNet_GetError());
    }

    g_client_set = SDLNet_AllocSocketSet(MAX_PLAYERS);

    reset_roster();
    g_player_names[0] = g_my_name;
    g_player_count = 1;
    g_local_slot = 0;
    g_role = Role::Host;
    g_match_starting = false;
    g_current_tick = 0;
    return true;
}

bool join_game(const std::string& my_name, const std::string& host_ip) {
    if (!g_inited) return false;
    leave_session();

    g_my_name = my_name.empty() ? DEFAULT_CLIENT_NAME : my_name;

    IPaddress addr;
    if (SDLNet_ResolveHost(&addr, host_ip.c_str(), GAME_PORT) < 0) {
        std::fprintf(stderr, "net_session: join_game: couldn't resolve %s: %s\n", host_ip.c_str(), SDLNet_GetError());
        return false;
    }
    g_server_sock = SDLNet_TCP_Open(&addr);
    if (!g_server_sock) {
        std::fprintf(stderr, "net_session: join_game: connect failed: %s\n", SDLNet_GetError());
        return false;
    }
    g_server_set = SDLNet_AllocSocketSet(1);
    SDLNet_TCP_AddSocket(g_server_set, g_server_sock);

    reset_roster();
    g_local_slot = -1; // assigned once WELCOME arrives
    g_role = Role::Client;
    g_match_starting = false;
    g_current_tick = 0;

    send_line(g_server_sock, std::string("JOIN|") + g_my_name);
    return true;
}

void leave_session() {
    if (g_discovery_sock) { SDLNet_UDP_Close(g_discovery_sock); g_discovery_sock = nullptr; }
    if (g_discovery_packet) { SDLNet_FreePacket(g_discovery_packet); g_discovery_packet = nullptr; }

    if (g_host_discovery_responder) { SDLNet_UDP_Close(g_host_discovery_responder); g_host_discovery_responder = nullptr; }
    if (g_listen_sock) { SDLNet_TCP_Close(g_listen_sock); g_listen_sock = nullptr; }
    for (auto& c : g_clients) {
        if (c.sock) SDLNet_TCP_Close(c.sock);
        c = ClientConn();
    }
    if (g_client_set) { SDLNet_FreeSocketSet(g_client_set); g_client_set = nullptr; }

    if (g_server_sock) { SDLNet_TCP_Close(g_server_sock); g_server_sock = nullptr; }
    if (g_server_set) { SDLNet_FreeSocketSet(g_server_set); g_server_set = nullptr; }
    g_server_rx = LineBuffer();

    reset_roster();
    g_role = Role::None;
    g_local_slot = -1;
    g_match_starting = false;
    g_tick_ready = false;
}

Role role() { return g_role; }
bool is_connected() { return g_role == Role::Host || (g_role == Role::Client && g_local_slot >= 0); }
int local_slot() { return g_local_slot; }
int connected_player_count() { return g_player_count; }
int max_player_count() { return g_max_players; }
std::string player_name(int slot) {
    if (slot < 0 || slot >= MAX_PLAYERS) return "";
    return g_player_names[slot];
}

namespace {

void host_broadcast_roster() {
    std::ostringstream oss;
    oss << "ROSTER|" << g_player_count;
    for (int i = 0; i < g_player_count; i++) oss << "|" << g_player_names[i];
    std::string line = oss.str();
    for (auto& c : g_clients) {
        if (c.sock && c.welcomed) send_line(c.sock, line);
    }
}

void host_handle_message(ClientConn& c, int slot, const std::string& line) {
    auto parts = split_pipe(line);
    if (parts.empty()) return;
    if (parts[0] == "JOIN" && parts.size() >= 2 && !c.welcomed) {
        g_player_names[slot] = parts[1].empty() ? DEFAULT_CLIENT_NAME : parts[1];
        g_player_count++;
        c.welcomed = true;
        send_line(c.sock, "WELCOME|" + std::to_string(slot));
        host_broadcast_roster();
    } else if (parts[0] == "INPUT" && parts.size() >= 3) {
        int tick = std::atoi(parts[1].c_str());
        int action = std::atoi(parts[2].c_str());
        if (tick == g_current_tick) c.tick_action = action;
        // Inputs for a tick we've already moved past are stale (e.g. a
        // retransmit or a straggler) -- harmless to ignore.
    }
}

void host_update() {
    // Answer any pending discovery scans.
    if (g_host_discovery_responder) {
        UDPpacket* pkt = SDLNet_AllocPacket(512);
        if (pkt) {
            while (SDLNet_UDP_Recv(g_host_discovery_responder, pkt) == 1) {
                if (pkt->len > 0 && std::strncmp((const char*)pkt->data, "NEUTRON_DISCOVER", 16) == 0) {
                    std::ostringstream oss;
                    oss << "NEUTRON_HOST|" << g_my_name << "|" << g_player_count << "|" << g_max_players;
                    std::string reply = oss.str();
                    UDPpacket* reply_pkt = SDLNet_AllocPacket(512);
                    if (reply_pkt) {
                        std::memcpy(reply_pkt->data, reply.c_str(), reply.size() + 1);
                        reply_pkt->len = (int)reply.size() + 1;
                        reply_pkt->address = pkt->address;
                        SDLNet_UDP_Send(g_host_discovery_responder, -1, reply_pkt);
                        SDLNet_FreePacket(reply_pkt);
                    }
                }
            }
            SDLNet_FreePacket(pkt);
        }
    }

    // Accept new connections (SDLNet_TCP_Accept is non-blocking: returns
    // NULL immediately if nobody's waiting).
    if (g_listen_sock && g_player_count < g_max_players) {
        TCPsocket incoming = SDLNet_TCP_Accept(g_listen_sock);
        if (incoming) {
            for (size_t i = 0; i < g_clients.size(); i++) {
                if (!g_clients[i].sock) {
                    g_clients[i].sock = incoming;
                    SDLNet_TCP_AddSocket(g_client_set, incoming);
                    break;
                }
            }
        }
    }

    // Pump data from every connected client.
    if (g_client_set && SDLNet_CheckSockets(g_client_set, 0) > 0) {
        for (size_t i = 0; i < g_clients.size(); i++) {
            ClientConn& c = g_clients[i];
            if (!c.sock) continue;
            if (!SDLNet_SocketReady(c.sock)) continue;
            char buf[512];
            int n = SDLNet_TCP_Recv(c.sock, buf, sizeof(buf));
            if (n <= 0) {
                // Disconnected.
                SDLNet_TCP_DelSocket(g_client_set, c.sock);
                SDLNet_TCP_Close(c.sock);
                int slot = (int)i + 1;
                if (c.welcomed && !g_player_names[slot].empty()) {
                    g_player_names[slot].clear();
                    g_player_count--;
                    host_broadcast_roster();
                }
                c = ClientConn();
                continue;
            }
            c.rx.feed(buf, n);
            std::string line;
            while (c.rx.extract_line(line)) {
                host_handle_message(c, (int)i + 1, line);
            }
        }
    }

    // Lockstep tick assembly: once the host's own local action is staged
    // AND every currently-connected client has reported in for this tick,
    // broadcast the assembled set and mark it ready for the host's own
    // simulation to consume.
    //
    // Reported bug this guards against: a real match froze mid-game with
    // no recovery. Root cause -- there was NO timeout here at all: if even
    // one connected client's input for the current tick never arrives (a
    // dropped packet, a brief network hiccup, anything), "all_in" can
    // never become true again, and the WHOLE match hangs forever for
    // every player, with no way out short of everyone quitting. Real fix:
    // track how long we've been stuck waiting on this exact tick, and
    // after a generous timeout, treat any client that still hasn't
    // reported in as disconnected -- the match continues without them
    // rather than hanging forever. 5s is deliberately generous (a real
    // LAN round-trip is milliseconds; this only fires for an actually
    // dead/hung connection, not ordinary latency).
    if (!g_tick_ready && g_local_action >= 0) {
        bool all_in = true;
        for (size_t i = 0; i < g_clients.size(); i++) {
            if (g_clients[i].welcomed && g_clients[i].tick_action < 0) { all_in = false; break; }
        }
        if (!all_in) {
            if (g_tick_wait_started_ms == 0) {
                g_tick_wait_started_ms = SDL_GetTicks();
            } else if (SDL_GetTicks() - g_tick_wait_started_ms > TICK_STALL_TIMEOUT_MS) {
                for (size_t i = 0; i < g_clients.size(); i++) {
                    ClientConn& c = g_clients[i];
                    if (c.welcomed && c.tick_action < 0) {
                        if (c.sock) { SDLNet_TCP_DelSocket(g_client_set, c.sock); SDLNet_TCP_Close(c.sock); }
                        int slot = (int)i + 1;
                        g_player_names[slot].clear();
                        g_player_count--;
                        c = ClientConn();
                    }
                }
                host_broadcast_roster();
                all_in = true; // re-check below with the straggler(s) now gone
                g_tick_wait_started_ms = 0;
            }
        } else {
            g_tick_wait_started_ms = 0;
        }
        if (all_in) {
            g_tick_actions[0] = g_local_action;
            for (size_t i = 0; i < g_clients.size(); i++) {
                g_tick_actions[i + 1] = g_clients[i].welcomed ? g_clients[i].tick_action : 0;
            }
            std::ostringstream oss;
            oss << "TICK|" << g_current_tick;
            for (int s = 0; s < g_max_players; s++) oss << "|" << g_tick_actions[s];
            std::string line = oss.str();
            for (auto& c : g_clients) {
                if (c.sock && c.welcomed) send_line(c.sock, line);
            }
            g_tick_ready = true;
        }
    }
}

void client_handle_message(const std::string& line) {
    auto parts = split_pipe(line);
    if (parts.empty()) return;
    if (parts[0] == "WELCOME" && parts.size() >= 2) {
        g_local_slot = std::atoi(parts[1].c_str());
    } else if (parts[0] == "ROSTER" && parts.size() >= 2) {
        int count = std::atoi(parts[1].c_str());
        reset_roster();
        g_player_count = count;
        for (int i = 0; i < count && (size_t)(2 + i) < parts.size(); i++) {
            g_player_names[i] = parts[2 + i];
        }
    } else if (parts[0] == "START" && parts.size() >= 3) {
        g_match_map_idx = std::atoi(parts[1].c_str());
        g_match_seed = (unsigned)std::strtoul(parts[2].c_str(), nullptr, 10);
        g_match_starting = true;
        g_match_generation++;
    } else if (parts[0] == "TICK" && parts.size() >= 2) {
        int tick = std::atoi(parts[1].c_str());
        if (tick == g_current_tick) {
            for (int s = 0; s < g_max_players && (size_t)(2 + s) < parts.size(); s++) {
                g_tick_actions[s] = std::atoi(parts[2 + s].c_str());
            }
            g_tick_ready = true;
        }
    }
}

void client_update() {
    if (!g_server_sock) return;
    if (g_server_set && SDLNet_CheckSockets(g_server_set, 0) > 0 && SDLNet_SocketReady(g_server_sock)) {
        char buf[512];
        int n = SDLNet_TCP_Recv(g_server_sock, buf, sizeof(buf));
        if (n <= 0) {
            // Lost connection to host.
            leave_session();
            return;
        }
        g_server_rx.feed(buf, n);
        std::string line;
        while (g_server_rx.extract_line(line)) {
            client_handle_message(line);
        }
    }
}

std::string ip_to_string(const IPaddress& addr) {
    Uint32 h = addr.host; // stored in network (big-endian wire) byte order regardless of host CPU
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", h & 0xFF, (h >> 8) & 0xFF, (h >> 16) & 0xFF, (h >> 24) & 0xFF);
    return buf;
}

// Collects NEUTRON_HOST replies into g_discovered. Runs regardless of role
// (discovery happens on the join-setup screen, before a role is chosen) --
// genuinely missing before this fix: start_discovery_scan() only ever sent
// the request, nothing anywhere in this file ever called SDLNet_UDP_Recv
// on g_discovery_sock, so g_discovered could NEVER be populated, at all,
// for any reason -- the reported "LAN discovery never found anything" bug
// was this, not a network/router issue. (If it still doesn't find a host
// after this fix, THAT'S the point to suspect WiFi client isolation or a
// firewall blocking UDP port 27771 -- but this bug alone fully explains
// what was reported.)
void poll_discovery_replies() {
    if (!g_discovery_sock || !g_discovery_packet) return;
    while (SDLNet_UDP_Recv(g_discovery_sock, g_discovery_packet) == 1) {
        if (g_discovery_packet->len <= 0) continue;
        std::string msg((const char*)g_discovery_packet->data, (size_t)g_discovery_packet->len);
        size_t nul = msg.find('\0');
        if (nul != std::string::npos) msg.resize(nul);
        auto parts = split_pipe(msg);
        if (parts.size() < 4 || parts[0] != "NEUTRON_HOST") continue;

        DiscoveredGame g;
        g.host_name = parts[1];
        g.ip = ip_to_string(g_discovery_packet->address);
        g.player_count = std::atoi(parts[2].c_str());
        g.max_players = std::atoi(parts[3].c_str());

        bool known = false;
        for (auto& existing : g_discovered) {
            if (existing.ip == g.ip) { existing = g; known = true; break; } // refresh in place
        }
        if (!known) g_discovered.push_back(g);
    }
}

} // namespace

void update() {
    poll_discovery_replies();
    if (g_role == Role::Host) host_update();
    else if (g_role == Role::Client) client_update();
}

bool start_match(int map_idx, unsigned seed) {
    if (g_role != Role::Host) return false;
    g_match_map_idx = map_idx;
    g_match_seed = seed;
    g_match_starting = true;
    g_match_generation++;
    std::string line = "START|" + std::to_string(map_idx) + "|" + std::to_string(seed);
    for (auto& c : g_clients) {
        if (c.sock && c.welcomed) send_line(c.sock, line);
    }
    return true;
}

bool match_starting() { return g_match_starting; }
int match_map_idx() { return g_match_map_idx; }
unsigned match_seed() { return g_match_seed; }
int match_generation() { return g_match_generation; }

void submit_local_action(int action) {
    g_local_action = action;
    if (g_role == Role::Client && g_server_sock) {
        send_line(g_server_sock, "INPUT|" + std::to_string(g_current_tick) + "|" + std::to_string(action));
    }
    // Host's own local action feeds directly into host_update()'s tick
    // assembly above -- no message needed, it already knows its own input.
}

bool tick_ready() { return g_tick_ready; }

int tick_action(int slot) {
    if (slot < 0 || slot >= MAX_PLAYERS) return 0;
    return g_tick_actions[slot];
}

void advance_tick() {
    g_current_tick++;
    g_local_action = -1;
    g_tick_ready = false;
    g_tick_wait_started_ms = 0;
    for (auto& c : g_clients) c.tick_action = -1;
}

} // namespace net_session
