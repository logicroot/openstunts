# Stunts asset formats (working notes)

Notes derived from the [restunts](https://github.com/4d-stunts/restunts) project
(see `reference/restunts/`). This file grows as formats are implemented.

## Track file (`*.TRK`) — 1802 bytes

| Offset | Size | Content |
| --- | --- | --- |
| 0 | 900 | item bytes, 30×30, left→right, **top→bottom** |
| 900 | 1 | skybox id (0 desert, 1 tropical, 2 alpine, 3 city, 4 country) |
| 901 | 900 | terrain bytes, 30×30, left→right, **bottom→top** |
| 1801 | 1 | unknown |

Source: `reference/restunts/docs/format.html`.

## Containers (`*.PRE`, `*.RES`)

Resource files contain a table of named sub-files; some are compressed with an
RLE or variable-length scheme. Decompression reference:
`reference/restunts/src/restunts/c/fileio.c` (see "Decompress run-length encoded
sub-file", "Decompress variable-length encoded sub-file"). TODO: implement.

## `GAME.PRE` resources

`plan` (536 × 34-byte PLANE structs) and `wall` (WORD angleXZ, WORD x, WORD z)
describe drivable surfaces and vertical walls. See
`reference/restunts/docs/game.res.txt`.

## 3D shapes (`*.P3S`, `*.3SH`)

TODO: from `reference/restunts/src/restunts/c/shape3d.c`.
