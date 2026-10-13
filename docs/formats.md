# Stunts asset formats (working notes)

Notes derived from the [restunts](https://github.com/4d-stunts/restunts) project
(see `reference/restunts/`) and verified against the shipping files in `~/stunts`.
This file grows as formats are implemented.

## Track file (`*.TRK`) — 1802 bytes

| Offset | Size | Content |
| --- | --- | --- |
| 0 | 900 | item bytes, 30×30, left→right, **top→bottom** |
| 900 | 1 | skybox id (0 desert, 1 tropical, 2 alpine, 3 city, 4 country) |
| 901 | 900 | terrain bytes, 30×30, left→right, **bottom→top** |
| 1801 | 1 | unknown |

Source: `reference/restunts/docs/format.html`.

## Resource container (`.RES`, and the payload of `.PRE`/`.PVS`/`.PES`/`.COD`)

All multi-byte integers are little-endian.

| Offset | Size | Content |
| --- | --- | --- |
| 0 | u32 | `size` — total archive size, including this header |
| 4 | u16 | `chunks` — number of resources, N |
| 6 | 4·N | resource identifiers, 4 raw bytes each |
| 6+4N | 4·N | u32 offsets, parallel to the identifier table |
| 6+8N | … | resource payloads; offsets are relative to this address |

`data_offset = 6 + chunks*8`. Identifiers are space-padded to 4 chars when
matched (a stored NUL matches a query space). Payload lengths are *not* stored;
derive them by sorting the offset table: `length[i] = next_higher_offset - offset[i]`,
and the last resource ends at `size`.

The same header is used for 2D (`.PVS`) and 3D (`.P3S`) shape archives;
`chunks` is the shape count and offsets index the shapes.

## Compression wrapper (`.PRE`, `.PVS`, `.PES`, `.COD`)

A compressed file begins with a 4-byte `compr_header`:

| Offset | Size | Content |
| --- | --- | --- |
| 0 | u8 | `passes_or_type`. If bit 7 set → multi-pass; low 7 bits = pass count. Else single pass, and this byte is the pass type. |
| 1 | u16 | `sizel` |
| 3 | u8 | `sizeh` |

`final_size = sizel | (sizeh << 16)`. For a multi-pass file the pass stream
begins at offset 4, and each pass output is itself a compressed stream for the
next pass (it starts with its own 4-byte header whose first byte is the type).

Pass types: **1 = RLE**, **2 = VLE**. Every shipping `.PRE`/`.PVS`/`.PES`/`.COD`
uses VLE; `.PVS`/`.PES` use two passes. Implemented in
`src/assets/compression.{h,cpp}` (ported from restunts `fileio.c`).

### RLE (type 1)

After the 4-byte header, a `compr_rle_header`: `u16 srcsizel`, `u8 srcsizeh`,
`u8 unk`, `u8 esclen`, `u8 esc[16]`. Low 7 bits of `esclen` give the escape-count;
bit 7 flags "skip the sequence pass". Two passes:

1. *sequence pass* — `[esc][bytes…][esc][count]` expands a byte sequence `count`
   times (using `esc[1]` as the escape code).
2. *single pass* — each escape code is looked up in a 256-entry table; its
   index selects run-length semantics (1 = byte run, 3 = word run, else
   `index-1` repetitions).

### VLE (type 2)

After the 4-byte header: `u8 esclen_byte` (bit 7 = additive mode, low 7 bits =
escape count), then `esclen` width bytes, then the alphabet, then the bit-packed
code stream. Escape tables are built as `code_limit *= 2` per level. The decoder
emits exactly `final_size` bytes; the bit reader may over-read the last symbol,
so the input is zero-padded. No shipping file uses additive mode.

### Verified test vectors

| file | on-disk | passes | final | contents |
| --- | --- | --- | --- | --- |
| `GAME.PRE` | 15434 | 1 | 20890 | archive, 20 chunks, `plan`+`wall` |
| `SDMAIN.PVS` | 1176 | 2 | 1732 | archive, 5 chunks (`!pal`, `!eg0`, …) |
| `GAME1.P3S` | 17556 | 2 | 29078 | 3D shape archive |
| `GAME2.P3S` | 21852 | 2 | ~34156 | 3D shape archive (overflow shapes) |

## `GAME.PRE` resources

`plan` (536 × 34-byte `PLANE`) and `wall` (191 × 6-byte `WALL`) describe
drivable surfaces and vertical walls. See `reference/restunts/docs/game.res.txt`.

```
struct PLANE {            // 34 bytes
    s16 angleYZ, angleXY; // angles in 1/1024 circle
    VECTOR origin;        // 3×s16
    VECTOR normal;        // 3×s16, length 8192 (0x2000)
    VECTOR rotation[3];   // 9×s16, columns; identity is 16384 (0x4000)
};
struct WALL { s16 angleXZ, x, z; };  // 6 bytes
```

Planes are grouped in quartets; the four planes differ only by xz axis
permutation/sign. There is no stored link between plan/wall and track elements —
the assembly rules live in the `trkObjectList[215]` table (see
`reference/restunts/src/restunts/c/frame.c`).

The crashed-windscreen overlay (seg003.asm `init_crak`) uses two more chunks:

```
struct CINF {                    // "cinf": stage table
    u16 stage_count;             // 4
    u16 segment_counts[stage_count];  // 6, 18, 42, 77
};
struct CRAK {                    // "crak": 8-byte crack segments
    s16 x1, y1, x2, y2;          // 320x200 view space, flat array of 77
};
```

`init_crak` draws the first `segment_counts[min(frame / (framespersec / 7),
stage_count - 1)]` segments, each as a white 1px line with a black edge 1px
above and below; the y coordinates scale with the view height / 200.

The water crash (`car_crashBmpFlag == 2`) uses `do_sinking` instead: a rect
`[0, 320] x [bottom - fill, bottom]` filled with `skybox_wat_color`, where

```
span = framespersec << 2                       // 80 at 20 Hz
fill = min(frame, span) * clip_height / span   // grows 0 -> clip_height
```

`skybox_wat_color` comes from `material_color_list` at byte offset `0xC8`
(element `0x64`, because the table stores words): palette index 136 (blue).
`load_skybox` (seg003.asm loc_1D88E) reads the sky and ground colours from the
same table at element `0x11` and `0x10`.

## Palettes

The VGA palette lives in `SDMAIN.PVS` as resource `!pal` (784 bytes): a 16-byte
2D-shape header followed by 768 bytes of 256 RGB triplets with **6-bit**
components (0–63). Convert with `(v << 2) | (v >> 4)`.

## 3D shapes (`*.P3S` / `.3SH`)

A decompressed `.P3S` is a resource archive of named shapes. A raw shape:

```
struct SHAPE3DHEADER { u8 numverts, numprimitives, numpaints, reserved; };
VECTOR verts[numverts];        // 3×s16 each
u8 cull1[numprimitives * 4];
u8 cull2[numprimitives * 4];
primitives[];                  // variable, see below
```

Each primitive is `u8 type`, `u8 flags`, `u8 paint[numpaints]`, then vertex
indices. The index count per type is
`{0,1,2,3,4,5,6,7,8,9,10,2,6,3,0,0}` (types 11/12/13 = 2/6/3). Source:
`reference/restunts/src/restunts/c/shape3d.c` (`shape3d_init_shape`).

The paint bytes select *materials* (`world/material.h`); a shape with
`numpaints > 1` can be drawn with any of them. The object's `surface` field
picks which slot to use: `0`..`2` select a fixed paint (e.g. the `road` shape's
paved/dirt/ice variants) and `0xFF` marks an **animated** object whose slot
cycles with `byte_3C0C6[game_frame & 15] = {0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3}`.
An out-of-range slot clamps to 0. This drives the windmill sails (`wind`:
material 113 sail vs. 118 transparent mask) and Joe's flashing sign (`rest`).
Material 118 is a fully transparent mask and is never drawn.

Car bodies (`ST<ID>.P3S`) are shaded exactly like the track: every paint byte is
a material index resolved through `material_color_list`, so each body panel is
flat-filled with the palette colour of its material. The racing bodies carry
several paint slots (`num_paints`, 7 for the retail cars); the draw's
`material` field selects the livery variant: `frame.c` sets it to
`gameconfig.game_playermaterial` (default 0) for the player car and
`gameconfig.game_opponentmaterial` for the rival. Slot 0 of `STCOUN.P3S` is the
blue Countach livery (`car_primitive_color`, `append_car_model`).

One special paint job, `0x2D`, always marks the **rear light** polygons. The
original never draws that livery: it substitutes `backlights_paint_override`,
which `frame.c` sets to material `0x2E` while coasting and `0x2F` while the
brake pedal is down (`shape3d.c` / `seg006.asm loc_25D3C`). Material `0x2E`
resolves to palette 4 (dark red) and `0x2F` to palette 12 (bright red), so the
lights brighten under braking.

## 2D sprites (`.PVS`)

A `.PVS` also decompresses to a resource archive, but its chunks are flat 2D
shapes:

```
struct SHAPE2D { u16 width, u16 height, u16 pivot_x, u16 pivot_y,
                 u16 pos_x, u16 pos_y, u8 unk3, u8 unk4, u8 unk5, u8 unk6; };
u8 pixels[width * height];    // palette indices; 0xFF (and any index >= 16) clear
```

`pos_x`/`pos_y` anchor the unscaled blit (`sprite_putimage`); `pivot_x`/`pivot_y`
(`s2d_unk1`/`s2d_unk2` in the decompilation) anchor the **scaled** blit:
`shape_op_explosion` places that pixel on the caller's centre point, so it is the
sprite's pivot in authored pixels.

**Flip flag.** A payload is frequently stored *transposed* (column-major) and is
transposed back in place when the archive loads (`seg012.asm`
`file_unflip_shape2d`; `shape2d.c:file_unflip_shape2d`). The variant is
`unk5 >> 4` (1 = plain transpose, 2 = transpose with the stored columns split
into two interleaved halves, 3 = a third interleaved variant) and is only
honoured when `unk6 & 0xF0 == 0`; 0 means "stored as-is". Both flags are
per-shape, so e.g. in `STDBVETT.PVS` the digit glyphs `dig0/dig1/dig4/dig8`
are flipped while `dig2/dig3/dig5/dig6/dig7/dig9` are not. Skipping the
unflip renders those shapes as diagonal noise; `load_sprite2d` applies it so
every caller sees the upright bitmap the original blits.

`SDGAME2.PVS` (7 chunks, `ex01 ex02 ex03 left righ !cg0 !eg0`) holds the HUD 2D
graphics (`seg003.asm load_sdgame2_shapes`):

* `left` / `righ` (24×15): the direction arrows, drawn unscaled at screen centre.
* `ex01` (32×19), `ex02` (32×19), `ex03` (32×21): the crash-explosion cycle,
  selected by `(game_frame >> 2) % 3` and scaled so its width fills the longer
  side of the crashed car's viewport-clipped screen rectangle
  (`frame.c` `shape_op_explosion`).

### Cockpit dashboard (`STDA<car>.PVS`)

Each player car ships a dashboard archive named `STDA` + the car id (e.g.
`STDACOUN.PVS`). It holds the 2D panel drawn over the lower/upper screen bands
in the original's default view (`dashb_toggle = 1`, `seg005.asm`
`setup_car_shapes` / `c/restunts.c run_game`). For the Countach:

