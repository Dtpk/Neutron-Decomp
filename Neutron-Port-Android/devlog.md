# Neutron - Modern C++ Linux Port

A 1:1 clean-room architectural C++ port of the classic 3D PSP homebrew game **Neutron** (by Josep Florit), modernized to run natively on Linux using SDL2 and OpenGL.

---

## About Neutron

**Neutron** is a 3D Tron-style lightcycle arena combat game originally created for the Sony PlayStation Portable (PSP). Players control the "Proton" lightcycle traversing multi-faceted 3D polyhedral grids, dropping trails and competing against AI "Electron" enemies across 25 distinct geometric stages.

This port reconstructs the entire game engine in modern C++17 from decompiled pseudocode and DWARF metadata extracted from the original MIPS binary, completely removing proprietary PSP SDK dependencies (`sceGu`, `sceCtrl`, `sceDisplay`) and replacing them with cross-platform SDL2 and standard OpenGL.

---

## Features

- **1:1 Gameplay Fidelity:** Preserves the original game physics, AI navigation algorithms, cube union topology, and scoring systems.
- **25 Levels:** All original 3D levels (Cube, Tomb, Stairs, Tetris, Double, Snake, V, Crazy Cross, Death Stairs, Cross, Angry Snake, Tripod, Keops, Star, Corner, Magnet, Donut, Curve, Castle, Piramid, Knot, Blender, Shoes, Death Star, Helix) directly extracted and accurately reproduced.
- **Procedural 3D Vector Font:** The original custom Tron-style 6x6 vector block font is fully implemented for all UI, HUD, and menu text rendering.
- **Modern Controls:** Full keyboard navigation, steering, menu selection, and mouse touch/click input.
- **Multi-Resolution OpenGL:** High-resolution rendering (default 960x544, resizable) with smooth 60 FPS viewport scaling.
- **Game Modes:** Single player campaign (25 stages), Training mode, Stage Select with rotating 3D miniature level previews, and Credits.

---

## Directory Structure

```
neutron-port/
├── CMakeLists.txt        # CMake build configuration
├── Makefile              # Makefile build configuration
├── README.md             # This documentation
├── include/              # Header files
│   ├── button.h          # 2D/3D UI buttons and font rendering
│   ├── camera.h          # Perspective camera and target locator
│   ├── enemy.h           # AI Electron ship logic and steering
│   ├── game_data.h       # Extracted level matrices, font, and intro keyframes
│   ├── glinterface.h     # OpenGL 3D drawing primitives (cubes, quads, lines)
│   ├── miniature.h       # 3D miniature stage preview models
│   ├── neutron.h         # Main game state manager and loop controller
│   ├── piece.h           # Grid cell and trail state
│   ├── plane.h           # 3D arena face and directional traversal
│   ├── player.h          # Player Proton ship logic and controls
│   ├── ship.h            # Base ship physics and step movement
│   ├── sound_mixer.h     # Audio playback manager
│   ├── stage_interface.h # Menus, HUD, and UI screens
│   └── terrain.h         # Map topology, plane unions, and wireframe skeletons
└── src/                  # Implementation files
    ├── button.cpp
    ├── camera.cpp
    ├── enemy.cpp
    ├── game_data.cpp
    ├── glinterface.cpp
    ├── main.cpp
    ├── miniature.cpp
    ├── neutron.cpp
    ├── piece.cpp
    ├── plane.cpp
    ├── player.cpp
    ├── ship.cpp
    ├── sound_mixer.cpp
    ├── stage_interface.cpp
    └── terrain.cpp
```

---

## Building

### Prerequisites

Install the required development packages on Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libsdl2-mixer-dev libgl1-mesa-dev libglu1-mesa-dev
```

### Build with Make

```bash
cd /workspace/neutron-port
make
```

### Build with CMake

```bash
cd /workspace/neutron-port
mkdir build && cd build
cmake ..
make
```

---

## Running

Launch the game using the portable launcher:

```bash
./run.sh
```

Or execute the binary directly (it will automatically find the bundled libraries via embedded `$ORIGIN/libs` runpath):

```bash
./neutron
```

To run the automated verification test:

```bash
./run.sh --test
```

---

## Controls

| Action | Keyboard | Notes |
|---|---|---|
| **Turn Left** | `Left Arrow` / `A` / `Q` | Turn lightcycle left / Navigate menu left |
| **Turn Right** | `Right Arrow` / `D` / `E` | Turn lightcycle right / Navigate menu right |
| **Navigate Up** | `Up Arrow` / `W` | Menu selection up |
| **Navigate Down** | `Down Arrow` / `S` | Menu selection down |
| **Select / Confirm** | `Return` / `Space` / `Z` | Confirm choice / Select |
| **Cancel / Back** | `Escape` / `Backspace` / `X` / `Tab` | Back / Pause / Exit menu |
| **Change Camera** | `V` | Toggle third-person / cockpit view |
| **Mouse / Touch** | `Left Click` | Select buttons / Tap steering controls |

---

## Credits

- **Original Game:** Designed and programmed by **Josep Florit** for PSP Homebrew.
- **Original Music:** **Moish Vexler**
- **Modern Port:** Clean-room reverse engineered and ported to modern C++ / SDL2 / OpenGL.
