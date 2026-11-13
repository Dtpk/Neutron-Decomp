#include "asset_path.h"

#ifdef __ANDROID__
#include <SDL.h>
#include <cstdio>
#endif

#ifdef NXDK
#include <cstdio>
#include <cstring>
#include <nxdk/mount.h>
#include <nxdk/path.h>
#include <windows.h>
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

#ifdef NXDK
// Xbox has no "external storage" concept like Android's -- the real
// homebrew save-data convention (also used by e.g. the hode nxdk port,
// JayFoxRox/hode#1) is E:\UDATA\<app>\, the hard drive partition reserved
// for save/user data. D:\ -- where the running XBE itself lives -- is the
// disc/XISO content and is read-only when actually run from a real disc,
// so it can't be used for the save file, and isn't a reliable place for
// the player to drop a custom map.txt either.
static bool g_xbox_udata_ready = false;

static std::string xbox_udata_path(const char* filename) {
    if (!g_xbox_udata_ready) {
        if (!nxIsDriveMounted('E')) {
            nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
        }
        CreateDirectoryA("E:\\UDATA", NULL);
        CreateDirectoryA("E:\\UDATA\\neutron", NULL);
        g_xbox_udata_ready = true;
    }
    std::string p = "E:\\UDATA\\neutron\\";
    p += filename;
    return p;
}

// Real bug this fixes: a bare relative filename (e.g. "travel.wav") never
// resolves to anything on Xbox, on ANY partition -- confirmed directly in
// nxdk's own CreateFileA() (lib/winapi/fileio.c), which resolves its
// lpFileName via InitializeObjectAttributes() against
// ObDosDevicesDirectory() (the kernel's raw \??\ device-object
// namespace), NOT against "the directory the running XBE is in." There
// is no current-working-directory concept here at all, unlike every
// other platform this project targets.
//
// A first attempt handed CreateFileA() the raw \Device\Harddisk0\
// PartitionN\...\ path straight from nxGetCurrentXbeNtPath() -- this
// LOOKED right (confirmed on real hardware: it correctly identified
// \Device\Harddisk0\Partition6\Games\Neutron\, matching where the XBE
// actually was, on the F: partition) but still reported NOT FOUND.
// Real cause: CreateFileA() resolves EVERY name against
// ObDosDevicesDirectory() (the kernel's \??\ drive-letter symlink
// table) regardless of whether the name already looks absolute -- a
// raw \Device\...\ string just gets looked up AS IF it were itself a
// path under \??\ (i.e. an object literally named "Device" under
// \??\, which doesn't exist), not treated as a fully-qualified
// override. Confirmed by every one of nxdk's own real file-I/O
// samples (winapi_filefind, winapi_drivelist): they ALL explicitly
// nxMountDrive() a letter first and use plain "X:\..." paths for
// actual file access -- never a raw \Device\...\ string.
//
// Real fix: mount a dedicated drive letter ('N', for Neutron -- not
// one of hal/fileio.c's own hardcoded C/D/E/F/G/X/Y/Z partition-letter
// assumptions, so this can't collide with what any of those
// conventionally mean elsewhere) pointing at the exact NT directory
// computed from nxGetCurrentXbeNtPath(), then build an "N:\..." path
// from that -- same two-step "compute the real NT path, then
// nxMountDrive() a letter onto it" pattern xbox_udata_path() above
// already uses successfully for E:\UDATA\neutron\, just pointed at a
// different target.
static bool g_xbox_bundled_ready = false;

static std::string xbox_bundled_path(const char* filename) {
    if (!g_xbox_bundled_ready) {
        char xbe_path[MAX_PATH];
        nxGetCurrentXbeNtPath(xbe_path);
        char* last_slash = std::strrchr(xbe_path, '\\');
        if (last_slash) {
            *(last_slash + 1) = '\0';
        }
        if (!nxIsDriveMounted('N')) {
            nxMountDrive('N', xbe_path);
        }
        g_xbox_bundled_ready = true;
    }
    std::string p = "N:\\";
    p += filename;
    return p;
}
#endif

std::string asset_path(const char* filename) {
#ifdef __ANDROID__
    return android_external_path(filename);
#elif defined(NXDK)
    return xbox_udata_path(filename);
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
#elif defined(NXDK)
    // Real audio now exists (see shim/sdl_mixer_stub -- a small real
    // Mix_* backend over nxdk's own SDL2 audio driver, not a stub
    // anymore). Same override story as Android: check the save location
    // (E:\UDATA\neutron\) first -- a player-supplied replacement wins --
    // and fall back to xbox_bundled_path() otherwise (see that function's
    // own comment for why a bare filename never worked here at all) --
    // i.e. whatever bundled default sits next to default.xbe (see
    // neutron-xbox's own Makefile/README for how those get onto the XISO).
    std::string override_path = xbox_udata_path(filename);
    if (std::FILE* f = std::fopen(override_path.c_str(), "rb")) {
        std::fclose(f);
        return override_path;
    }
    return xbox_bundled_path(filename);
#else
    return filename;
#endif
}
