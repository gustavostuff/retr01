# Worlds and screens

How a Retr01 cart lays out maps. Graphics timing, pattern banks, palettes, and live VRAM are in [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md). The MAP port wiring is in [08_memory_map.md](08_memory_map.md).

A **world** is a cart chapter: 4 CHR banks plus a sparse atlas of screens. A cart has up to **8** worlds (0-7). `$FE30` world select picks which chapter's CHR the PPU is drawing. Software `world` in RAM should match when you change chapter.

A **screen** is one **32x30** nametable plus packed attrs (1200 bytes uncompressed). It has a **(col, row)** on that world's virtual grid. Only stored screens occupy MAP bytes.

The **virtual grid** is up to **64 x 64** cells. At most **64** cells hold a real screen. The rest are holes. Holes are not stored. Connectivity is optional: a world may be a packed rectangle, a corridor, a blob, several islands, or a single room.

Hardware has no map camera. RAM holds `world`, `map_x`, `map_y` (one byte each). Set those and call `load_screen`. Pixel scroll is `scroll_x` / `scroll_y` over the live 1/2/4 nametable slots in VRAM.

## Caps (locked)

| Cap | Value |
|-----|-------|
| Worlds per cart | **8** (0-7) |
| Virtual grid | up to **64 x 64** |
| Stored screens per world | **64** max |
| Stored screens per cart | **512** max |
| Screen size | **32x30** tiles (256x240 px) |
| Live nametables in VRAM | up to **4** (2x2 field) |
| `map_x`, `map_y` | one byte each, **0-63** used, 64-255 invalid |

RAM is three separate bytes: `world` (0-7), `map_x` (0-63), `map_y` (0-63). World count stays **8**. The 64x64 grid is why X and Y each get their own byte, not a pair of nibbles.

CHR: **4 banks per world**, each 256 BG + 256 sprite patterns. That stays in [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md).

## Example layouts

These are **examples**, not an enum. Any set of at most 64 cells on the 64x64 grid is legal. The screenshot is one roll of [`world_examples_generator/`](../world_examples_generator/). Grid sizes and screen counts change when you randomize.

![Example world layouts](images/world_layouts.png)

*Image: `images/world_layouts.png`. Replace this file if you recapture the visualizer.*

The visualizer currently draws **seven** shapes. You can invent more (ring, two hubs plus a corridor, checker, four isolated rooms, a plus, a donut of holes, ...).

| Example | This shot | What it proves |
|---------|-----------|----------------|
| **1x1 Single** | grid 1x1, 1 screen | Isolated room. All four neighbors are empty. Pixel-scroll still works. You see the empty template around the edges. |
| **Linear Horiz** | grid 14x1, 14 screens | Side scroller. East/west are real screens. North/south are empty. |
| **Linear Vert** | grid 1x13, 13 screens | Tower or pit. North/south real. East/west empty. |
| **Snake Path** | grid 29x16, 64 screens | Hits the 64-screen cap on a much larger grid. Most cells have **two** neighbors (the path). Ends have one. Corners still pixel-scroll into empty on the open sides. |
| **Packed Grid** | grid 8x8, 64 screens | Dense rectangle. 8x8 is the largest full rectangle that fits 64 screens. Interior cells have all four neighbors. |
| **Hole Grid** | grid 8x8, 39 screens | Same bounding box as packed, with cells omitted. A screen may have 4, 3, 2, 1, or 0 stored neighbors. Holes draw the empty template. They do not consume MAP. |
| **Random Cluster** | grid 10x14, 64 screens | Irregular blob, 64 screens, not a rectangle. Outline cells have missing neighbors. Interior is packed. |

Blue cells are stored screens. Dark cells are holes (or past `grid_w` x `grid_h`). The PPU never sees this diagram. It only sees the live VRAM slots.

**Neighbors are 4-way** (N/E/S/W). Diagonals are not seam partners. Two screens that only touch on a corner are not adjacent for streaming.

## Empty neighbors and scroll

Pixel-scroll is **always** allowed, even if the current screen has 4, 3, 2, 1, or **zero** stored neighbors. The live 2x2 VRAM field still gets a nametable in the incoming slot:

- Directory **hit:** decompress that screen.
- Directory **miss** (in-grid hole, or a neighbor past `grid_w`/`grid_h`): fill the slot with the world's **empty template**.
- Default empty template: **solid black** (tile 0 / backdrop). No extra MAP bytes.
- Optional later: one per-world empty nametable (rocks, mountains, void art). Stored **once** in that world's MAP (`empty_off`). Not once per hole.

