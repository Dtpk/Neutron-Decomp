#pragma once

#include "stage_interface.h"
#include "sound_mixer.h"
#include "terrain.h"
#include "player.h"
#include "enemy.h"
#include "miniature.h"
#include "camera.h"

class neutron {
public:
    int game_status;
    int go_status;
    int game_mode;
    int actual_stage;
    int score;
    int user_score;
    int high_score;
    int colision;
    int actual_miniature;
    int actual_option;
    int user_color;
    int user_difficulty;
    int user_stage;
    bool user_sound;
    bool isTutorial;
    int tutorial_step;
    int tutorial_counter;
    int time;
    float speed;
    camera* game_cam;
    stage_interface* interface;
    sound_mixer* mixer;
    terrain* ground;
    player* proton;
    enemy* electron[30];
    miniature* miniatures[25];
    int N_SHIPS;
    int n_enemies;
    int charged_electrons;
    int last_charged_electron;
    int uncharged_electrons;
    char enemies_buffer[16];
    char score_buffer[16];
    int background_colors[50][13];
    int bg_current_count;
    int bg_current_size;
    bool game_over;
    int intro_step;
    int intro_counter;
    int intro_substep;
    int vision;
    int prints;
    bool low_quality;
    int actual_artifact;

    neutron(int color, int difficulty, int stage, bool sound);
    ~neutron();

    bool move_lookat(camera* cam, float x, float y, float z, int spd);
    bool move_camera(camera* cam, float x, float y, float z, int spd);
    void level_complete();
    void wait_option();
    void show_miniatures();
    void are_you_sure();
    void show_staff();
    void on_miniature_change();
    void game_intro();
    void print_stars();
    void autoload_eletrons(int n_enemies);
    void delay(int status, int delay_time);
    void delay_artifact(int status, int delay_time);
    void execute_artifact();
    void load_artifact(int artifact);
    void print_background();
    void show_stage();
    void change_option();
    void create_miniatures();
    void load_miniatures();
    void print_stage();
    void print();
    int run();
    void restart();
    int navigate();
    void menu_move(int side);
    int combat();
    void load_proton(bool setup_cams);
    void unload_stage();
    void stage_over();
    void load_stage(int stage, bool setup_cams);
    int change_stage(int stage, bool setup_cams);
    void next_level();
    void step_tutorial();
    void tutorial();
    void main_loop();
    void execute_action(int action);
    void load_new_enemy();
};
