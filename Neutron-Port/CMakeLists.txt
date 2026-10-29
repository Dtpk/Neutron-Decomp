cmake_minimum_required(VERSION 3.16)
project(neutron CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(
    include
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

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
    src/neutron.cpp
)

add_executable(neutron ${SOURCES})

target_link_libraries(neutron
    ${SDL2_LIBRARIES}
    SDL2_mixer
    OpenGL::GL
    OpenGL::GLU
)
