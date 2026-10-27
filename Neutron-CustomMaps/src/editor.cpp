#include "editor.h"

#include <cstdio>
#include <cstring>

bool MapEditor::has_cube(int x, int y, int z) const {
    for (const Cube& c : cubes) {
        if (c.x == x && c.y == y && c.z == z) return true;
    }
    return false;
}

bool MapEditor::toggle_cube(int x, int y, int z) {
    for (size_t i = 0; i < cubes.size(); i++) {
        if (cubes[i].x == x && cubes[i].y == y && cubes[i].z == z) {
            cubes.erase(cubes.begin() + (long)i);
            return true;
        }
    }
    if ((int)cubes.size() >= kMaxCubes) return false;
    cubes.push_back({x, y, z});
    return true;
}

void MapEditor::clear() {
    cubes.clear();
}

bool MapEditor::is_fully_connected() const {
    if (cubes.size() <= 1) return true;
    for (const Cube& a : cubes) {
        bool found = false;
        for (const Cube& b : cubes) {
            if (&a == &b) continue;
            int dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            int adx = dx < 0 ? -dx : dx;
            int ady = dy < 0 ? -dy : dy;
            int adz = dz < 0 ? -dz : dz;
            bool one_axis_step = (adx == kGridStep && dy == 0 && dz == 0) ||
                                  (ady == kGridStep && dx == 0 && dz == 0) ||
                                  (adz == kGridStep && dx == 0 && dy == 0);
            if (one_axis_step) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

static void rstrip(char* s) {
    size_t len = std::strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                        s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

bool MapEditor::save(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "# Neutron custom stage -- made with Neutron-CustomMaps editor.\n");
    std::fprintf(f, "# Format: name=<text>, then one cube=x,y,z per cube (grid step %d).\n", kGridStep);
    std::fprintf(f, "name=%.31s\n", name.c_str());
    for (const Cube& c : cubes) {
        std::fprintf(f, "cube=%d,%d,%d\n", c.x, c.y, c.z);
    }
    std::fclose(f);
    return true;
}

bool MapEditor::load(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    std::string new_name;
    std::vector<Cube> new_cubes;

    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        rstrip(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        if (std::strncmp(line, "name=", 5) == 0) {
            new_name = line + 5;
        } else if (std::strncmp(line, "cube=", 5) == 0) {
            if ((int)new_cubes.size() >= kMaxCubes) continue;
            int x, y, z;
            if (std::sscanf(line + 5, "%d,%d,%d", &x, &y, &z) == 3) {
                new_cubes.push_back({x, y, z});
            }
        }
    }
    std::fclose(f);

    if (new_cubes.empty()) return false;

    cubes = new_cubes;
    if (!new_name.empty()) name = new_name;
    current_layer_y = cubes[0].y;
    return true;
}
