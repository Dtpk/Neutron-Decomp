Neutron - Android port: what was done, why, and how to redo it
================================================================
This covers everything done to get the existing desktop (SDL2/OpenGL)
build of Neutron running as an installable Android APK. Written so this
can be reconstructed from scratch with no prior context -- every tool
version, every bug hit, and every file touched is listed.

Status as of writing: installed and tested on a real device via scrcpy.
Confirmed working: rendering (enemies/trails/player), controller input,
touch input, keyboard input, custom map load (map.txt), save file
(neutron_save.txt), bundled + replaceable sound.


1. Goal
-------
Take the existing desktop C++/SDL2/OpenGL (desktop GL, fixed-function)
build and produce an installable arm64 Android APK, without rewriting
the game's own rendering code.


2. Tools needed, and where they ended up
-----------------------------------------
Installed into a fresh Debian 12 container for this work; none of this
comes with the OS by default.

  apt packages (apt-get -o APT::Sandbox::User=root install -y ...):
    - openjdk-17-jdk-headless      (JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64)
    - curl, unzip (to fetch/extract the NDK and SDK)
  Note: this sandbox blocks apt's download helper from dropping
  privileges (setgroups denied) unless you pass
  -o APT::Sandbox::User=root to apt-get -- without it, "apt-get update"
  just dies with "Method http has died unexpectedly!". Also, dbus's
  postinst fails here (tries to chown a setuid helper, blocked by the
  same sandbox) -- harmless, this project never touches dbus, safe to
  ignore / `dpkg --purge --force-depends dbus` if it blocks other
  installs.

  CMake: Debian's packaged 3.25.1 could not configure one of the
  FetchContent'd Android dependencies (a generator-expression feature
  needing 3.26+). Downloaded a newer release directly instead:
    https://github.com/Kitware/CMake/releases/download/v3.31.2/cmake-3.31.2-linux-x86_64.tar.gz
    extracted to /opt/cmake -- use /opt/cmake/bin/cmake for everything
    below, not whatever `cmake` resolves to on PATH.

  Android NDK r27c:
    https://dl.google.com/android/repository/android-ndk-r27c-linux.zip
    extracted to /opt/android-ndk/android-ndk-r27c

  Android SDK command-line tools:
    https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
    extracted so the "cmdline-tools" folder inside becomes
    /opt/android-sdk/cmdline-tools/latest
    Then:
      export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
      export ANDROID_HOME=/opt/android-sdk
      yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager \
        --sdk_root=$ANDROID_HOME "platform-tools" "platforms;android-34" "build-tools;34.0.0"
    (Gradle's own build also auto-installed build-tools 33.0.1 the first
    time it ran, alongside 34.0.0 -- harmless, AGP 8.1.1 apparently wants
    that specific version for something regardless of compileSdkVersion.)

  Gradle: not installed separately -- the SDL2 android-project template
  ships its own wrapper (gradlew / gradle-wrapper.properties), which
  downloaded Gradle 8.1.1 itself on first run. Android Gradle Plugin is
  also pinned to 8.1.1 in the template's root build.gradle.


3. The GLES1 compatibility shim
--------------------------------
Files: include/gles_compat/GL/gl.h, include/gles_compat/GL/glu.h,
src/gles_compat.cpp

The desktop code uses old-style fixed-function OpenGL (glBegin/glVertex/
glEnd immediate mode, GL_QUADS, gluPerspective/gluLookAt). OpenGL ES 1.1
keeps almost the entire fixed-function pipeline (matrix stack, lighting,
texture env, client-side vertex arrays all work identically to desktop
GL1 under the exact same function names) -- the only things it dropped
are immediate mode and GLU. Audited actual usage before deciding how to
handle this: only 7 glBegin/glEnd blocks total across 6 files (6x
GL_QUADS, 1x GL_LINES), ~50 glVertex calls, 4 GLU calls (3x
gluPerspective, 1x gluLookAt). Small and bounded, so: wrote a shim rather
than rewriting those 6 files.

