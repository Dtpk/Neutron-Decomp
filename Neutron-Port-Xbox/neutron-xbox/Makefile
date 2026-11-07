# Xbox (nxdk) build of Neutron. Game-logic sources are reused DIRECTLY from
# /workspace/neutron-port-android/src (not copied) -- confirmed by grepping
# that whole src/ tree that only main.cpp and asset_path.cpp have any
# platform-specific code, and asset_path.cpp's own non-Android branch
# already does what Xbox needs unchanged. main_xbox.cpp (this folder) is
# the one real fork -- see its own header comment for exactly what differs
# from the original main.cpp.
#
# Prerequisites: run ../setup-nxdk.sh once, and build nxdk-gles11 once via
# ../smoketest-gles11 (its CMake project) -- this Makefile links the .lib
# files that build produces rather than rebuilding nxdk-gles11 itself.

XBE_TITLE = Neutron
GEN_XISO = $(XBE_TITLE).iso
NXDK_DIR ?= $(CURDIR)/../nxdk
NXDK_CXX = y
NXDK_SDL = y

ANDROID_PORT = $(CURDIR)/../../../neutron-port-android
GLES11_DIR   = $(CURDIR)/../nxdk-gles11
GLES11_BUILD = $(CURDIR)/../smoketest-gles11/build/nxdk-gles11-build

SRCS = \
	$(CURDIR)/src/main_xbox.cpp \
	$(ANDROID_PORT)/src/game_data.cpp \
	$(ANDROID_PORT)/src/glinterface.cpp \
	$(ANDROID_PORT)/src/camera.cpp \
	$(ANDROID_PORT)/src/piece.cpp \
	$(ANDROID_PORT)/src/plane.cpp \
	$(ANDROID_PORT)/src/ship.cpp \
	$(ANDROID_PORT)/src/player.cpp \
	$(ANDROID_PORT)/src/enemy.cpp \
	$(ANDROID_PORT)/src/miniature.cpp \
	$(ANDROID_PORT)/src/terrain.cpp \
	$(ANDROID_PORT)/src/button.cpp \
	$(ANDROID_PORT)/src/stage_interface.cpp \
	$(ANDROID_PORT)/src/sound_mixer.cpp \
	$(ANDROID_PORT)/src/save_data.cpp \
	$(ANDROID_PORT)/src/custom_stage.cpp \
	$(ANDROID_PORT)/src/menu_battle.cpp \
	$(ANDROID_PORT)/src/controller_input.cpp \
	$(ANDROID_PORT)/src/touch_input.cpp \
	$(ANDROID_PORT)/src/split_screen_menu.cpp \
	$(ANDROID_PORT)/src/split_screen_match.cpp \
	$(ANDROID_PORT)/src/neutron.cpp \
	$(ANDROID_PORT)/src/asset_path.cpp \
	$(ANDROID_PORT)/src/gles_compat.cpp

# Order matters: gles_compat's own GL/gl.h + GL/glu.h must be found ahead of
# nxdk-gles11's real GLES/gl.h (same convention the Android build uses --
# see neutron-port-android/CMakeLists.txt's own comment on this), and the
# SDL_mixer stub must be found ahead of... nothing, actually, since nxdk's
# own SDL2 fork has no SDL_mixer at all to conflict with -- it's just
# standing in for a header that doesn't otherwise exist on this platform.
NXDK_CXXFLAGS += \
	-std=c++17 \
	-DM_PI=3.14159265358979323846 \
	-I$(CURDIR)/shim/sdl_mixer_stub \
	-I$(ANDROID_PORT)/include/gles_compat \
	-I$(GLES11_DIR)/include \
	-I$(ANDROID_PORT)/include \
	-I$(CURDIR)/shim

LDFLAGS += \
	$(GLES11_BUILD)/libGLESv1_CM.lib \
	$(GLES11_BUILD)/arena_build/libarena.lib \
	$(GLES11_BUILD)/libswizzle.lib \
	$(NXDK_DIR)/lib/libpbkit.lib

include $(NXDK_DIR)/Makefile