| id | size | pos | role |
| --- | --- | --- | --- |
| `dash` | 320×70 | (0,130) | the main panel; `s2d_pos_y` is `dashbmp_y` |
| `roof` | 320×9 | (0,0) | the cockpit frame; `s2d_height` is `roofbmpheight` |
| `whl1`/`whl2`/`whl3` | 184×32 | (40,168) | steering wheel: turned left / centred / turned right |
| `gbox` | 64×56 | (256,144) | the gear-box gauge (animated) |
| `ins1`/`ins3` | 72×18 | (64,21)/(0,21) | turned-wheel face patches (wheel held left / right) |
| `ins2` | 136×39 | (64,148) | the instrument face (speedometer + tachometer dials) |
| `inm1`/`inm3` | 72×19 | (64,20)/(0,20) | the patches' AND stencils, one row above the patch |
| `dast`/`dasm` | 320×8 | (0,130) | per-frame band filling the gap above the panel (JAGU/P962/PMIN only) |

The gear-box face `gbox` (64×56 at (256,144)), the gear lever and the steering
dots live in the companion archive `STDB<car>.PVS`: `gnob`/`gnab` (the knob's
colour and its AND stencil), `dot `/`dota` (the indicator's colour and its AND
stencil) and `dot1`/`dot2`, which the engine uses only to erase the previous
frame's dot (the port redraws the panel every frame, so it ignores them).

