#include "sound_mixer.h"
#include <cstring>
#include <cmath>
#include <algorithm>

// Pan sweep duration for the corner-warp effect, in simulation ticks.
static const int WARP_DURATION_TICKS = 18;
// Distance (in world units) at which a sound has faded to silence.
static const float DISTANCE_FALLOFF_RANGE = 40.0f;
// Turn-duck dip duration (ticks) and depth (0..1, fraction of volume cut at
// the deepest point).
static const int DUCK_DURATION_TICKS = 18;
static const float DUCK_DEPTH = 0.6f;

sound_mixer::sound_mixer()
    : TOTAL_SOUNDS(12),
      TOTAL_MUSIC(4),
      refresh_effects(false),
      refresh_music(false),
      travel_chunk(nullptr),
      explosion_chunk(nullptr),
      music_chunk(nullptr),
      travel_channel(-1),
      music_channel(-1),
      enabled(true),
      volume_percent(100),
      warp_ticks_remaining(0),
      warp_ticks_total(0),
      warp_direction(0),
      duck_ticks_remaining(0),
      duck_ticks_total(0) {
    std::memset(effects, 0, sizeof(effects));
    std::memset(music, 0, sizeof(music));

    // Each of these is independently optional -- a missing file just leaves
    // the chunk null, and every playback call below already checks for that.
    travel_chunk = Mix_LoadWAV("travel.wav");
    explosion_chunk = Mix_LoadWAV("explosion.wav");
    music_chunk = Mix_LoadWAV("music.wav");
}

sound_mixer::~sound_mixer() {
    stop_travel();
    stop_music();
    if (travel_chunk) Mix_FreeChunk(travel_chunk);
    if (explosion_chunk) Mix_FreeChunk(explosion_chunk);
    if (music_chunk) Mix_FreeChunk(music_chunk);
}

bool sound_mixer::has_audio_device() const {
    // Mix_QuerySpec fails if Mix_OpenAudio was never successfully called
    // (no device, or a future port with no audio backend at all).
    int freq; Uint16 fmt; int chans;
    return Mix_QuerySpec(&freq, &fmt, &chans) != 0;
}

float sound_mixer::distance_volume(float cam_x, float cam_y, float cam_z,
                                    float src_x, float src_y, float src_z) {
    float dx = cam_x - src_x, dy = cam_y - src_y, dz = cam_z - src_z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    float vol = 1.0f - dist / DISTANCE_FALLOFF_RANGE;
    return std::clamp(vol, 0.0f, 1.0f);
}

void sound_mixer::set_enabled(bool on) {
    enabled = on;
    if (!enabled) {
        stop_travel();
        stop_music();
    }
}

void sound_mixer::set_volume(int percent) {
    volume_percent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    if (music_channel >= 0) {
        Mix_Volume(music_channel, (int)(volume_percent / 100.0f * MIX_MAX_VOLUME));
    }
    // travel's volume is distance-scaled and recomputed every tick in
    // update_travel(), which already factors in volume_percent.
}

void sound_mixer::start_music() {
    if (!enabled || !has_audio_device() || !music_chunk) return;
    if (music_channel >= 0 && Mix_Playing(music_channel)) return;
    music_channel = Mix_PlayChannel(-1, music_chunk, -1);
    if (music_channel >= 0) Mix_Volume(music_channel, (int)(volume_percent / 100.0f * MIX_MAX_VOLUME));
}

void sound_mixer::stop_music() {
    if (music_channel >= 0) {
        Mix_HaltChannel(music_channel);
        music_channel = -1;
    }
}

void sound_mixer::start_travel() {
    if (!enabled || !has_audio_device() || !travel_chunk) return;
    if (travel_channel >= 0 && Mix_Playing(travel_channel)) return;
    travel_channel = Mix_PlayChannel(-1, travel_chunk, -1);
}

void sound_mixer::stop_travel() {
    if (travel_channel >= 0) {
        Mix_HaltChannel(travel_channel);
        travel_channel = -1;
    }
    warp_ticks_remaining = 0;
    duck_ticks_remaining = 0;
}

void sound_mixer::update_travel() {
    if (travel_channel < 0) return;

    float vol = volume_percent / 100.0f;

    if (duck_ticks_remaining > 0) {
        float t = 1.0f - (float)duck_ticks_remaining / (float)duck_ticks_total;
        float dip = std::sin(t * (float)M_PI); // 0 -> 1 -> 0, trough at t=0.5
        vol *= 1.0f - DUCK_DEPTH * dip;
        duck_ticks_remaining--;
    }

    Mix_Volume(travel_channel, (int)(vol * MIX_MAX_VOLUME));

    if (warp_ticks_remaining > 0) {
        // Sweep pan from the starting side, through center, out the other
        // side, and back to center as the warp completes -- an SDL2_mixer
        // approximation of a siren passing by (no pitch bend, just motion).
        float t = 1.0f - (float)warp_ticks_remaining / (float)warp_ticks_total;
        float sweep = std::sin(t * (float)M_PI); // 0 -> 1 -> 0
        float pan = (float)warp_direction * sweep; // -1..1
        Uint8 left = (Uint8)std::clamp(255.0f * (1.0f - std::max(0.0f, pan)), 0.0f, 255.0f);
        Uint8 right = (Uint8)std::clamp(255.0f * (1.0f + std::min(0.0f, pan)), 0.0f, 255.0f);
        Mix_SetPanning(travel_channel, left, right);
        warp_ticks_remaining--;
        if (warp_ticks_remaining == 0) {
            Mix_SetPanning(travel_channel, 255, 255); // back to centered
        }
    }
}

void sound_mixer::trigger_corner_warp(int direction) {
    if (travel_channel < 0) return;
    warp_direction = direction < 0 ? -1 : 1;
    warp_ticks_total = WARP_DURATION_TICKS;
    warp_ticks_remaining = WARP_DURATION_TICKS;
}

void sound_mixer::trigger_turn_duck() {
    if (travel_channel < 0) return;
    duck_ticks_total = DUCK_DURATION_TICKS;
    duck_ticks_remaining = DUCK_DURATION_TICKS;
}

void sound_mixer::play_explosion(float cam_x, float cam_y, float cam_z,
                                  float src_x, float src_y, float src_z) {
    if (!enabled || !has_audio_device() || !explosion_chunk) return;
    float vol = distance_volume(cam_x, cam_y, cam_z, src_x, src_y, src_z) * (volume_percent / 100.0f);
    int channel = Mix_PlayChannel(-1, explosion_chunk, 0);
    if (channel >= 0) Mix_Volume(channel, (int)(vol * MIX_MAX_VOLUME));
}

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
