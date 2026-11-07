#include "controller_input.h"
#include "neutron.h"
#include "stage_interface.h"
#include "sound_mixer.h"
#include "split_screen_menu.h"

// Implemented in main.cpp: bridges to the SDL window handle (g_window) that
// toggle_fullscreen() needs and that this module has no access to (and
// shouldn't -- window lifetime stays main.cpp's job).
void app_toggle_fullscreen();

namespace controller_input {

namespace {

SDL_GameController* s_controllers[4] = {nullptr, nullptr, nullptr, nullptr};
SDL_JoystickID s_joystick_ids[4] = {-1, -1, -1, -1};

const Sint16 AXIS_DEADZONE = 12000;
const Sint16 TRIGGER_DEADZONE = 8000; // triggers report 0..32767, not -32768..32767 like sticks
const Uint32 AXIS_REPEAT_MS = 150;
Uint32 s_next_axis_repeat = 0;

SDL_GameController* first_controller() {
    for (int i = 0; i < 4; i++) {
        if (s_controllers[i]) return s_controllers[i];
    }
    return nullptr;
}

void open_slot(int device_index) {
    if (!SDL_IsGameController(device_index)) return;
    for (int slot = 0; slot < 4; slot++) {
        if (s_controllers[slot]) continue;
        SDL_GameController* c = SDL_GameControllerOpen(device_index);
        if (c) {
            s_controllers[slot] = c;
            s_joystick_ids[slot] = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
        }
        return;
    }
}

enum class Direction { Up, Down, Left, Right };

// Mirrors main.cpp's handle_input()'s Up/Down/Left/Right branches
// (main.cpp:145-428), context for context, using a logical Direction
// instead of an SDL_Keycode. Keep this in lockstep with main.cpp if that
// handler ever changes -- see the header comment on why this is a
// deliberately separate, parallel copy rather than a shared helper.
void handle_direction(neutron* game, Direction dir) {
    if (!game) return;
    stage_interface* ui = game->interface;

    // Graphics & Sound submenu (actual_option 30-33)
    if (game->actual_stage == 1000 && game->game_status != 13 &&
        ui && ui->actual_option >= 30 && ui->actual_option < 34) {
        int& opt = ui->actual_option;
        if (dir == Direction::Up) { opt = (opt > 30) ? opt - 1 : 33; return; }
        if (dir == Direction::Down) { opt = (opt < 33) ? opt + 1 : 30; return; }
        if (dir == Direction::Left) { if (opt == 31) game->execute_action(29); return; }
        if (dir == Direction::Right) { if (opt == 31) game->execute_action(28); return; }
        return;
    }

    // Options submenu (actual_option 10-13)
    if (game->actual_stage == 1000 && game->game_status != 13 &&
        ui && ui->actual_option >= 10 && ui->actual_option < 14) {
        int& opt = ui->actual_option;
        if (dir == Direction::Up) { opt = (opt > 10) ? opt - 1 : 13; return; }
        if (dir == Direction::Down) { opt = (opt < 13) ? opt + 1 : 10; return; }
        return; // Left/Right do nothing on this submenu, matching keyboard
    }

    // Main menu (the real/live one -- direct actual_option arithmetic, NOT menu_move())
    if (game->actual_stage == 1000 && game->game_status != 13) {
        if (!ui) return;
        if (dir == Direction::Up) { ui->actual_option = (ui->actual_option + 6) % 7; return; }
        if (dir == Direction::Down) { ui->actual_option = (ui->actual_option + 1) % 7; return; }
        return; // Left/Right unused on the main menu
    }

    // Stage Select
    if (game->actual_stage == 3000) {
        if (dir == Direction::Left) { game->execute_action(19); return; }
        if (dir == Direction::Right) { game->execute_action(20); return; }
        return;
    }

    // In-game (gameplay or Training)
    if (game->game_status == 0) {
        if (game->paused) return; // swallow steering while paused, matches keyboard
        if (dir == Direction::Left) { game->execute_action(0); return; }
        if (dir == Direction::Right) { game->execute_action(1); return; }
        return;
    }

    // Scripted tutorial
    if (game->game_status == 10) {
        if (dir == Direction::Left) { game->execute_action(0); return; }
        if (dir == Direction::Right) { game->execute_action(1); return; }
        return;
    }
}

// Mirrors every Enter/Space/Z branch in main.cpp's handle_input().
void handle_confirm(neutron* game) {
    if (!game) return;
    stage_interface* ui = game->interface;

    if (game->actual_stage == 1000 &&
        (game->game_status == 1000 || game->game_status == 4)) {
        game->intro_step = 93;
        game->game_status = 6;
        int active_btns[4] = {7, 8, 9, 10};
        if (ui) ui->activate_buttons(active_btns, 4);
        return;
    }

    if (game->actual_stage == 1000 && game->game_status != 13 &&
        ui && ui->actual_option >= 30 && ui->actual_option < 34) {
        switch (ui->actual_option) {
            case 30: game->execute_action(17); break;
            case 32: app_toggle_fullscreen(); break;
            case 33: ui->actual_option = 10; break;
        }
        return;
    }

    if (game->actual_stage == 1000 && game->game_status != 13 &&
        ui && ui->actual_option >= 10 && ui->actual_option < 14) {
        switch (ui->actual_option) {
            case 10: ui->actual_option = 30; break;
            case 11: game->execute_action(15); break;
            case 12: game->execute_action(16); break;
            case 13: ui->actual_option = 0; break;
        }
        return;
    }

    if (game->actual_stage == 1000 && game->game_status != 13) {
        int opt = ui ? ui->actual_option : 0;
        if (opt == 0) game->execute_action(2);
        else if (opt == 1) game->execute_action(11);
        else if (opt == 2) game->execute_action(5);
        else if (opt == 3) game->execute_action(12);
        else if (opt == 4) { split_screen_menu::reset(game); game->game_status = 20; }
        else if (opt == 5) { if (ui) ui->actual_option = 10; }
        else game->game_status = 13;
        return;
    }

    if (game->game_status == 13) { game->execute_action(8); return; }

    if (game->actual_stage == 3000) { game->execute_action(21); return; }

    if (game->game_status == 0) {
        if (game->paused) { game->paused = false; return; }
        if (game->actual_stage == 2000) { game->execute_action(9); return; }
        return;
    }

    if (game->game_status == 10) {
        if (game->tutorial_step != 10 && game->tutorial_step != 16) game->step_tutorial();
        return;
    }

    if (game->game_status == 1) { game->execute_action(18); return; }

    if (game->game_status == 8 || game->game_status == 3) {
        if (game->colision == 1) game->execute_action(18);
        else game->next_level();
        return;
    }
}

// Mirrors every Escape branch EXCEPT the top-level main-menu quit (Escape
// there does a hard g_running=false in main.cpp -- deliberately not
// mapped to any controller button, so there's no accidental-quit button
// on a pad; keyboard Escape remains the only way to quit outright).
void handle_back(neutron* game) {
    if (!game) return;
    stage_interface* ui = game->interface;

    if (game->actual_stage == 1000 && game->game_status != 13 &&
        ui && ui->actual_option >= 30 && ui->actual_option < 34) {
        ui->actual_option = 10;
        return;
    }
    if (game->actual_stage == 1000 && game->game_status != 13 &&
        ui && ui->actual_option >= 10 && ui->actual_option < 14) {
        ui->actual_option = 0;
        return;
    }
    // Top-level main menu: no-op, see comment above.
    if (game->actual_stage == 1000 && game->game_status != 13) return;

    if (game->game_status == 13) { game->execute_action(8); return; }
    if (game->actual_stage == 3000) { game->execute_action(8); return; }

    if (game->game_status == 0) {
        if (game->paused) { game->paused = false; game->execute_action(8); return; }
        game->paused = true;
        if (game->mixer) game->mixer->stop_travel();
        return;
    }

    if (game->game_status == 10) { game->execute_action(8); return; }
    if (game->game_status == 1) { game->execute_action(8); return; }
    if (game->game_status == 8 || game->game_status == 3) { game->execute_action(8); return; }
}

// Mirrors the 'V' camera-toggle key, the only context-dependent single key
// not already covered by direction/confirm/back above.
void handle_vision_toggle(neutron* game) {
    if (!game) return;
    if (game->game_status == 0 || game->game_status == 10) {
        game->vision = 1 - game->vision;
    }
}

} // namespace

void init() {
    SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt"); // optional; fine if missing

    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; i++) {
        open_slot(i);
    }
}

