# Stunts3D

A cross-platform, modernized 3D reimplementation of the classic DOS racing game
**Stunts / 4D Sports Driving** (Distinctive Software / Brøderbund, 1990).

It loads the original game assets (tracks, car models, terrain, sprites,
physics data) directly from a Stunts installation and renders them with a
modern OpenGL renderer, while reproducing the original driving model.

* Reference disassembly / decompilation: [restunts](https://github.com/4d-stunts/restunts)
* Original game data: expected in `~/stunts` (or via `--assets` / `$STUNTS_DIR`)

## Status

Early development. The current milestone (M1) renders the original track layout
in 3D:

- [x] Cross-platform CMake project + SDL2 window + OpenGL 3.3 core renderer
- [x] `.TRK` track parser (items, terrain, skybox)
- [x] Terrain heightfield + placeholder road/scenery geometry
- [x] Fly camera + PNG screenshot capture
- [ ] Original 3D shape assets (`.P3S`/`.3SH`), palettes, sprites, fonts
- [ ] Faithful car physics (from restunts)
- [ ] Race rules, opponents, career, replays, audio

## Building

Requirements: CMake ≥ 3.20, a C++17 compiler, SDL2 development files, OpenGL.

### Linux

```sh
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/stunts3d
```

### macOS

```sh
brew install cmake sdl2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/stunts3d
```

### Windows (MSVC)

1. Install CMake and SDL2 (e.g. via vcpkg: `vcpkg install sdl2:x64-windows`).
2. Configure with the vcpkg toolchain:
   ```
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```
   (or point `SDL2_DIR` at an official SDL2 development package.)

## Running

```
./build/stunts3d [--assets DIR] [--track FILE.trk] [--screenshot out.png] [--frames N]
```

The asset directory is searched in this order: `--assets`, `$STUNTS_DIR`,
`~/stunts`, `./stunts`, `../stunts`.

### Controls (debug camera)

| Key | Action |
| --- | --- |
| `W`/`A`/`S`/`D` | Move (hold `Shift` to sprint) |
| `Q`/`E` | Down / Up |
| Right mouse drag | Look around |
| `F1` | Toggle mouse look |
| `F12` | Save screenshot `stunts3d_NNN.png` |
| `Esc` | Quit |

## Tools

```
./build/s3d_dump trk FILE.trk   # ASCII item + terrain map and statistics
./build/s3d_dump dir            # Print the detected asset directory
```

## Layout

```
src/core      logging, path discovery
src/platform  SDL2 window / GL context / input
src/render    GL loader, shaders, meshes, camera, screenshots
src/assets    asset format parsers (.TRK, ...)
src/world     track -> geometry, element tables, physics helpers
src/physics   car dynamics (ported from restunts)
src/game      menus, race logic, opponents, replays
tools/dump    asset inspection CLI
tests         unit tests (ctest)
reference/    copy of the restunts reverse-engineering project (spec)
```

## Credits

Original game © Distinctive Software / Brøderbund. This project ships no
original assets and requires a legitimate Stunts installation.
