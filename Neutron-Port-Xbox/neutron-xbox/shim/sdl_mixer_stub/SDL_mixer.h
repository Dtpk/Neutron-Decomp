/* Real (if minimal) SDL_mixer stand-in for the Xbox build, backed by
 * nxdk's own real SDL2 audio driver (lib/sdl/SDL2/src/audio/xbox) --
 * NOT the no-op stub this file used to be. nxdk doesn't bundle
 * SDL2_mixer itself (confirmed via its .gitmodules -- only SDL2,
 * SDL2_image, SDL_ttf), and porting all of real SDL2_mixer's own
 * CMake-based build through nxdk's Makefile convention would be a lot
 * of integration work for features this game never uses (streaming
 * music, non-WAV decoders, etc. -- sound_mixer.cpp only ever calls
 * Mix_LoadWAV() on plain bundled .wav files, same as every other
 * platform). So this implements exactly the small Mix_* surface
 * sound_mixer.cpp/main_xbox.cpp actually use, as real functions (see
 * SDL_mixer_xbox.cpp) instead of SDL_mixer itself: a fixed pool of
 * mixing channels over one real SDL audio device, WAV loading via
 * SDL2 core's own SDL_LoadWAV/SDL_ConvertAudio (already proven to
 * exist and build for nxdk -- see SDL_wave.c in its vendored SDL2).
 */
#pragma once

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mix_Chunk { int allocated; Uint8* abuf; Uint32 alen; Uint8 volume; } Mix_Chunk;

#define MIX_DEFAULT_FORMAT AUDIO_S16LSB
#define MIX_MAX_VOLUME 128
#define MIX_CHANNELS 8

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void Mix_CloseAudio(void);
const char* Mix_GetError(void);
Mix_Chunk* Mix_LoadWAV(const char* file);
void Mix_FreeChunk(Mix_Chunk* chunk);
int Mix_QuerySpec(int* frequency, Uint16* format, int* channels);
int Mix_Volume(int channel, int volume);
int Mix_Playing(int channel);
int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops);
int Mix_HaltChannel(int channel);
int Mix_SetPanning(int channel, Uint8 left, Uint8 right);

#ifdef __cplusplus
}
#endif