How the shim works: it provides its own GL/gl.h and GL/glu.h, placed
AHEAD of the real <GLES/gl.h> on the include path only for the Android
build (desktop never sees this directory, keeps using the real Mesa
<GL/gl.h>/<GL/glu.h> unchanged). No source file needed to change a single
#include line.

  - glBegin/glVertex2f/glVertex3f/glEnd: buffers vertex positions
    client-side into a small array. At glEnd(), flushes via
    glVertexPointer + glDrawArrays. GL_QUADS has no ES equivalent, so
    every run of 4 buffered vertices gets split into two GL_TRIANGLES
    (shared 0-2 diagonal, same winding as the original quad).
    Color/texcoord/normal are NOT buffered per-vertex -- every call site
    in this project sets color via the real glColor4ub *before* glBegin
    and never calls glColor between vertices, so GL_COLOR_ARRAY is left
    disabled and GL just uses whatever flat color was already set via
    the real (un-shimmed) glColor4ub call. This matches actual usage
    exactly; it is not a general-purpose immediate-mode emulator.

  - gluPerspective/gluLookAt: standard textbook projection/view matrix
    formulas, built on ES 1.1's own glFrustumf/glMultMatrixf/
    glTranslatef (all real ES 1.1 core functions).

  - glOrtho: ES only has the float version, glOrthof. One-line wrapper.

  - GL_QUADS itself: GLES/gl.h does not define this token at all (no
    quad primitive exists in ES). Defined our own sentinel macro
    (0x7FFF0007, chosen to not collide with any real GLenum this project
    uses) so the existing `glBegin(GL_QUADS)` call sites keep compiling
    unchanged; the shim's own glEnd() is what actually recognizes and
    handles it.

Bugs hit building this (in order, useful if redoing it):
  a) GLdouble used in the shim's own gluPerspective/gluLookAt signatures
     -- GLES headers don't define GLdouble at all (ES has no
     double-precision type). Fixed: use plain `double` instead.
  b) GL_QUADS undeclared -- see above, fixed by defining the sentinel.
  c) glOrtho undeclared -- missed in the first grep-based audit (only
     caught glBegin/gluPerspective/gluLookAt explicitly). Found by
     building with `ninja -k 0` to collect every remaining
     "undeclared identifier" error in one pass instead of iterating
     error-by-error. Fixed with the glOrthof wrapper above.
  d) THE REAL BUG (only found by testing on a real device): enemies and
     trail pieces were completely invisible, while other GL_LINES-based
     UI elements rendered fine. Root cause: glinterface::cube()/
     solid_cube() (used by piece::print() for every trail piece, and by
     enemy rendering) draws a WHOLE CUBE as 6 quads inside ONE
     glBegin(GL_QUADS)/glEnd() pair -- 24 glVertex3f calls, not 4. The
     shim's glEnd() originally only special-cased exactly 4 buffered
     vertices for the quad-to-triangle split; with 24 it fell through to
     the generic branch, which called glDrawArrays() with the GL_QUADS
     sentinel value itself as the primitive type -- not a real ES enum,
     so the draw silently did nothing. The simpler GL_LINES-based UI
     elements worked because 2 is not 4, so they always took that
     generic (and in their case correct) branch. Fixed by generalizing
     the split to handle any multiple of 4 vertices, looping per quad.

  e) Also real, found on-device: a black screen on first launch (icon
     opened, SDL initialized, no crash, nothing ever drew). Root cause:
     src/main.cpp's try_init_gl() never called SDL_GL_SetAttribute for
     SDL_GL_CONTEXT_PROFILE_MASK/MAJOR_VERSION/MINOR_VERSION, only
     DOUBLEBUFFER/DEPTH_SIZE/ALPHA_SIZE. On desktop, omitting those is
     fine -- Mesa defaults to a compatibility-profile context with
     fixed-function support regardless. On Android, SDL's EGL context
     creation defaults to the HIGHEST ES version the device supports
     (ES2 or ES3 on any modern phone) when no profile is requested, not
     ES1. That context creates successfully (window/surface appears, no
     crash) but every fixed-function call in this project (and the shim)
     has zero effect on a real ES2/3 context -- nothing ever draws.
     Fixed with an `#ifdef __ANDROID__` block right after
     SDL_GL_ResetAttributes() in try_init_gl(), explicitly requesting
     SDL_GL_CONTEXT_PROFILE_ES, major 1, minor 1.


4. Asset / save-file paths
----------------------------
Files: include/asset_path.h, src/asset_path.cpp

