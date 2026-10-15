# OpenStunts

**Status: alpha.** Expect bugs, missing features and unfinished menu screens.
See [Status](#status).

A cross-platform, modernized 3D reimplementation of the classic DOS racing game
**Stunts / 4D Sports Driving** (Distinctive Software / Brøderbund, 1990).

It loads the original game data (tracks, car models, terrain, sprites, physics
tables) and renders it with a modern OpenGL 3.3 core renderer, while
reproducing the original driving model. A Minecraft-style front-end menu with
an AI-driven attract demo sits in front of the game.

**This project exists to test the capabilities of current AI models:** porting a
complete commercial game from its disassembly — geometry, physics, opponents,
HUD and a front-end — together with its tests and documentation.

* Reference disassembly / decompilation: [restunts](https://github.com/4d-stunts/restunts)
* Game data: bundled in [`assets/`](assets/) — see [Assets](#assets); an
  original installation is *optional*, via `--assets`, `$STUNTS_DIR` or
  `~/stunts`

## Status

**Alpha.** The port runs the original tracks with the original car physics;
large parts of the game (audio, replays, career) and several menu screens are
still missing or inert. Reference geometry, tables and constants come from the
restunts disassembly; where the disassembly and the partial C decompilation
disagree, the assembly wins.

Implemented:

- [x] CMake project + SDL2 window + OpenGL 3.3 core renderer
- [x] Bundled game data (`assets/`) + automatic copy into the build directory
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
- [x] Front-end menu (main / tracks / car / opponent / options) with an
      AI-driven attract demo, keyboard, gamepad and mouse support

Not yet:

- [ ] Audio (engine / skid / crash effects)
- [ ] Replays (`.RPL`)
- [ ] Career mode
- [ ] New / Edit Tracks, Auto-Manual transmission, Colour and Audio rows are
      placeholders (shown, disabled)
- [ ] Several Options rows are shown but not yet acted on

## Building

Requirements: CMake ≥ 3.20, a C++17 compiler, SDL2 development files, OpenGL.

The game data already lives in the repository (`assets/`) and CMake copies it
into the build directory next to the executables, so there is nothing to
download or install apart from the usual build dependencies — see
[Assets](#assets).

### Prebuilt binaries

Tagged releases are built natively on Linux and Windows by
[GitHub Actions](.github/workflows/build.yml) and attached to the GitHub
release:

| Archive | Contents |
| --- | --- |
| `OpenStunts-linux-x86_64.tar.gz` | `stunts3d`, `s3d_dump`, `assets/`, `README.md`, `LICENSE`, `NOTICE` |
| `OpenStunts-windows-x64.zip` | `stunts3d.exe`, `s3d_dump.exe`, `SDL2.dll`, `assets/`, `README.md`, `LICENSE`, `NOTICE` |

Every archive is self-contained: unpack it and run the binary — no Stunts
installation and no SDL2 install needed.

```sh
tar -xzf OpenStunts-linux-x86_64.tar.gz
./OpenStunts/stunts3d
```

The Linux build links the runner's glibc, so it needs an equal-or-newer glibc
than the build image provides.

### Linux

```sh
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/stunts3d
```

### macOS

> **Note:** macOS is not currently built in CI and the build may need fixes.

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

## Assets

The original game **data** ships in this repository under
[`assets/`](assets/) and CMake copies it next to every executable on each build
(`build/assets/`, `build/tests/assets/`), so a fresh clone runs with no further
setup:

```sh
git clone <repo> && cd <repo>
cmake -S . -B build && cmake --build build -j
./build/stunts3d          # menu + attract demo, no Stunts install needed
```

What is in `assets/`, and what is deliberately not:

| Included — data files | Excluded — DOS programs |
| --- | --- |
| `.TRK` tracks, `.PVS`/`.P3S`/`.PES` shapes, `.PRE` element tables, `.RES` cars, `.FNT` fonts, `.KMS`/`.SFX`/`.VCE` effects, `.RPL` replay, `.COD`/`.CMN`/`.DIF`/`.HDR` video-mode data, and the rest of the original data set | `*.EXE`, `*.COM` and the `*.DRV` loadable driver binaries — `STUNTS.COM`, `ST.COM`, `LOAD.EXE`, `SETUP.EXE`, `AD15.DRV`, `MT15.DRV`, `PC15.DRV`, `TD15.DRV` |

The `.DRV` files are COM-style programs too (they start with a near `JMP`),
they merely look like data. None of these are needed: the port re-implements
the game code in `src/`.

To re-import the data from your own installation:

```sh
tools/import_assets.sh /path/to/stunts
```

An original installation still works as an alternative (see
[Running](#running)), and `--assets DIR` always wins.

## Running

```
./build/stunts3d [--menu] [--menu-screen N] [--assets DIR] [--track FILE.trk]
                 [--config FILE] [--car ID] [--opp-car ID] [--opp N]
                 [--screenshot out.png] [--frames N] [--camera N]
                 [--wheel-scale F] [--top] [--drive]
```

With no arguments the front-end menu starts, with an AI-driven demo race
running behind it. *Let's Drive* starts a race with the selected track, car and
opponent, *Tracks* shows a top-down view of the selected track, *Car* and
*Opponent* pick the racers, and *Exit* quits.

A scene can also be requested directly (this is what the screenshot tooling
does):

```sh
./build/stunts3d --drive --car PMIN --camera 1     # driveable car
./build/stunts3d --top --track HELENS.TRK          # debug top-down viewer
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
| `--drive` | Run the driveable car scene instead of the front-end menu |
| `--menu` | Force the front-end menu, even alongside other flags |
| `--menu-screen N` | Start the menu on a screen: 0 main, 1 tracks, 2 car, 3 opponent, 4 options |

With `--screenshot` the loop ignores the wall clock and advances the simulation
by one fixed step per rendered frame, so a given `--frames N` always captures the
same simulated moment even on a loaded machine (repeated captures are
byte-identical).

The asset directory is searched in this order: `--assets`, `$STUNTS_DIR`, the
bundled copy (`<executable>/assets`, `<executable>/../assets`, then `./assets`),
`~/stunts`, `./stunts`, `../stunts`. Unless you override it, the bundled copy is
what gets used, so no Stunts installation is required.

### Controls

Front-end menu:

| Input | Action |
| --- | --- |
| `Up` / `Down` | Move the selection (wraps, skips disabled rows) |
| `Left` / `Right` | Step the value of the selected row (track, option) |
| `Enter`, gamepad `A` | Activate the selected row |
| `Esc`, gamepad `B` | Back / main menu |
| Mouse | Hover to select, click to activate; the left/right half of a value row steps it |

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

Tests that need game assets skip themselves when no asset directory is found;
with the bundled `assets/` they run against the copy in `build/tests/assets/`.

## Layout

```
assets/       original Stunts game data shipped with the repo (no programs)
src/core      settings, logging, path discovery, fixed timestep
src/platform  SDL2 window / GL context / input
src/render    GL loader, shaders, meshes, camera, HUD, sprites, screenshots
src/assets    asset format parsers (.TRK, .P3S, .RES, palettes, sprites, fonts)
src/world     track -> geometry, element/object tables, materials, skybox,
              car meshes, dashboard, route graph
src/physics   car dynamics, collision, opponents (ported from restunts)
src/game      scenes, race rules
src/ui        menu model + GUI drawing (font, panels, buttons)
tools/        asset inspection CLI, asset importer
tests         unit tests (ctest)
.github/      CI: native Windows/Linux builds + release packaging
reference/    (git-ignored) local checkout of the restunts disassembly, kept
              as the porting reference; not part of the repository
```

## Credits

Original game © Distinctive Software / Brøderbund.

The data files under `assets/` come from a Stunts installation and remain the
property of their original rights holders. They are included so the project can
be cloned and run for research, interoperability and testing purposes — and the
DOS program files (`*.EXE`, `*.COM`, `*.DRV`) are deliberately *not*
distributed with it.

## License

The original source code in this repository is licensed under the
[Apache License 2.0](LICENSE); third-party attributions are listed in
[`NOTICE`](NOTICE).

That license covers **only** the original code. It does not cover:

- the game data in [`assets/`](assets/) — copyright its original owners, and
  included so the project can be built and run;
- material derived from the [Restunts](https://github.com/4d-stunts/restunts)
  disassembly (which publishes no license) or from the original game —
  including the generated tables under `src/world/` and the game constants and
  tables under `src/physics/`;
- vendored third-party code, which keeps its own license: GLM
  (`third_party/glm/copying.txt`) and stb_image_write (MIT or public domain,
  see the header of `third_party/stb/stb_image_write.h`).

"Stunts" and "4D Sports Driving" are trademarks of their respective owners;
this project is not affiliated with or endorsed by them.
