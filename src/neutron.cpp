#include "neutron.h"
#include "game_data.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

/*
 * ============================================================================
 * NEUTRON C++ PORT - DWARF SYMBOL MANIFEST MAPPING (Ref: neutron_functions.txt)
 * ============================================================================
 * 0x08902aa0: neutron::neutron(int color, int difficulty, int stage, bool sound)
 * 0x0890bde0: neutron::move_lookat(camera*, float, float, float, int)
 * 0x0890bfa0: neutron::move_camera(camera*, float, float, float, int)
 * 0x08903330: neutron::load_new_enemy()
 * 0x08903520: neutron::autoload_eletrons(int)
 * 0x0890868c: neutron::navigate()
 * 0x08908850: neutron::combat()
 * 0x08908da0: neutron::run()
 * 0x08908e00: neutron::print()
 * 0x089037c0: neutron::load_stage(int, bool)
 * ============================================================================
 */

neutron::neutron(int color, int difficulty, int stage, bool sound)
    : game_status(1000),
      go_status(1000),
      game_mode(0),
      actual_stage(1000),
      score(0),
      user_score(0),
      high_score(111111),
      colision(0),
      actual_miniature(0),
      actual_option(0),
      user_color(color),
      user_difficulty(difficulty),
      user_stage(stage),
      user_sound(sound),
      isTutorial(false),
      tutorial_step(0),
      tutorial_counter(0),
      time(10),
      speed(0.03f),
      game_cam(new camera()),
      interface(new stage_interface(color, difficulty, stage, sound)),
      mixer(new sound_mixer()),
      ground(nullptr),
      proton(nullptr),
      N_SHIPS(1),
      n_enemies(0),
      charged_electrons(0),
      last_charged_electron(0),
      uncharged_electrons(0),
      bg_current_count(0),
      bg_current_size(0),
      game_over(false),
      intro_step(0),
      intro_counter(0),
      intro_substep(0),
      vision(0),
      prints(0),
      low_quality(false),
      actual_artifact(0) {

    std::memset(electron, 0, sizeof(electron));
    std::memset(miniatures, 0, sizeof(miniatures));
    std::strcpy(enemies_buffer, "00");
    std::strcpy(score_buffer, "000000");

    game_cam->x = 11.0f;
    game_cam->y = 5.0f;
    game_cam->z = 10.0f;
    game_cam->x_dir = 11.0f;
    game_cam->y_dir = 5.0f;
    game_cam->z_dir = 0.0f;
    game_cam->x_ori = 0.0f;
    game_cam->y_ori = 1.0f;
    game_cam->z_ori = 0.0f;

    for (int i = 0; i < 50; i++) {
        background_colors[i][0] = std::rand() % 500 + 200;
        background_colors[i][1] = std::rand() % 500 - 250;
        background_colors[i][2] = std::rand() % 500 - 250;
        background_colors[i][3] = std::rand() % 255;
        background_colors[i][4] = std::rand() % 255;
        background_colors[i][5] = std::rand() % 255;
        background_colors[i][6] = std::rand() % 3 + 1;
        background_colors[i][7] = std::rand() % 2;
        background_colors[i][8] = 1;
        background_colors[i][9] = std::rand() % 359 + 1;
        background_colors[i][10] = std::rand() % 50 - 25;
        background_colors[i][11] = std::rand() % 50 - 25;
        background_colors[i][12] = std::rand() % 50 - 25;
    }

    create_miniatures();
    load_stage(1000, true);
}

neutron::~neutron() {
    delete game_cam;
    delete interface;
    delete mixer;
    unload_stage();
    for (int i = 0; i < 25; i++) {
        delete miniatures[i];
    }
}