The original renders the 3D world inside `rect_windshield =
[roofbmpheight, dashbmp_y]` and composites the panel over the rest. The port
draws, in this order, `dash` → `dast` → steering wheel → `ins2` → digital
glyphs → needles → turned-wheel patch → `gbox` → gear knob → steering dot →
`roof`. The panel art is blitted opaquely, so later pieces hide earlier ones
where they overlap; the `dast`/digital/patch overlays are alpha-blended instead
(their transparent and palette-index-0 texels leave the destination alone,
matching the engine's OR/AND blits). The gear knob and steering dot are
alpha-blended for the same reason: their `gnab`/`dota` stencil is blitted
opaquely (0x00 → black, 0xFF → keep the destination) and then `gnob`/`dot ` with
palette index 0 as a no-op, which reproduces `(dst & mask) | colour`. In
particular:

* The steering wheel is picked from `whl1`/`whl2`/`whl3` by the steering angle
  (`seg005.asm loc_2319D`: `trunc(angle/8)` against a ±10 dead zone), then
  `ins2` is blitted over its lower rows. `ins2`'s upper 20 rows are identical to
  `dash`, so only the turned-wheel art inside the wheel cut-out is actually
  hidden.
* `gbox` is blitted at (256,144), overwriting the `dash` art underneath it. It
  is on screen only around a gear change: `seg005.asm loc_23030` restores the
  saved background (the `dash` art) and blanks the plate once both
  `car_changing_gear` and `car_fpsmul2` reach zero, so the plate and lever
  disappear again after the shift. `car_fpsmul2` is set to 1.5 s of frames when
  a shift starts (`statecar.c`, `(framespersec >> 1) + framespersec`) and only
  counts down once the lever has reached its gate, i.e. the pair stays up for
  the lever animation plus 1.5 s.

### Cockpit instruments (`seg005.asm setup_car_shapes`, case 2)

The gauge geometry comes from the car's `simd` chunk (`CARSTATE`/`SIMD`,
`c/externs.h`): the needle origins are `POINT2D`s and each needle's tip list is
a run of byte-sized `(x, y)` pairs anchored at its origin.

