#include "plane.h"
#include <cstring>

static void step_coords(int adv, int& x, int& y, int& z) {
    switch (adv) {
        case  1: x += 1; break;
        case -1: x -= 1; break;
        case  2: y += 1; break;
        case -2: y -= 1; break;
        case  3: z += 1; break;
        case -3: z -= 1; break;
        default: break;
    }
}

plane::plane()
    : ident(0), i(0), j(0), side_size(0), dead_axis(0), dead_offset(0),
      face_number(0), axis_polarity(1),
      id_union_top(-1), id_union_left(-1), id_union_bottom(-1), id_union_right(-1),
      visited(false) {
    std::memset(cam_positions, 0, sizeof(cam_positions));
    std::memset(a_vertex, 0, sizeof(a_vertex));
    std::memset(b_vertex, 0, sizeof(b_vertex));
    std::memset(c_vertex, 0, sizeof(c_vertex));
    std::memset(d_vertex, 0, sizeof(d_vertex));
    std::memset(max_vertex, 0, sizeof(max_vertex));
    std::memset(pieces, 0, sizeof(pieces));
    std::memset(index, 0, sizeof(index));
}

plane::~plane() {
    int total = side_size * side_size;
    if (total > 400) total = 400;
    for (int k = 0; k < total; k++) {
        delete pieces[k];
        pieces[k] = nullptr;
    }
}

float plane::get_cam_position_x(int value) {
    if (dead_axis == 1) {
        return (float)(value + side_size * axis_polarity);
    }
    return (float)(value * 3 - side_size);
}

float plane::get_cam_position_y(int value) {
    if (dead_axis == 2) {
        return (float)(value + side_size * axis_polarity);
    }
    return (float)(value * 3 - side_size);
}

float plane::get_cam_position_z(int value) {
    if (dead_axis != 1 && dead_axis != 2) {
        return (float)(side_size * axis_polarity + value);
    }
    return (float)(value * 3 - side_size);
}

int plane::find_piece_index(float x, float y, float z) {
    int row, col;
    if (dead_axis == 1) {
        row = (int)y - i;
        col = (int)z - j;
    } else if (dead_axis == 2) {
        row = (int)x - i;
        col = (int)z - j;
    } else {
        row = (int)x - i;
        col = (int)y - j;
    }
    if (row < 0 || row >= 20 || col < 0 || col >= 20) return -1;
    return index[row][col];
}

bool plane::has_piece(float x, float y, float z) {
    int total = side_size * side_size;
    if (total > 400) total = 400;
    for (int k = 0; k < total; k++) {
        if (pieces[k] && pieces[k]->x == x && pieces[k]->y == y && pieces[k]->z == z) {
            return true;
        }
    }
    return false;
}

int plane::longest_way(int x, int y, int z, int right_advance, int left_advance) {
    int rx = x, ry = y, rz = z;
    int r_dist = 0;
    while (true) {
        if (!has_piece((float)rx, (float)ry, (float)rz)) break;
        int idx = find_piece_index((float)rx, (float)ry, (float)rz);
        if (idx < 0 || pieces[idx]->state) break;
        r_dist++;
        step_coords(right_advance, rx, ry, rz);
    }

    int lx = x, ly = y, lz = z;
    int l_dist = 0;
    while (true) {
        if (!has_piece((float)lx, (float)ly, (float)lz)) break;
        int idx = find_piece_index((float)lx, (float)ly, (float)lz);
        if (idx < 0 || pieces[idx]->state) break;
        l_dist++;
        step_coords(left_advance, lx, ly, lz);
    }

    if (r_dist == l_dist) return 2;
    return (r_dist > l_dist) ? 1 : 0;
}

