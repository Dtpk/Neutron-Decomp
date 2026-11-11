#include "custom_stage.h"
#include "asset_path.h"
#include <cstdio>
#include <cstring>

CustomStageData g_custom_stage;

static void rstrip(char* s) {
    size_t len = std::strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                        s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

void load_custom_stage() {
    g_custom_stage = CustomStageData(); // reset to "not loaded" defaults

    std::FILE* f = std::fopen(asset_path("map.txt").c_str(), "r");
    if (!f) return; // no file -- perfectly normal, custom stage just stays absent

    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        rstrip(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        if (std::strncmp(line, "name=", 5) == 0) {
            std::snprintf(g_custom_stage.name, sizeof(g_custom_stage.name), "%.31s", line + 5);
        } else if (std::strncmp(line, "cube=", 5) == 0) {
            if (g_custom_stage.cube_count >= 20) continue; // matches miniature's own cap
            int x, y, z;
            if (std::sscanf(line + 5, "%d,%d,%d", &x, &y, &z) == 3) {
                int* c = g_custom_stage.cubes[g_custom_stage.cube_count];
                c[0] = x;
                c[1] = y;
                c[2] = z;
                g_custom_stage.cube_count++;
            }
        }
    }
    std::fclose(f);

    if (g_custom_stage.cube_count < 1) {
        g_custom_stage = CustomStageData(); // malformed/empty -- stay absent
        return;
    }
    if (g_custom_stage.name[0] == '\0') {
        std::snprintf(g_custom_stage.name, sizeof(g_custom_stage.name), "custom");
    }
    g_custom_stage.loaded = true;
}

std::string serialize_custom_stage() {
    if (!g_custom_stage.loaded) return "";
    std::string out = g_custom_stage.name;
    for (int i = 0; i < g_custom_stage.cube_count; i++) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), ";%d,%d,%d",
                      g_custom_stage.cubes[i][0], g_custom_stage.cubes[i][1], g_custom_stage.cubes[i][2]);
        out += buf;
    }
    return out;
}

void deserialize_custom_stage(const std::string& data) {
    g_custom_stage = CustomStageData(); // reset -- discards whatever this peer had, by design
    if (data.empty()) return;

    size_t name_end = data.find(';');
    std::string name_part = (name_end == std::string::npos) ? data : data.substr(0, name_end);
    std::snprintf(g_custom_stage.name, sizeof(g_custom_stage.name), "%.31s", name_part.c_str());

    size_t cursor = (name_end == std::string::npos) ? data.size() : name_end + 1;
    while (cursor < data.size() && g_custom_stage.cube_count < 20) {
        size_t next = data.find(';', cursor);
        std::string token = data.substr(cursor, next == std::string::npos ? std::string::npos : next - cursor);
        int x, y, z;
        if (std::sscanf(token.c_str(), "%d,%d,%d", &x, &y, &z) == 3) {
            int* c = g_custom_stage.cubes[g_custom_stage.cube_count];
            c[0] = x;
            c[1] = y;
            c[2] = z;
            g_custom_stage.cube_count++;
        }
        if (next == std::string::npos) break;
        cursor = next + 1;
    }

    if (g_custom_stage.cube_count > 0) g_custom_stage.loaded = true;
}

