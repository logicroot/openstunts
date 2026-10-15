# OpenStunts

A cross-platform, modernized 3D reimplementation of the classic DOS racing game
**Stunts / 4D Sports Driving** (Distinctive Software / Brøderbund, 1990).

It loads the original game assets (tracks, car models, terrain, sprites,
physics data) directly from a Stunts installation and renders them with a
modern OpenGL renderer, while reproducing the original driving model.

* Reference disassembly / decompilation: [restunts](https://github.com/4d-stunts/restunts)
* Original game data: expected in `~/stunts` (or via `--assets` / `$STUNTS_DIR`)

## Status

The port runs the original tracks with the original car physics. Reference
geometry, tables and constants come from the restunts disassembly; where the
disassembly and the partial C decompilation disagree, the assembly wins.

Implemented:

- [x] CMake project + SDL2 window + OpenGL 3.3 core renderer
- [x] Asset loaders: `.TRK` tracks, `.P3S` shapes, `.RES` archives, palettes,
      sprites, `.FNT` fonts, compressed resources
- [x] Track geometry: elements, terrain heightfield, skybox panorama
- [x] Car models + the original physics (engine, steering, motion, collision,
      debris) ported from restunts
- [x] Opponents with per-car AI
- [x] Race rules: start/finish resolution, checkpoints, penalties, the 20 Hz
      race clock, lap timing
- [x] HUD, in-cockpit dashboard and crash overlay
- [x] Drive scene (chase + cockpit cameras) and a debug fly-camera scene

Not yet:

- [ ] Audio (engine / skid / crash effects)
- [ ] Replays (`.RPL`)
- [ ] Front-end menus / career

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
./build/stunts3d [--assets DIR] [--track FILE.trk] [--config FILE]
                 [--car ID] [--opp-car ID] [--opp N]
                 [--screenshot out.png] [--frames N] [--camera N]
                 [--wheel-scale F] [--top] [--drive]
```

Without `--drive` the debug fly-camera track viewer is shown; with `--drive`
you get the driveable car on the track:

```sh
./build/stunts3d --drive --car PMIN --camera 1
```

| Flag | Meaning |
| --- | --- |
| `--assets DIR` | Stunts asset directory (overrides the search below) |
| `--track FILE.trk` | Track to load (default: `last_track` from the config, else `DEFAULT.TRK`) |
| `--config FILE` | Config file to read (default: see [Configuration](#configuration)) |
| `--car ID` | Player car id, e.g. `PMIN`, `COUN`, `VETT` |
| `--opp-car ID` | Opponent car id (omit for a solo race) |
| `--opp N` | Opponent driving style, 1..6 |
| `--screenshot out.png` | Save a PNG once `--frames` frames have elapsed |
| `--frames N` | Frame at which to capture the screenshot (default 3) |
| `--camera N` | Drive camera: `0` = cockpit, non-zero = chase |
| `--wheel-scale F` | Scale each wheel about its centre (default `1.25`) |
| `--top` | (debug scene) top-down view |
| `--drive` | Run the driveable car scene instead of the debug viewer |

With `--screenshot` the loop ignores the wall clock and advances the simulation
by one fixed step per rendered frame, so a given `--frames N` always captures the
same simulated moment even on a loaded machine (repeated captures are
byte-identical).

The asset directory is searched in this order: `--assets`, `$STUNTS_DIR`,
`~/stunts`, `./stunts`, `../stunts`.

### Controls

Drive scene:

| Key | Action |
| --- | --- |
| `Up`/`W`, `Down`/`S` | Accelerate / brake-reverse |
| `Left`/`A`, `Right`/`D` | Steer |
| `Space` | Handbrake |
| `X` / `Z` | Gear up / gear down |
| `R` | Reset car |
| `C` | Cycle camera modes (cockpit / chase) |
| `Tab` | Toggle the cockpit dashboard |
| `P` | Pause |
| `Esc` | Quit |

A gamepad (if connected) is also supported: left stick steers, triggers are
throttle/brake.

Debug camera scene:

| Key | Action |
| --- | --- |
| `W`/`A`/`S`/`D` | Move (hold `Shift` to sprint) |
| `Q`/`E` | Down / Up |
| Right mouse drag | Look around |
| `F1` | Toggle mouse look |
| `T` | Toggle top-down view |
| `F12` | Save screenshot `stunts3d_NNN.png` |
| `Esc` | Quit |

## Configuration

Settings live in an INI file (`key = value`, `#`/`;` comments):

* Linux: `$XDG_CONFIG_HOME/stunts3d/config.ini` (or `~/.config/stunts3d/config.ini`)
* Windows: `%APPDATA%/stunts3d/config.ini`

| Key | Default | Meaning |
| --- | --- | --- |
| `width`, `height` | 1280, 720 | Window size |
| `vsync` | true | Vertical sync |
| `fullscreen` | false | Fullscreen |
| `highdpi` | true | HiDPI framebuffer |
| `mouse_sensitivity` | 0.12 | Debug-camera mouse look |
| `fov_deg` | 80.0 | Horizontal field of view |
| `wheel_scale` | 1.25 | Per-wheel visual scale (1.0 = original) |
| `show_fps` | true | FPS overlay |
| `asset_dir` | | Asset directory |
| `last_track` | DEFAULT.TRK | Track loaded when `--track` is omitted |
| `player_car` | COUN | Player car id |
| `opponent_car` | | Opponent car id (empty = solo race) |
| `opponent_type` | 0 | Opponent driving style, 1..6 |
| `pattern_fill` | true | Stipple patterned materials (elevated/sloped road) |

## Tools

```
./build/s3d_dump trk FILE.trk   # ASCII item + terrain map and statistics
./build/s3d_dump p3s FILE.p3s   # List the resources in a 3D shape archive
./build/s3d_dump dir            # Print the detected asset directory
```

## Tests

```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Tests that need game assets skip themselves when no asset directory is found.

## Layout

```
src/core      settings, logging, path discovery, fixed timestep
src/platform  SDL2 window / GL context / input
src/render    GL loader, shaders, meshes, camera, HUD, sprites, screenshots
src/assets    asset format parsers (.TRK, .P3S, .RES, palettes, sprites, fonts)
src/world     track -> geometry, element/object tables, materials, skybox,
              car meshes, dashboard, route graph
src/physics   car dynamics, collision, opponents (ported from restunts)
src/game      scenes, race rules
tools/dump    asset inspection CLI
tests         unit tests (ctest)
reference/    copy of the restunts reverse-engineering project (spec)
```

## Credits

Original game © Distinctive Software / Brøderbund. This project ships no
original assets and requires a legitimate Stunts installation.
