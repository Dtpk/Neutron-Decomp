/* Real Mix_* implementation backing SDL_mixer.h's declarations -- see
 * that header's own comment for why this exists instead of porting all
 * of real SDL2_mixer. Backed entirely by nxdk's own real SDL2 audio
 * driver (SDL_OpenAudioDevice + a mixing callback) and SDL2 core's own
 * WAV loader (SDL_LoadWAV/SDL_BuildAudioCVT/SDL_ConvertAudio -- already
 * vendored and building for nxdk, confirmed via lib/sdl/SDL2/src/audio/
 * SDL_wave.c). A fixed pool of MIX_CHANNELS mixing slots is enough for
 * what this game actually plays at once (travel + music + the
 * occasional explosion -- never more than a handful of sounds
 * simultaneously), so unlike real SDL_mixer this never needs to steal a
 * channel from something still playing; Mix_PlayChannel(-1, ...) just
 * returns -1 (matching "no device"/failure semantics every caller
 * already handles) if the pool is ever exhausted.
 */
#include "SDL_mixer.h"
#include <cstring>
#include <algorithm>

namespace {

struct Channel {
    Mix_Chunk* chunk = nullptr;
    Uint32 position = 0;   // byte offset into chunk->abuf
    int loops = 0;          // -1 = infinite, 0 = don't repeat, >0 = repeat N more times
    int volume = MIX_MAX_VOLUME;
    Uint8 pan_left = 255, pan_right = 255;
    bool active = false;
};

Channel g_channels[MIX_CHANNELS];
SDL_AudioDeviceID g_device = 0;
SDL_AudioSpec g_spec;
bool g_device_open = false;
char g_error[256] = {0};

void set_error(const char* msg) {
    std::strncpy(g_error, msg, sizeof(g_error) - 1);
    g_error[sizeof(g_error) - 1] = '\0';
}

// Mixes every active channel's S16 stereo samples into the output buffer.
// Runs on SDL's own audio thread -- every field this touches is only
// ever mutated elsewhere under SDL_Lock/UnlockAudioDevice (see the Mix_*
// functions below), matching real SDL_mixer's own thread-safety
// convention for exactly this reason.
void audio_callback(void*, Uint8* stream, int len) {
    std::memset(stream, 0, (size_t)len);
    Sint16* out = reinterpret_cast<Sint16*>(stream);
    int out_samples = len / (int)sizeof(Sint16); // interleaved L/R int16 slots

    for (int c = 0; c < MIX_CHANNELS; c++) {
        Channel& ch = g_channels[c];
        if (!ch.active || !ch.chunk || !ch.chunk->abuf) continue;

        const Sint16* src = reinterpret_cast<const Sint16*>(ch.chunk->abuf);
        Uint32 src_samples = ch.chunk->alen / sizeof(Sint16);
        Uint32 pos = ch.position / sizeof(Sint16);

        float vol_scale = ch.volume / (float)MIX_MAX_VOLUME;
        float left_scale = vol_scale * (ch.pan_left / 255.0f);
        float right_scale = vol_scale * (ch.pan_right / 255.0f);

        for (int i = 0; i + 1 < out_samples; i += 2) {
            if (pos + 1 >= src_samples) {
                if (ch.loops < 0) {
                    pos = 0; // infinite loop
                } else if (ch.loops > 0) {
                    ch.loops--;
                    pos = 0;
                } else {
                    ch.active = false;
                    break;
                }
                if (src_samples < 2) { ch.active = false; break; }
            }
            Sint32 l = out[i] + (Sint32)(src[pos] * left_scale);
            Sint32 r = out[i + 1] + (Sint32)(src[pos + 1] * right_scale);
            out[i] = (Sint16)std::clamp(l, -32768, 32767);
            out[i + 1] = (Sint16)std::clamp(r, -32768, 32767);
            pos += 2;
        }
        ch.position = pos * sizeof(Sint16);
    }
}

} // namespace

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize) {
    if (g_device_open) return 0;

    SDL_AudioSpec wanted;
    std::memset(&wanted, 0, sizeof(wanted));
    wanted.freq = frequency;
    wanted.format = format;
    wanted.channels = (Uint8)channels;
    wanted.samples = (Uint16)chunksize;
    wanted.callback = audio_callback;
    wanted.userdata = nullptr;

    // 0 allowed-changes flags: g_spec comes back identical to wanted, or
    // the open fails outright -- Mix_LoadWAV below relies on g_spec being
    // exactly what every chunk gets converted to.
    g_device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &g_spec, 0);
    if (g_device == 0) {
        set_error(SDL_GetError());
        return -1;
    }

    for (int i = 0; i < MIX_CHANNELS; i++) g_channels[i] = Channel();
    g_device_open = true;
    SDL_PauseAudioDevice(g_device, 0); // starts the callback running
    return 0;
}

void Mix_CloseAudio(void) {
    if (!g_device_open) return;
    SDL_CloseAudioDevice(g_device);
    g_device = 0;
    g_device_open = false;
}

const char* Mix_GetError(void) {
    return g_error[0] ? g_error : SDL_GetError();
}

