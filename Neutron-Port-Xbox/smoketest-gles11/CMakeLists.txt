cmake_minimum_required(VERSION 3.10)
project(gles11_smoketest C)

# Smoke test: does Ryzee119/nxdk-gles11 build + link against nxdk in this
# container? Pulls the library in as a subdirectory (its own documented
# usage pattern) and links its triangle_gles11.c sample.
#
# GLI_STAGING_ARENA_SIZE override (real Neutron port, not the smoke test
# itself): default is 2MB (gles_private.h), nxdk-gles11's own documented
# knob for "very large client-side draws" causing silent per-draw failures.
# Real hardware testing of ../neutron-xbox showed some arena faces render
# grey (correct) and others fully transparent/no color -- every one of
# those faces is drawn via its own glBegin(GL_QUADS)/glEnd() immediate-mode
# call (see terrain.cpp's skel_draw_quad, through gles_compat.cpp's own
# client-side vertex buffer), so a staging-arena-related failure on SOME
# but not all of these very similar draw calls is a real, concrete
# candidate. Bumped 4x as a first experiment -- not confirmed as the fix
# yet, needs a hardware retest.
add_compile_definitions(GLI_STAGING_ARENA_SIZE=8388608)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nxdk-gles11 nxdk-gles11-build)

add_executable(triangle_gles11 ${CMAKE_CURRENT_SOURCE_DIR}/../nxdk-gles11/samples/triangle_gles11.c)
target_link_libraries(triangle_gles11 PRIVATE GLESv1_CM "$ENV{NXDK_DIR}/lib/libpbkit.lib")
