/* No-op SDL_mixer stand-in for the Xbox build.
 *
 * nxdk does not bundle SDL2_mixer (its own .gitmodules only vendors
 * SDL2, SDL2_image, SDL_ttf -- confirmed, not guessed). sound_mixer.h's
 * own comment already documents that this class is designed to degrade
 * to silent no-ops when "a future port with no audio backend at all"
 * shows up -- this header is exactly that case: every call here behaves
 * the same way a real Mix_OpenAudio() failure already does on every
 * other platform (has_audio_device() returns false, every playback call
 * becomes a no-op). Real Xbox audio (building SDL2_mixer against nxdk's
 * own SDL2 fork, or a native nxdk-audio backend) is tracked as follow-up
 * work, not attempted here.
 */
#pragma once

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mix_Chunk { int allocated; Uint8* abuf; Uint32 alen; Uint8 volume; } Mix_Chunk;

#define MIX_DEFAULT_FORMAT 0
#define MIX_MAX_VOLUME 128

static inline int Mix_OpenAudio(int, Uint16, int, int) { return -1; }
static inline void Mix_CloseAudio(void) {}
static inline const char* Mix_GetError(void) { return "SDL_mixer not available on this platform (stub)"; }
static inline Mix_Chunk* Mix_LoadWAV(const char*) { return nullptr; }
static inline void Mix_FreeChunk(Mix_Chunk*) {}
static inline int Mix_QuerySpec(int*, Uint16*, int*) { return 0; }
static inline int Mix_Volume(int, int) { return 0; }
static inline int Mix_Playing(int) { return 0; }
static inline int Mix_PlayChannel(int, Mix_Chunk*, int) { return -1; }
static inline int Mix_HaltChannel(int) { return 0; }
static inline int Mix_SetPanning(int, Uint8, Uint8) { return 0; }

#ifdef __cplusplus
}
#endif
