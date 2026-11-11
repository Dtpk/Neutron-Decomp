#include "asset_path.h"

#ifdef __ANDROID__
#include <SDL.h>
#include <cstdio>
#endif

#ifdef __ANDROID__
static std::string android_external_path(const char* filename) {
    const char* dir = SDL_AndroidGetExternalStoragePath();
    if (!dir || !*dir) return filename;
    std::string p = dir;
    if (!p.empty() && p.back() != '/') p += '/';
    p += filename;
    return p;
}
#endif

std::string asset_path(const char* filename) {
#ifdef __ANDROID__
    return android_external_path(filename);
#else
    return filename;
#endif
}

std::string asset_path_sound(const char* filename) {
#ifdef __ANDROID__
    std::string override_path = android_external_path(filename);
    if (std::FILE* f = std::fopen(override_path.c_str(), "rb")) {
        std::fclose(f);
        return override_path; // player-supplied replacement wins
    }
    return filename; // no override -- SDL resolves this against assets/ in the APK
#else
    return filename;
#endif
}
