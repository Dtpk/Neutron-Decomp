#include "player.h"

player::player()
    : ship(),
      start_x(0), start_y(0), start_z(0),
      start_actual_face(1), start_previous_face(1), start_plane(0),
      user_color(0), cam_first_person(new camera()) {
    change_color(user_color);
}

player::~player() {
    delete cam_first_person;
}

void player::change_color(int uc) {
    user_color = uc;
    if (uc == 0) {
        // Light Blue / Cyan
        material[0] = 60;
        material[1] = 200;
        material[2] = 255;
        material[3] = 200;
    } else if (uc == 1) {
        material[0] = 255;
        material[1] = 100;
        material[2] = 200;
        material[3] = 200;
    } else {
        material[0] = 50;
        material[1] = 255;
        material[2] = 180;
        material[3] = 200;
    }
}

void player::restart() {
    step_speed = 0;
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
    status = 1;
    step_counter = 0;
}

int player::move(plane** map, int mode) {
    if (!map || !map[actual_plane] || !map[actual_plane]->pieces[0]) return 0;
    int idx = map[actual_plane]->find_piece_index(x, y, z);
    if (idx < 0 || idx >= 400 || !map[actual_plane]->pieces[idx]) return 0;

    piece* pp = map[actual_plane]->pieces[idx];
    if (pp) {
        if (!pp->state) {
            pp->change_state(material, 1);
            pp->owner = ident;
            if (mode == 1) {
                pp->prints_counter = -cue_counter;
                cue_counter++;
            }
        } else {
            if (mode == 1) {
                return pp->type;
            }
            pp->change_state(material, 1);
            pp->owner = ident;
        }
        if (mode == 1) {
            pp->temporal = false;
        } else {
            pp->temporal = true;
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

void player::print(int axis, bool low_quality) {
    if (!printer) return;

    if (status != 6) {
        if (status == 3) {
            printer->solid_cube(x, y, z, 255, 255, 255, (int)effect_alpha, effect_size);
            if (effect_alpha < 255.0f) effect_alpha += 7.0f;
            if (size < effect_size) effect_size -= 0.3f;
            else if (effect_alpha >= (float)material[3]) status = 1;
        }
        // Player cycle dot is bright clean WHITE
        printer->solid_cube(x, y, z, 255, 255, 255, 255, size);
        return;
    }

    if (effect_size < 3.0f) {
        effect_size += 0.1f;
    }
    int alpha = (int)effect_alpha;
    if (alpha < 7) {
        status = 2;
        effect_alpha = 0.0f;
    } else {
        alpha -= 7;
        effect_alpha = (float)alpha;
    }
    if (effect_alpha < 0.0f) effect_alpha = 0.0f;

    printer->solid_cube(x, y, z, 255, 255, 255, (int)effect_alpha, effect_size);
    printer->solid_cube(x, y, z, 255, 255, 255, material[3], size);
}

void player::print_guides(int face, int axis) {
    if (!printer) return;
    float lx = x, ly = y, lz = z;
    float rx = x, ry = y, rz = z;
    float dist = 0.8f;

    switch (face) {
        case 1:
        case 4:
            // Dead axis 3 (Z fixed, movement in X / Y)
            if (advance == 1) { // Moving +X
                ly += dist; ry -= dist;
            } else if (advance == -1) { // Moving -X
                ly -= dist; ry += dist;
            } else if (advance == 2) { // Moving +Y
                lx -= dist; rx += dist;
            } else if (advance == -2) { // Moving -Y
                lx += dist; rx -= dist;
            }
            break;

        case 2:
        case 5:
            // Dead axis 2 (Y fixed, movement in X / Z)
            if (advance == 1) { // Moving +X
                lz += dist; rz -= dist;
            } else if (advance == -1) { // Moving -X
                lz -= dist; rz += dist;
            } else if (advance == 3) { // Moving +Z
                lx -= dist; rx += dist;
            } else if (advance == -3) { // Moving -Z
                lx += dist; rx -= dist;
            }
            break;

        case 3:
        case 6:
            // Dead axis 1 (X fixed, movement in Y / Z)
            if (advance == 2) { // Moving +Y
                lz += dist; rz -= dist;
            } else if (advance == -2) { // Moving -Y
                lz -= dist; rz += dist;
            } else if (advance == 3) { // Moving +Z
                ly -= dist; ry += dist;
            } else if (advance == -3) { // Moving -Z
                ly += dist; ry -= dist;
            }
            break;

        default:
            break;
    }

    // Verified against the actual plane geometry: for a ship standing on a
    // face with inward normal Up = -axis_polarity * dead_axis_unit_vector
    // and forward direction F (from `advance`), the true "turn right" side
    // is Up x F. Checked all 6 faces x all 4 travel directions (24 cases)
    // against the offset math above: faces 1/2/3 already match Up x F as
    // written, but faces 4/5/6 come out mirrored (NOT simply the
    // negative-axis_polarity faces -- that was the first attempt at this fix
    // and was wrong for the 2/5 pair, since face 2 is already correct and
    // face 5 is the one that needed mirroring). Swapping red/blue on 4/5/6
    // makes every face agree with Up x F.
    bool needs_mirror = (face == 4 || face == 5 || face == 6);
    if (needs_mirror) {
        float tx = lx, ty = ly, tz = lz;
        lx = rx; ly = ry; lz = rz;
        rx = tx; ry = ty; rz = tz;
    }

    // Left guide is RED (turn left), Right guide is BLUE (turn right)
    printer->square_3D(lx, ly, lz, 255, 30, 30, 220, 0.35f, axis);
    printer->square_3D(rx, ry, rz, 30, 100, 255, 220, 0.35f, axis);
}
