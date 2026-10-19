#include "ship.h"
#include <cstdlib>
#include <cstring>

ship::ship()
    : x(0.0f), y(0.0f), z(0.0f),
      x_predict(0.0f), y_predict(0.0f), z_predict(0.0f),
      size(0.3f), parts(1),
      cam_ship(new camera()), printer(new glinterface()),
      advance(1), last_advance(1),
      previous_face(1), actual_face(1),
      actual_plane(0), last_plane(0),
      cue_counter(0), ident(0),
      path_found(false), status(1),
      step_counter(0), step_speed(1),
      effect_size(0.3f), effect_alpha(200.0f) {
    material[0] = 0;
    material[1] = 100;
    material[2] = 250;
    material[3] = 200;
    std::memset(plane_path, 0, sizeof(plane_path));
}

ship::~ship() {
    delete cam_ship;
    delete printer;
}

void ship::set_position(GLfloat nx, GLfloat ny, GLfloat nz) {
    x = nx;
    y = ny;
    z = nz;
}

void ship::arrange_direction_to(int face_forward) {
    switch (actual_face) {
        case 1:
        case 4:
            if (face_forward == 3) { previous_face = 6; return; }
            if (face_forward == 2) { previous_face = 5; return; }
            if (face_forward == 5) { previous_face = 2; return; }
            if (face_forward == 6) { previous_face = 3; return; }
            break;
        case 2:
        case 5:
            if (face_forward == 3) { previous_face = 6; return; }
            if (face_forward == 1) { previous_face = 4; return; }
            if (face_forward == 4) { previous_face = 1; return; }
            if (face_forward == 6) { previous_face = 3; return; }
            break;
        case 3:
            if (face_forward == 2) { previous_face = 5; return; }
            if (face_forward == 4) { previous_face = 1; return; }
            if (face_forward == 5) { previous_face = 2; return; }
            if (face_forward == 1) { previous_face = 4; return; }
            if (face_forward == 6) { previous_face = 3; return; }
            break;
        case 6:
            if (face_forward == 2) { previous_face = 5; return; }
            if (face_forward == 4) { previous_face = 1; return; }
            if (face_forward == 5) { previous_face = 2; return; }
            if (face_forward == 1) { previous_face = 4; return; }
            if (face_forward == 3) { previous_face = 6; return; }
            break;
    }
}

void ship::change_direction(int turn) {
    if (turn != 0) { // Turn right
        switch (actual_face) {
            case 1:
                if (previous_face == 3) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 2; return; }
                if (previous_face == 2) { previous_face = 3; return; }
                break;
            case 2:
                if (previous_face == 3) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 3; return; }
                break;
            case 3:
                if (previous_face == 2) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 2; return; }
                break;
            case 4:
                if (previous_face == 3) { previous_face = 2; return; }
                if (previous_face == 2) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 3; return; }
                break;
            case 5:
                if (previous_face == 3) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 3; return; }
                break;
            case 6:
                if (previous_face == 2) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 2; return; }
                break;
        }
    } else { // Turn left
        switch (actual_face) {
            case 1:
                if (previous_face == 3) { previous_face = 2; return; }
                if (previous_face == 2) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 3; return; }
                break;
            case 2:
                if (previous_face == 3) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 3; return; }
                break;
            case 3:
                if (previous_face == 2) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 2; return; }
                break;
            case 4:
                if (previous_face == 3) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 2; return; }
                if (previous_face == 2) { previous_face = 3; return; }
                break;
            case 5:
                if (previous_face == 3) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 6; return; }
                if (previous_face == 6) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 3; return; }
                break;
            case 6:
                if (previous_face == 2) { previous_face = 4; return; }
                if (previous_face == 4) { previous_face = 5; return; }
                if (previous_face == 5) { previous_face = 1; return; }
                if (previous_face == 1) { previous_face = 2; return; }
                break;
        }
    }
}

