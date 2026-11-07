Original Xbox (2001) port -- feasibility notes
===================================================================================
UPDATE: toolchain is now proven working end-to-end in this sandbox (see
"Toolchain -- confirmed working" below) -- built nxdk's own "hello"
sample to a real bootable hello.iso. Two corrections below fix wrong
claims from the first research pass (marked CORRECTION).

UPDATE 2: nxdk-gles11 itself now proven building and linking cleanly
too -- see "Smoke test: nxdk-gles11" below. Its triangle_gles11.c
sample built all the way to a real default.xbe. Found along the way:
nxdk ships its own CMake support (bin/nxdk-cmake +
share/toolchain-nxdk.cmake), so nxdk-gles11's CMake-based build and
nxdk's own Makefile-based samples aren't actually in conflict -- nxdk
natively supports both. The "bridging CMake into Makefiles" concern
from the first pass turned out to be a non-issue.


Toolchain -- confirmed working
----------------------------------
nxdk -- the modern, actively maintained open-source Xbox homebrew SDK
(XboxDev/nxdk, recent commits). No console/signing step needed to
produce a binary (the Xbox homebrew loader chain accepts unsigned
builds).

CORRECTION: nxdk does NOT bundle its own LLVM toolchain -- that was
wrong. It needs clang, lld, bison, flex, cmake, make, and git already
on the system (its own README says so under "Prerequisites"; the
"Bundles its own toolchain" framing in the original pass was a
misreading). In this container: `apt-get install clang lld bison flex
cmake make git` installs these cleanly (no devkitPro, no Cloudflare
involvement -- see below).

One real gap hit and fixed: Debian's llvm-14/clang-14/lld-14 packages
only ship VERSIONED binary names (llvm-dlltool-14, llvm-ar-14, etc.),
no update-alternatives entries. nxdk's Makefiles call the UNVERSIONED
names. Fixed by symlinking every /usr/bin/llvm-*-14 to its unversioned
name. See setup-nxdk.sh in this folder for the full working recipe
(apt install, the symlink fix, env vars, a smoke-test build) --
written and confirmed working this session.

Setup steps are a plain `git clone --recursive` from GitHub + exporting
NXDK_DIR/PATH (same two exports nxdk's own bin/activate script does --
set them by hand rather than executing that script directly, since an
environment with Bash permissions that flag running freshly-downloaded
scripts will block it; the env vars themselves are not sensitive).
This whole path goes through GitHub + Debian's own apt repos only --
NOT devkitPro's apt.devkitpro.org/pkg.devkitpro.org infrastructure, so
it sidesteps both blockers hit on the Wii path (Cloudflare blocking by
User-Agent, and the devkitpro-pacman .deb's postinst chown failure).


SDL2 on Xbox: covers less than you'd expect
------------------------------------------------
The official nxdk SDL2 port covers input, audio, and 2D -- it does NOT
cover 3D rendering. (A newer community fork, nxdk-sdl, updated as
recently as July 2025, and an experimental SDL3-GPU fork both exist, but
neither is the proven/default baseline.) So unlike Android or even
Switch, SDL2 here isn't "the one abstraction that covers everything" --
3D has to go through a different, lower-level path entirely.


The real GPU story: no native GL, but TWO real compatibility layers exist
----------------------------------------------------------------------------
The original Xbox's GPU (NV2A) has no OpenGL support at all natively.
nxdk's own pbkit is a low-level wrapper around the NV2A's push-buffer
command format -- closer in spirit to Direct3D 8 than to GL. Two
different libraries wrap it in GL-shaped APIs, cloned into this folder:

1. pbGL (fgsfdsfgs/pbgl on GitHub -- confirmed the real upstream,
   actively maintained, pushed 2026-05-20, 19 stars; Ryzee119/pbgl is
   a stale fork of it, not the other way around). A real, working
   *partial* OpenGL 1.x implementation on top of nxdk/pbkit --
   GL_ARB_multitexture, texture combine, paletted textures.
   Known rough edges (from its own docs/issues):
     - Assumes RGBA8888 color buffers and D24S8 depth/stencil.
     - glVertexPointer-style calls do NOT copy data -- each draw call
       needs its own freshly-populated client-side vertex array,
       unlike desktop GL's more forgiving behavior. Real adaptation
       point versus this engine's gles_compat-style shim (see the
       Android port's src/gles_compat.cpp).
     - No 1D/3D/cubemap texture support.
     - Kodi4Xbox (also pbkit/pbgl-based) calls it needing "lots of
       improvements" in their own notes.

2. nxdk-gles11 (Ryzee119/nxdk-gles11 -- real, NOT a fork, actively
   maintained, pushed 2026-10-29, the week this was written). A much
   more complete OpenGL ES 1.1 implementation: all ES1.1 primitives,
   fog, lighting, NPOT textures, mipmaps, VBOs, clip planes,
   automatic client-side array staging (glVertexPointer-style calls
   DO work the forgiving way here, unlike pbGL -- it stages
   malloc/stack arrays into GPU-contiguous memory automatically).
   Also bundles gl4es (via CMake FetchContent) for real Desktop
   OpenGL 1.5 support through the same library -- `#include <GL/gl.h>`
   instead of `<GLES/gl.h>`, link `GL` instead of `GLESv1_CM`.
   CORRECTED real precedent: its own README states and shows a
   screenshot of "Neverball rendering with nxdk-gles11 on real
   hardware." (The earlier pass's "neverball-nxdk" citation was wrong
   -- no such repo exists. The real, much weaker precedent on pbGL's
   side is a single dormant 2020 pull request on a stale Neverball
   fork, JayFoxRox/neverball#1, "depends on experimental changes to
   nxdk" per the nxdk wiki's own project list -- not a working port.
   The actual working, photographed precedent is on nxdk-gles11, not
   pbGL.)
   Builds via CMake (add_subdirectory), not nxdk's own Makefile
   convention -- bridging the two is an open integration question, not
   yet worked out.


