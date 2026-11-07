#pragma once

#include "plane.h"
#include "glinterface.h"

class miniature {
public:
    int color[4];
    int vertex[20][3];
    int paint_pos[20][3];
    int max_vertex[3];
    int min_vertex[3];
    int offset[3];
    int cam[3];
    int cam_center[3];
    int cubes;
    glinterface* printer;
    int high_score;
    int rotation;
    const char* name;

    miniature(const int (*data)[3], int cubes, int offset_x, int offset_y, int offset_z, const char* new_name);
    ~miniature();

    void change_color(int r, int g, int b, int a);
    plane* convert_to_plane(int cube, int face_number, int id_plane, int side_size);
    void print();
};