void ship::do_step(plane** map) {
    last_plane = actual_plane;
    last_advance = advance;

    switch (actual_face) {
        case 1:
        case 4:
            if (previous_face == 3) {
                if (x != map[actual_plane]->pieces[0]->x) {
                    advance = -1;
                    x -= 1.0f;
                    return;
                }
                int nxt = map[actual_plane]->id_union_left;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refZ = pNext->pieces[pNext->side_size + 1]->z;
                if (z < refZ) {
                    previous_face = 4;
                    advance = 3;
                    actual_face = pNext->face_number;
                    z += 1.0f;
                    return;
                }
                if (refZ == z) {
                    advance = -1;
                    x -= 1.0f;
                    return;
                }
                previous_face = 1;
                advance = -3;
                actual_face = pNext->face_number;
                z -= 1.0f;
                return;
            } else if (previous_face == 2) {
                plane* cur = map[actual_plane];
                if (y != ((float)cur->side_size + cur->pieces[0]->y) - 1.0f) {
                    advance = 2;
                    y += 1.0f;
                    return;
                }
                int nxt = cur->id_union_top;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refZ = pNext->pieces[pNext->side_size + 1]->z;
                if (z < refZ) {
                    previous_face = 4;
                    advance = 3;
                    actual_face = pNext->face_number;
                    z += 1.0f;
                    return;
                }
                if (refZ == z) {
                    advance = 2;
                    y += 1.0f;
                    return;
                }
                previous_face = 1;
                advance = -3;
                actual_face = pNext->face_number;
                z -= 1.0f;
                return;
            } else if (previous_face == 5) {
                if (y != map[actual_plane]->pieces[0]->y) {
                    advance = -2;
                    y -= 1.0f;
                    return;
                }
                int nxt = map[actual_plane]->id_union_bottom;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refZ = pNext->pieces[pNext->side_size + 1]->z;
                if (z < refZ) {
                    previous_face = 4;
                    advance = 3;
                    actual_face = pNext->face_number;
                    z += 1.0f;
                    return;
                }
                if (refZ == z) {
                    advance = -2;
                    y -= 1.0f;
                    return;
                }
                previous_face = 1;
                advance = -3;
                actual_face = pNext->face_number;
                z -= 1.0f;
                return;
            } else if (previous_face == 6) {
                plane* cur = map[actual_plane];
                if (x != ((float)cur->side_size + cur->pieces[0]->x) - 1.0f) {
                    advance = 1;
                    x += 1.0f;
                    return;
                }
                int nxt = cur->id_union_right;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refZ = pNext->pieces[pNext->side_size + 1]->z;
                if (z < refZ) {
                    previous_face = 4;
                    advance = 3;
                    actual_face = pNext->face_number;
                    z += 1.0f;
                    return;
                }
                if (refZ == z) {
                    advance = 1;
                    x += 1.0f;
                    return;
                }
                previous_face = 1;
                advance = -3;
                actual_face = pNext->face_number;
                z -= 1.0f;
                return;
            }
            break;

        case 2:
        case 5:
            if (previous_face == 3) {
                if (x != map[actual_plane]->pieces[0]->x) {
                    advance = -1;
                    x -= 1.0f;
                    return;
                }
                int nxt = map[actual_plane]->id_union_left;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refY = pNext->pieces[pNext->side_size + 1]->y;
                if (y < refY) {
                    advance = 2;
                    previous_face = 2;
                    actual_face = pNext->face_number;
                    y += 1.0f;
                    return;
                }
                previous_face = 5;
                advance = -2;
                actual_face = pNext->face_number;
                y -= 1.0f;
                return;
            } else if (previous_face == 4) {
                plane* cur = map[actual_plane];
                if (z != ((float)cur->side_size + cur->pieces[0]->z) - 1.0f) {
                    advance = 3;
                    z += 1.0f;
                    return;
                }
                int nxt = cur->id_union_bottom;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refY = pNext->pieces[pNext->side_size + 1]->y;
                if (y < refY) {
                    advance = 2;
                    previous_face = 2;
                    actual_face = pNext->face_number;
                    y += 1.0f;
                    return;
                }
                previous_face = 5;
                advance = -2;
                actual_face = pNext->face_number;
                y -= 1.0f;
                return;
            } else if (previous_face == 6) {
                plane* cur = map[actual_plane];
                if (x != ((float)cur->side_size + cur->pieces[0]->x) - 1.0f) {
                    advance = 1;
                    x += 1.0f;
                    return;
                }
                int nxt = cur->id_union_right;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refY = pNext->pieces[pNext->side_size + 1]->y;
                if (y < refY) {
                    advance = 2;
                    previous_face = 2;
                    actual_face = pNext->face_number;
                    y += 1.0f;
                    return;
                }
                previous_face = 5;
                advance = -2;
                actual_face = pNext->face_number;
                y -= 1.0f;
                return;
            } else if (previous_face == 1) {
                if (z != map[actual_plane]->pieces[0]->z) {
                    advance = -3;
                    z -= 1.0f;
                    return;
                }
                int nxt = map[actual_plane]->id_union_top;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refY = pNext->pieces[pNext->side_size + 1]->y;
                if (y < refY) {
                    advance = 2;
                    previous_face = 2;
                    actual_face = pNext->face_number;
                    y += 1.0f;
                    return;
                }
                previous_face = 5;
                advance = -2;
                actual_face = pNext->face_number;
                y -= 1.0f;
                return;
            }
            break;

        case 3:
        case 6:
            if (previous_face == 1) {
                if (z != map[actual_plane]->pieces[0]->z) {
                    advance = -3;
                    z -= 1.0f;
                    return;
                }
                int nxt = map[actual_plane]->id_union_left;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refX = pNext->pieces[pNext->side_size + 1]->x;
                if (x < refX) {
                    previous_face = 6;
                    advance = 1;
                    actual_face = pNext->face_number;
                    x += 1.0f;
                    return;
                }
                previous_face = 3;
                advance = -1;
                actual_face = pNext->face_number;
                x -= 1.0f;
                return;
            } else if (previous_face == 5) {
                if (y != map[actual_plane]->pieces[0]->y) {
                    advance = -2;
                    y -= 1.0f;
                    return;
                }
                int nxt = map[actual_plane]->id_union_bottom;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refX = pNext->pieces[pNext->side_size + 1]->x;
                if (x < refX) {
                    previous_face = 6;
                    advance = 1;
                    actual_face = pNext->face_number;
                    x += 1.0f;
                    return;
                }
                previous_face = 3;
                advance = -1;
                actual_face = pNext->face_number;
                x -= 1.0f;
                return;
            } else if (previous_face == 2) {
                plane* cur = map[actual_plane];
                if (y != ((float)cur->side_size + cur->pieces[0]->y) - 1.0f) {
                    advance = 2;
                    y += 1.0f;
                    return;
                }
                int nxt = cur->id_union_top;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refX = pNext->pieces[pNext->side_size + 1]->x;
                if (x < refX) {
                    previous_face = 6;
                    advance = 1;
                    actual_face = pNext->face_number;
                    x += 1.0f;
                    return;
                }
                previous_face = 3;
                advance = -1;
                actual_face = pNext->face_number;
                x -= 1.0f;
                return;
            } else if (previous_face == 4) {
                plane* cur = map[actual_plane];
                if (z != ((float)cur->side_size + cur->pieces[0]->z) - 1.0f) {
                    advance = 3;
                    z += 1.0f;
                    return;
                }
                int nxt = cur->id_union_right;
                plane* pNext = map[nxt];
                actual_plane = nxt;
                float refX = pNext->pieces[pNext->side_size + 1]->x;
                if (x < refX) {
                    previous_face = 6;
                    advance = 1;
                    actual_face = pNext->face_number;
                    x += 1.0f;
                    return;
                }
                previous_face = 3;
                advance = -1;
                actual_face = pNext->face_number;
                x -= 1.0f;
                return;
            }
            break;
    }
}