bool neutron::move_lookat(camera* cam, float tx, float ty, float tz, int spd) {
    if (!cam) return true;
    bool done_x = false, done_y = false, done_z = false;
    float step = (float)spd * 0.1f;

    if (std::fabs(cam->x_dir - tx) <= step) { cam->x_dir = tx; done_x = true; }
    else if (cam->x_dir < tx) cam->x_dir += step;
    else cam->x_dir -= step;

    if (std::fabs(cam->y_dir - ty) <= step) { cam->y_dir = ty; done_y = true; }
    else if (cam->y_dir < ty) cam->y_dir += step;
    else cam->y_dir -= step;

    if (std::fabs(cam->z_dir - tz) <= step) { cam->z_dir = tz; done_z = true; }
    else if (cam->z_dir < tz) cam->z_dir += step;
    else cam->z_dir -= step;

    return done_x && done_y && done_z;
}

bool neutron::move_camera(camera* cam, float tx, float ty, float tz, int spd) {
    if (!cam) return true;
    bool done_x = false, done_y = false, done_z = false;
    float step = (float)spd * 0.1f;

    if (std::fabs(cam->x - tx) <= step) { cam->x = tx; done_x = true; }
    else if (cam->x < tx) cam->x += step;
    else cam->x -= step;

    if (std::fabs(cam->y - ty) <= step) { cam->y = ty; done_y = true; }
    else if (cam->y < ty) cam->y += step;
    else cam->y -= step;

    if (std::fabs(cam->z - tz) <= step) { cam->z = tz; done_z = true; }
    else if (cam->z < tz) cam->z += step;
    else cam->z -= step;

    return done_x && done_y && done_z;
}

void neutron::create_miniatures() {
    for (int i = 0; i < 25; i++) {
        if (!miniatures[i]) {
            int v_pos = i / 5;
            int h_pos = i % 5;
            miniatures[i] = new miniature(LEVEL_TABLE[i].data, LEVEL_TABLE[i].cubes,
                                          0, -16 * v_pos, 16 * h_pos, LEVEL_TABLE[i].name);

            miniature* m = miniatures[i];
            m->cam[0] = m->max_vertex[0] - 14;
            if (i < 5)       m->cam[1] = 4;
            else if (i < 10) m->cam[1] = -12;
            else if (i < 15) m->cam[1] = -28;
            else if (i < 20) m->cam[1] = -42;
            else             m->cam[1] = -56;
            m->cam[2] = m->cam_center[2];
        }
    }
}

void neutron::load_miniatures() {
    create_miniatures();
    actual_miniature = 0;
    if (miniatures[0]) {
        game_cam->update_pos(miniatures[0]->cam[0], miniatures[0]->cam[1], miniatures[0]->cam[2],
                             (float)miniatures[0]->cam_center[0], (float)miniatures[0]->cam_center[1],
                             (float)miniatures[0]->cam_center[2]);
        miniatures[0]->color[3] = 255;
    }
}

void neutron::on_miniature_change() {
    if (actual_miniature >= 0 && actual_miniature < 25 && miniatures[actual_miniature]) {
        miniature* m = miniatures[actual_miniature];
        move_camera(game_cam, (float)m->cam[0], (float)m->cam[1], (float)m->cam[2], 2);
        move_lookat(game_cam, (float)m->cam_center[0], (float)m->cam_center[1], (float)m->cam_center[2], 2);
        m->color[3] = 255;
    }
}

void neutron::show_miniatures() {
    speed = 30.0f;
    for (int i = 0; i < 25; i++) {
        if (miniatures[i]) {
            miniatures[i]->rotation = (miniatures[i]->rotation + 1) % 360;
        }
    }
    on_miniature_change();
}

void neutron::show_stage() {
    speed = 30.0f;
    if (ground && ground->status == 1) {
        game_status = 0;
    }
}

void neutron::wait_option() {
    speed = 30.0f;
}

void neutron::level_complete() {
    speed = 30.0f;
    if (game_status != 8) {
        score += 1000;
        std::snprintf(score_buffer, sizeof(score_buffer), "%06d", score);
        if (interface && interface->buttons[1]) {
            interface->buttons[1]->load_text(score_buffer, -1);
        }
        if (mixer) mixer->play_effect(1);
        game_status = 8; // Stage Complete state
    }

    // Victory camera zoom-out animation
    if (proton && proton->cam_ship) {
        move_camera(proton->cam_ship, -20.0f, -20.0f, -20.0f, 1);
    }
}

