#include "neutron.h"
#include "menu_battle.h"
#include "split_screen_menu.h"
#include "split_screen_match.h"
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

neutron::neutron(int color, int difficulty, int stage, bool sound, int volume, bool fullscreen)
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
      fullscreen(fullscreen),
      isTutorial(false),
      tutorial_step(0),
      tutorial_counter(0),
      time(10),
      camera_passes(0),
      win_cam_phase(0),
      win_cam_tx(0.0f), win_cam_ty(0.0f), win_cam_tz(0.0f),
      stage_over_done(false),
      intro_cam_tx(0.0f), intro_cam_ty(0.0f), intro_cam_tz(0.0f),
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
      new_high_score(false),
      unlock_all_stages(false),
      paused(false),
      isSolo(false),
      intro_step(0),
      intro_counter(0),
      intro_substep(0),
      vision(0),
      prints(0),
      low_quality(false),
      actual_artifact(0) {

    std::memset(electron, 0, sizeof(electron));
    menu_battle_respawn[0] = -1;
    menu_battle_respawn[1] = -1;
    std::memset(miniatures, 0, sizeof(miniatures));
    std::strcpy(enemies_buffer, "00");
    std::strcpy(score_buffer, "000000");

    // Reflects the optional 25+1 (custom map.txt) count into the UI's own
    // stage total -- Stage Select's title text and next_level()'s "wrap back
    // to stage 1 after the last one" both read interface->max_stages.
    if (interface) interface->max_stages = total_stage_count();

    if (mixer) {
        mixer->set_enabled(sound);
        mixer->set_volume(volume);
    }

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
    for (int i = 0; i < total_stage_count(); i++) {
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
    for (int i = 0; i < total_stage_count(); i++) {
        if (!miniatures[i]) {
            int v_pos = i / 5;
            int h_pos = i % 5;
            if (i < 25) {
                miniatures[i] = new miniature(LEVEL_TABLE[i].data, LEVEL_TABLE[i].cubes,
                                              0, -16 * v_pos, 16 * h_pos, LEVEL_TABLE[i].name);
            } else {
                // Index 25: optional custom map.txt stage (see custom_stage.h).
                miniatures[i] = new miniature(g_custom_stage.cubes, g_custom_stage.cube_count,
                                              0, -16 * v_pos, 16 * h_pos, g_custom_stage.name);
            }

            miniature* m = miniatures[i];
            m->cam[0] = m->max_vertex[0] - 14;
            if (i < 5)       m->cam[1] = 4;
            else if (i < 10) m->cam[1] = -12;
            else if (i < 15) m->cam[1] = -28;
            else if (i < 20) m->cam[1] = -42;
            else if (i < 25) m->cam[1] = -56;
            else             m->cam[1] = -70; // custom stage gets its own row
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
    // buttons[6] is a shared HUD text slot (also used for "navigation mode",
    // "you lose", etc. during gameplay) -- stamp it with the first stage's name
    // so Stage Select doesn't show whatever text was left over from before.
    if (interface && interface->buttons[6]) {
        interface->buttons[6]->load_text(LEVEL_TABLE[0].name, -1);
    }
}

void neutron::on_miniature_change() {
    if (actual_miniature >= 0 && actual_miniature < total_stage_count() && miniatures[actual_miniature]) {
        miniature* m = miniatures[actual_miniature];
        move_camera(game_cam, (float)m->cam[0], (float)m->cam[1], (float)m->cam[2], 2);
        move_lookat(game_cam, (float)m->cam_center[0], (float)m->cam_center[1], (float)m->cam_center[2], 2);
        m->color[3] = 255;
        if (interface && interface->buttons[6]) {
            interface->buttons[6]->load_text(stage_display_name(actual_miniature), -1);
        }
    }
}

void neutron::show_miniatures() {
    speed = 30.0f;
    for (int i = 0; i < total_stage_count(); i++) {
        if (miniatures[i]) {
            miniatures[i]->rotation = (miniatures[i]->rotation + 1) % 360;
        }
    }
    on_miniature_change();
}

void neutron::show_stage() {
    // neutron::show_stage @0x0890dbd8: 3-phase camera_passes flythrough into the
    // stage, then handoff into the same materialize+pause as change_stage()'s
    // !show_it path (status=3, time=25, go_status=0 -> game_status=4).
    speed = 30.0f;
    if (!proton || !proton->cam_ship) {
        game_status = 0;
        if (proton) proton->status = 1;
        return;
    }

    proton->status = 3;
    // Keep the camera looking at the player throughout the flythrough -- the player
    // isn't moving yet (status==3 pauses input), so this can just track directly.
    proton->cam_ship->x_dir = proton->x;
    proton->cam_ship->y_dir = proton->y;
    proton->cam_ship->z_dir = proton->z;

    bool done;
    float pull = intro_flythrough_distance();
    // speed=1 confirmed via disassembly (every move_camera call in the real
    // show_stage/level_complete/stage_over uses the literal speed arg 1).
    if (camera_passes == 0) {
        done = move_camera(proton->cam_ship, intro_cam_tx - pull * 0.47f, intro_cam_ty + pull * 0.47f, intro_cam_tz - pull * 0.47f, 1);
        if (done) camera_passes = 1;
    } else if (camera_passes == 1) {
        done = move_camera(proton->cam_ship, intro_cam_tx - pull * 0.13f, intro_cam_ty + pull * 0.13f, intro_cam_tz - pull * 0.13f, 1);
        if (done) camera_passes = 2;
    } else {
        done = move_camera(proton->cam_ship, intro_cam_tx, intro_cam_ty, intro_cam_tz, 1);
        if (done) {
            time = 25;
            // neutron::load_stage @0x0890fc90 (stage==1, isTutorial branch): the
            // materialize delay lands on game_status 10 (tutorial, dispatched by
            // neutron::tutorial()) instead of 0 (normal navigate()) when the New
            // Game flow set isTutorial -- see execute_action cases 2/10.
            go_status = isTutorial ? 10 : 0;
            proton->effect_size = 3.0f;
            if (ground) ground->change_color(100, 100, 100, 255);
            if (mixer) mixer->play_effect(11);
            if (interface && interface->buttons[6]) {
                interface->buttons[6]->load_text(
                    isTutorial ? "welcome to neutrons tutorial" : "navigation mode", 255);
            }
            game_status = 4;
        }
    }
}

void neutron::age_terrain_pieces() {
    // piece::print @0x08908f9c's temporal branch increments prints_counter by
    // 1.0 every call in decomp; this port's print() runs at uncapped render
    // rate instead of once per logic frame (see main.cpp), so that increment
    // has to live here -- called once per logic tick, unconditionally, from
    // the top of main_loop() -- to match the original's ~6-tick cadence
    // instead of completing in a fraction of a tick.
    //
    // plane::print() only ever calls piece::print() -- where decomp's
    // increment lives -- for pieces where (state || status==2): pieces
    // actually claimed, or mid-fade after dying. Never-claimed pieces sitting
    // idle never got print() called on them in decomp, so never got
    // incremented either. This loop has to apply that exact same gate, or
    // every untouched piece on the whole terrain ages from the moment the
    // level loads, so a piece the player finally drives onto has already
    // blown through the 6.0 threshold and expires on its very first frame --
    // which is exactly what broke the navigation trail entirely just now.
    if (!ground) return;
    for (int i = 0; i < ground->N_PLANES; i++) {
        plane* p = ground->map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        if (total > 400) total = 400;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc && pc->temporal && (pc->state || pc->status == 2)) pc->prints_counter += 1.0;
        }
    }
}

void neutron::wait_option() {
    speed = 30.0f;
    // neutron::wait_option @0x0890c024: this, not change_option(), is the
    // dispatch the menu actually lands on and then sits at indefinitely after
    // the intro finishes (game_intro()/the manual skip-intro key handler both
    // set game_status=6) -- change_option()'s game_status==11 only happens
    // transiently, right when change_stage(1000,...) is called (e.g.
    // "back to menu" from a submenu). The menu battle needs to tick from
    // whichever of the two the game is actually sitting in, or it never
    // starts moving until the player happens to visit and leave a submenu.
    menu_battle::update(this);
}

void neutron::level_complete(enemy* last_enemy) {
    speed = 30.0f;
    if (game_status != 8) {
        // neutron::stage_over @0x0890f380 (win path): score is whatever's left of the
        // per-stage countdown, not a flat bonus -- compare it against that stage's
        // best and only overwrite on improvement.
        new_high_score = false;
        int idx = actual_stage - 1;
        if (idx >= 0 && idx < total_stage_count() && miniatures[idx]) {
            if (miniatures[idx]->high_score < score) {
                miniatures[idx]->high_score = score;
                new_high_score = true;
                save_progress();
            }
        }
        if (interface && interface->buttons[6]) {
            interface->buttons[6]->load_text(new_high_score ? "new high score" : "stage clear", 255);
        }
        if (mixer) mixer->play_effect(new_high_score ? 9 : 8);
        game_status = 8; // Stage Complete state

        // neutron::combat @0x0890de9c (win branch): pan the camera to look at the
        // last enemy's death position instead of wherever the player was facing,
        // then zoom in on it before pulling back out. stage_over() drives the
        // actual per-frame camera animation every tick afterward (this only runs
        // once, on the frame the win is detected).
        win_cam_phase = 0;
        stage_over_done = false;
        if (proton && proton->cam_ship && last_enemy) {
            win_cam_tx = last_enemy->x;
            win_cam_ty = last_enemy->y;
            win_cam_tz = last_enemy->z;
            proton->cam_ship->x_dir = win_cam_tx;
            proton->cam_ship->y_dir = win_cam_ty;
            proton->cam_ship->z_dir = win_cam_tz;
        }
    }
}

void neutron::stage_over() {
    // neutron::stage_over @0x0890f380: called every tick while game_status is 1 or
    // 8 (main_loop's dispatch), so the camera animation here runs continuously;
    // the one-shot "show result UI" step only fires once the camera's settled
    // (stage_over_done guards it from re-firing every subsequent tick).
    speed = 30.0f;
    if (actual_stage != 2000) {
        if (colision == 1) {
            bool done = true;
            if (proton && proton->cam_ship) {
                // (-20,-20,-20) confirmed via disassembly of level_complete()'s
                // move_camera call -- a literal absolute target, not computed from
                // player/stage position. Earlier this was "fixed" to be relative to
                // the player on the theory that every stage past the first sits at
                // its own world offset (create_miniatures()'s -16*v_pos/16*h_pos
                // layout) -- that offset turns out to only ever apply to the
                // stage-select preview model (miniature::convert_to_plane(), which
                // builds the real gameplay terrain, never reads this->offset at
                // all), so actual gameplay terrain is origin-centered for every
                // stage and the absolute constant is correct everywhere.
                done = move_camera(proton->cam_ship, -20.0f, -20.0f, -20.0f, 1);
            }
            if (done && !stage_over_done) {
                stage_over_done = true;
                int active_btns[2] = {26, 13};
                if (interface) {
                    interface->activate_buttons(active_btns, 2);
                    if (interface->buttons[26]) {
                        interface->buttons[26]->visible = true;
                        interface->buttons[26]->enabled = true;
                        interface->buttons[26]->load_text("try again", -1);
                    }
                }
                if (mixer) {
                    mixer->play_effect(6);
                }
            }
        } else {
            // Win: zoom in close on the last enemy's death spot, then pull back
            // out before showing the result UI.
            bool done = true;
            if (proton && proton->cam_ship) {
                if (win_cam_phase == 0) {
                    done = move_camera(proton->cam_ship, win_cam_tx - 3.0f, win_cam_ty + 2.0f, win_cam_tz - 3.0f, 1);
                    if (done) win_cam_phase = 1;
                    done = false;
                } else if (win_cam_phase == 1) {
                    done = move_camera(proton->cam_ship, -20.0f, -20.0f, -20.0f, 1);
                }
            }
            if (done && !stage_over_done) {
                stage_over_done = true;
                // (high-score text/sound already fired once in level_complete()
                // when the win was first detected; this just unlocks the "next
                // stage" / "back" buttons once the camera's done moving.)
                int active_btns[2] = {16, 13};
                if (interface) {
                    interface->activate_buttons(active_btns, 2);
                }
            }
        }
        game_status = 8;
        return;
    }

    // neutron::stage_over @0x0890f380 (actual_stage==2000 branch): training has no
    // win/lose outcome -- whether the player or the lone training enemy died, this
    // waits for its death animation (status==6, decremented every frame over in
    // player::print()/enemy::print()) to finish, then silently clears the enemy and
    // drops back into navigation mode so [ENTER] can spawn another one. Never
    // touches game_status 8 (the stage-clear/game-over UI), so no "you win" screen
    // and no routing into next_level().
    if (colision == 1) {
        if (proton && proton->status == 6) return;
    } else if (colision == 2) {
        if (electron[0] && electron[0]->status == 6) return;
    }

    for (int i = 0; i < N_SHIPS - 1; i++) {
        delete electron[i];
        electron[i] = nullptr;
    }
    N_SHIPS = 1;
    if (ground) {
        ground->navigation_mode();
        ground->change_color(ground->material[0], ground->material[1], ground->material[2], 255);
    }
    // game_mode = 0 here (not decomp's literal "2") -- this port's player::move()
    // only treats mode==0 as "navigation" (advances each temporal trail piece's
    // fade-out countdown, no collision checks); every other navigation-mode reset
    // in this file (load_new_enemy(), the mid-combat "last active enemy down"
    // path in run()) already uses 0 for the same state, so 2 was never a value
    // this codebase's mode-dispatch logic actually handles -- the trail pieces
    // got stuck marked temporal with their countdown never advancing.
    game_mode = 0;
    game_status = 0;
}

void neutron::next_level() {
    // neutron::next_level @0x08910178: campaign progression only advances the
    // actual_stage/user_stage watermark when NOT playing via stage-select (isSolo)
    // and the player didn't just lose; otherwise it just replays the current stage.
    if (!isSolo && colision != 1) {
        int max_stg = (interface ? interface->max_stages : 25);
        if (actual_stage < max_stg) {
            actual_stage++;
            if (user_stage < actual_stage) {
                user_stage = actual_stage;
                save_progress();
            }
        } else {
            actual_stage = 1;
        }
        game_status = change_stage(actual_stage, true);
        return;
    }
    game_status = change_stage(actual_stage, false);
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
            interface->actual_staff = (interface->actual_staff + 1) % 7;
            if (interface->buttons[38]) interface->buttons[38]->load_text(interface->whats[interface->actual_staff], 200);
            if (interface->buttons[39]) interface->buttons[39]->load_text(interface->whos[interface->actual_staff], 200);
        }
    }
}

