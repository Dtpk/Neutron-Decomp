# Neutron Custom Map Editor

A small standalone GUI for building a custom Neutron stage (`map.txt`) without
hand-editing coordinates in a text file. Built with SDL2 + OpenGL (the same
stack the game itself uses) — no other dependencies.

Saving writes `map.txt` next to the editor's own executable. Drop that file
into the same folder as the game's `neutron` binary (next to
`neutron_save.txt`) to add it as a 26th stage in Stage Select.

## Build

### Linux
```
sudo apt install libsdl2-dev   # if not already installed
make
./build/neutron_map_editor

If the above is too much I bundled the "libs" folder with it so just do this to run in a portable manner. 
chmod +x neutron_map_editor
chmod +x run.sh
Then run using this.
./run.sh
```

### Windows
Install [SDL2 development libraries](https://github.com/libsdl-org/SDL/releases)
(the `SDL2-devel-*-VC` or `-mingw` package) and [CMake](https://cmake.org/download/),
then from the project folder:
```
cmake -B build -S .
cmake --build build --config Release
```
The exe lands in `build/build/` (CMake's own build dir plus the project's
`CMAKE_RUNTIME_OUTPUT_DIRECTORY`). Run it from there, or copy it anywhere —
`map.txt` always saves next to wherever the exe itself lives.

## Using it

- **Left panel** — stage name, Save/Reload/Clear, current editing layer
  (`Y`), cube count (max 20, matching the game's own per-stage limit), and a
  connectivity check.
- **Middle grid** — a top-down slice of the current `Y` layer. Click a cell
  to add/remove a cube there.
- **Right view** — a live 3D preview of the whole shape. Drag to orbit,
  scroll to zoom. The layer you're currently editing is highlighted with a
  faint cyan plane so you can see where it sits in the full shape.
- **PageUp/PageDown** (or `[`/`]`) also change the editing layer.

Cubes snap to the same grid the game itself uses — each step is 2 units on
every axis. A stage is just a hollow shell built by placing solid cubes and
letting the game cancel out the internal walls where two cubes touch, so for
a sensible playable arena every cube should share a face with at least one
other cube (the "SHAPE: CONNECTED" indicator checks this, but doesn't block
saving — a disconnected cube just floats as its own separate little room).

## File format

```
name=My Custom Stage
cube=0,0,0
cube=0,0,2
cube=2,0,2
```

- `name=` — up to 31 characters, shown in Stage Select and the in-game HUD.
- `cube=x,y,z` — one line per cube, up to 20 cubes. Coordinates should be
  even integers (the grid the game itself builds stages on).
- Lines starting with `#` are comments and ignored, same as the game's own
  `neutron_save.txt`.