void neutron::stage_over() {
    speed = 30.0f;
    if (actual_stage != 2000) {
        if (interface && interface->buttons[26]) {
            interface->buttons[26]->visible = true;
            interface->buttons[26]->enabled = true;
        }
    }
}

void neutron::next_level() {
    actual_stage++;
    int max_stg = (interface ? interface->max_stages : 25);
    if (actual_stage > max_stg) {
        actual_stage = 1;
        game_status = 13; // Staff credits
        return;
    }
    change_stage(actual_stage, true);
    game_status = 5;
}

void neutron::are_you_sure() {
    speed = 30.0f;
}

void neutron::show_staff() {
    speed = 30.0f;
    time++;
    if (time > 100) {
        time = 0;
        if (interface) {
            interface->actual_staff = (interface->actual_staff + 1) % 6;
            if (interface->buttons[38]) interface->buttons[38]->load_text(interface->whats[interface->actual_staff], 200);
            if (interface->buttons[39]) interface->buttons[39]->load_text(interface->whos[interface->actual_staff], 200);
        }
    }
}

void neutron::step_tutorial() {
    tutorial_step++;
    tutorial_counter = 0;
    if (tutorial_step > 5) {
        isTutorial = false;
        game_status = 0;
    }
}

void neutron::tutorial() {
    speed = 30.0f;
    tutorial_counter++;
    if (tutorial_counter > 150) {
        step_tutorial();
    }
}

void neutron::game_intro() {
    speed = 30.0f;
    intro_step++;
    if (intro_step >= 94) {
        intro_step = 93;
        game_status = 6;
        int active_btns[4] = {7, 8, 9, 10};
        if (interface) interface->activate_buttons(active_btns, 4);
    }
}

void neutron::print_stars() {
    glPushMatrix();
    for (int i = 0; i < 50; i++) {
        float sx = (float)background_colors[i][0];
        float sy = (float)background_colors[i][1];
        float sz = (float)background_colors[i][2];
        int r = background_colors[i][3];
        int g = background_colors[i][4];
        int b = background_colors[i][5];
        float sz_val = (float)background_colors[i][6] * 0.1f;
        game_cam->locator->solid_cube(sx, sy, sz, r, g, b, 200, sz_val);
    }
    glPopMatrix();
}

void neutron::print_background() {
    print_stars();
}

void neutron::delay(int next_stat, int dtime) {
    speed = 30.0f;
    time--;
    if (time <= 0) {
        time = 10;
        game_status = next_stat;
    }
}

void neutron::delay_artifact(int next_stat, int dtime) {}
void neutron::execute_artifact() {}
void neutron::load_artifact(int artifact) {}
void neutron::change_option() { speed = 30.0f; }

void neutron::menu_move(int side) {
    if (!interface) return;
    if (side == 0) { // Up
        if (interface->actual_option < 4) {
            if (interface->actual_option > 0) interface->actual_option--;
            else interface->actual_option = 3;
        } else if (interface->actual_option < 8) {
            if (interface->actual_option > 4) interface->actual_option--;
            else interface->actual_option = 7;
        } else {
            if (interface->actual_option > 8) interface->actual_option--;
            else interface->actual_option = 11;
        }
    } else if (side == 1) { // Down
        if (interface->actual_option < 4) {
            if (interface->actual_option < 3) interface->actual_option++;
            else interface->actual_option = 0;
        } else if (interface->actual_option < 8) {
            if (interface->actual_option < 7) interface->actual_option++;
            else interface->actual_option = 4;
        } else {
            if (interface->actual_option < 11) interface->actual_option++;
            else interface->actual_option = 8;
        }
    } else if (side == 2) { // Back
        if (interface->actual_option >= 4 && interface->actual_option < 8) {
            interface->actual_option = 0;
        } else if (interface->actual_option >= 8) {
            interface->actual_option = 1;
        }
    } else if (side == 3) { // Forward / Select
        if (interface->actual_option == 0) interface->actual_option = 4;
        else if (interface->actual_option == 1) interface->actual_option = 8;
    }

    if (ground && ground->map[interface->menu_option[interface->actual_option]]) {
        plane* p = ground->map[interface->menu_option[interface->actual_option]];
        move_lookat(ground->cam_terrain, (float)p->a_vertex[0], (float)p->a_vertex[1], (float)p->a_vertex[2], 2);
    }
    if (mixer) mixer->play_effect(2);
}