void shutdown() {
    for (int i = 0; i < 4; i++) {
        if (s_controllers[i]) {
            SDL_GameControllerClose(s_controllers[i]);
            s_controllers[i] = nullptr;
            s_joystick_ids[i] = -1;
        }
    }
}

void handle_device_added(const SDL_ControllerDeviceEvent& e) {
    // e.which is a joystick DEVICE INDEX for this event (not an instance id).
    open_slot(e.which);
}

void handle_device_removed(const SDL_ControllerDeviceEvent& e) {
    // e.which is an SDL_JoystickID (instance id) for this event.
    for (int slot = 0; slot < 4; slot++) {
        if (s_controllers[slot] && s_joystick_ids[slot] == e.which) {
            SDL_GameControllerClose(s_controllers[slot]);
            s_controllers[slot] = nullptr;
            s_joystick_ids[slot] = -1;
            return;
        }
    }
}

void handle_button_down(neutron* game, const SDL_ControllerButtonEvent& e) {
    if (!game) return;

    // Phase 1 is single-player: only the first-connected controller drives
    // the game. Other controllers stay inert until Phase 3's join screen
    // assigns them a slot.
    SDL_GameController* first = first_controller();
    if (!first) return;
    SDL_JoystickID first_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(first));
    if (e.which != first_id) return;

    switch (e.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    handle_direction(game, Direction::Up); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  handle_direction(game, Direction::Down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  handle_direction(game, Direction::Left); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: handle_direction(game, Direction::Right); break;
        // Bumpers double as Left/Right too -- requested as a more comfortable
        // alternative to the D-pad/stick for steering/menu nav.
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  handle_direction(game, Direction::Left); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: handle_direction(game, Direction::Right); break;
        case SDL_CONTROLLER_BUTTON_A:          handle_confirm(game); break;
        case SDL_CONTROLLER_BUTTON_START:      handle_back(game); break;
        case SDL_CONTROLLER_BUTTON_B:          handle_vision_toggle(game); break;
        default: break;
    }
}

