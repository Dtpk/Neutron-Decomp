cmake_minimum_required(VERSION 3.16)
project(neutron_web CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/gles_compat.cpp
)

include_directories(
    include/gles_compat
    include
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "shell.html")

add_executable(neutron ${SOURCES})

target_compile_options(neutron PRIVATE
    "-sUSE_SDL=2"
    "-sUSE_SDL_MIXER=2"
    "-sLEGACY_GL_EMULATION=1"
)

target_link_options(neutron PRIVATE
    "-sUSE_SDL=2"
    "-sUSE_SDL_MIXER=2"
    "-sLEGACY_GL_EMULATION=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    # Lets shell.html's uploadCustomMap() call web_reload_custom_stage()
    # (main.cpp) via Module.ccall() right after writing the uploaded file
    # into MEMFS -- see that C function's own comment for the real bug
    # this exists to fix (a page reload was destroying the just-uploaded
    # file before this module ever got a chance to read it).
    "-sEXPORTED_RUNTIME_METHODS=ccall"
    "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

set_target_properties(neutron PROPERTIES
    SUFFIX ".html"
)
