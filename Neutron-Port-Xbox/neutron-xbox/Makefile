# Xbox (nxdk) build of Neutron. Game-logic sources used to be reused
# DIRECTLY from /workspace/neutron-port-android/src (not copied) via a
# GAME_SRC path reaching three levels up and OUTSIDE this folder -- that
# broke the moment that external project got reorganized/moved elsewhere
# on disk (it briefly did), making this build unbuildable for anyone who
# only has neutron-ports-console. Game logic is now VENDORED (copied)
# into this folder's own shared/src and shared/include instead, so this
# Makefile has everything it needs right here -- see shared/src's own
# copy of asset_path.cpp (its non-Android, non-NXDK branch already does
# what desktop needs unchanged; its NXDK branch is Xbox's own real
# save/custom-map path, E:\UDATA\neutron\) and gles_compat.cpp.
# main_xbox.cpp (this folder, NOT vendored) is the one real fork -- see
# its own header comment for exactly what differs from the original
# main.cpp. shared/src also already carries three Xbox-hardware-
# discovered fixes (split-screen glScissor clipping, a guide-marker
# depth-fighting lift, batched HUD-glyph rendering for a frame-rate bug)
# -- harmless on every other platform, but do NOT replace shared/ with a
# different project's copy (e.g. Neutron-Port) without carrying those
# three forward, or real, already-fixed hardware bugs come back.
#
# Prerequisites: run ../setup-nxdk.sh once, and build nxdk-gles11 once via
# ../smoketest-gles11 (its CMake project) -- this Makefile links the .lib
# files that build produces rather than rebuilding nxdk-gles11 itself.

XBE_TITLE = Neutron
GEN_XISO = $(XBE_TITLE).iso
NXDK_DIR ?= $(CURDIR)/../nxdk
NXDK_CXX = y
NXDK_SDL = y

GAME_SRC = $(CURDIR)/shared
GLES11_DIR   = $(CURDIR)/../nxdk-gles11
GLES11_BUILD = $(CURDIR)/../smoketest-gles11/build/nxdk-gles11-build

SRCS = \
	$(CURDIR)/src/main_xbox.cpp \
	$(CURDIR)/shim/sdl_mixer_stub/SDL_mixer_xbox.cpp \
	$(GAME_SRC)/src/game_data.cpp \
	$(GAME_SRC)/src/glinterface.cpp \
	$(GAME_SRC)/src/camera.cpp \
	$(GAME_SRC)/src/piece.cpp \
	$(GAME_SRC)/src/plane.cpp \
	$(GAME_SRC)/src/ship.cpp \
	$(GAME_SRC)/src/player.cpp \
	$(GAME_SRC)/src/enemy.cpp \
	$(GAME_SRC)/src/miniature.cpp \
	$(GAME_SRC)/src/terrain.cpp \
	$(GAME_SRC)/src/button.cpp \
	$(GAME_SRC)/src/stage_interface.cpp \
	$(GAME_SRC)/src/sound_mixer.cpp \
	$(GAME_SRC)/src/save_data.cpp \
	$(GAME_SRC)/src/custom_stage.cpp \
	$(GAME_SRC)/src/menu_battle.cpp \
	$(GAME_SRC)/src/controller_input.cpp \
	$(GAME_SRC)/src/touch_input.cpp \
	$(GAME_SRC)/src/split_screen_menu.cpp \
	$(GAME_SRC)/src/split_screen_match.cpp \
	$(GAME_SRC)/src/neutron.cpp \
	$(GAME_SRC)/src/asset_path.cpp \
	$(GAME_SRC)/src/gles_compat.cpp \
	$(GAME_SRC)/src/cube_batch.cpp

# Order matters: gles_compat's own GL/gl.h + GL/glu.h must be found ahead of
# nxdk-gles11's real GLES/gl.h (same convention the Android build uses --
# see neutron-port-android/CMakeLists.txt's own comment on this). The
# shim/sdl_mixer_stub include (despite the "stub" dirname -- real Mix_*
# backend now, see that header's own comment) doesn't need to beat
# anything else to this path; nxdk's own SDL2 fork has no SDL_mixer at
# all to conflict with, it's just standing in for a header that doesn't
# otherwise exist on this platform.
NXDK_CXXFLAGS += \
	-std=c++17 \
	-DM_PI=3.14159265358979323846 \
	-I$(CURDIR)/shim/sdl_mixer_stub \
	-I$(GAME_SRC)/include/gles_compat \
	-I$(GLES11_DIR)/include \
	-I$(GAME_SRC)/include \
	-I$(CURDIR)/shim

LDFLAGS += \
	$(GLES11_BUILD)/libGLESv1_CM.lib \
	$(GLES11_BUILD)/arena_build/libarena.lib \
	$(GLES11_BUILD)/libswizzle.lib \
	$(NXDK_DIR)/lib/libpbkit.lib

include $(NXDK_DIR)/Makefile