Recommendation
-----------------
nxdk-gles11 is very likely the better target for this engine
specifically, not pbGL:
  - It's genuine ES1.1, the same thing neutron-port-android's
    gles_compat shim already targets -- much less adaptation than
    pbGL's stricter per-draw-call array requirement would need.
  - Its gl4es-backed Desktop GL1.5 mode means /workspace/Neutron-Port
    (the pure desktop-GL variant) could plausibly target the exact
    same library too, just switching the header/link target.
  - It has an actual working, photographed 3D-game precedent
    (Neverball), where pbGL's comparable precedent never got finished.
  - It's more actively maintained (commits this month) than pbGL.
The real cost either way is architectural: 3D rendering needs to
bypass SDL2 entirely and go straight through whichever GL layer is
chosen (SDL2 on Xbox doesn't cover 3D -- see above), and nxdk-gles11's
CMake-based build needs to be bridged into nxdk's Makefile convention.

Smoke test: nxdk-gles11
---------------------------
DONE, confirmed working. smoketest-gles11/ in this folder is a small
CMake project (own CMakeLists.txt, add_subdirectory(../nxdk-gles11))
built via:
  export NXDK_DIR=/workspace/neutron-ports-console/xbox/nxdk
  export PATH="$NXDK_DIR/bin:$PATH"
  "$NXDK_DIR/bin/nxdk-cmake" -G "Unix Makefiles" -S smoketest-gles11 -B smoketest-gles11/build
  make -C smoketest-gles11/build
  "$NXDK_DIR/tools/cxbe/cxbe" -OUT:smoketest-gles11/build/default.xbe \
      -TITLE:"gles11 smoketest" smoketest-gles11/build/triangle_gles11.exe
One real gap hit: nxdk-gles11 has its own git submodule (lib/arena,
Ryzee119/arena) not fetched by a non-recursive clone -- fixed with
`git submodule update --init --recursive` inside nxdk-gles11/. Once
that's in place, the whole GLESv1_CM library (lighting, fog, textures,
mipmaps, VBOs, rasterization -- the works) compiled clean against
nxdk's toolchain with only one harmless warning (a NULL macro
redefined between pdclib and xboxkrnl.h, pre-existing in nxdk itself,
not something this library caused), linked against pbkit, and
converted to a real default.xbe via cxbe. Didn't go as far as
packaging an .iso or triggering gl4es mode (NXDK_GLES11_WITH_GL4ES) --
both straightforward next steps if needed, not done here since the
goal was just proving the GL layer itself builds and links.

Real port: DONE, building a full real Neutron.iso
----------------------------------------------------------------------
neutron-xbox/ in this folder is a real, working Xbox build of the
actual game -- not a stub, not a trimmed-down demo. It builds via
nxdk's own Makefile convention (`make` in neutron-xbox/, after
setup-nxdk.sh and the smoketest-gles11 CMake build have both run once)
and produces a real bootable neutron-xbox/Neutron.iso +
bin/default.xbe.

