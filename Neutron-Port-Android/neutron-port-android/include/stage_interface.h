#pragma once

#include "button.h"
#include "glinterface.h"

class stage_interface {
public:
    glinterface* locator;
    button* buttons[40];
    int intro_positions[94][4];
    int menu_option[12];
    int actual_option;
    int choosen_color;
    int max_stages;
    const char* whats[7];
    const char* whos[7];
    int actual_staff;

    stage_interface(int color, int difficulty, int stage, bool sound);
    ~stage_interface();

    int who_is(int screen_x, int screen_y);
    int catch_key(int key);
    int catch_tap(int screen_x, int screen_y, int action_type);
    void activate_buttons(const int* id_enableds, int count);
    void print_staff(int actual_stage);
    void print_solo_interface(int actual_stage);
    void print_tutorial_interface(int actual_stage, bool isTutorial, int tutorial_step);
    void print_train_interface(int actual_stage, bool paused);
    void print_game_interface(int actual_stage, int game_status, int colision, bool new_high_score, bool paused);
    void print_menu_interface(int actual_stage, int difficulty, bool sound, int color, int volume, bool fullscreen);
    void print_intro(int intro_step);
};