int plane::available_ways(int x, int y, int z, int advance) {
    int right_adv = 0, left_adv = 0;
    int idx_r = -1, idx_l = -1;

    if (dead_axis == 2) {
        if (advance == -1) {
            if (face_number == 5) { right_adv = -3; left_adv =  3; }
            else                  { right_adv =  3; left_adv = -3; }
        } else if (advance == 1) {
            if (face_number == 5) { right_adv =  3; left_adv = -3; }
            else                  { right_adv = -3; left_adv =  3; }
        } else if (advance == -3) {
            if (face_number == 5) { right_adv =  1; left_adv = -1; }
            else                  { right_adv = -1; left_adv =  1; }
        } else if (advance == 3) {
            if (face_number == 5) { right_adv = -1; left_adv =  1; }
            else                  { right_adv =  1; left_adv = -1; }
        }
    } else if (dead_axis == 3) {
        if (advance == -1) {
            if (face_number == 4) { right_adv = -2; left_adv =  2; }
            else                  { right_adv =  2; left_adv = -2; }
        } else if (advance == 1) {
            if (face_number == 4) { right_adv =  2; left_adv = -2; }
            else                  { right_adv = -2; left_adv =  2; }
        } else if (advance == -2) {
            if (face_number == 4) { right_adv =  1; left_adv = -1; }
            else                  { right_adv = -1; left_adv =  1; }
        } else if (advance == 2) {
            if (face_number == 4) { right_adv = -1; left_adv =  1; }
            else                  { right_adv =  1; left_adv = -1; }
        }
    } else if (dead_axis == 1) {
        if (advance == -2) {
            if (face_number == 6) { right_adv = -3; left_adv =  3; }
            else                  { right_adv =  3; left_adv = -3; }
        } else if (advance == 2) {
            if (face_number == 6) { right_adv =  3; left_adv = -3; }
            else                  { right_adv = -3; left_adv =  3; }
        } else if (advance == -3) {
            if (face_number == 6) { right_adv =  2; left_adv = -2; }
            else                  { right_adv = -2; left_adv =  2; }
        } else if (advance == 3) {
            if (face_number == 6) { right_adv = -2; left_adv =  2; }
            else                  { right_adv =  2; left_adv = -2; }
        }
    }

    int rx = x, ry = y, rz = z;
    step_coords(right_adv, rx, ry, rz);
    idx_r = find_piece_index((float)rx, (float)ry, (float)rz);

    int lx = x, ly = y, lz = z;
    step_coords(left_adv, lx, ly, lz);
    idx_l = find_piece_index((float)lx, (float)ly, (float)lz);

    bool canGoRight = (idx_r >= 0 && idx_r < side_size * side_size && pieces[idx_r] && !pieces[idx_r]->state);
    bool canGoLeft  = (idx_l >= 0 && idx_l < side_size * side_size && pieces[idx_l] && !pieces[idx_l]->state);

    if (canGoRight) {
        if (canGoLeft) {
            if (right_adv != 0 && left_adv != 0) {
                return longest_way(x, y, z, right_adv, left_adv);
            }
            return 2;
        }
        return 1;
    } else if (canGoLeft) {
        return 0;
    }
    return -1;
}

int plane::print(bool low_quality) {
    int total = side_size * side_size;
    if (total > 400) total = 400;
    int count = 0;
    for (int k = 0; k < total; k++) {
        piece* p = pieces[k];
        if (p && (p->state || p->status == 2)) {
            p->print(dead_axis, low_quality);
            count++;
        }
    }
    return count;
}

void plane::generate_positions(int offset_x, int offset_y, int offset_z, int cam_distance) {
    if (side_size < 1) return;

    int count = 0;
    int local_40 = 0;

    for (int row = 0; row < side_size; row++) {
        int local_44 = 0;
        for (int col = 0; col < side_size; col++) {
            piece* pp = new piece();
            pp->ident = count;
            pp->temporal = true;
            pp->state = false;
            pp->type = 0;
            pp->owner = -2;
            pp->status = 0;
            pp->size = 0.3f;
            pp->effect_size = pp->size;
            pp->effect_alpha = 180.0f;
            pieces[count] = pp;

            float px = 0.0f, py = 0.0f, pz = 0.0f;
            if (dead_axis == 1) {
                py = (float)(row + offset_y + i);
                pz = (float)(col + offset_z + j);
                px = (float)(offset_x + dead_offset);
                cam_positions[count][0] = (int)px + cam_distance * axis_polarity;
                cam_positions[count][1] = (local_40 + (int)py) - cam_distance;
                cam_positions[count][2] = (local_44 + (int)pz) - cam_distance;
            } else if (dead_axis == 2) {
                px = (float)(row + offset_x + i);
                pz = (float)(col + offset_z + j);
                py = (float)(offset_y + dead_offset);
                cam_positions[count][0] = (local_40 + (int)px) - cam_distance;
                cam_positions[count][1] = (int)py + cam_distance * axis_polarity;
                cam_positions[count][2] = (local_44 + (int)pz) - cam_distance;
            } else {
                px = (float)(row + offset_x + i);
                py = (float)(offset_y + j + col);
                pz = (float)(offset_z + dead_offset);
                cam_positions[count][0] = (local_40 + (int)px) - cam_distance;
                cam_positions[count][1] = ((int)py + local_44) - cam_distance;
                cam_positions[count][2] = (int)pz + cam_distance * axis_polarity;
            }

            pp->x = px;
            pp->y = py;
            pp->z = pz;

            index[row][col] = count;

            if ((int)px > max_vertex[0]) max_vertex[0] = (int)px;
            if ((int)py > max_vertex[1]) max_vertex[1] = (int)py;
            if ((int)pz > max_vertex[2]) max_vertex[2] = (int)pz;

            if (row == 0) {
                if (col == 0) {
                    a_vertex[0] = (int)px; a_vertex[1] = (int)py; a_vertex[2] = (int)pz;
                }
                if (col == side_size - 1) {
                    b_vertex[0] = (int)px; b_vertex[1] = (int)py; b_vertex[2] = (int)pz;
                }
            }
            if (row == side_size - 1) {
                if (col == 0) {
                    c_vertex[0] = (int)px; c_vertex[1] = (int)py; c_vertex[2] = (int)pz;
                }
                if (col == side_size - 1) {
                    d_vertex[0] = (int)px; d_vertex[1] = (int)py; d_vertex[2] = (int)pz;
                }
            }

            count++;
            local_44 += cam_distance / 10;
        }
        local_40 += cam_distance / 10;
    }
}
