#include "sound_mixer.h"
#include <cstring>

sound_mixer::sound_mixer()
    : TOTAL_SOUNDS(12),
      TOTAL_MUSIC(4),
      refresh_effects(false),
      refresh_music(false) {
    std::memset(effects, 0, sizeof(effects));
    std::memset(music, 0, sizeof(music));
}

sound_mixer::~sound_mixer() {}

void sound_mixer::play_effect(int ident) {
    if (!refresh_effects) {
        refresh_effects = true;
    }
    if (ident >= 0 && ident < 20) {
        effects[ident] = true;
    }
}

void sound_mixer::play_music(int ident) {
    if (!refresh_music) {
        refresh_music = true;
    }
    if (ident >= 0 && ident < 10) {
        music[ident] = true;
    }
}

void sound_mixer::stop_all(int ident) {
    if (ident == 0) {
        for (int i = 0; i < TOTAL_SOUNDS && i < 20; i++) {
            effects[i] = false;
        }
    } else {
        for (int i = 0; i < TOTAL_MUSIC && i < 10; i++) {
            music[i] = false;
        }
    }
}