What's reused vs. forked, and why that split is safe:
Grepped the ENTIRE neutron-port-android/src+include tree for
__ANDROID__/JNI/AAssetManager before starting -- only main.cpp and
asset_path.cpp had any. Also confirmed via reading glinterface.cpp
that this game has NO texture/image loading at all -- every object is
flat-colored procedural geometry (glColor4ub + glVertexPointer +
glDrawArrays) -- so there's no SDL_image/texture-format concern on any
platform. Based on that:
  - All 22 other .cpp files (game_data, player, ship, camera, piece,
    plane, enemy, miniature, terrain, button, stage_interface,
    sound_mixer, save_data, custom_stage, menu_battle,
    controller_input, touch_input, split_screen_menu,
    split_screen_match, neutron, glinterface) are linked DIRECTLY from
    neutron-port-android/src -- not copied, not modified. Same for
    gles_compat.cpp/.h and asset_path.cpp -- both are already fully
    platform-generic (gles_compat targets real GLES/gl.h, which
    nxdk-gles11 also provides; asset_path's non-Android branch already
    does exactly what Xbox needs).
  - neutron-xbox/src/main_xbox.cpp is the one real fork -- a copy of
    main.cpp with ONLY the GL-context/window/swap plumbing replaced
    (SDL_CreateWindow+SDL_GL_CreateContext -> XVideoSetMode+
    glContextInit, SDL_GL_SwapWindow -> glFlipNV2A, dropped
    SDL_INIT_VIDEO since SDL2 doesn't do 3D on Xbox) and the --test
    harness excluded under #ifndef NXDK (it depends on
    SDL_JoystickAttachVirtual, a desktop-only synthetic-input testing
    API nxdk's SDL2 fork doesn't implement, and can't run on Xbox
    anyway with no real argv). Every input handler (keyboard, mouse,
    touch, controller) and the entire game loop's control flow is
    copied verbatim, unchanged.
  - Audio is stubbed, not ported: shim/sdl_mixer_stub/SDL_mixer.h is a
    header-only no-op implementation of the small Mix_* surface
    sound_mixer.cpp/main.cpp use (nxdk doesn't bundle SDL2_mixer at
    all -- confirmed via its .gitmodules). This isn't a hack -- it's
    exactly the degraded-but-not-crashing behavior sound_mixer.h's own
    header comment already documents for "a future port with no audio
    backend at all." Real Xbox audio (building SDL2_mixer against
    nxdk's SDL2 fork, or a native nxdk-audio backend) is genuine
    follow-up work, not done here.

Three real build issues hit and fixed along the way (all in
neutron-xbox/Makefile, none required touching the shared
neutron-port-android source):
  - No -std= flag in nxdk-cxx -> clang's pre-C++17 default, so
    std::clamp (sound_mixer.cpp) didn't exist. Fixed with
    -std=c++17 (matching neutron-port-android/CMakeLists.txt's own
    CMAKE_CXX_STANDARD 17).
  - nxdk's pdclib math.h doesn't define M_PI (it's a BSD/POSIX
    extension, not standard C) -- gles_compat.cpp's gluPerspective
    needed it. Fixed with -DM_PI=3.14159265358979323846, the same fix
    MSVC users apply for the same reason, rather than touching the
    shared file.
  - nxdk-gles11's own include order matters: gles_compat's GL/gl.h
    must be found ahead of nxdk-gles11's real GLES/gl.h (same
    convention the Android CMakeLists.txt already documents for its
    own build).

Not yet done / real open items:
  - Never run on real hardware or in an emulator (xemu) -- this
    sandbox can build but not execute Xbox binaries. The user running
    xemu on their own machine against Neutron.iso is the real next
    verification step.
  - Real audio (see above).
  - Save data path: asset_path()'s unchanged #else branch returns a
    bare filename, which depends on the Xbox's default working
    directory resolving sensibly from the mounted XISO/D: drive --
    plausible (same assumption the desktop build already makes) but
    not verified against real hardware.
  - gl4es/desktop-GL1.5 mode (NXDK_GLES11_WITH_GL4ES) was never
    exercised -- this build goes through nxdk-gles11's native ES1.1
    path only, matching neutron-port-android's own ES1.1 target
    exactly, which is why gles_compat ported over with zero changes.