void poll_axes(neutron* game, Uint32 now_ms) {
    if (!game) return;
    SDL_GameController* c = first_controller();
    if (!c) return;
    if (now_ms < s_next_axis_repeat) return;

    Sint16 x = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 y = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    // Analog triggers double as Left/Right too, same as the bumpers --
    // these report 0 (released) .. 32767 (fully pressed), unlike sticks.
    Sint16 trig_l = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    Sint16 trig_r = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

    bool fired = false;
    if (y < -AXIS_DEADZONE) { handle_direction(game, Direction::Up); fired = true; }
    else if (y > AXIS_DEADZONE) { handle_direction(game, Direction::Down); fired = true; }
    if (x < -AXIS_DEADZONE || trig_l > TRIGGER_DEADZONE) { handle_direction(game, Direction::Left); fired = true; }
    else if (x > AXIS_DEADZONE || trig_r > TRIGGER_DEADZONE) { handle_direction(game, Direction::Right); fired = true; }

    if (fired) s_next_axis_repeat = now_ms + AXIS_REPEAT_MS;
}

int num_connected() {
    int n = 0;
    for (int i = 0; i < 4; i++) if (s_controllers[i]) n++;
    return n;
}

SDL_GameController* get_controller(int index) {
    if (index < 0 || index >= 4) return nullptr;
    return s_controllers[index];
}

} // namespace controller_input
