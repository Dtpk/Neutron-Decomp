cmake_minimum_required(VERSION 3.16)
project(neutron_map_editor CXX)

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

add_executable(neutron_map_editor
    src/main.cpp
    src/editor.cpp
)

target_link_libraries(neutron_map_editor
    ${SDL2_LIBRARIES}
    OpenGL::GL
    OpenGL::GLU
)

if(WIN32)
    # SDL2 ships its own main() hook (SDL_main) on Windows; link it so a
    # normal int main(int,char**) works without a console-less WinMain shim.
    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(neutron_map_editor SDL2::SDL2main)
    endif()
endif()