void neutron::load_new_enemy() {
    if (N_SHIPS < 30) {
        enemy* e = new enemy();
        e->ident = N_SHIPS;
        e->actual_plane = std::rand() % (ground ? ground->N_PLANES : 1);
        electron[N_SHIPS - 1] = e;
        N_SHIPS++;
        n_enemies++;
        std::snprintf(enemies_buffer, sizeof(enemies_buffer), "%02d", n_enemies);
        if (interface && interface->buttons[3]) interface->buttons[3]->load_text(enemies_buffer, -1);
    }
}

void neutron::autoload_eletrons(int count) {
    if (!ground || ground->N_PLANES < 1) return;
    int spawned = 0;
    for (int i = 0; i < count && spawned < 29; i++) {
        int pl = (i + 1) % ground->N_PLANES;
        plane* p = ground->map[pl];
        if (!p) {
            for (int k = 0; k < ground->N_PLANES; k++) {
                if (ground->map[k]) {
                    pl = k;
                    p = ground->map[k];
                    break;
                }
            }
        }
        if (!p) continue;

        enemy* e = new enemy();
        e->ident = spawned + 1;
        int rank = (spawned % 4);
        e->type = rank;

        // Distinct colors for enemy ranks
        if (rank == 0) {
            e->material[0] = 220; e->material[1] = 30;  e->material[2] = 30;  e->material[3] = 255; // Red
        } else if (rank == 1) {
            e->material[0] = 240; e->material[1] = 140; e->material[2] = 20;  e->material[3] = 255; // Amber
        } else if (rank == 2) {
            e->material[0] = 160; e->material[1] = 0;   e->material[2] = 80;  e->material[3] = 255; // Crimson
        } else {
            e->material[0] = 180; e->material[1] = 40;  e->material[2] = 220; e->material[3] = 255; // Purple
        }

        e->start_plane = pl;
        e->actual_plane = pl;
        e->start_actual_face = p->face_number;
        e->start_previous_face = p->face_number;
        e->actual_face = p->face_number;
        e->previous_face = p->face_number;

        int mid_idx = (p->side_size * p->side_size) / 2;
        piece* mp = p->pieces[mid_idx];
        if (mp) {
            e->x = mp->x;
            e->y = mp->y;
            e->z = mp->z;
        } else {
            float mid = (float)p->side_size * 0.5f;
            e->x = p->pieces[0]->x + mid;
            e->y = p->pieces[0]->y + mid;
            e->z = p->pieces[0]->z + mid;
            if (p->dead_axis == 1) e->x = (float)p->dead_offset;
            if (p->dead_axis == 2) e->y = (float)p->dead_offset;
            if (p->dead_axis == 3) e->z = (float)p->dead_offset;
        }

        e->start_x = (int)e->x;
        e->start_y = (int)e->y;
        e->start_z = (int)e->z;
        int idx = p->find_piece_index(e->x, e->y, e->z);
        if (idx >= 0 && p->pieces[idx]) {
            p->pieces[idx]->restart();
        }
        e->restart();
        e->status = 0; // Sleeping! Waiting for player to hit this trigger tile
        e->init(ground->map);

        electron[spawned] = e;
        spawned++;
    }
    N_SHIPS = spawned + 1;
    n_enemies = spawned;
    uncharged_electrons = 0;
    charged_electrons = 0;
    std::snprintf(enemies_buffer, sizeof(enemies_buffer), "%02d", n_enemies);
    if (interface && interface->buttons[3]) interface->buttons[3]->load_text(enemies_buffer, -1);
}

