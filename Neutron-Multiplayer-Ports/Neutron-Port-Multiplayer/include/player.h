#pragma once

#include "ship.h"

class player : public ship {
public:
    int start_x;
    int start_y;
    int start_z;
    int start_actual_face;
    int start_previous_face;
    int start_plane;
    int user_color;
    camera* cam_first_person;

    player();
    virtual ~player();

    void change_color(int user_color);
    void restart();
    int move(plane** map, int mode);
    void print(int axis, bool low_quality);
    void print_guides(int face_number, int dead_axis);
};
