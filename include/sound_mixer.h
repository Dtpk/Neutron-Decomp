#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

class sound_mixer {
public:
    int TOTAL_SOUNDS;
    int TOTAL_MUSIC;
    bool refresh_effects;
    bool refresh_music;
    bool effects[20];
    bool music[10];

    sound_mixer();
    ~sound_mixer();

    void play_effect(int ident);
    void play_music(int ident);
    void stop_all(int ident);
};
