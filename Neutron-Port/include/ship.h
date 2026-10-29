#pragma once

#include "plane.h"
#include "camera.h"

class ship {
public:
    float x;
    float y;
    float z;
    float x_predict;
    float y_predict;
    float z_predict;
    float size;
    int parts;
    int material[4];
    camera* cam_ship;
    glinterface* printer;
    int advance;
    int last_advance;
    int previous_face;
    int actual_face;
    int actual_plane;
    int last_plane;
    int cue_counter;
    int plane_path[20];
    int ident;
    bool path_found;
    int status;
    int step_counter;
    int step_speed;
    float effect_size;
    float effect_alpha;

    ship();
    virtual ~ship();

    int tell_me_the_cost(float x, float y, float z, plane** map, int advance, int steps, int target_x, int target_y, int target_z);
    void set_position(GLfloat x, GLfloat y, GLfloat z);
    void arrange_direction_to(int face_forward);
    void change_direction(int turn);
    void do_step(plane** map);
    void init_pos(int x, int y, int z);
    void init(plane** map);
    bool find_path(int x, int y, int z, int dead_axis, int polarity);
    int where_is(float x, float y, float z, plane** map, int n_planes);
    bool look_for_pos(float x, float y, float z, plane** map, int n_planes);
    bool go_to(float x, float y, float z, plane** map, int n_planes);
};