The player should not walk into empty. That is **collision in PRG**, not a camera lock.

Warps (`load_screen` to a new col/row) are for doors and teleports. They are not required just because a screen is isolated.

**Streaming cue (software, not a PPU register):** when the camera is within **2 tiles (16 px)** of a seam, look up neighbor `(map_x±1, map_y)` or `(map_x, map_y±1)`. Hit: decompress into the incoming slot. Miss: empty template.

**Out-of-grid debug:** if `load_screen` is given coords outside 0-63, software may paint a lettered **EMPTY** pattern so the bug is obvious. That is not the same as an in-world hole (black / mountains).

## Where the atlas lives

| Place | Holds | Does not hold |
|-------|-------|----------------|
| **MAP-ROM** | World headers, screen directory, compressed screens | Game logic |
| **PRG** | `load_screen` / seam-stream code. Tiny table of MAP base offsets if you want labels in ASM | The nametable pixels |
| **System RAM** | `world`, `map_x`, `map_y`. Optional cached copy of the current world's directory | |
| **VRAM** | Only the 1-4 screens under the camera | The 64x64 atlas |

```
Cartridge
+-- World 0..7
    +-- Pattern banks 0..3
    |     bank: BG patterns (256) + sprite patterns (256) = 512
    +-- Virtual grid up to 64 x 64 (sparse)
          +-- at most 64 screens, each with (col, row) + compressed NT/attrs
```

## MAP-ROM layout (planning)

Do **not** store a 64x64 pointer matrix (that would be ~12 KB of mostly zeros per world). Store a **compact directory** of the screens that exist (max 64 rows).

```
MAP-ROM
+-- cart header
|     magic, version, world_count
|     world_base[8]: 24-bit offset to each world (0 = unused)
+-- world N
      grid_w, grid_h          ; 1..64 each
      screen_count            ; 1..64
      empty_off               ; 24-bit MAP offset to optional empty nametable, 0 = solid black
      directory[screen_count]:
          col, row            ; position on the virtual grid
          data_off            ; 24-bit MAP offset to payload
      payloads...
          optional copy of col, row (so a .bin is self-describing)
          RLE (or similar) tile plane (960 bytes uncompressed)
          packed attr plane (240 bytes uncompressed)
```

Authoring sketch (assembled **into MAP**, not into PRG). Labels are for the map build. The 6502 never JMPs here.

```
world_01:
    .byte 12, 8     ; virtual grid 12 cols x 8 rows
    .byte 15        ; 15 real screens (the rest of 12x8 is empty)
    .byte 0, 0, 0   ; empty_off = 0 (solid black)
    ; directory bytes may be emitted by a macro from the includes below

    .incbin "hub.bin"     ; file starts with col, row, then RLE tiles, then attrs
    .incbin "cave.bin"
    .incbin "boss.bin"    ; isolated cell. still pixel-scrolls into empty fill
```

### `load_screen`

1. Seek `$FE90` to `world_base[world]`, read `grid_w`, `grid_h`, `screen_count`, `empty_off`.
2. Scan the directory for `(map_x, map_y)`. At 8 MHz, 64 rows is cheap. You may cache the directory in system RAM on world enter (~64 * 5 bytes).
3. On miss (in-grid hole, or neighbor past the grid): fill the VRAM slot with the empty template (`empty_off == 0` = solid black, else that nametable). Do not invent a dummy screen in MAP.
4. On hit: seek to `data_off`, read RLE tiles then attrs, write the live nametable through `$FE1x`.
5. Coords outside 0-63: optional lettered EMPTY debug fill.

Seam fill is the same lookup for a neighbor cell.

How to poke `$FE90` (24-bit address, auto-inc read): [08_memory_map.md](08_memory_map.md).

### Why 24-bit offsets

MAP-ROM is up to **~1.17 MB**. A 16-bit absolute address only covers 64 KB, so it cannot point at the whole MAP. `$FE90` is already a 24-bit seek, so `world_base`, `empty_off`, and `data_off` are the same width: poke the three bytes and read. The extra byte vs 16-bit is 64 directory rows * 1 = 64 bytes per world.

A 16-bit **world-relative** `data_off` would work only if each world's MAP blob stays under 64 KB. Uncompressed, 64 screens * 1200 bytes is already 76.8 KB, so that cap forces compression and a hard per-world limit. Not worth it. Keep 24-bit.
