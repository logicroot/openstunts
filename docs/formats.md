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
