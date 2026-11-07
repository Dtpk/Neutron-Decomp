#pragma once

#include "game_data.h"

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