Mix_Chunk* Mix_LoadWAV(const char* file) {
    if (!g_device_open) {
        set_error("Mix_LoadWAV: audio device not open");
        return nullptr;
    }

    SDL_AudioSpec wav_spec;
    Uint8* wav_buf = nullptr;
    Uint32 wav_len = 0;
    if (SDL_LoadWAV(file, &wav_spec, &wav_buf, &wav_len) == nullptr) {
        set_error(SDL_GetError());
        return nullptr;
    }

    Uint8* final_buf;
    Uint32 final_len;

    if (wav_spec.format == g_spec.format && wav_spec.channels == g_spec.channels &&
        wav_spec.freq == g_spec.freq) {
        // Already in the device's own format -- no conversion needed.
        final_buf = wav_buf;
        final_len = wav_len;
    } else {
        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt, wav_spec.format, wav_spec.channels, wav_spec.freq,
                               g_spec.format, g_spec.channels, g_spec.freq) < 0) {
            set_error(SDL_GetError());
            SDL_FreeWAV(wav_buf);
            return nullptr;
        }
        cvt.len = (int)wav_len;
        Uint8* cvt_buf = (Uint8*)SDL_malloc((size_t)wav_len * (size_t)cvt.len_mult);
        if (!cvt_buf) {
            set_error("Mix_LoadWAV: out of memory converting audio");
            SDL_FreeWAV(wav_buf);
            return nullptr;
        }
        std::memcpy(cvt_buf, wav_buf, wav_len);
        cvt.buf = cvt_buf;
        SDL_FreeWAV(wav_buf); // the converted copy already has everything it needs
        if (SDL_ConvertAudio(&cvt) < 0) {
            set_error(SDL_GetError());
            SDL_free(cvt_buf);
            return nullptr;
        }
        final_buf = cvt_buf;
        final_len = cvt.len_cvt;
    }

    Mix_Chunk* chunk = new Mix_Chunk();
    chunk->allocated = 1;
    chunk->abuf = final_buf;
    chunk->alen = final_len;
    chunk->volume = MIX_MAX_VOLUME;
    return chunk;
}

void Mix_FreeChunk(Mix_Chunk* chunk) {
    if (!chunk) return;
    // Stop any channel currently playing this chunk first, or the mix
    // callback could read freed memory on the very next audio buffer.
    if (g_device_open) {
        SDL_LockAudioDevice(g_device);
        for (int i = 0; i < MIX_CHANNELS; i++) {
            if (g_channels[i].chunk == chunk) g_channels[i].active = false;
        }
        SDL_UnlockAudioDevice(g_device);
    }
    if (chunk->abuf) SDL_free(chunk->abuf);
    delete chunk;
}

int Mix_QuerySpec(int* frequency, Uint16* format, int* channels) {
    if (!g_device_open) return 0;
    if (frequency) *frequency = g_spec.freq;
    if (format) *format = g_spec.format;
    if (channels) *channels = g_spec.channels;
    return 1;
}

int Mix_Volume(int channel, int volume) {
    if (!g_device_open || channel < 0 || channel >= MIX_CHANNELS) return 0;
    int clamped = std::clamp(volume, 0, MIX_MAX_VOLUME);
    SDL_LockAudioDevice(g_device);
    int old = g_channels[channel].volume;
    g_channels[channel].volume = clamped;
    SDL_UnlockAudioDevice(g_device);
    return old;
}

int Mix_Playing(int channel) {
    if (!g_device_open || channel < 0 || channel >= MIX_CHANNELS) return 0;
    SDL_LockAudioDevice(g_device);
    bool active = g_channels[channel].active;
    SDL_UnlockAudioDevice(g_device);
    return active ? 1 : 0;
}

int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops) {
    if (!g_device_open || !chunk) return -1;

    int target = channel;
    SDL_LockAudioDevice(g_device);
    if (target < 0) {
        target = -1;
        for (int i = 0; i < MIX_CHANNELS; i++) {
            if (!g_channels[i].active) { target = i; break; }
        }
    }
    if (target < 0 || target >= MIX_CHANNELS) {
        SDL_UnlockAudioDevice(g_device);
        return -1;
    }
    Channel& ch = g_channels[target];
    ch.chunk = chunk;
    ch.position = 0;
    ch.loops = loops;
    ch.volume = MIX_MAX_VOLUME;
    ch.pan_left = 255;
    ch.pan_right = 255;
    ch.active = true;
    SDL_UnlockAudioDevice(g_device);
    return target;
}

int Mix_HaltChannel(int channel) {
    if (!g_device_open) return 0;
    SDL_LockAudioDevice(g_device);
    if (channel < 0) {
        for (int i = 0; i < MIX_CHANNELS; i++) g_channels[i].active = false;
    } else if (channel < MIX_CHANNELS) {
        g_channels[channel].active = false;
    }
    SDL_UnlockAudioDevice(g_device);
    return 0;
}

int Mix_SetPanning(int channel, Uint8 left, Uint8 right) {
    if (!g_device_open || channel < 0 || channel >= MIX_CHANNELS) return 0;
    SDL_LockAudioDevice(g_device);
    g_channels[channel].pan_left = left;
    g_channels[channel].pan_right = right;
    SDL_UnlockAudioDevice(g_device);
    return 1;
}
