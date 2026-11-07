#pragma once

#include <cstdint>

struct LevelInfo {
    const char* name;
    int cubes;
    const int (*data)[3];
};

extern const int FONT[49][6][6];
extern const int INTRO_DATA[94][4];
extern const int MENU_DATA[14][7];

extern const int CUBE_MINI_DATA[1][3];
extern const int RECTANGLE_MINI_DATA[2][3];
extern const int ELLE_MINI_DATA[3][3];
extern const int TE_MINI_DATA[4][3];
extern const int DOUBLECUBE_MINI_DATA[2][3];
extern const int ESE_MINI_DATA[4][3];
extern const int V_MINI_DATA[3][3];
extern const int CRAZY_CROSS_MINI_DATA[4][3];
extern const int DEATH_STAIRS_MINI_DATA[3][3];
extern const int CROSS_MINI_DATA[5][3];
extern const int ESE_CURVE_MINI_DATA[5][3];
extern const int TRIPOD_MINI_DATA[4][3];
extern const int KEOPS_MINI_DATA[9][3];
extern const int STAR_MINI_DATA[7][3];
extern const int CORNER_MINI_DATA[8][3];
extern const int MAGNET_MINI_DATA[7][3];
extern const int DONUT_MINI_DATA[8][3];
extern const int CURVE_MINI_DATA[7][3];
extern const int CASTLE_MINI_DATA[8][3];
extern const int PIRAMID_MINI_DATA[8][3];
extern const int KNOT_MINI_DATA[5][3];
extern const int BLENDER_MINI_DATA[8][3];
extern const int SHOES_MINI_DATA[9][3];
extern const int DEATH_STAR_MINI_DATA[12][3];
extern const int HELIX_MINI_DATA[11][3];

extern const LevelInfo LEVEL_TABLE[25];
