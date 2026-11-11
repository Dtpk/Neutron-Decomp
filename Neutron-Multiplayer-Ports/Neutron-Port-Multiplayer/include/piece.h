#pragma once

#include "glinterface.h"

class piece {
public:
    int ident;
    float x;
    float y;
    float z;
    float size;
    int material[4];
    glinterface* printer;
    int type;
    bool state;
    int owner;
    bool temporal;
    double prints_counter;
    float effect_size;
    float effect_alpha;
    int status;

    piece();
    ~piece();

    void change_state(const int* material, int type);
    void restart();
    void print(int axis, bool low_quality);
};