void neutron::step_tutorial() {
    // neutron::step_tutorial @0x089101ec: advances to the next scripted line and
    // (re)loads buttons[6] with its text, same shared HUD slot "navigation mode"/
    // "you lose"/etc use everywhere else. Decomp's own step_tutorial has no case
    // for step 13 (falls to default:return, i.e. no text at all) -- skipped here
    // rather than reproduced, per discussion with the project owner.
    tutorial_step++;
    if (tutorial_step == 13) tutorial_step = 14;
    tutorial_counter = 0;

    const char* msg = nullptr;
    switch (tutorial_step) {
        case 1:
            msg = "you are this blinking cube";
            if (proton) {
                proton->status = 3;
                proton->effect_alpha = 0.0f;
                proton->effect_size = proton->size;
            }
            break;
        case 2: msg = "look, you have a tail"; break;
        case 3: msg = "and this colored squares are your guides"; break;
        case 4: msg = "red means pods left. blue means pods right"; break;
        case 5: msg = "tap left part of the screen to turn left"; break;
        case 6: msg = "now try with right part to turn right"; break;
        case 7: msg = "ok, good. practice a beat ..."; break;
        case 8: msg = "navigation mode done, lets see combat mode"; break;
        case 9: msg = "now, i will load an enemy, tap next to do it"; break;
        case 10:
            load_new_enemy();
            msg = "enemys location is colored ...";
            break;
        case 11: msg = "there it is, he also has a tail ..."; break;
        case 12: msg = "his tail is growing, also yours ..."; break;
        case 14: msg = "your mission: dont crash to any tail"; break;
        case 15: msg = "tap next when ready to fight"; break;
        case 16: msg = "combat mode"; break;
        case 17:
            msg = (colision == 1) ? "you lose, best luck next time" : "you win, congratulations";
            break;
        case 18:
            msg = "remember, left and right are relatives ...";
            restart();
            break;
        case 19:
            // neutron::step_tutorial @0x089101ec case 0x13: graduates out of the
            // tutorial into the real stage 1, exactly like a normal stage load.
            tutorial_step = -1;
            isTutorial = false;
            game_status = change_stage(1, true);
            return;
        default:
            return;
    }
    if (interface && interface->buttons[6]) interface->buttons[6]->load_text(msg, 255);
}

