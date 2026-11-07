#pragma once

#include <array>
#include <string>
#include <vector>

// Mirrors the port's own limit (miniature.h: int vertex[20][3]) -- a map.txt
// with more than 20 cube= lines would silently lose cubes past that cap when
// the game loads it, so the editor refuses to create more than this.
constexpr int kMaxCubes = 20;

// The game's cube grid steps by 2 on every axis (see neutron-port's
// game_data.cpp MINI_DATA tables); the editor only ever places cubes on that
// grid, so every coordinate here is implicitly even.
constexpr int kGridStep = 2;
constexpr int kGridHalfExtent = 12; // supports coords from -12 to +12

struct Cube {
    int x, y, z;
};

class MapEditor {
public:
    std::string name = "Custom Stage";
    std::vector<Cube> cubes;
    int current_layer_y = 0; // which Y slice the 2D grid editor is showing

    bool has_cube(int x, int y, int z) const;
    // Returns false (and leaves the set unchanged) if this would exceed kMaxCubes.
    bool toggle_cube(int x, int y, int z);
    void clear();

    // True if every cube shares a face (differs by exactly kGridStep on
    // exactly one axis) with at least one other cube. A disconnected cube
    // produces walls the game's terrain builder can't cancel against a
    // neighbor, so it ends up floating/unreachable in-game.
    bool is_fully_connected() const;

    // Writes map.txt in the given directory (cwd-relative convention the
    // game itself uses for neutron_save.txt/music.wav). Returns false on
    // write failure.
    bool save(const std::string& path) const;

    // Loads an existing map.txt (same format), replacing current state.
    // Returns false if the file doesn't exist or has no valid cubes.
    bool load(const std::string& path);
};
