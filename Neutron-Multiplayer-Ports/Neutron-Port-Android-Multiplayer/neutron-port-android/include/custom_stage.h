#pragma once

#include "game_data.h"
#include <string>

// Optional 26th stage, loaded from a plain-text "map.txt" sitting next to the
// executable (same cwd-relative convention as neutron_save.txt/music.wav).
// Not shipped in releases -- this only appears in Stage Select / Continue
// progression when a map.txt file is actually present and parses to at least
// one cube.
struct CustomStageData {
    bool loaded = false;
    char name[32] = "";
    int cube_count = 0;
    int cubes[20][3] = {};
};

extern CustomStageData g_custom_stage;

// Reads map.txt (if present) into g_custom_stage. Call once at startup,
// before the first create_miniatures(). No-op (g_custom_stage.loaded stays
// false) if the file is missing or contains no valid cube lines.
void load_custom_stage();

// 25 built-in LEVEL_TABLE stages, plus the custom one (index 25) if loaded.
inline int total_stage_count() {
    return g_custom_stage.loaded ? 26 : 25;
}

// Display name for miniature/level index idx (0-based: 0..24 are LEVEL_TABLE,
// 25 is the custom map.txt stage).
inline const char* stage_display_name(int idx) {
    if (idx >= 0 && idx < 25) return LEVEL_TABLE[idx].name;
    if (idx == 25 && g_custom_stage.loaded) return g_custom_stage.name;
    return "GRID";
}

// Cube count for miniature/level index idx, same indexing convention as
// stage_display_name() above. menu_multiplayer.cpp uses this to filter out
// maps too small for the current player count in its host-side map picker.
inline int stage_cube_count(int idx) {
    if (idx >= 0 && idx < 25) return LEVEL_TABLE[idx].cubes;
    if (idx == 25 && g_custom_stage.loaded) return g_custom_stage.cube_count;
    return 0;
}

// Packs g_custom_stage into one plain-text blob -- "name;x,y,z;x,y,z;..."
// (';'/',' only, deliberately never '|', so this nests safely as a single
// opaque field inside net_session's own pipe-delimited protocol messages).
// Empty string if not loaded. net_match.cpp calls this on the HOST when
// the custom stage is the match's chosen map, so it can travel in the
// START message -- see deserialize_custom_stage()'s own comment for why
// that matters at all.
std::string serialize_custom_stage();

// Reverses serialize_custom_stage() -- REPLACES g_custom_stage outright
// (in memory only, for the rest of this running session; never written
// back to the local map.txt file). Real bug this fixes: custom maps were
// never synced over the network at all, only the map INDEX was -- a
// client with no local map.txt (or a different one) ended up with an
// empty, un-spawned, blank match the instant the host picked the custom
// stage, since create_miniatures() never even allocates that slot
// locally when g_custom_stage.loaded is false for THAT peer specifically.
// Deliberately a one-way "download," not a save/restore pair: a client's
// own different custom map (if they had one) is NOT restored once the
// match ends -- an explicit, deliberate scope decision, not an oversight.
void deserialize_custom_stage(const std::string& data);