void neutron::tutorial() {
    // neutron::tutorial @0x08910634: runs every tick while game_status==10.
    // Most steps are dialogue-only (frozen or free-roam, per decomp); only
    // 10/12/16 drive real gameplay state (charging/moving the tutorial enemy,
    // resolving the scripted fight).
    speed = 30.0f;
    if (!proton || !ground) return;

    switch (tutorial_step) {
        case 0: case 11: case 14: case 15: case 17: case 18: case 1:
            // Frozen dialogue steps (1 only sets the materialize effect once,
            // handled in step_tutorial() -- no per-tick movement either way).
            return;

        case 10: {
            // main_loop()'s case 0 (normal navigate()/combat()) sets this same
            // difficulty-based speed before moving the player; tutorial() must
            // match it in every step that actually steps proton/electron[0], or
            // the tutorial ticks ~2x faster than real gameplay (shorter frame
            // interval -- see main.cpp's target_frame_time).
            speed = (user_difficulty == 1) ? 55.0f : 75.0f;
            int move_res = proton->move(ground->map, game_mode);
            (void)move_res;
            if (proton->actual_plane != proton->last_plane && proton->last_advance != proton->advance) {
                if (mixer) mixer->play_effect(7);
            }
            enemy* e = electron[0];
            if (!e) return;
            if (proton->actual_plane == e->actual_plane && e->status == 0) {
                game_mode = 1;
                ground->combat_mode();
                ground->change_color(e->material[0], e->material[1], e->material[2], 255);
                e->status = 1;
                if (mixer) mixer->play_effect(5);
            }
            if (e->status == 1) {
                e->move(ground->map, proton->actual_face);
                tutorial_counter++;
            }
            if (tutorial_counter < 2) return;
            step_tutorial();
            tutorial_counter = 0;
            return;
        }

        case 12: {
            speed = (user_difficulty == 1) ? 55.0f : 75.0f;
            int move_res = proton->move(ground->map, game_mode);
            (void)move_res;
            if (proton->actual_plane != proton->last_plane && proton->last_advance != proton->advance) {
                if (mixer) mixer->play_effect(7);
            }
            enemy* e = electron[0];
            if (!e) return;
            if (e->status == 1) {
                if (tutorial_counter == 0) e->change_direction(1);
                e->move(ground->map, proton->actual_face);
                tutorial_counter++;
            }
            if (tutorial_counter < 2) return;
            step_tutorial();
            tutorial_counter = 0;
            return;
        }

        case 16: {
            speed = (user_difficulty == 1) ? 55.0f : 75.0f;
            int move_res = proton->move(ground->map, game_mode);
            if (proton->actual_plane != proton->last_plane && proton->last_advance != proton->advance) {
                if (mixer) mixer->play_effect(7);
            }
            if (move_res != 0) {
                ground->fire_up_cue(proton->ident);
                colision = 1;
                proton->status = 6;
                if (mixer) mixer->play_effect(4);
                step_tutorial();
                return;
            }
            enemy* e = electron[0];
            if (!e) return;
            int e_res = e->move(ground->map, proton->actual_face);
            if (e_res == 0) return;
            ground->fire_up_cue(e->ident);
            colision = 2;
            e->status = 6;
            if (mixer) mixer->play_effect(4);
            step_tutorial();
            return;
        }

        default: {
            // 2,3,4,5,6,7,8,9: free-roam dialogue -- player can move/turn while
            // reading, but nothing here advances the step; [ENTER] does that
            // (see main.cpp's game_status==10 input handling).
            speed = (user_difficulty == 1) ? 55.0f : 75.0f;
            int move_res = proton->move(ground->map, game_mode);
            (void)move_res;
            if (proton->actual_plane != proton->last_plane && proton->last_advance != proton->advance) {
                if (mixer) mixer->play_effect(7);
            }
            return;
        }
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

float neutron::intro_flythrough_distance() {
    // How far back show_stage()'s 3-phase flythrough starts from the player's
    // real spawn camera position. The real waypoints are lost to decompilation,
    // but the per-tick speed (1) IS confirmed via disassembly of every
    // move_camera call in show_stage(), and it's slower than what this port was
    // using (2) -- so to land on a similarly-paced few-second flythrough rather
    // than a much longer one, the distance needs to come down too, not scale up
    // with the grid (side_size turning out to be 20 instead of 10 doesn't imply
    // the flythrough itself covers more ground, only that gameplay does).
    return 8.0f;
}

void neutron::decrement_score() {
    score--;
    std::snprintf(score_buffer, sizeof(score_buffer), "%06d", score);
    if (interface && interface->buttons[1]) interface->buttons[1]->load_text(score_buffer, -1);
}

void neutron::apply_save_data(const SaveData& data) {
    // Called once right after construction (miniatures already exist by then)
    // to restore per-stage high scores from a loaded save file. Scores are
    // matched by stage name, not index, so this still works if the stage
    // list gets reordered, or the map.txt stage at index 25 slots in/out.
    for (int i = 0; i < total_stage_count(); i++) {
        if (!miniatures[i]) continue;
        std::string key = save_key_for_stage_name(stage_display_name(i));
        auto it = data.high_scores.find(key);
        if (it != data.high_scores.end()) {
            miniatures[i]->high_score = it->second;
        }
    }
}

void neutron::save_progress() {
    SaveData data;
    data.user_color = user_color;
    data.user_difficulty = user_difficulty;
    data.user_sound = user_sound;
    data.user_volume = mixer ? mixer->get_volume() : 100;
    data.user_stage = user_stage;
    data.fullscreen = fullscreen;
    for (int i = 0; i < total_stage_count(); i++) {
        if (!miniatures[i]) continue;
        std::string key = save_key_for_stage_name(stage_display_name(i));
        data.high_scores[key] = miniatures[i]->high_score;
    }
    write_save_data(data);
}

void neutron::delay_artifact(int next_stat, int dtime) {}
void neutron::execute_artifact() {}
void neutron::load_artifact(int artifact) {}
void neutron::change_option() {
    speed = 30.0f;
    menu_battle::update(this);
}

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
        if (proton) {
            proton->x = (float)p->a_vertex[0];
            proton->y = (float)p->a_vertex[1];
            proton->z = (float)p->a_vertex[2];
            proton->actual_plane = interface->menu_option[interface->actual_option];
        }
    }
    if (mixer) mixer->play_effect(2);
}

