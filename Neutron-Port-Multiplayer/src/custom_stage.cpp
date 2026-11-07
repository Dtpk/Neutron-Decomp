#include "custom_stage.h"
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

    std::FILE* f = std::fopen("map.txt", "r");
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
