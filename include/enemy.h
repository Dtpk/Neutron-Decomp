#pragma once

#include "ship.h"

class enemy : public ship {
public:
    int start_x;
    int start_y;
    int start_z;
    int start_actual_face;
    int start_previous_face;
    int start_plane;
    int type;
    bool active;

    enemy();
    virtual ~enemy();

    void restart();
    void predict(plane** map);
    int move(plane** map, int proton_face);
    void print(int axis, bool low_quality);
};
