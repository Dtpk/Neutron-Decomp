#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

// Real playback for the three moddable assets (travel.wav, explosion.wav,
// music.wav), loaded from the working directory next to the executable.
// Every one of them is optional: a missing file, a failed Mix_OpenAudio, or
// a future port with no audio backend at all just means these calls are
// silent no-ops. Nothing in gameplay code ever checks a return value from
// this class, so dropping audio entirely costs nothing but silence.
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

    // Mirrors the existing "Sound: on/off" option -- when disabled, every
    // playback call below becomes a no-op (and anything already playing is
    // halted), exactly like a missing file or missing device would.
    void set_enabled(bool on);

    // 0..100, scales every sound this class plays (independent of on/off).
    void set_volume(int percent);
    int get_volume() const { return volume_percent; }

    void start_music();
    void stop_music();

    void start_travel();
    void stop_travel();
    // Call once per simulation tick while travel is playing: cam_* is the
    // listener (camera), src_* is the sound source (the traveling ship).
    // Drives distance-based volume and steps any in-progress corner warp.
    void update_travel(float cam_x, float cam_y, float cam_z,
                        float src_x, float src_y, float src_z);
    // "Siren passing a car" pan sweep, kicked off at the moment the player
    // actually completes a turn onto a new plane (a corner, in this game's
    // grid-of-planes geometry). direction<0 sweeps toward the left speaker
    // first, direction>0 toward the right.
    void trigger_corner_warp(int direction);

    void play_explosion(float cam_x, float cam_y, float cam_z,
                         float src_x, float src_y, float src_z);

private:
    bool has_audio_device() const;
    static float distance_volume(float cam_x, float cam_y, float cam_z,
                                  float src_x, float src_y, float src_z);

    Mix_Chunk* travel_chunk;
    Mix_Chunk* explosion_chunk;
    Mix_Chunk* music_chunk;
    int travel_channel;
    int music_channel;
    bool enabled;
    int volume_percent;

    int warp_ticks_remaining;
    int warp_ticks_total;
    int warp_direction;
};
