#include "touch_input.h"
#include "neutron.h"
#include "stage_interface.h"
#include "sound_mixer.h"
#include "split_screen_menu.h"

// Implemented in main.cpp -- see controller_input.cpp's identical forward
// declaration for why this lives there instead of here.
void app_toggle_fullscreen();

namespace touch_input {

namespace {

// Logical space is the same fixed 480x272 ortho stage_interface.cpp's
// begin_2d() sets up (glOrtho(0,480,0,272,-1,1)), independent of actual
// window size.
const float LOGICAL_W = 480.0f;
const float LOGICAL_H = 272.0f;

// True if (lx, ly) falls within a generous horizontal band around a menu
// row whose text baseline sits at row_y, with +-(row_spacing/2) of
// vertical slop -- mirrors a row rendered by print_menu_interface()'s own
// `y = top_y - i * row_spacing` formula. Horizontal bounds are intentionally
// wide (not measured against actual text width) since these are short
// single lines centered in a narrow logical screen; precision isn't needed.
bool hit_row(float lx, float ly, float row_y, float row_spacing) {
    return lx >= 50.0f && lx <= 430.0f &&
           ly >= row_y - row_spacing * 0.5f && ly <= row_y + row_spacing * 0.5f;
}

// Which row (0-based) of `count` rows starting at `top_y` and spaced
// `row_spacing` apart was tapped, or -1. Mirrors the exact formulas used in
// print_menu_interface() -- keep these two in lockstep if that renderer's
// layout ever changes.
int hit_row_index(float lx, float ly, float top_y, float row_spacing, int count) {
    for (int i = 0; i < count; i++) {
        float row_y = top_y - (float)i * row_spacing;
        if (hit_row(lx, ly, row_y, row_spacing)) return i;
    }
    return -1;
}

// Precise hit-zones over each screen's own on-screen HUD hint text (not a
// generic corner) -- tapping the actual visible "[ESC]"/"[ENTER]" text does
// what it says, matching its real position/size in stage_interface.cpp's
// draw_hud_text() calls, padded generously since the glyphs themselves are
// tiny. Keep these in lockstep with stage_interface.cpp if those strings,
// positions, or sizes ever change.

// print_game_interface()/print_tutorial_interface()'s shared steer/view/menu
// line: "[A:D] STEER   [V] VIEW   [ESC] MENU" -- "[ESC]" is characters 25-29.
bool hit_escape_hint_ingame(float lx, float ly) {
    return lx >= 232.0f && lx <= 300.0f && ly >= 4.0f && ly <= 34.0f;
}
bool hit_escape_hint_tutorial(float lx, float ly) {
    return lx >= 236.0f && lx <= 307.0f && ly >= 10.0f && ly <= 42.0f;
}

// print_train_interface(): "[ENTER] SPAWN ENEMY   [A:D] STEER   [ESC] MENU"
// -- "[ENTER]" is characters 0-6, "[ESC]" is characters 36-40.
bool hit_confirm_hint_training(float lx, float ly) {
    return lx >= 28.0f && lx <= 102.0f && ly >= 10.0f && ly <= 42.0f;
}
bool hit_escape_hint_training(float lx, float ly) {
    return lx >= 280.0f && lx <= 350.0f && ly >= 10.0f && ly <= 42.0f;
}

// print_tutorial_interface()'s separate "[ENTER] NEXT" hint (only drawn
// when a step accepts it) -- "[ENTER]" is characters 0-6.
bool hit_confirm_hint_tutorial(float lx, float ly) {
    return lx >= 150.0f && lx <= 228.0f && ly >= 48.0f && ly <= 76.0f;
}

// print_solo_interface(): "[ENTER] PLAY STAGE   [ESC] BACK" -- "[ESC]" is
// characters 21-25.
bool hit_escape_hint_stage_select(float lx, float ly) {
    return lx >= 255.0f && lx <= 322.0f && ly >= 6.0f && ly <= 40.0f;
}

// print_game_interface()/print_train_interface()'s shared paused-state line:
// "[ENTER:CLICK] RESUME   [ESC] MENU" -- "[ESC]" is characters 23-27.
bool hit_escape_hint_paused(float lx, float ly) {
    return lx >= 258.0f && lx <= 330.0f && ly >= 95.0f && ly <= 128.0f;
}

void handle_main_menu_tap(neutron* game, float lx, float ly) {
    stage_interface* ui = game->interface;
    if (!ui) return;

    // Graphics & Sound submenu (actual_option 30-33)
    if (ui->actual_option >= 30 && ui->actual_option < 34) {
        int row = hit_row_index(lx, ly, 170.0f, 24.0f, 4);
        if (row < 0) return;
        switch (row) {
            case 0: game->execute_action(17); break; // Sound on/off
            case 1: // Volume -- left half of the row = down, right half = up
                if (lx < LOGICAL_W * 0.5f) game->execute_action(29);
                else game->execute_action(28);
                break;
            case 2: app_toggle_fullscreen(); break;
            case 3: ui->actual_option = 10; break; // Back
        }
        return;
    }

    // Options submenu (actual_option 10-13)
    if (ui->actual_option >= 10 && ui->actual_option < 14) {
        int row = hit_row_index(lx, ly, 170.0f, 24.0f, 4);
        if (row < 0) return;
        switch (row) {
            case 0: ui->actual_option = 30; break; // Open Graphics & Sound
            case 1: game->execute_action(15); break; // Player color
            case 2: game->execute_action(16); break; // Difficulty
            case 3: ui->actual_option = 0; break; // Back to main menu
        }
        return;
    }

    // Root main menu (7 items)
    int row = hit_row_index(lx, ly, 190.0f, 19.0f, 7); // must match print_menu_interface()'s row layout
    if (row < 0) return;
    switch (row) {
        case 0: game->execute_action(2); break;  // New Game
        case 1: game->execute_action(11); break; // Continue Game
        case 2: game->execute_action(5); break;  // Training
        case 3: game->execute_action(12); break; // Stage Select
        case 4: // Split Screen -- only the join/map-select entry point is
                // touch-reachable; those screens themselves are keyboard/
                // controller only (touch is inherently single-player).
            split_screen_menu::reset(game);
            game->game_status = 20;
            break;
        case 5: ui->actual_option = 10; break;   // Options
        case 6: game->game_status = 13; break;   // Credits
    }
}

} // namespace

void handle_finger_down(neutron* game, const SDL_TouchFingerEvent& e) {
    if (!game) return;

    float lx = e.x * LOGICAL_W;
    float ly = (1.0f - e.y) * LOGICAL_H; // SDL touch y grows downward; ortho y grows upward

    // Intro skip
    if (game->actual_stage == 1000 &&
        (game->game_status == 1000 || game->game_status == 4)) {
        game->intro_step = 93;
        game->game_status = 6;
        int active_btns[4] = {7, 8, 9, 10};
        if (game->interface) game->interface->activate_buttons(active_btns, 4);
        return;
    }

    // Split-screen's join/map-select/match screens are keyboard/controller
    // only (see split_screen_menu.h/split_screen_match.h) -- ignore taps
    // rather than falling through to the main-menu/stage-select tap
    // handlers below, which would otherwise also match (join screen keeps
    // actual_stage==1000, map select reuses actual_stage==3000).
    if (game->game_status == 20 || game->game_status == 21 ||
        game->game_status == 23 || game->game_status == 24) {
        return;
    }

    if (game->actual_stage == 1000 && game->game_status != 13) {
        handle_main_menu_tap(game, lx, ly);
        return;
    }

    if (game->game_status == 13) { // Credits -- tap anywhere to go back
        game->execute_action(8);
        return;
    }

    if (game->actual_stage == 3000) { // Stage Select
        if (hit_escape_hint_stage_select(lx, ly)) { game->execute_action(8); return; }
        if (lx < LOGICAL_W / 3.0f) game->execute_action(19);      // Prev
        else if (lx > LOGICAL_W * 2.0f / 3.0f) game->execute_action(20); // Next
        else game->execute_action(21);                            // Play
        return;
    }

    if (game->game_status == 0) { // Gameplay or Training
        if (game->paused) {
            // Tapping the actual "[ESC] MENU" hint a second time quits to
            // the main menu, matching keyboard/controller's paused+Escape
            // dual semantics; tapping anywhere else just resumes play.
            if (hit_escape_hint_paused(lx, ly)) {
                game->paused = false;
                game->execute_action(8);
            } else {
                game->paused = false;
            }
            return;
        }
        if (game->actual_stage == 2000) { // Training -- its own distinct hint line
            if (hit_escape_hint_training(lx, ly)) {
                game->paused = true;
                if (game->mixer) game->mixer->stop_travel();
                return;
            }
            if (hit_confirm_hint_training(lx, ly)) {
                game->execute_action(9); // Spawn enemy
                return;
            }
        } else if (hit_escape_hint_ingame(lx, ly)) {
            game->paused = true;
            if (game->mixer) game->mixer->stop_travel();
            return;
        }
        if (lx < LOGICAL_W * 0.5f) game->execute_action(0); // Turn left
        else game->execute_action(1);                        // Turn right
        return;
    }

    if (game->game_status == 10) { // Scripted tutorial
        if (hit_escape_hint_tutorial(lx, ly)) { game->execute_action(8); return; }
        if (hit_confirm_hint_tutorial(lx, ly)) {
            if (game->tutorial_step != 10 && game->tutorial_step != 16) game->step_tutorial();
            return;
        }
        if (lx < LOGICAL_W * 0.5f) game->execute_action(0);
        else game->execute_action(1);
        return;
    }

    if (game->game_status == 1) { // Crash retry
        game->execute_action(18);
        return;
    }

    if (game->game_status == 8 || game->game_status == 3) { // Level complete / crash variant
        if (game->colision == 1) game->execute_action(18);
        else game->next_level();
        return;
    }
}

} // namespace touch_input