void ship::init_pos(int ix, int iy, int iz) {
    if (cam_ship) {
        cam_ship->update_pos(ix, iy, iz, 0.0f, 0.0f, 0.0f);
        cam_ship->last_x = (float)ix;
        cam_ship->last_y = (float)iy;
        cam_ship->last_z = (float)iz;
    }
}

void ship::init(plane** map) {
    if (!map || !map[actual_plane]) return;
    int idx = map[actual_plane]->find_piece_index(x, y, z);
    if (idx >= 0 && cam_ship) {
        cam_ship->update_pos(map[actual_plane]->cam_positions[idx][0],
                             map[actual_plane]->cam_positions[idx][1],
                             map[actual_plane]->cam_positions[idx][2],
                             0.0f, 0.0f, 0.0f);
    }
}

bool ship::find_path(int tx, int ty, int tz, int dead_axis, int polarity) {
    bool found = false;
    if (dead_axis == 3) {
        if (x == (float)tx && y != (float)ty) {
            if ((float)ty <= y) {
                if (advance == 1) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                } else if (advance == 2) {
                    change_direction(std::rand() % 2);
                    found = true;
                } else if (advance == -1) {
                    if (polarity <= 0) change_direction(1); else change_direction(0);
                    found = true;
                }
            } else {
                if (advance == -1) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                } else if (advance == 1) {
                    if (polarity <= 0) change_direction(1); else change_direction(0);
                    found = true;
                } else if (advance == -2) {
                    change_direction(std::rand() % 2);
                    found = true;
                }
            }
        }
        if (y == (float)ty && (float)tx != x) {
            if ((float)tx <= x) {
                if (advance == 1) {
                    change_direction(std::rand() % 2);
                    found = true;
                } else if (advance == 2) {
                    if (polarity > 0) change_direction(0); else change_direction(1);
                    found = true;
                } else if (advance == -2) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                }
            } else {
                if (advance == -1) {
                    change_direction(std::rand() % 2);
                    found = true;
                } else if (advance == 2) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                } else if (advance == -2) {
                    if (polarity > 0) change_direction(0); else change_direction(1);
                    found = true;
                }
            }
        }
    } else if (dead_axis == 2) {
        if (z == (float)tz && x != (float)tx) {
            if ((float)tx <= x) {
                if (advance == 1) {
                    change_direction(std::rand() % 2);
                    found = true;
                } else if (advance == 3) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                } else if (advance == -3) {
                    if (polarity > 0) change_direction(0); else change_direction(1);
                    found = true;
                }
            } else {
                if (advance == -1) {
                    change_direction(std::rand() % 2);
                    found = true;
                } else if (advance == 3) {
                    if (polarity > 0) change_direction(0); else change_direction(1);
                    found = true;
                } else if (advance == -3) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                }
            }
        }
    } else if (dead_axis == 1) {
        if (z == (float)tz && y != (float)ty) {
            if ((float)ty <= y) {
                if (advance == 2) {
                    change_direction(std::rand() % 2);
                    found = true;
                } else if (advance == 3) {
                    if (polarity > 0) change_direction(0); else change_direction(1);
                    found = true;
                } else if (advance == -3) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                }
            } else {
                if (advance == -2) {
                    change_direction(std::rand() % 2);
                    found = true;
                } else if (advance == 3) {
                    if (polarity > 0) change_direction(1); else change_direction(0);
                    found = true;
                } else if (advance == -3) {
                    if (polarity > 0) change_direction(0); else change_direction(1);
                    found = true;
                }
            }
        }
    }
    return found;
}