void neutron::load_new_enemy() {
    // neutron::load_new_enemy @0x0890ec88: only runs outside active combat (game_mode
    // 0 or 2); it does NOT add an enemy to the existing set, it resets the training
    // arena down to exactly one freshly-spawned (dormant, difficulty-typed) enemy.
    if (game_mode != 2 && game_mode != 0) return;

    for (int i = 0; i < N_SHIPS - 1; i++) {
        delete electron[i];
        electron[i] = nullptr;
    }

    N_SHIPS = 2;
    autoload_eletrons(1);
    game_mode = 0;
    if (interface && interface->buttons[6]) interface->buttons[6]->load_text("navigation mode", 255);

    last_charged_electron = 0;
    charged_electrons = 0;
    uncharged_electrons = 0;
    colision = 0;
    if (proton) {
        proton->effect_size = 0.0f;
        proton->status = 1;
        proton->effect_alpha = 100.0f;
    }
}

void neutron::autoload_eletrons(int count) {
    if (!ground || ground->N_PLANES < 1) return;
    int spawned = 0;
    int start_pl = (proton ? proton->actual_plane : 0);
    for (int i = 0; i < count && spawned < 29; i++) {
        int pl = std::rand() % ground->N_PLANES;
        if (pl == start_pl && ground->N_PLANES > 1) {
            pl = (pl + 1) % ground->N_PLANES;
        }
        plane* p = ground->map[pl];
        if (!p) {
            for (int k = 0; k < ground->N_PLANES; k++) {
                if (ground->map[k] && k != start_pl) {
                    pl = k;
                    p = ground->map[k];
                    break;
                }
            }
        }
        if (!p) continue;

        enemy* e = new enemy();
        e->ident = spawned + 1;
        int enemy_type = 0;
        if (actual_stage != 2000) {
            if (user_difficulty == 0) {
                if (actual_stage < 3) {
                    enemy_type = 0;
                } else if (actual_stage < 10) {
                    enemy_type = std::rand() % 2;
                } else if (actual_stage < 15) {
                    enemy_type = std::rand() % 3;
                } else {
                    enemy_type = std::rand() % 4;
                }
            } else {
                if (actual_stage < 10) {
                    enemy_type = std::rand() % 3;
                } else {
                    enemy_type = std::rand() % 4;
                }
            }
        }
        e->type = enemy_type;

        // Enemy type and material assignment matching autoload_eletrons_0x890e5e8.c
        if (e->type == 2) {
            e->material[0] = 120; e->material[1] = 0;   e->material[2] = 0;   e->material[3] = 255;
        } else if (e->type == 0) {
            e->material[0] = 100; e->material[1] = 0;   e->material[2] = 0;   e->material[3] = 255;
        } else if (e->type == 1) {
            e->material[0] = 220; e->material[1] = 90;  e->material[2] = 5;   e->material[3] = 255;
        } else {
            e->material[0] = std::rand() % 200; e->material[1] = std::rand() % 200; e->material[2] = std::rand() % 200; e->material[3] = 255;
        }

        e->start_plane = pl;
        e->actual_plane = pl;
        e->start_actual_face = p->face_number;
        int prev_face = (p->face_number < 6) ? p->face_number + 1 : p->face_number - 1;
        e->start_previous_face = prev_face;
        e->actual_face = p->face_number;
        e->previous_face = prev_face;

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

    if (p->dead_axis == 1) {
        proton->start_x = p->dead_offset;
        proton->start_y = p->i + p->side_size / 2;
        proton->start_z = p->j + p->side_size / 2;
    } else if (p->dead_axis == 2) {
        proton->start_x = p->i + p->side_size / 2;
        proton->start_y = p->dead_offset;
        proton->start_z = p->j + p->side_size / 2;
    } else {
        proton->start_x = p->i + p->side_size / 2;
        proton->start_y = p->j + p->side_size / 2;
        proton->start_z = p->dead_offset;
    }

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
    colision = 0;
    unload_stage();

    ground = new terrain();
    // terrain::init @0x08904818's cam_distance param: confirmed via disassembly of
    // the 4 real call sites in load_stage (all pass the literal 0x14 = 20, matching
    // the real side_size) -- was hardcoded to 10 here, making the gameplay chase
    // camera sit twice as close to the player as it should.
    if (stg == 1000) {
        ground->create_from_array(14, MENU_DATA);
        ground->init(0, 0, 0, 20);
        ground->status = 1;
        N_SHIPS = 1;
        load_proton(setup_cams);
        menu_battle::spawn(this);
    } else if (stg == 2000) {
        create_miniatures();
        ground->create_from_miniature(miniatures[0]);
        ground->init(0, 0, 0, 20);
        ground->status = 4;
        load_proton(setup_cams);
    } else {
        create_miniatures();
        int idx = stg - 1;
        if (idx < 0) idx = 0;
        if (idx >= total_stage_count()) idx = total_stage_count() - 1;
        ground->create_from_miniature(miniatures[idx]);
        ground->init(0, 0, 0, 20);
        ground->status = 4;
        load_proton(setup_cams);
        if (user_difficulty == 0) {
            N_SHIPS = miniatures[idx]->cubes + 1;
        } else {
            N_SHIPS = miniatures[idx]->cubes * 2 + 1;
        }
        if (N_SHIPS > 30) N_SHIPS = 30;
        autoload_eletrons(N_SHIPS - 1);
        if (interface && interface->buttons[6]) {
            interface->buttons[6]->load_text(stage_display_name(idx), -1);
        }
    }
}

int neutron::change_stage(int stg, bool show_it) {
    // neutron::change_stage @0x0890fc90
    load_stage(stg, show_it);

    if (stg == 1000) {
        // Port's `vision` polarity is inverted from the decomp field: here 0 means
        // "follow proton->cam_ship" and 1 means "free game_cam" (see print_stage()).
        // Decomp sets vision=1 for stage 1000 meaning "use cam_ship" -> port's 0.
        vision = 0;
        return 11; // -> change_option(): idle main-menu state
    }
    if (stg == 3000) {
        // Decomp sets vision=0 here meaning "use game_cam" -> port's 1.
        vision = 1;
        on_miniature_change();
        return 9; // -> show_miniatures()
    }

    // Playable stage (numbered 1-25, or 2000 training): per-stage score pool is
    // side_size^2 * N_PLANES, counting down by one on every successful move.
    if (ground && ground->map[0]) {
        score = ground->map[0]->side_size * ground->map[0]->side_size * ground->N_PLANES;
        std::snprintf(score_buffer, sizeof(score_buffer), "%06d", score);
        if (interface && interface->buttons[1]) interface->buttons[1]->load_text(score_buffer, -1);
    }
    vision = 0; // decomp vision=1 ("use cam_ship") -> port's 0

    if (!show_it) {
        // Skip the camera flythrough (e.g. "try again"): go straight to the brief
        // materialize-in pause.
        if (proton) {
            proton->status = 3;
            proton->effect_size = 3.0f; // DAT_089dc368
            if (proton->cam_ship) {
                // ship::init()/init_pos() leave the look-target at the origin; point
                // it at the player so the pause doesn't show a stale/wrong view.
                proton->cam_ship->x_dir = proton->x;
                proton->cam_ship->y_dir = proton->y;
                proton->cam_ship->z_dir = proton->z;
            }
        }
        if (ground) {
            ground->status = 1;
            ground->painted_planes = 0;
            ground->painted_bones = 0;
            ground->change_color(100, 100, 100, 255);
        }
        time = 25; // DAT: 0x19
        go_status = 0;
        if (mixer) mixer->play_effect(11);
        return 4; // -> delay(): the brief pause before control resumes
    }

    if (ground) {
        ground->status = 4;
        ground->painted_planes = 1;
        ground->painted_bones = 1;
    }
    if (mixer) mixer->play_effect(3);
    if (ground && proton) {
        ground->change_color(proton->material[0], proton->material[1], proton->material[2], 255);
    }
    camera_passes = 0;
    if (proton && proton->cam_ship) {
        // Real entry camera position was already computed by load_proton()/ship::init()
        // above; stash it and pull the camera back so show_stage() has distance to
        // glide in across (exact original waypoints are lost to decompilation, this
        // is a reasonable approximation of the flythrough).
        intro_cam_tx = proton->cam_ship->x;
        intro_cam_ty = proton->cam_ship->y;
        intro_cam_tz = proton->cam_ship->z;
        float pull = intro_flythrough_distance();
        proton->cam_ship->x = intro_cam_tx - pull;
        proton->cam_ship->y = intro_cam_ty + pull;
        proton->cam_ship->z = intro_cam_tz - pull;
        proton->cam_ship->x_dir = proton->x;
        proton->cam_ship->y_dir = proton->y;
        proton->cam_ship->z_dir = proton->z;
    }
    return 5; // -> show_stage(): camera flythrough, then the same pause
}

int neutron::navigate() {
    if (!proton || !ground) return game_status;
    if (proton->status == 3) {
        return game_status;
    }
    if (colision == 1) {
        proton->status = 2;
        if (interface && interface->buttons[6]) interface->buttons[6]->load_text("you lose", -1);
        return 1;
    }

    if (proton->step_counter < proton->step_speed) {
        proton->step_counter++;
    } else {
        proton->step_counter = 0;
        int move_res = proton->move(ground->map, game_mode);
        if (proton->actual_plane != proton->last_plane) {
            if (proton->last_advance != proton->advance) {
                if (mixer) {
                    mixer->play_effect(7);
                    mixer->trigger_corner_warp(proton->advance < 0 ? -1 : 1);
                }
            }
        }
        // Score only counts down once a trail fight is actually underway (combat
        // mode); navigate() only ever runs while game_mode == 0, so no decrement here.
        if (move_res != 0) {
            // Matches the enemy's own death handling exactly (run()'s enemy-move
            // branch): status=6 and fire_up_cue() fire the same tick, with no
            // artificial hold -- the explosion is carried entirely by status==6's
            // effect_alpha/effect_size fade in print() plus stage_over()'s camera
            // pan, same as it already is for the enemy. The death_pause/
            // trail_fade_pause staged delay this replaced was a port-only
            // invention (added to compensate for print() running at uncapped
            // render rate) that was never applied to the enemy path, so the
            // player's death dragged out ~2x30 ticks longer than the enemy's.
            proton->status = 6;
            proton->effect_size = proton->size;
            proton->effect_alpha = (float)proton->material[3];
            if (ground) ground->fire_up_cue(proton->ident);
            colision = 1;
            stage_over_done = false;
            if (interface && interface->buttons[6]) interface->buttons[6]->load_text("you lose", -1);
            if (mixer) {
                mixer->play_effect(6);
                if (game_cam) mixer->play_explosion(game_cam->x, game_cam->y, game_cam->z,
                                                      proton->x, proton->y, proton->z);
            }
            return 1;
        }
    }

    for (int i = 0; i < N_SHIPS - 1; i++) {
        enemy* e = electron[i];
        if (e && proton->actual_plane == e->actual_plane && e->status == 0) {
            game_mode = 1; // Combat mode!
            if (interface && interface->buttons[6]) interface->buttons[6]->load_text("combat mode", 255);
            ground->combat_mode();
            e->status = 1;
            last_charged_electron = i;
            charged_electrons++;
            ground->change_color(e->material[0], e->material[1], e->material[2], 255);
            if (mixer) mixer->play_effect(5);
        }
    }
    return game_status;
}

int neutron::combat() {
    if (!proton || !ground) return game_status;

    if (colision == 1) {
        proton->status = 2;
        if (interface && interface->buttons[6]) interface->buttons[6]->load_text("you lose", -1);
        return 1;
    }
    if (proton->step_counter < proton->step_speed) {
        proton->step_counter++;
    } else {
        proton->step_counter = 0;
        int move_res = proton->move(ground->map, game_mode);
        if (proton->actual_plane != proton->last_plane && proton->last_advance != proton->advance) {
            if (mixer) {
                mixer->play_effect(7);
                mixer->trigger_corner_warp(proton->advance < 0 ? -1 : 1);
            }
        }
        // Score only counts down while a trail fight is actually underway.
        if (move_res == 0) decrement_score();
        if (move_res == 3) {
            execute_artifact();
        } else if (move_res != 0) {
            // Matches the enemy's own death handling (status=6 and fire_up_cue()
            // fire the same tick, no artificial hold) -- see navigate()'s longer
            // comment on this same change.
            proton->status = 6;
            proton->effect_size = proton->size;
            proton->effect_alpha = (float)proton->material[3];
            if (ground) ground->fire_up_cue(proton->ident);
            colision = 1;
            stage_over_done = false;
            if (interface && interface->buttons[6]) interface->buttons[6]->load_text("you lose", -1);
            if (mixer) {
                mixer->play_effect(6);
                if (game_cam) mixer->play_explosion(game_cam->x, game_cam->y, game_cam->z,
                                                      proton->x, proton->y, proton->z);
            }
            return 1;
        }
    }

    // Check for additional enemy triggers while in combat mode
    for (int i = 0; i < N_SHIPS - 1; i++) {
        enemy* e = electron[i];
        if (e && proton->actual_plane == e->actual_plane && e->status == 0) {
            e->status = 1;
            last_charged_electron = i;
            charged_electrons++;
            if (mixer) mixer->play_effect(5);
        }
    }

    for (int i = 0; i < N_SHIPS - 1; i++) {
        enemy* e = electron[i];
        if (e && e->status == 1) {
            if (e->step_counter < e->step_speed) {
                e->step_counter++;
            } else {
                e->step_counter = 0;
                int e_res = e->move(ground->map, proton->actual_face);
                if (e_res != 0) {
                    e->status = 6;
                    if (ground) ground->fire_up_cue(e->ident);
                    uncharged_electrons++;
                    n_enemies--;
                    if (n_enemies < 0) n_enemies = 0;
                    std::snprintf(enemies_buffer, sizeof(enemies_buffer), "%02d", n_enemies);
                    if (interface && interface->buttons[3]) interface->buttons[3]->load_text(enemies_buffer, -1);
                    if (mixer) {
                        mixer->play_effect(4);
                        if (game_cam) mixer->play_explosion(game_cam->x, game_cam->y, game_cam->z,
                                                              e->x, e->y, e->z);
                    }

                    if (uncharged_electrons >= N_SHIPS - 1) {
                        if (actual_stage == 2000) {
                            // neutron::combat @0x0890de9c (generic win trigger): training
                            // has no score/high-score UI, so skip level_complete() entirely
                            // and let stage_over()'s actual_stage==2000 branch handle it
                            // (wait for the explosion to finish, then reset to navigation).
                            colision = 2;
                            game_status = 1;
                            return game_status;
                        }
                        if (ground) ground->change_color(255, 255, 255, 255);
                        level_complete(e);
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
                            if (ground) {
                                ground->navigation_mode();
                                ground->change_color(ground->material[0], ground->material[1], ground->material[2], 255);
                            }
                            if (interface && interface->buttons[6]) interface->buttons[6]->load_text("navigation mode", 255);
                        }
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
    colision = 0;
    if (ground) ground->restart();
    if (proton) proton->restart();
    for (int i = 0; i < N_SHIPS - 1; i++) {
        if (electron[i]) electron[i]->restart();
    }
}

void neutron::print_stage() {
    if (actual_stage == 1000) {
        if (game_cam) {
            float angle = (float)SDL_GetTicks() * 0.0003f;
            float radius = 24.0f;
            game_cam->x = 10.0f + radius * std::cos(angle);
            game_cam->y = 15.0f + radius * std::sin(angle) * 0.5f;
            game_cam->z = 18.0f + radius * std::sin(angle);
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
        for (int i = 0; i < total_stage_count(); i++) {
            if (miniatures[i]) miniatures[i]->print();
        }
    }

    if (ground) {
        ground->print(true, proton ? proton->actual_plane : 0, low_quality);
    }

    if (proton && actual_stage != 1000) {
        proton->print(0, low_quality);
        // neutron::print @0x08908e00: during the tutorial the guide squares stay
        // hidden until tutorial_step 3 ("...your guides"), matching the scripted
        // reveal; outside the tutorial this condition is unaffected.
        if ((user_difficulty == 0 || actual_stage == 2000) && proton->status == 1 && ground &&
            (!isTutorial || tutorial_step >= 3)) {
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
            // SLEEPING ENEMY TRIGGER TILE ON THIS PLANE (Hidden during combat mode)
            if (game_mode != 1 && ground && e->actual_plane >= 0 && e->actual_plane < ground->N_PLANES) {
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

                    // Sleeping enemy trigger tile only (enemy cycle model is invisible until combat trigger is hit)
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
        if (game_status == 20) {
            split_screen_menu::print_join_screen(this);
        } else if (interface) {
            if (game_status == 13) {
                interface->print_staff(actual_stage);
            } else {
                interface->print_menu_interface(actual_stage, user_difficulty, user_sound, user_color,
                                                 mixer ? mixer->get_volume() : 100, fullscreen);
            }
        }
        return;
    } else if (actual_stage == 2000) {
        print_stage();
        if (interface) interface->print_train_interface(actual_stage, paused);
        return;
    } else if (actual_stage == 3000) {
        print_stage();
        if (game_status == 21) {
            split_screen_menu::print_map_select(this);
        } else if (interface) {
            interface->print_solo_interface(actual_stage);
        }
        return;
    } else if (actual_stage == 4000) {
        // Split-screen match -- its own multi-viewport rendering entirely,
        // not print_stage() (which assumes a single proton/camera).
        split_screen_match::print_match(this);
        if (game_status == 24) split_screen_match::print_match_over(this);
        return;
    }
    print_stage();
    if (interface) {
        if (game_status == 10) {
            interface->print_tutorial_interface(actual_stage, isTutorial, tutorial_step);
        } else {
            interface->print_game_interface(actual_stage, game_status, colision, new_high_score, paused);
        }
    }
}

void neutron::main_loop() {
    speed = 30.0f;
    age_terrain_pieces();
    if (mixer && game_status != 0) mixer->stop_travel(); // idempotent if already stopped
    switch (game_status) {
        case 0:
            speed = (user_difficulty == 1) ? 55.0f : 75.0f;
            if (mixer && proton && game_cam) {
                mixer->start_travel(); // no-op if already playing
                mixer->update_travel();
            }
            if (run() == 1) {
                game_status = 1;
                stage_over();
            }
            break;
        case 1:  
        case 8:  stage_over(); break;
        case 3:  next_level(); break;
        case 4:  delay(go_status, time); break;
        case 5:  show_stage(); break;
        case 6:  wait_option(); break;
        case 9:  show_miniatures(); break;
        case 10: tutorial(); break;
        case 11: change_option(); break;
        case 12: are_you_sure(); break;
        case 13: show_staff(); break;
        case 20: split_screen_menu::update_join_screen(this); break;
        case 21: split_screen_menu::update_map_select(this); break;
        case 23: split_screen_match::tick(this); break;
        case 24: split_screen_match::update_match_over(this); break;
        case 1000: game_intro(); break;
    }
}

void neutron::execute_action(int action) {
    if (action < 0) return;

    switch (action) {
        case 0:
            if (proton) proton->change_direction(0); // Left
            if (mixer) mixer->trigger_turn_duck();
            break;
        case 1:
            if (proton) proton->change_direction(1); // Right
            if (mixer) mixer->trigger_turn_duck();
            break;
        case 2:
            // "Game" / Start Game -- begins a fresh run at stage 1. user_stage is the
            // unlock watermark (furthest stage reached) and is NOT reset here; it only
            // ever advances in next_level(). neutron::execute_action case 10 (New Game)
            // @0x08910ff8: isTutorial=true routes stage 1's materialize sequence into
            // the scripted tutorial instead of straight gameplay (see show_stage()).
            isSolo = false;
            isTutorial = true;
            tutorial_step = 0;
            actual_stage = 1;
            game_status = change_stage(1, true);
            break;
        case 3:
            menu_move(3);
            break;
        case 5: // Train
            game_status = change_stage(2000, true);
            break;
        case 6: // Menu up
            menu_move(0);
            break;
        case 7: // Menu down
            menu_move(1);
            break;
        case 8: // Back to menu
            // neutron::execute_action @0x08910f34 case 8: leaving mid-tutorial
            // clears tutorial_step/isTutorial so a later Continue Game doesn't
            // resume a half-finished tutorial state.
            if (tutorial_step >= 0) tutorial_step = -1;
            isTutorial = false;
            game_status = change_stage(1000, true);
            break;
        case 9: // Load new enemy in training
            load_new_enemy();
            break;
        case 10: // New Game
            isSolo = false;
            isTutorial = true;
            tutorial_step = 0;
            actual_stage = 1;
            game_status = change_stage(1, true);
            break;
        case 11: // Play user stage (continue at furthest unlocked stage)
            isSolo = false;
            actual_stage = user_stage;
            game_status = change_stage(user_stage, true);
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
            save_progress();
            break;
        case 16: // Difficulty toggle
            user_difficulty = 1 - user_difficulty;
            if (interface && interface->buttons[23]) {
                interface->buttons[23]->load_text(user_difficulty ? "hard" : "easy", -1);
            }
            save_progress();
            break;
        case 17: // Sound toggle
            user_sound = !user_sound;
            if (mixer) mixer->set_enabled(user_sound);
            if (interface && interface->buttons[25]) {
                interface->buttons[25]->load_text(user_sound ? "on" : "off", -1);
            }
            save_progress();
            break;
        case 18: // Try again -- retry/restart skips the flythrough (matches decomp's
                 // next_level() routing a loss/solo retry through change_stage(..., false)).
            game_status = change_stage(actual_stage, false);
            break;
        case 19: { // Prev miniature (clamped to the unlocked range, matching
                   // neutron::execute_action's select-stage prev/next @0x08910xxx)
            int unlocked = unlock_all_stages ? total_stage_count() : user_stage;
            if (unlocked < 1) unlocked = 1;
            if (actual_miniature > 0) actual_miniature--;
            else actual_miniature = unlocked - 1;
            on_miniature_change();
            break;
        }
        case 20: { // Next miniature (clamped to the unlocked range)
            int unlocked = unlock_all_stages ? total_stage_count() : user_stage;
            if (unlocked < 1) unlocked = 1;
            if (actual_miniature + 1 < unlocked) actual_miniature++;
            else actual_miniature = 0;
            on_miniature_change();
            break;
        }
        case 21: // Play selected miniature -- does NOT touch the unlock watermark,
                 // it just plays whatever's currently selected (always unlocked,
                 // since navigation already clamps to it).
            isSolo = true;
            actual_stage = actual_miniature + 1;
            game_status = change_stage(actual_stage, true);
            break;
        case 22: // Toggle "unlock all stages" -- hidden main-menu cheat code
                 // (Up,Up,Down,Down,Left,Right,Enter), not in the original game
            unlock_all_stages = !unlock_all_stages;
            break;
        case 23: // Camera up
            break;
        case 24: // Camera down
            break;
        case 25: // Confirm Yes
            game_status = change_stage(1000, true);
            break;
        case 26: // Confirm No
            game_status = 0;
            break;
        case 27: // Staff / Credits
            game_status = 13;
            break;
        case 28: // Volume up
            if (mixer) mixer->set_volume(mixer->get_volume() + 10);
            save_progress();
            break;
        case 29: // Volume down
            if (mixer) mixer->set_volume(mixer->get_volume() - 10);
            save_progress();
            break;
    }
}