Desktop behavior is unchanged in every case below -- these functions are
no-ops there (return the filename as-is, cwd-relative, exactly as before
this existed).

  asset_path(filename) -- used by save_data.cpp (neutron_save.txt) and
  custom_stage.cpp (map.txt). On Android, resolves to the app's external
  files directory via SDL_AndroidGetExternalStoragePath()
  (/storage/emulated/0/Android/data/<package>/files/ in practice) --
  there's no "next to the executable" concept in an APK, and the player
  is expected to place/replace these themselves. No bundled default for
  either: there's no sensible default custom map, and a save file is
  output, not an input asset.

  asset_path_sound(filename) -- used by sound_mixer.cpp for the three
  .wav files. On Android: checks the SAME external-storage override
  location first (a file the player drops in themselves still wins, same
  modding behavior as asset_path()). If nothing's there, returns the
  BARE filename instead of a resolved path -- SDL's own Android file
  layer (SDL_RWFromFile, and Mix_LoadWAV through it) automatically
  resolves a relative path against the APK's bundled assets/ folder, no
  manual extraction code needed. This only makes sense for files that
  actually ship a bundled default (the three sounds); map.txt/
  neutron_save.txt still use plain asset_path().

  The three .wav files that get bundled live at
  android/app/src/main/assets/{travel,explosion,music}.wav -- copied
  in from /workspace/neutron-port-android/assets/sounds/ (the source of truth; re-copy
  from there and rebuild the APK if they change).


5. CMakeLists.txt -- the ANDROID branch
------------------------------------------
Added an `if(ANDROID) ... else() ... endif()` split. The else() branch
(desktop) is completely unchanged from before this port existed.

The ANDROID branch:
  - Adds src/gles_compat.cpp to the source list.
  - Builds SDL2 and SDL2_mixer FROM SOURCE via FetchContent (git tags
    release-2.30.9 and release-2.8.0) -- there's no system SDL2 in the
    NDK sysroot. Both built static.
  - Puts include/gles_compat ahead of include/ on the include path (see
    section 3).
  - Builds a SHARED library (not an executable) named "neutron" -->
    libneutron.so -- Android requires native code as a shared object
    loaded by the Java/Activity side, not a standalone executable.
  - Links GLESv1_CM, log, android (standard Android NDK system libs).