int ship::where_is(float qx, float qy, float qz, plane** map, int n_planes) {
    if (!map) return 0;
    for (int k = 0; k < n_planes; k++) {
        if (map[k] && map[k]->has_piece(qx, qy, qz)) {
            return k;
        }
    }
    return 0;
}

int ship::tell_me_the_cost(float cx, float cy, float cz, plane** map, int adv, int steps, int tx, int ty, int tz) {
    return 0;
}

bool ship::look_for_pos(float qx, float qy, float qz, plane** map, int n_planes) {
    int target_plane = where_is(qx, qy, qz, map, n_planes);
    plane_path[0] = target_plane;
    if (target_plane == actual_plane) {
        return true;
    }
    path_found = false;
    if (map[target_plane]->face_number != actual_face) {
        arrange_direction_to(map[target_plane]->face_number);
        return false;
    }
    return true;
}

bool ship::go_to(float tx, float ty, float tz, plane** map, int n_planes) {
    if (x == tx && y == ty && z == tz) {
        plane_path[0] = -1;
        path_found = false;
        return true;
    }
    bool onPlane = look_for_pos(tx, ty, tz, map, n_planes);
    if (onPlane && !path_found) {
        path_found = find_path((int)tx, (int)ty, (int)tz, map[actual_plane]->dead_axis, map[actual_plane]->axis_polarity);
    }
    return false;
}
