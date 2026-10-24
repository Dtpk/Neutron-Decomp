#pragma once

#include "piece.h"

class plane {
public:
    int ident;
    int i;
    int j;
    int side_size;
    int dead_axis;
    int dead_offset;
    int face_number;
    int axis_polarity;
    int id_union_top;
    int id_union_left;
    int id_union_bottom;
    int id_union_right;
    bool visited;
    int cam_positions[400][3];
    int a_vertex[3];
    int b_vertex[3];
    int c_vertex[3];
    int d_vertex[3];
    int max_vertex[3];
    piece* pieces[400];
    int index[20][20];

    plane();
    ~plane();

    float get_cam_position_x(int value);
    float get_cam_position_y(int value);
    float get_cam_position_z(int value);
    int find_piece_index(float x, float y, float z);
    bool has_piece(float x, float y, float z);
    int longest_way(int x, int y, int z, int right_advance, int left_advance);
    int available_ways(int x, int y, int z, int advance);
    int print(bool low_quality);
    void generate_positions(int offset_x, int offset_y, int offset_z, int cam_distance);
};
