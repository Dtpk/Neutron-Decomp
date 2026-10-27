#include "enemy.h"
#include <cstdlib>

enemy::enemy()
    : ship(),
      start_x(0), start_y(0), start_z(0),
      start_actual_face(1), start_previous_face(1), start_plane(0),
      type(0), active(true) {
    status = 0;
    material[0] = 250;
    material[1] = 0;
    material[2] = 0;
    material[3] = 200;
}

enemy::~enemy() {}

void enemy::restart() {
    active = true;
    x = (float)start_x;
    y = (float)start_y;
    z = (float)start_z;
    x_predict = x;
    y_predict = y;
    z_predict = z;
    actual_face = start_actual_face;
    previous_face = start_previous_face;
    actual_plane = start_plane;
    effect_size = size;
    effect_alpha = (float)material[3];
    cue_counter = 0;
    status = 0;
    step_counter = 0;
    step_speed = 0;
}

void enemy::predict(plane** map) {
    x_predict = x;
    y_predict = y;
    z_predict = z;
    if (!map || !map[actual_plane] || !map[actual_plane]->pieces[0]) return;
    plane* cur = map[actual_plane];
    if (!cur || !cur->pieces[0]) return;

    switch (advance) {
        case 1:
            if (x < ((float)cur->side_size + cur->pieces[0]->x) - 2.0f) {
                x_predict = x + 1.0f;
            }
            break;
        case 2:
            if (y < ((float)cur->side_size + cur->pieces[0]->y) - 2.0f) {
                y_predict = y + 1.0f;
            }
            break;
        case 3:
            if (z < ((float)cur->side_size + cur->pieces[0]->z) - 2.0f) {
                z_predict = z + 1.0f;
            }
            break;
        case -3:
            if (cur->pieces[0]->z < z) {
                z_predict = z - 1.0f;
            }
            break;
        case -2:
            if (cur->pieces[0]->y < y) {
                y_predict = y - 1.0f;
            }
            break;
        case -1:
            if (cur->pieces[0]->x < x) {
                x_predict = x - 1.0f;
            }
            break;
    }
}

int enemy::move(plane** map, int proton_face) {
    if (!map || !map[actual_plane] || !map[actual_plane]->pieces[0]) return 0;
    int idx = map[actual_plane]->find_piece_index(x, y, z);
    if (idx < 0 || idx >= 400 || !map[actual_plane]->pieces[idx]) return 0;

    if (map[actual_plane]->pieces[idx]->state) {
        return map[actual_plane]->pieces[idx]->type;
    }

    predict(map);
    int idx_pred = map[actual_plane]->find_piece_index(x_predict, y_predict, z_predict);

    int turn = map[actual_plane]->available_ways((int)x, (int)y, (int)z, advance);

    if (idx_pred >= 0 && idx_pred < 400 && map[actual_plane]->pieces[idx_pred] && !map[actual_plane]->pieces[idx_pred]->state) {
        if (std::rand() % 15 == 5) {
            if (turn < 2) {
                ship::change_direction(turn);
            } else if (turn == 2) {
                ship::change_direction(std::rand() % 2);
            }
        }
    } else {
        if (turn < 2) {
            ship::change_direction(turn);
        } else {
            ship::change_direction(std::rand() % 2);
        }
    }

    if (type == 2) {
        active = !active;
    }
    if (type == 3) {
        if (active) {
            active = (std::rand() % 2) != 0;
        } else {
            active = true;
        }
    } else {
        if (!active) {
            // jump change_state
            plane* cur = map[actual_plane];
            int camX = cur->cam_positions[idx][0];
            int camY = cur->cam_positions[idx][1];
            int camZ = cur->cam_positions[idx][2];
            ship::do_step(map);
            if (cam_ship) {
                cam_ship->update_pos(camX, camY, camZ, x, y, z);
            }
            return 0;
        }
    }

    if (active) {
        piece* pp = map[actual_plane]->pieces[idx];
        pp->change_state(material, 2);
        pp->prints_counter = -cue_counter;
        pp->owner = ident;
        cue_counter++;
        if (type == 3) {
            material[0] = std::rand() % 200;
            material[1] = std::rand() % 200;
            material[2] = std::rand() % 200;
        }
    }

    plane* cur = map[actual_plane];
    int camX = cur->cam_positions[idx][0];
    int camY = cur->cam_positions[idx][1];
    int camZ = cur->cam_positions[idx][2];

    ship::do_step(map);

    if (cam_ship) {
        cam_ship->update_pos(camX, camY, camZ, x, y, z);
    }
    return 0;
}

void enemy::print(int axis, bool low_quality) {
    if (!printer) return;

    if (status != 6) {
        printer->solid_cube(x, y, z, material[0], material[1], material[2], material[3], size);
        printer->solid_cube(x, y, z, 255, 255, 255, 255, size);
        return;
    }

    if (effect_size < 3.0f) {
        effect_size += 0.1f;
    }
    if (effect_alpha < 1.0f) {
        status = 2;
    } else {
        effect_alpha -= 7.0f;
    }
    if (effect_alpha < 0.0f) effect_alpha = 0.0f;

    printer->solid_cube(x, y, z, material[0], material[1], material[2], (int)effect_alpha, effect_size);
    printer->solid_cube(x, y, z, material[0], material[1], material[2], material[3], size);
    printer->solid_cube(x, y, z, 255, 255, 255, 255, size);
}