Real hardware test #1 -- results and fixes (2026-11-06)
-------------------------------------------------------------
User tested bin/default.xbe directly on real Xbox hardware (not from
the .iso). Genuinely playable -- reached ~4 levels in. Findings and
fixes:

  - D-pad: correct. Controller bumpers (LB/RB steering): backwards.
    D-pad left/right and the shoulder buttons both route through the
    exact same shared controller_input::handle_direction(), so this
    can't be a game-logic bug. Checked nxdk's SDL2 fork directly:
    lib/sdl/SDL2/src/joystick/xbox/SDL_xboxjoystick.c's btn_map
    (index 4=LEFT_SHOULDER, 5=RIGHT_SHOULDER) and
    src/joystick/SDL_gamecontroller.c's built-in mapping string
    (leftshoulder:b4,rightshoulder:b5) are both internally consistent
    with the XInput spec -- not something safe to "fix" in the shared
    SDL fork or cross-platform controller_input.cpp without risking
    breaking a correctly-wired pad. Fixed with a small, Xbox-only
    swap in main_xbox.cpp's SDL_CONTROLLERBUTTONDOWN handler instead
    (swaps the two button codes right before dispatch) -- needs
    re-test to confirm.

  - "Easy markers" (the red/blue turn-left/turn-right guide squares
    player.cpp's print_guides() draws next to the ship on Easy
    difficulty) disappearing on certain planes: real likely cause --
    they're drawn exactly coplanar with the ground tile beneath them
    (same height, no vertical offset). Fine on desktop/Android's
    higher-precision depth buffers; real z-fighting territory on the
    original Xbox's much lower-precision Z24S8 depth buffer. Fixed in
    player.cpp (shared file, harmless on every platform) with a fixed
    0.05-unit lift along the surface's dead axis, breaking the exact
    coplanar tie -- needs re-test to confirm.

  - "Some planes render odd" -- not yet root-caused, no fix attempted.
    Not enough specifics yet (which planes, what "odd" looks like) to
    find the right code with confidence rather than guessing.

  - Crash on menu idle: user sent a real photo of the actual crash
    screen (not just the "pb_reset: too long" line reported
    verbally), which changed the diagnosis -- the FULL text is:
      Software Put=03dac000
      Hardware Put=03dac000
      Hardware Get=03f5d000
      Dma push buffer engine encountered invalid data at these addresses.
      pb_reset: too long
    "Dma push buffer engine encountered invalid data" is NOT a benign
    "GPU fell behind" warning -- the GPU's command processor actually
    hit garbage/invalid data in the push buffer; the Hardware Get
    pointer (0x03f5d000) had run past Put (0x03dac000), meaning it
    read into stale/uninitialized push-buffer memory. Real likely
    cause found: pbkit's own push buffer (the raw GPU COMMAND buffer
    -- a DIFFERENT, separate buffer from nxdk-gles11's client-side
    vertex staging arena bumped earlier) defaults to only 512KB
    (pbkit.h's own documented default), and glContextInit()->
    pb_init() never raises it. This game draws the whole visible
    arena through many small immediate-mode glBegin/glEnd calls per
    piece/plane (gles_compat.cpp, terrain.cpp's skel_draw_quad) --
    each one is its own burst of GPU state + draw commands, plausibly
    adding up past 512KB within one complex frame. Fixed in
    main_xbox.cpp's xbox_init_video() with pb_size(2*1024*1024)
    (quadrupled, same ratio as the staging-arena bump) called BEFORE
    glContextInit() -- pb_size() must run before pb_init() per
    pbkit.h's own documented requirement, which is why this has to
    happen at the app level rather than inside nxdk-gles11 itself.
    Not yet confirmed on hardware.

  - "One-way pane" rendering bug (user's own precise description:
    "like mesh modeling in blender, one side has the image, one
    don't" -- i.e. textbook one-sided/backface-culling behavior,
    consistently the same faces, described as "back and sides" on the
    single-cube stage). Ruled out GL_CULL_FACE being left enabled
    (nxdk-gles11 explicitly calls glDisable(GL_CULL_FACE) at context
    init, gles_core.c:840) and ruled out axis_polarity-driven vertex
    winding reversal (traced plane.cpp's actual a/b/c/d_vertex
    generation -- no polarity-dependent sign flip exists in the
    row/col->world-space mapping, so winding should already be
    consistent across all 6 faces). Current best theory, not yet
    confirmed: the SAME push-buffer corruption as the crash above --
    if a cull-face/front-face GPU state-setting command gets dropped
    or garbled by an overflowing 512KB push buffer, a STALE winding-
    related setting from an unrelated earlier draw call could easily
    end up active for the wrong faces, which would look exactly like
    inconsistent one-sided rendering without the actual geometry or
    culling logic being wrong at all. The pb_size() fix above is the
    single change to retest against both symptoms before trying
    anything else.

    UPDATE: pb_size() fixed the crash (confirmed) but did NOT fix the
    rendering bug, and a polarity-corrected version of the guide-
    marker lift (axis_polarity is +1 on faces 1/2/3, -1 on 4/5/6 per
    terrain.cpp's PLANES table -- the first lift attempt used a fixed
    sign, which was actively wrong, pushing the marker further INTO
    the surface on half the faces) also didn't resolve it. Since a
    depth-only fix changed nothing, this probably isn't z-fighting at
    all -- which points back at the user's own original read
    ("one-way pane... like Blender, one side has the image, one
    don't" -- textbook backface culling). Found a real, concrete
    suspect in nxdk-gles11 itself: GL_CULL_FACE's glEnable/glDisable
    is cached in gles_core.c (early-returns if the requested value
    already matches the tracked state) -- a freshly-created context's
    cull_face_enabled field defaults to 0/false already, so the
    context-init code's own glDisable(GL_CULL_FACE) call may never
    actually reach the GPU at all, leaving the real NV2A hardware
    register at whatever ITS OWN power-on default is instead of
    genuinely disabled. Fixed at the app level in main_xbox.cpp's
    xbox_init_video() -- glEnable(GL_CULL_FACE) immediately followed
    by glDisable(GL_CULL_FACE), right after glContextInit(), forces a
    real state transition so the disable command is guaranteed to
    actually reach hardware this time. Doesn't touch nxdk-gles11
    itself.

    CONFIRMED FIXED by the user on real hardware.


Real hardware test #2 -- save/custom-map paths, frame-rate question (2026-11-06)
----------------------------------------------------------------------------------
  - Save data + custom map (map.txt) loading: real bug found and
    fixed. asset_path.cpp's non-Android branch just returned the bare
    filename, resolved via plain fopen() against the current working
    directory -- on Xbox that's D:\, the disc/XISO content itself,
    which is READ-ONLY on real hardware. Saving would fail silently
    every time. Fixed with a new NXDK branch in asset_path.cpp
    (extends the file's own existing per-platform pattern, same as
    the Android branch) resolving to E:\UDATA\neutron\ instead -- the
    standard Xbox homebrew save/user-data convention (confirmed
    against a real comparable nxdk game port, JayFoxRox/hode#1, which
    uses the same E:\UDATA\<app>\ location), mounting the drive and
    creating the folder on first use via nxMountDrive +
    CreateDirectoryA (both real, confirmed-implemented nxdk APIs).
    Not yet confirmed on real hardware.

  - Frame rate much better in Hard mode than Easy, asked whether an
    adaptive-timestep fix made sense. Investigated and concluded NO,
    not a bug: the Easy-only extra draw calls (print_guides' two small
    guide-marker quads) are trivially cheap, nowhere near enough to
    explain a real difference. The actual cause is
    neutron.cpp's own `speed = (user_difficulty == 1) ? 55.0f : 75.0f`
    -- Hard mode ticks the simulation every 55ms (~18Hz), Easy every
    75ms (~13Hz), by original-game design, present on every platform
    (desktop/Android included), not something this port introduced.
    Deliberately NOT implementing adaptive/dynamic timestep math to
    "fix" this -- it would mean overriding the original game's
    intentional difficulty pacing to mask a difference that isn't
    actually broken, which cuts against this whole project's parity-
    with-the-decompiled-original goal. Skipped per the user's own
    fallback.

    Enabled nxdk-gles11's own built-in on-screen FPS counter
    (glShowFPSNV2A) in xbox_init_video() as a diagnostic -- real
    measured numbers came back: ~29.9fps in actual level-1 gameplay,
    ~19.5fps in the in-game pause menu. 29.9 is suspiciously exactly
    half of 60Hz -- consistent with being on NTSC 60Hz video and
    vsync halving the rate because frames take just over one vblank
    (16.7ms) to render. The pause menu being SLOWER than active
    gameplay was the real surprise worth chasing (a paused screen
    should be cheaper to draw, not more expensive).

    Real cause found and fixed: stage_interface.cpp's draw_hud_text()
    renders its 6x6-pixel-grid bitmap font with a SEPARATE
    glBegin(GL_QUADS)/glEnd() pair per lit pixel, not per glyph or per
    string -- a single on-screen line of text could mean 500+
    individual immediate-mode draw calls, each with real per-call
    cost on NV2A (vertex staging + a GPU command submission) that's
    cheap to ignore on a modern desktop/mobile GPU and genuinely not
    cheap here. The pause screen shows more simultaneous text (3
    lines) than normal gameplay HUD, which is exactly why it measured
    slower despite being the "simpler" screen. Fixed in the SHARED
    file (benefits every platform, changes no visible pixel) by
    batching each glyph's lit-pixel quads into ONE glBegin/glEnd pair
    instead of one per pixel -- cuts draw-call count per glyph by up
    to ~36x. Required bumping gles_compat.cpp's kMaxVerts from 64 to
    256 (one glyph's worst case is 36 quads = 144 vertices) -- also a
    shared-file change, trivial memory cost (3KB) on every platform.

    CONFIRMED on real hardware: now mostly locked near 60fps. One real
    remaining puzzle, still open, user is continuing to narrow it down
    themselves: frame rate drops to a stable 29fps at some point during
    play (maybe tied to 2 enemies being active, or trail length growing
    -- not confirmed yet) and doesn't recover. Not chased further yet.

  - Guide-button quit-to-dashboard shortcut (added earlier): REMOVED.
    Confirmed broken by design, not a bug to fix -- original Xbox
    controllers (Duke/Controller S) predate the Guide-button concept
    entirely and have no such physical input. Verified directly in
    nxdk's own raw joystick driver (lib/sdl/SDL2/src/joystick/xbox/
    SDL_xboxjoystick.c): only 10 real button bits are ever read from
    hardware; SDL_GameController's "guide" maps to an 11th index real
    hardware never sets, so it could never have fired. ESC from the
    main menu already reaches quit_game() -> std::exit() ->
    HalReturnToFirmware(HalQuickRebootRoutine) (pdclib's own Xbox
    _PDCLIB_Exit.c) -- the standard homebrew dashboard-return path --
    so that remains the way to exit; no replacement shortcut added.

  - FPS counter (glShowFPSNV2A): removed now that it served its
    diagnostic purpose.


Real hardware test #3 -- split-screen (2026-11-06)
-------------------------------------------------------
Reported: player 2's half appeared to bleed/clip into player 1's.
Real, concrete cause found: split_screen_match.cpp's per-player
render loop sets glViewport per quadrant but never calls glScissor --
glViewport alone remaps NDC space to a rectangle but doesn't guarantee
every GL implementation clips rasterized fragments strictly to it at
the pixel level. Fixed with glEnable(GL_SCISSOR_TEST) + glScissor
matched to each viewport's rect (the standard, guaranteed-correct way
every real split-screen renderer isolates viewports), disabled again
before the shared divider-line/status overlay pass that intentionally
draws across the whole window. GL_SCISSOR_TEST/glScissor confirmed
implemented in nxdk-gles11 (gles_core.c) before relying on it.
Checked split_screen_menu.cpp too -- no multi-viewport rendering
there, fix only needed in split_screen_match.cpp. Not yet confirmed
on real hardware.

Also worth noting, likely NOT a bug: user also flagged possible
"double rendering." Each viewport's render pass intentionally draws
EVERY active player's ship (ground->print() + a loop over all
players), not just that viewport's own player -- so player 2's ship
correctly appears in player 1's view and vice versa, since it's one
shared 3D world seen from two cameras. That's the intended split-
screen behavior, not a bug, unless it turns out to look wrong in some
other way once the scissor fix is retested.


Sources
----------
nxdk, nxdk wiki (Getting Started, Projects-using-nxdk), fgsfdsfgs/pbgl,
Ryzee119/pbgl, Ryzee119/nxdk-gles11, JayFoxRox/neverball#1, kodi4xbox.
All repo facts (fork status, star counts, push dates) verified directly
via the GitHub API this session, not taken on faith from the first
research pass.