void neutron::load_proton(bool setup_cams) {
    if (!ground) return;
    delete proton;
    proton = new player();
    proton->change_color(user_color);

    int start_pl = ground->start_on_plane;
    if (start_pl < 0 || start_pl >= ground->N_PLANES) start_pl = 0;
    plane* p = ground->map[start_pl];

    proton->start_plane = start_pl;
    proton->actual_plane = start_pl;
    proton->start_actual_face = p->face_number;
    proton->start_previous_face = (actual_stage == 1000) ? p->face_number + 1 : p->face_number - 2;
    proton->start_x = (int)p->pieces[0]->x + p->side_size / 2;
    proton->start_y = (int)p->pieces[0]->y + p->side_size / 2;
    proton->start_z = (int)p->pieces[0]->z + p->side_size / 2;
    proton->restart();
    proton->init(ground->map);
}

void neutron::unload_stage() {
    delete ground;
    ground = nullptr;
    delete proton;
    proton = nullptr;
    for (int i = 0; i < 30; i++) {
        delete electron[i];
        electron[i] = nullptr;
    }
    N_SHIPS = 1;
    n_enemies = 0;
}

void neutron::load_stage(int stg, bool setup_cams) {
    actual_stage = stg;
    game_mode = 0;
    unload_stage();

    ground = new terrain();
    if (stg == 1000) {
        ground->create_from_array(14, MENU_DATA);
        ground->init(0, 0, 0, 10);
        ground->status = 1;
        N_SHIPS = 1;
        load_proton(setup_cams);
    } else if (stg == 2000) {
        create_miniatures();
        ground->create_from_miniature(miniatures[0]);
        ground->init(0, 0, 0, 10);
        ground->status = 4;
        load_proton(setup_cams);
    } else {
        create_miniatures();
        int idx = stg - 1;
        if (idx < 0) idx = 0;
        if (idx >= 25) idx = 24;
        ground->create_from_miniature(miniatures[idx]);
        ground->init(0, 0, 0, 10);
        ground->status = 4;
        load_proton(setup_cams);
        autoload_eletrons(user_difficulty == 0 ? 2 : 4);
        if (interface && interface->buttons[6]) {
            interface->buttons[6]->load_text(LEVEL_TABLE[idx].name, -1);
        }
    }
}

int neutron::change_stage(int stg, bool setup_cams) {
    load_stage(stg, setup_cams);
    if (stg == 1000) return 6;
    if (stg == 2000) return 0;
    return 5;
}

int neutron::navigate() {
    if (!proton || !ground) return game_status;
    proton->move(ground->map, game_mode);

    for (int i = 0; i < N_SHIPS - 1; i++) {
        enemy* e = electron[i];
        if (e && e->actual_plane == proton->actual_plane && e->status == 0) {
            game_mode = 1; // Combat mode!
            if (interface && interface->buttons[6]) interface->buttons[6]->load_text("combat mode", 255);
            ground->combat_mode();
            // When enemy is triggered, turn the terrain outline RED!
            ground->change_color(255, 30, 30, 255);
            e->status = 1;
            int idx = ground->map[e->actual_plane]->find_piece_index(e->x, e->y, e->z);
            if (idx >= 0 && ground->map[e->actual_plane]->pieces[idx]) {
                ground->map[e->actual_plane]->pieces[idx]->state = false;
                ground->map[e->actual_plane]->pieces[idx]->owner = -2;
            }
            last_charged_electron = i;
            charged_electrons++;
            if (mixer) mixer->play_effect(5);
            break;
        }
    }
    return game_status;
}

