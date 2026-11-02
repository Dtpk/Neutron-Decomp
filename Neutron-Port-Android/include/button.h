#pragma once

#include "glinterface.h"

class button {
public:
    glinterface* printer;
    float x;
    float y;
    float z;
    int color[4];
    float size;
    int left;
    int top;
    int right;
    int bottom;
    float width;
    float height;
    bool visible;
    bool enabled;
    int characters[256];
    int action;
    int action_type;
    int effect_alpha;
    bool isFadeing;
    int key;

    button();
    ~button();

    void change_color(int r, int g, int b, int a);
    void show_skeleton();
    void print_3D(float x, float y, float z, int axis, int polarity);
    void print_2D(float x, float y, float z);
    void print(float x, float y, float z, int method);
    void load_text(const char* word, int fade_alpha);
};
