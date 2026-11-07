cmake_minimum_required(VERSION 3.16)
project(neutron CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

set(SOURCES
    src/main.cpp
    src/game_data.cpp
    src/glinterface.cpp
    src/camera.cpp
    src/piece.cpp
    src/plane.cpp
    src/ship.cpp
    src/player.cpp
    src/enemy.cpp
    src/miniature.cpp
    src/terrain.cpp
    src/button.cpp
    src/stage_interface.cpp
    src/sound_mixer.cpp
    src/save_data.cpp
    src/custom_stage.cpp
    src/menu_battle.cpp
    src/controller_input.cpp
    src/touch_input.cpp
    src/split_screen_menu.cpp
    src/split_screen_match.cpp
    src/neutron.cpp
    src/asset_path.cpp
)

if(ANDROID)
    # OpenGL ES 1.1 has no GLU and no immediate mode (glBegin/glVertex.../
    # glEnd, GL_QUADS) -- gles_compat.cpp implements the handful this
    # project actually uses (see include/gles_compat/GL/{gl,glu}.h for why
    # this is a safe drop-in: every existing call site is unchanged).
    list(APPEND SOURCES src/gles_compat.cpp)

    # SDL2/SDL2_mixer aren't present in the NDK sysroot -- build them from
    # source for the target ABI, same FetchContent-and-build-static approach
    # already proven for the Lain port's Windows cross-build.
    include(FetchContent)
    FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.30.9)
    FetchContent_Declare(SDL2_mixer GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git GIT_TAG release-2.8.0)
    # The NDK toolchain file defaults the global BUILD_SHARED_LIBS to ON
    # (Android convention) -- SDL_mixer's CMakeLists re-derives its OWN
    # static/shared flag straight from that global, silently overriding any
    # more specific SDL2MIXER_* setting. Force the global off for these
    # vendored deps; our own add_library(neutron SHARED ...) below is an
    # explicit library type and is unaffected by this either way.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_VENDORED ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    # This game only ever calls Mix_LoadWAV() on plain bundled .wav files
    # (see sound_mixer.cpp) -- never Mix_LoadMUS()/streaming music, so none
    # of SDL2_mixer's other decoders (FLAC/MOD/MP3/MIDI/Opus/Vorbis/
    # WavPack, all ON by default) are ever actually exercised. Disabling
    # them matters beyond APK size: FLAC, opusfile, vorbisfile, and
    # WavPack's decoders all call ftello/fseeko under _FILE_OFFSET_BITS=64,
    # which Bionic's stdio.h aliases to fseeko64/ftello64 -- gated to
    # API >= 24 on 32-bit ABIs (__INTRODUCED_IN(24)). Pulling in dead
    # decoders was silently forcing minSdkVersion up to 24 on the
    # armeabi-v7a side. With only WAVE left, SDL2's own file I/O falls
    # back to the plain fseeko/ftello (32-bit off_t, no LFS alias), which
    # the NDK's own per-API libc.so stubs confirm have been exported since
    # API 21 on armeabi-v7a (checked directly: llvm-nm -D against
    # sysroot/.../arm-linux-androideabi/{21,22,23}/libc.so all show
    # `T fseeko@@LIBC`/`T ftello@@LIBC`; only the 64-bit aliases are new
    # at 24) -- see neutron-port-android-older-phones' README for the full
    # investigation. arm64-v8a never had this problem (off_t is already
    # 64-bit there) but is built at the same reduced dependency set for
    # consistency/size.
    set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_VORBIS OFF CACHE STRING "" FORCE)
    set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
    FetchContent_MakeAvailable(SDL2_mixer)

    # Game code includes <SDL2/SDL_mixer.h> (matching the system package
    # layout the desktop build uses) -- the fetched source tree has
    # SDL_mixer.h directly under include/, no SDL2/ subfolder. A generated
    # symlink closes that gap without touching any #include lines.
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/sdl2_mixer_include_shim)
    file(CREATE_LINK ${sdl2_mixer_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/sdl2_mixer_include_shim/SDL2 SYMBOLIC)
    include_directories(${CMAKE_BINARY_DIR}/sdl2_mixer_include_shim)

    # include/gles_compat goes FIRST so our GL/gl.h and GL/glu.h are found
    # ahead of (and wrap) <GLES/gl.h> -- the desktop build never sees this
    # directory at all.
    include_directories(
        include/gles_compat
        include
    )

    add_library(neutron SHARED ${SOURCES})
    target_link_libraries(neutron
        SDL2::SDL2-static
        SDL2_mixer::SDL2_mixer-static
        GLESv1_CM
        log
        android
    )
else()
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)

    include_directories(
        include
        ${SDL2_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIR}
    )

    add_executable(neutron ${SOURCES})

    target_link_libraries(neutron
        ${SDL2_LIBRARIES}
        SDL2_mixer
        OpenGL::GL
        OpenGL::GLU
    )
endif()