int neutron::combat() {
    if (!proton || !ground) return game_status;

    int p_res = proton->move(ground->map, game_mode);
    if (p_res != 0) { // Collision!
        colision = 1;
        proton->status = 6;
        if (mixer) mixer->play_effect(6);
        game_status = 1; // Stage over
        return game_status;
    }

    for (int i = 0; i < N_SHIPS - 1; i++) {
        enemy* e = electron[i];
        if (e && e->status == 1) {
            int e_res = e->move(ground->map, proton->actual_face);
            if (e_res != 0) {
                e->status = 6;
                ground->fire_up_cue(e->ident);
                if (mixer) mixer->play_effect(4);
                score += 100;
                std::snprintf(score_buffer, sizeof(score_buffer), "%06d", score);
                if (interface && interface->buttons[1]) interface->buttons[1]->load_text(score_buffer, -1);

                n_enemies--;
                if (n_enemies < 0) n_enemies = 0;
                std::snprintf(enemies_buffer, sizeof(enemies_buffer), "%02d", n_enemies);
                if (interface && interface->buttons[3]) interface->buttons[3]->load_text(enemies_buffer, -1);

                if (n_enemies == 0) {
                    ground->change_color(255, 255, 255, 255);
                    level_complete();
                    return game_status;
                } else {
                    bool active_remain = false;
                    for (int j = 0; j < N_SHIPS - 1; j++) {
                        if (electron[j] && electron[j]->status == 1) {
                            active_remain = true;
                            break;
                        }
                    }
                    if (!active_remain) {
                        game_mode = 0;
                        ground->navigation_mode();
                        // Reset outline to white in navigation mode
                        ground->change_color(255, 255, 255, 255);
                    }
                }
            }
        }
    }
    return game_status;
}

int neutron::run() {
    if (game_mode == 1) {
        return combat();
    }
    return navigate();
}

void neutron::restart() {
    if (ground) ground->restart();
    if (proton) proton->restart();
    for (int i = 0; i < N_SHIPS - 1; i++) {
        if (electron[i]) electron[i]->restart();
    }
}