| field | offset | meaning |
| --- | --- | --- |
| `steeringdots[62]` | 234 | 31 steering-dot `(x, y)` screen positions |
| `spdcenter` | 296 | speedometer needle origin |
| `spdnumpoints` | 300 | number of speedometer tip points |
| `spdpoints[208]` | 302 | speedometer tip list (byte pairs) |
| `revcenter` | 510 | tachometer needle origin |
| `revnumpoints` | 514 | number of tachometer tip points |
| `revpoints[256]` | 516 | tachometer tip list (byte pairs) |

Each needle is a single line `preRender_line(meter_needle_color, center, tip)`.
`meter_needle_color` is initialised from `dialog_fnt_colour` (palette 15) when
the skybox loads (`seg003.asm load_skybox`), so the port reuses the HUD
foreground colour.

The gauge origins are relative to the `ins2` sprite; the tips are indexed by:

* **speedometer** — from `spdcenter.y`: `-1` (`0xFFFF`) ⇒ no needle, `0` ⇒ the
  digital readout (`car_speed >> 8` drawn with `dig0`..`dig9` placed at
  `spdpoints[0..5]`), anything else ⇒ analog with index
  `min(car_speed / 640, spdnumpoints - 1)`. `car_speed` is the rev-coupled
  speed (the port's `car_rev_speed`); `car_speed2` is `car_actual_speed`.
* **tachometer** — always analog, index
  `min((uint16_t)car_currpm >> 7, revnumpoints - 1)`.

The Countach (`CARCOUN.RES`) is analog on both dials
(`spdcenter = (25,19)`, 84 points; `revcenter = (107,19)`, 71 points); the
Vetty (`CARVETT.RES`) has `spdcenter.y == 0`, i.e. the digital speedometer.

The gear knob is drawn pivot-anchored by its AND stencil + OR colour pair
(`loc_23120`: `sprite_putimage_and_alt2` with `gnab`, then
`sprite_putimage_or_alt` with `gnob`) at `gbox_pos + knob_points[gear]`:
`gnob`'s pivot (12,11) is its centre, so the lever sits centred on the selected
gate. `car_knob_x/y` are seeded to `knob_points[1]` at race start
(`restunts.c init_game_state`). `gnab` is 0x00 over the knob's footprint and
0xFF over the surround, so drawing it opaquely first is what keeps the gear-box
art visible around the lever (the knob's own palette-index-0 pixels stay black).
The plate and the lever share the same visibility window (see `gbox` above): the
port draws neither while `car_changing_gear` and `car_fpsmul2` are both zero.

The steering dot (`dot ` and its `dota` stencil) is staged the same way: the
steering angle maps to an index `min(|sign(a) * (|a| >> 3)|, 30)` into
`steeringdots`, and the dot is drawn at `steeringdots[i]` with its x mirrored
about `steeringdots[0].x` (`2*steeringdots[0].x - x`) when steering left, using
the same AND-stencil + OR-colour blit as the knob. Note the resource id is the
four-character `"dot "` (trailing space).

#### Digital readout, `dast` strip and turned-wheel patches

The digital speedometer (only the Vetty, `spdcenter.y == 0`) stamps up to three
8x7 glyphs from `STDB<car>.PVS` (`"dig0"`..`"dig9"`, located at loc_2105).
`loc_2338C`/loc_233A2 split the reading `si = car_speed >> 8`:

* **hundreds** — `2` for `si >= 200`, `1` for `si >= 100`, else suppressed (and
  subtracted from `si`);
* **tens** — `si / 10`, suppressed only while nothing has been drawn yet;
* **ones** — `si % 10`, always stamped.

Each glyph is an OR-blit in which palette index 0 is a no-op, so the suppressed
leading columns leave the face untouched; the k-th glyph reads the k-th byte
pair of `spdpoints` (anchored at the `ins2` origin).

The turned-wheel patches `ins1`/`ins3` (same `STDA<car>.PVS`) are panel-local
like the needles. They are stamped into the face-sized window with their
`inm1`/`inm3` stencils whenever the wheel is held left/right
(`dashboard_wheel_index`): the stencil ANDs a black region — a row taller than
the patch — and the patch ORs its colours back. The stencil's index 0 is opaque
black while the patch's index 0 is a no-op, so the face shows through wherever
the stencil is transparent.

The `dast` strip (with its `dasm` stencil) is present only on the cars whose
`dash` starts below y=130 (JAGU, P962, PMIN): a 320-wide absolute-coordinate
band at (0,130) that fills the gap above the panel.

## Track assembly (`.TRK` → geometry)

The 900-byte item grid and the 900-byte terrain grid address the same physical
cells but are stored in **opposite row orders** in the file:

* item grid: row 0 is the top (south = 29 in engine coordinates);
* terrain grid: row 0 is the bottom (south = 0).

`Track` normalises both to a top→bottom layout, so for a normalised cell
`(x, y)`, `items[y*30 + x]` and `terrain[y*30 + x]` describe the same cell and
its world position is `(x*1024 + 512, y*1024 + 512)` (world `+x` = column,
`+z` = row, `+y` = up).

Placement rules (`reference/restunts/src/restunts/c/frame.c`,
`trkutil.c`, `full_data.c`):

* **Terrain** byte 0 is flat land at sea level and is not a shape — render a
  plain ground quad. Bytes 1..18 index `terrain_scene_objects[byte]`
  (`{rot_y, shape}`); the shape is drawn at the cell centre, at `y = 0`, except
  terrain 6 (elevated flat) which is drawn at `y = 450` (`hillHeightConsts[1]`).
* **Elements** are indexed directly: item byte `i` → `trkObjectList[i]`. Item 0
  is empty. Item values `0xFD`, `0xFE`, `0xFF` are continuation fillers that
  belong to a multi-tile neighbour:
  * `0xFD` → owner at `(x-1, y+1)`,
  * `0xFE` → owner at `(x, y+1)`,
  * `0xFF` → owner at `(x-1, y)`.
* **Multi-tile** origin (`multi_tile` bits: 1 = +row/z, 2 = +column/x): a
  single-tile element sits at the cell centre `(x*1024+512, y*1024+512)`; a
  multi-tile element sits on the shared edge so its centred shape spans the
  covered cells — `x = (x+1)*1024` if `&2`, `z = y*1024` if `&1`.
* **Elements on elevated terrain** (terrain 6) are drawn at `y = 450`, and the
  `high` shape (43) is tiled underneath as the plateau.
* **Overlays**: `trkObjectList[i].overlay` is another element to draw at the
  same transform. Indices ≥ 215 fall through into `terrain_scene_objects` at
  `index - 215`.
* **Hill terrain** (7..10) rewrites certain flat road pieces to sloped
  `ramp`/`elsp` pieces (`trkutil.c` `subst_hillroad_track`), e.g. terrain 7
  maps items `{4,14,24}` → `{182,186,190}` and `{39,59,98}` → `194`.

Terrain shape indices (all shapes span roughly one 1024-unit cell):

| terrain | 1 | 2–5 | 6 | 7–10 | 11–14 | 15–18 |
|---|---|---|---|---|---|---|
| shape | 45 `lake` | 44 `lakc` | 43 `high` | 42 `goup` | 41 `gouo` | 40 `goui` |

The four rotations of each sloped terrain shape (0/768/512/256) select which
corner is raised. `game3dshapes[]` is indexed by the fixed `game_shape_names`
order; only indices ≤ 110 are track/scenery (124–130 are cars and explosions).

## Crash debris (`state_op_unk` / `sub_19BA0`)

A hard wall hit throws a burst of flat shards off the car. The original keeps
24 of them in fixed engine-state slots, shared by the player, the opponent and
smashed track objects. Sources:
`reference/restunts/src/restunts/asmorig/seg001.asm` `state_op_unk` (spawn,
~9215) and `sub_19BA0` (motion, ~9386); the draw is in `frame.c` (~903).

| slot | field | meaning |
|---|---|---|
| — | `game_longs1/2/3[i]` | position, in 1/64 world units, **relative to the owner car's pivot** (x/z horizontal, y up) |
| — | `field_2FE[i]`, `field_32E[i]` | tumble about the shard's local z / x (1/1024 turn) |
| — | `field_35E[i]` | launch heading |
| — | `field_38E[i]` | launch speed (1/64 units per 20 Hz frame); **0 marks the slot free** |
| — | `field_3BE[i]` | vertical speed; gravity (0x13) is subtracted every frame |
| — | `field_42B[i]` | shard index, see the table below |
| — | `field_443[i]` | owner: 0 player, 1 opponent, ≥ 2 a track object |
| — | `field_42A` | "any slot live", re-derived every frame |

`state_op_unk(owner, car_yaw, extra)` picks one of two burst profiles from the
owner:

| | owner < 2 (car) | owner ≥ 2 (track object) |
|---|---|---|
| base heading | `car_yaw` | `car_yaw - 0x60` |
| arc (`var_2`) | `0x400` (full circle) | `0xC0` |
| cap (`var_E`) | 18 | 8 |
| shard base | `owner*4 + 4` | 0 |
| vertical multiplier | 6 | 1 |

It fills `min(free_slots, cap)` free slots; the *n*th piece gets
`field_42B = (n & 3) + base`, `field_35E = ((n * arc) / count + heading) & 0x3FF`
(an even spread over the arc), `field_2FE/field_32E = rnd * 4` and
`field_38E = ((rnd * 6) >> 2) + extra + 0x180`, consuming three kevinrandom
bytes per piece in that order. `field_3BE = (multiplier * field_38E) >> 2`.

`field_42B` indexes `sceneshapes3`, whose `shape` offsets resolve through
`game3dshapes` to:

| `field_42B` | shapes |
|---|---|
| 0..3 | `exp0`..`exp3` of `GAME1`/`GAME2` (generic scenery shards) |
| 4..7 | the player car's own `exp0`..`exp3` (`ST<ID>.P3S`) |
| 8..11 | the opponent's `exp0`..`exp3` |

The car shards are tiny flat polygons (±5..±15 units at `y = 0`); `exp2` in
`ST<ID>.P3S` is a bare 2-vertex **line**. Each shard carries 7 paint slots (the
per-livery material variants), the same ones the car body uses.

`sub_19BA0` advances each live piece one 20 Hz frame: it adds the tumble step
(0x10) to `field_2FE/field_32E`, moves the piece by
`mat_rot_zxy(0, 0, field_35E, 1) * (0, 0, field_38E)` (only x/z; gravity owns
y), subtracts gravity from `field_3BE` and adds it to `game_longs2`, applies
gravity a second time in the 10 Hz slow-motion mode, and retires the piece
(`field_38E = 0`) once `game_longs2 + playerstate.car_posWorld1.ly < 0`. Every
piece's height is tested against the **player's** position, even the opponent's
and the scenery's.

The renderer places the piece at `(game_longs + car_posWorld) >> 6` and tumbles
it with `mat_rot_zxy(rotvec.x, rotvec.y, rotvec.z, 0)` where
`rotvec = (-field_2FE, -field_32E, -field_35E)` (the same negation the car body
uses).

`kevinrandom` (`get_kevinrandom`, seeded with the 6 bytes `"kevin\0"`) is a
6-byte additive generator with a 6-byte counter; it returns `seed[0]` after
each step. The original draws from it for the race line-up and other things
too, so the port's debris angles/speeds are repeatable within a run but do not
match a recorded original run tick-for-tick.
