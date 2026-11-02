#pragma once
#include <string>

// Resolves a loose-file asset name (travel.wav, explosion.wav, music.wav,
// neutron_save.txt, map.txt) to wherever it should actually be read/written
// from on the current platform.
//
// Desktop: unchanged -- returns the name as-is (cwd-relative, same as
// before this existed), so the existing "next to the executable" desktop
// behavior isn't touched at all.
//
// Android: there's no meaningful "next to the binary" location in an APK,
// and the player is expected to place these files themselves (see
// UNINSTALL.txt-style docs) in the app's own external files directory --
// resolved here via SDL_AndroidGetExternalStoragePath() (SDL already wraps
// the JNI call), typically
// /storage/emulated/0/Android/data/<package>/files/.
std::string asset_path(const char* filename);

// Same idea, but for assets that ship a default inside the app itself
// (currently: the three sound effects) and can still be overridden by the
// player.
//
// Desktop: unchanged, same as asset_path() -- cwd-relative, no bundling
// concept applies there.
//
// Android: checks the same external-storage override location as
// asset_path() first (a file the player dropped in themselves wins, same
// modding story as before). If nothing's there, returns the bare filename
// instead of an external-storage path -- SDL's own Android file layer
// (SDL_RWFromFile, and Mix_LoadWAV through it) resolves a relative path
// against the APK's bundled assets/ folder automatically, no manual
// extraction needed. Only makes sense for files that actually have a
// bundled default (see android-apk/app/src/main/assets/) -- map.txt and
// neutron_save.txt have no such default and keep using plain asset_path().
std::string asset_path_sound(const char* filename);