void neutron::print_stage() {
    if (actual_stage == 1000) {
        if (game_cam) {
            game_cam->x = 10.0f;
            game_cam->y = 15.0f;
            game_cam->z = 28.0f;
            game_cam->x_dir = 10.0f;
            game_cam->y_dir = 10.0f;
            game_cam->z_dir = 10.0f;
            game_cam->x_ori = 0.0f;
            game_cam->y_ori = 1.0f;
            game_cam->z_ori = 0.0f;
            game_cam->locate();
        }
    } else if (actual_stage == 3000) {
        if (game_cam) game_cam->locate();
    } else {
        if (vision == 0 && proton && proton->cam_ship) {
            proton->cam_ship->locate();
        } else if (vision == 1 && ground) {
            // Isometric Arena Overview
            float cx = (float)ground->max_vertex[0] * 0.5f;
            float cy = (float)ground->max_vertex[1] * 0.5f;
            float cz = (float)ground->max_vertex[2] * 0.5f;
            float max_dim = (float)ground->max_vertex[0];
            if ((float)ground->max_vertex[1] > max_dim) max_dim = (float)ground->max_vertex[1];
            if ((float)ground->max_vertex[2] > max_dim) max_dim = (float)ground->max_vertex[2];
            if (max_dim < 10.0f) max_dim = 10.0f;

            float dist = max_dim * 1.6f;
            if (game_cam) {
                game_cam->x = cx;
                game_cam->y = cy + dist * 0.85f;
                game_cam->z = cz + dist * 1.05f;
                game_cam->x_dir = cx;
                game_cam->y_dir = cy;
                game_cam->z_dir = cz;
                game_cam->x_ori = 0.0f;
                game_cam->y_ori = 1.0f;
                game_cam->z_ori = 0.0f;
                game_cam->locate();
            }
        }
    }

    print_stars();

    if (actual_stage == 3000) {
        for (int i = 0; i < 25; i++) {
            if (miniatures[i]) miniatures[i]->print();
        }
    }

    if (ground) {
        ground->print(true, proton ? proton->actual_plane : 0, low_quality);
    }

    if (proton) {
        proton->print(0, low_quality);
        if ((user_difficulty == 0 || actual_stage == 2000) && proton->status == 1 && ground) {
            plane* pp = ground->map[proton->actual_plane];
            if (pp) {
                proton->print_guides(pp->face_number, pp->dead_axis);
            }
        }
    }

    // Render Enemies and Sleeping Enemy Trigger Tiles
    for (int i = 0; i < N_SHIPS - 1; i++) {
        enemy* e = electron[i];
        if (!e) continue;
        if (e->status == 0) {
            // SLEEPING ENEMY TRIGGER TILE ON THIS PLANE
            if (ground && e->actual_plane >= 0 && e->actual_plane < ground->N_PLANES) {
                plane* ep = ground->map[e->actual_plane];
                if (ep && e->printer) {
                    // Translucent trigger tile sitting 100% flush on the plane face (polygon offset prevents z-fighting)
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(-2.0f, -2.0f);
                    glBegin(GL_QUADS);
                    glColor4ub((GLubyte)e->material[0], (GLubyte)e->material[1], (GLubyte)e->material[2], 160);
                    glVertex3f((float)ep->a_vertex[0], (float)ep->a_vertex[1], (float)ep->a_vertex[2]);
                    glVertex3f((float)ep->b_vertex[0], (float)ep->b_vertex[1], (float)ep->b_vertex[2]);
                    glVertex3f((float)ep->d_vertex[0], (float)ep->d_vertex[1], (float)ep->d_vertex[2]);
                    glVertex3f((float)ep->c_vertex[0], (float)ep->c_vertex[1], (float)ep->c_vertex[2]);
                    glEnd();
                    glDisable(GL_POLYGON_OFFSET_FILL);

                    // Glowing border in the enemy rank color
                    glLineWidth(3.5f);
                    e->printer->line((float)ep->a_vertex[0], (float)ep->a_vertex[1], (float)ep->a_vertex[2],
                                     (float)ep->b_vertex[0], (float)ep->b_vertex[1], (float)ep->b_vertex[2], e->material);
                    e->printer->line((float)ep->b_vertex[0], (float)ep->b_vertex[1], (float)ep->b_vertex[2],
                                     (float)ep->d_vertex[0], (float)ep->d_vertex[1], (float)ep->d_vertex[2], e->material);
                    e->printer->line((float)ep->d_vertex[0], (float)ep->d_vertex[1], (float)ep->d_vertex[2],
                                     (float)ep->c_vertex[0], (float)ep->c_vertex[1], (float)ep->c_vertex[2], e->material);
                    e->printer->line((float)ep->c_vertex[0], (float)ep->c_vertex[1], (float)ep->c_vertex[2],
                                     (float)ep->a_vertex[0], (float)ep->a_vertex[1], (float)ep->a_vertex[2], e->material);

                    // Sleeping enemy cycle model perched right on the face
                    float out_x = e->x, out_y = e->y, out_z = e->z;
                    if (ep->dead_axis == 1) out_x += 0.2f * (float)ep->axis_polarity;
                    if (ep->dead_axis == 2) out_y += 0.2f * (float)ep->axis_polarity;
                    if (ep->dead_axis == 3) out_z += 0.2f * (float)ep->axis_polarity;
                    e->printer->solid_cube(out_x, out_y, out_z, e->material[0], e->material[1], e->material[2], 255, 0.4f);
                    e->printer->cube_skeleton(out_x, out_y, out_z, 255, 255, 255, 255, 0.4f);
                }
            }
        } else {
            // Active enemy cycle and its combat trail
            if (ground && e->actual_plane >= 0 && e->actual_plane < ground->N_PLANES) {
                e->print(ground->map[e->actual_plane]->dead_axis, low_quality);
            }
        }
    }
}

