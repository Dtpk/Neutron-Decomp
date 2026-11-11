#pragma once

#include "plane.h"
#include "camera.h"
#include "miniature.h"

class terrain {
public:
    int N_PLANES;
    plane* map[100];
    camera* cam_terrain;
    glinterface* printer;
    int material[4];
    int ground_material[4];
    int unions[100][4][3];
    int max_vertex[3];
    bool is_clear;
    bool painted[100][5];
    int painted_bones;
    int painted_planes;
    float time;
    int status;
    int start_on_plane;

    terrain();
    ~terrain();

    void create_unions();
    int wich_union(int a, int b, int c, int n_plane, int union_type);
    void set_unions_of_plane(int n_plane);
    bool am_i_clear();
    void fire_up_cue(int ship_ident);
    void clear_cue(int ship_ident);
    void restart();
    void combat_mode();
    void navigation_mode();
    void change_color(int r, int g, int b, int a);
    void create_from_miniature(miniature* mini_map);
    void create_from_array(int n_planes, const int (*PLANES)[7]);
    void print_skeleton();
    void build_skeleton(bool build);
    int print(bool skeleton, int actual_plane, bool low_quality);
    void init(int offset_x, int offset_y, int offset_z, int cam_distance);
};
