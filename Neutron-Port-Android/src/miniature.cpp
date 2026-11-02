#include "miniature.h"
#include <cstring>

miniature::miniature(const int (*data)[3], int num_cubes, int offset_x, int offset_y, int offset_z, const char* new_name)
    : cubes(num_cubes), printer(new glinterface()), high_score(111111), rotation(0), name(new_name) {
    max_vertex[0] = 0; max_vertex[1] = 0; max_vertex[2] = 0;
    min_vertex[0] = 1000; min_vertex[1] = 1000; min_vertex[2] = 1000;
    cam[0] = 0; cam[1] = 0; cam[2] = 0;
    cam_center[0] = 0; cam_center[1] = offset_y; cam_center[2] = 0;
    offset[0] = offset_x; offset[1] = offset_y; offset[2] = offset_z;
    color[0] = 255; color[1] = 255; color[2] = 255; color[3] = 75;

    std::memset(vertex, 0, sizeof(vertex));
    std::memset(paint_pos, 0, sizeof(paint_pos));

    for (int i = 0; i < cubes && i < 20; i++) {
        int vx = data[i][0];
        int vy = data[i][1];
        int vz = data[i][2];

        vertex[i][0] = vx;
        vertex[i][1] = vy;
        vertex[i][2] = vz;

        paint_pos[i][0] = offset_x + vx;
        paint_pos[i][1] = offset_y + vy;
        paint_pos[i][2] = offset_z + vz;

        if (vx + 2 > max_vertex[0]) max_vertex[0] = vx + 2;
        if (vy + 2 > max_vertex[1]) max_vertex[1] = vy + 2;
        if (vz + 2 > max_vertex[2]) max_vertex[2] = vz + 2;

        if (vx < min_vertex[0]) min_vertex[0] = vx;
        if (vy < min_vertex[1]) min_vertex[1] = vy;
        if (vz < min_vertex[2]) min_vertex[2] = vz;
    }

    cam_center[2] = offset_z + (max_vertex[2] / 2) + min_vertex[2];
}

miniature::~miniature() {
    delete printer;
}

void miniature::change_color(int r, int g, int b, int a) {
    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;
}

plane* miniature::convert_to_plane(int cube, int face_number, int id_plane, int side_size) {
    int dead_offset = 0;
    int pi = 0;
    int pj = 0;
    int dead_axis = 0;
    int axis_polarity = 1;

    int half_step = side_size - 1;

    switch (face_number) {
        case 1:
            dead_axis = 3;
            dead_offset = side_size + half_step * (vertex[cube][2] / 2) - 1;
            pi = (vertex[cube][0] / 2) * half_step;
            pj = half_step * (vertex[cube][1] / 2);
            axis_polarity = 1;
            break;
        case 2:
            dead_axis = 2;
            dead_offset = half_step * (vertex[cube][1] / 2);
            pi = (vertex[cube][0] / 2) * half_step;
            pj = half_step * (vertex[cube][2] / 2);
            axis_polarity = -1;
            break;
        case 3:
            dead_axis = 1;
            dead_offset = side_size + half_step * (vertex[cube][0] / 2) - 1;
            pi = (vertex[cube][1] / 2) * half_step;
            pj = half_step * (vertex[cube][2] / 2);
            axis_polarity = 1;
            break;
        case 4:
            dead_axis = 3;
            dead_offset = half_step * (vertex[cube][2] / 2);
            pi = (vertex[cube][0] / 2) * half_step;
            pj = half_step * (vertex[cube][1] / 2);
            axis_polarity = -1;
            break;
        case 5:
            dead_axis = 2;
            dead_offset = side_size + half_step * (vertex[cube][1] / 2) - 1;
            pi = (vertex[cube][0] / 2) * half_step;
            pj = half_step * (vertex[cube][2] / 2);
            axis_polarity = 1;
            break;
        case 6:
            dead_axis = 1;
            dead_offset = half_step * (vertex[cube][0] / 2);
            pi = (vertex[cube][1] / 2) * half_step;
            pj = half_step * (vertex[cube][2] / 2);
            axis_polarity = -1;
            break;
        default:
            break;
    }

    plane* p = new plane();
    p->ident = id_plane;
    p->i = pi;
    p->j = pj;
    p->side_size = side_size;
    p->dead_axis = dead_axis;
    p->dead_offset = dead_offset;
    p->face_number = face_number;
    p->axis_polarity = axis_polarity;
    p->id_union_right = -1;
    p->id_union_top = -1;
    p->id_union_left = -1;
    p->id_union_bottom = -1;
    return p;
}

void miniature::print() {
    if (!printer) return;

    int cz = min_vertex[2] + offset[2] + ((max_vertex[2] - min_vertex[2]) / 4) * 2;
    int cx = min_vertex[0] + offset[0] + ((max_vertex[0] - min_vertex[0]) / 4) * 2;
    float cy = (float)paint_pos[0][1];

    glPushMatrix();
    glTranslatef((float)cx, cy, (float)cz);
    glRotatef((float)rotation, 0.0f, 1.0f, 0.0f);
    glTranslatef(-(float)cx, -cy, -(float)cz);

    for (int i = 0; i < cubes && i < 20; i++) {
        printer->solid_cube((float)paint_pos[i][0], (float)paint_pos[i][1], (float)paint_pos[i][2],
                            color[0], color[1], color[2], color[3], 1.0f);
        printer->cube_skeleton((float)paint_pos[i][0], (float)paint_pos[i][1], (float)paint_pos[i][2],
                               255, 255, 255, color[3], 1.0f);
    }
    glPopMatrix();
}