void neutron::print() {
    if (actual_stage == 1000) {
        if (game_status == 1000 || game_status == 4) {
            if (game_cam) game_cam->locate();
            if (interface) interface->print_intro(intro_step);
            return;
        }
        print_stage();
        if (interface) {
            if (game_status == 13) {
                interface->print_staff(actual_stage);
            } else {
                interface->print_menu_interface(actual_stage);
            }
        }
        return;
    } else if (actual_stage == 2000) {
        print_stage();
        if (interface) interface->print_train_interface(actual_stage);
        return;
    } else if (actual_stage == 3000) {
        print_stage();
        if (interface) interface->print_solo_interface(actual_stage);
        return;
    }
    print_stage();
    if (interface) interface->print_game_interface(actual_stage, game_status, colision);
}

void neutron::main_loop() {
    speed = 30.0f;
    switch (game_status) {
        case 0:
            speed = (user_difficulty == 1) ? 55.0f : 75.0f;
            run();
            break;
        case 1:  stage_over(); break;
        case 3:  next_level(); break;
        case 4:  delay(go_status, time); break;
        case 5:  show_stage(); break;
        case 6:  wait_option(); break;
        case 8:  level_complete(); break;
        case 9:  show_miniatures(); break;
        case 10: tutorial(); break;
        case 11: change_option(); break;
        case 12: are_you_sure(); break;
        case 13: show_staff(); break;
        case 1000: game_intro(); break;
    }
}

void neutron::execute_action(int action) {
    if (action < 0) return;

    switch (action) {
        case 0:
            if (proton) proton->change_direction(0); // Left
            break;
        case 1:
            if (proton) proton->change_direction(1); // Right
            break;
        case 2:
            // "Game" / Start Game
            user_stage = 1;
            change_stage(1, true);
            game_status = 0;
            break;
        case 3:
            menu_move(3);
            break;
        case 5: // Train
            change_stage(2000, true);
            game_status = 0;
            break;
        case 6: // Menu up
            menu_move(0);
            break;
        case 7: // Menu down
            menu_move(1);
            break;
        case 8: // Back to menu
            change_stage(1000, true);
            game_status = 6;
            break;
        case 9: // Load new enemy in training
            load_new_enemy();
            break;
        case 10: // New Game
            user_stage = 1;
            change_stage(1, true);
            game_status = 0;
            break;
        case 11: // Play user stage
            change_stage(user_stage, true);
            game_status = 5;
            break;
        case 12: // Stage select
            actual_stage = 3000;
            load_miniatures();
            game_status = 9;
            break;
        case 13: // Menu back
            menu_move(2);
            break;
        case 14: // Options back
            menu_move(2);
            break;
        case 15: // Color change
            user_color = (user_color + 1) % 3;
            if (proton) proton->change_color(user_color);
            break;
        case 16: // Difficulty toggle
            user_difficulty = 1 - user_difficulty;
            if (interface && interface->buttons[23]) {
                interface->buttons[23]->load_text(user_difficulty ? "hard" : "easy", -1);
            }
            break;
        case 17: // Sound toggle
            user_sound = !user_sound;
            if (interface && interface->buttons[25]) {
                interface->buttons[25]->load_text(user_sound ? "on" : "off", -1);
            }
            break;
        case 18: // Try again
            change_stage(actual_stage, true);
            game_status = 0;
            break;
        case 19: // Prev miniature
            if (actual_miniature > 0) actual_miniature--;
            else actual_miniature = 24;
            on_miniature_change();
            break;
        case 20: // Next miniature
            if (actual_miniature < 24) actual_miniature++;
            else actual_miniature = 0;
            on_miniature_change();
            break;
        case 21: // Play selected miniature
            user_stage = actual_miniature + 1;
            change_stage(user_stage, true);
            game_status = 0;
            break;
        case 23: // Camera up
            break;
        case 24: // Camera down
            break;
        case 25: // Confirm Yes
            change_stage(1000, true);
            game_status = 6;
            break;
        case 26: // Confirm No
            game_status = 0;
            break;
        case 27: // Staff / Credits
            game_status = 13;
            break;
    }
}