Bugs hit configuring this (in order):
  a) `target_link_libraries` error: "SDL2_mixer::SDL2_mixer-static" not
     found. Root cause: the NDK's android.toolchain.cmake defaults the
     GLOBAL BUILD_SHARED_LIBS to ON (Android convention). SDL_mixer's
     own CMakeLists re-derives ITS OWN static/shared flag directly from
     that global variable with a plain `set()` (not a cache-respecting
     `option()`), silently overriding any more specific
     SDL2MIXER_BUILD_SHARED_LIBS setting. Fixed by forcing the global
     `set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)` before the
     FetchContent calls. (Our own add_library(neutron SHARED ...) is an
     explicit library type and is unaffected by this either way.)
  b) `'SDL2/SDL_mixer.h' file not found`. Root cause: the fetched
     SDL_mixer source puts SDL_mixer.h directly under include/, not
     nested in an SDL2/ subfolder the way the system package (and this
     project's #include <SDL2/SDL_mixer.h> lines) expect. Fixed with a
     generated symlink (file(CREATE_LINK ... SYMBOLIC)) creating an
     "SDL2" folder pointing at SDL_mixer's real include dir, added to
     the include path -- no #include line needed to change.

Configure/build commands used:
  NDK=/opt/android-ndk/android-ndk-r27c
  /opt/cmake/bin/cmake -S . -B build-android -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release
  ninja -C build-android
  # then strip it:
  $NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip build-android/libneutron.so


6. The Android app shell (Gradle/Java/APK)
---------------------------------------------
Directory: android/ (a full copy of the SDL2 source's own
android-project template, from build-android/_deps/sdl2-src/android-project/
-- that template is pulled in automatically by the CMake FetchContent
step in section 5, so it's always available after a configure).

Changes made to the stock template:
  - Deleted app/jni/ entirely -- we supply a prebuilt .so from our own
    CMake+NDK build (section 5), not Gradle's own ndk-build/CMake native
    integration.
  - app/src/main/java/com/neutronport/game/MainActivity.java (new file):
    extends org.libsdl.app.SDLActivity, overrides getLibraries() to
    return just {"neutron"} -- not the stock template's {"SDL2","main"},
    since SDL2/SDL2_mixer are statically linked INTO libneutron.so, there
    is no separate libSDL2.so to load.
  - AndroidManifest.xml: glEsVersion changed from 0x00020000 (ES 2.0) to
    0x00010001 (ES 1.1, matching what's actually built/linked); activity
    name changed from "SDLActivity" to
    "com.neutronport.game.MainActivity".
  - app/build.gradle: rewritten -- namespace/applicationId
    "com.neutronport.game" (placeholder, change freely), minSdkVersion
    24 (must match ANDROID_PLATFORM above), targetSdkVersion/
    compileSdkVersion 34, abiFilters restricted to arm64-v8a (the only
    ABI actually built), and the externalNativeBuild/ndkBuild hookup
    removed entirely (prebuilt .so instead, placed under
    app/src/main/jniLibs/arm64-v8a/ -- Gradle packages anything it finds
    there automatically).
  - local.properties (new file): sdk.dir=/opt/android-sdk
  - res/values/strings.xml: app_name changed from the template's "Game"
    to "Neutron".
  - res/mipmap-*/ic_launcher.png: replaced with a real icon -- see
    section 7.
  - app/src/main/assets/{travel,explosion,music}.wav: the bundled
    default sounds -- see section 4.
  - app/src/main/jniLibs/arm64-v8a/libneutron.so: the compiled native
    library from section 5 -- copy the fresh one here after every native
    rebuild, Gradle does not build it itself.

Build command:
  cd android
  export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
  export ANDROID_HOME=/opt/android-sdk
  export PATH=$JAVA_HOME/bin:$PATH
  ./gradlew assembleDebug
  # output: app/build/outputs/apk/debug/app-debug.apk

This is a DEBUG build (Gradle's own auto-generated debug signing key) --
fine to sideload via `adb install`, not suitable for the Play Store as-is
(would need a real signing config, and possibly more ABIs than just
arm64-v8a).


7. App icon
-----------
Source: /workspace/neutron-port-android/assets/icon.png (a 480x272 title/screenshot image
provided directly, "NEUTRON" wordmark over a gameplay shot). Not usable
as-is for a launcher icon -- too wide/short (not square), and contains
small text that turns to mush at launcher-icon sizes (48px up to 192px)
and can get clipped by a circular icon mask.

Cropped a 190x190 square from the gameplay-screenshot portion (roughly
the bottom-right, avoiding the logo text), then generated all 5 Android
mipmap densities from that crop with ImageMagick:
  convert assets/icon.png -crop 190x190+290+70 +repage /tmp/icon_square.png
  for pair in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
    convert /tmp/icon_square.png -resize ${pair#*:}x${pair#*:} \
      android/app/src/main/res/mipmap-${pair%:*}/ic_launcher.png
  done


8. Full rebuild-from-scratch checklist
-----------------------------------------
After changing game source (src/*.cpp, include/*.h):
  1. cd /workspace/neutron-port-android
  2. ninja -C build-android          (rebuilds libneutron.so)
  3. /opt/android-ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip build-android/libneutron.so
  4. cp build-android/libneutron.so android/app/src/main/jniLibs/arm64-v8a/
  5. cd android && JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ANDROID_HOME=/opt/android-sdk PATH=$JAVA_HOME/bin:$PATH ./gradlew assembleDebug
  6. cp app/build/outputs/apk/debug/app-debug.apk ../android-build/apk/neutron-debug.apk
  7. adb install -r ../android-build/apk/neutron-debug.apk

If starting completely fresh (no build-android/, no android/ yet):
  do section 2 (tools), then section 5's configure command (pulls in the
  SDL2 template via FetchContent as a side effect), then copy
  build-android/_deps/sdl2-src/android-project to android/ and redo
  all of section 6's changes, then section 7's icon, then the checklist
  above.

Desktop build is completely separate and unaffected by any of this:
  cmake -S . -B build -G Ninja && cmake --build build && ./build/neutron


9. Known gaps / not done
----------------------------
  - Only arm64-v8a is built. No armeabi-v7a/x86/x86_64 -- fine for any
    phone from roughly the last 8+ years, but excludes very old/32-bit
    or emulator (x86_64) devices.
  - Debug-signed only, not ready for Play Store distribution.
  - No adaptive icon (foreground/background layers) -- plain static
    mipmap PNGs only, which is enough to install and look correct, just
    not the newer themed-icon style some launchers support.
  - Gradle's own native-strip step logs "Unable to strip
    libneutron.so" every build -- harmless, it's already stripped
    manually in step 3 of the checklist above; Gradle just skips
    re-stripping an already-stripped binary.
