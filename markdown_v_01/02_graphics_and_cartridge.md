# Graphics & Cartridge Architecture

Canonical description of Retr01 tile graphics and cartridge layout.

## 1. Display geometry and timing

| Item | Value |
|------|-------|
| Visible resolution | 256x240 |
| Tile size | 8x8 |
| Nametable | **32x30** tile indices (960 bytes) + **240-byte** attr plane (1 byte per 2x2 cell, 2 bits per tile) |
| Color depth | 2 bpp (3 colors + transparency per draw unit) |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** (NTSC PPU-rate, same class as NES) |
| Dots per scanline | **341** (256 visible + 85 HBlank) |
| Scanlines per frame | **262** (240 visible + 22 VBlank) |
| Frame rate | **~60.098 Hz** (NMI metronome) |

CPU and dot clocks are **independent**. The W65C02S runs at 8 MHz for game logic and interleaved VRAM phases. The video beam advances on the 5.369318 MHz dot clock.

That dot clock **is** the CRT refresh if Retr01-A drives analog **RGBS** into a 15.7 kHz arcade/CGA monitor: 341 dots per line gives ~15.7 kHz horizontal, 262 lines gives **~60.1 Hz** vertical. NMI fires at that same rate. A modern TV is different. The panel has its own 60 Hz. An external analog-to-HDMI (or similar) converter on the RGBS pads resamples our output. The PPU still runs 341x262 either way. Game code is paced by NMI, not by the TV.

PPU VRAM fetches occur only on PPU-owned CPU phases (with line buffers / shift registers bridging the two domains as on real hardware). In other words, even though the PPU only accesses VRAM 50% of the time, the display never flickers. The hardware uses shift registers to cache the graphics data during the PPU's phase, continuously streaming pixels to the screen while the CPU executes game logic during its phase.


See also [08_memory_map.md](08_memory_map.md) for the VRAM port and interleave rules.

## 2. Worlds, screens, scroll, and banks

Three different ideas. Do not mix them.

| Word | What it is | Where it lives |
|------|------------|----------------|
| **World** | A cart chapter: CHR banks plus a **sparse screen atlas**. Up to **8** per cart (0-7). `$FE30` world select picks CHR. Software `world` in RAM should match when you change chapter. | CHR-ROM + MAP-ROM |
| **Virtual grid** | Logical map of that world, up to **64 x 64 cells**. Most cells are empty. At most **64 cells** hold a real screen. | World header in MAP-ROM |
| **Screen** | One **32x30** nametable plus packed attrs, stored compressed. Has a **(col, row)** on the virtual grid. Isolated screens are valid. You can still pixel-scroll. Missing neighbors show the world's **empty** fill (default black). | MAP-ROM. Up to **4** live copies in VRAM slots |
| **Scroll** | `scroll_x` / `scroll_y`, one byte each (0-255). Pixel offset of the 256x240 window across the live VRAM slots. | PPU latches (`$FE0x`) |
| **CHR bank** | 512 patterns (256 BG + 256 sprites). 4 banks per world. | CHR-ROM. `$FE30` picks BG bank and sprite bank separately |

**Where the atlas lives**

| Place | Holds | Does not hold |
|-------|-------|----------------|
| **MAP-ROM** | World headers, screen directory, compressed screens | Game logic |
| **PRG** | `load_screen` / seam-stream code. Tiny table of MAP base offsets if you want labels in ASM | The nametable pixels |
| **System RAM** | `world`, `map_x`, `map_y` (one byte each). Optional cached copy of the current world's directory | |
| **VRAM** | Only the 1-4 screens under the camera | The 64x64 atlas |

Set `world`, `map_x`, `map_y` and call `load_screen`. That is the whole reload API. Hardware has no "map camera."

**Smooth scrolling (always)**

Pixel-scroll is always allowed, even if the current screen has 4, 3, 2, 1, or **zero** stored neighbors. The live 2x2 VRAM field still gets a nametable in the incoming slot:

- Directory **hit:** decompress that screen.
- Directory **miss** (in-grid hole, or a neighbor past `grid_w`/`grid_h`): fill the slot with the world's **empty template**.
- Default empty template: **solid black** (tile 0 / backdrop). No extra MAP bytes.
- Optional later: one per-world empty nametable (rocks, mountains, void art). Stored **once** in that world's MAP (`empty_off`). Not once per hole. The player should not walk there. That is **collision in PRG**, not a camera lock.

Warps (`load_screen` to a new col/row) are still for doors and teleports. They are not required just because a screen is isolated.

Grid cells with no directory entry **do not occupy MAP bytes**. Never store dummy screens in MAP to fill the 64x64.

**Out-of-grid debug:** if `load_screen` is given coords outside 0-63, software may paint a lettered **EMPTY** pattern so the bug is obvious. That is not the same as an in-world hole (black / mountains).

**Coords:** `map_x` and `map_y` are **one byte each** (0-63 used, 64-255 invalid). Not nibbles. There are **8 worlds**, not 16. The extra byte is for the grid, not for more worlds.

### MAP-ROM layout (planning)

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
    ; directory bytes may be emitted by a macro from the includes below

    .incbin "hub.bin"     ; file starts with col, row, then RLE tiles, then attrs
    .incbin "cave.bin"
    .incbin "boss.bin"    ; isolated cell. still pixel-scrolls into empty fill
```

Lookup: given `world`, `map_x`, `map_y`, find `world_base[world]`, scan (or binary-search) that directory for matching col/row. On hit, `$FE90` seek to `data_off`, decompress into a VRAM slot. On miss, fill with the empty template (`empty_off == 0` means solid black, else decompress that one nametable). At 8 MHz, scanning 64 directory rows is cheap. You may copy the directory into system RAM when entering a world (~64 * 5 bytes).

**Why 24-bit offsets:** MAP-ROM is up to **~1.17 MB**. A 16-bit absolute address only covers 64 KB, so it cannot point at the whole MAP. `$FE90` is already a 24-bit seek, so `world_base`, `empty_off`, and `data_off` are the same width: poke the three bytes and read. The extra byte vs 16-bit is 64 directory rows * 1 = 64 bytes per world.

A 16-bit **world-relative** `data_off` would work only if each world's MAP blob stays under 64 KB. Uncompressed, 64 screens * 1200 bytes is already 76.8 KB, so that cap forces compression and a hard per-world limit. Not worth it. Keep 24-bit.

```
Cartridge
+-- World 0..7
    +-- Pattern banks 0..3
    |     bank: BG patterns (256) + sprite patterns (256) = 512
    +-- Virtual grid up to 64 x 64 (sparse)
          +-- at most 64 screens, each with (col, row) + compressed NT/attrs
```

### Rules

1. Up to **8** worlds. Each world: up to **64** stored screens on a virtual grid up to **64 x 64**. Cart max **512** stored screens.
2. Each world has **4** pattern banks.
3. Each bank: **first 256 patterns BG**, **second 256 sprites** -> **512** patterns / bank (8 KB @ 16 bytes/pattern).
4. **Authored bank:** each screen's nametable tile indices assume one of the world's 4 banks.
5. **Runtime banks:** PPU may select **BG bank** and **sprite bank** independently. Either may change **mid-frame**.
6. PPU fetches CHR **from cartridge CHR-ROM** (not from VRAM).
7. Atlas and compressed screens live in **MAP-ROM**. PRG only implements lookup + decompress + VRAM write.

## 3. Pattern math

| Unit | Patterns | Bytes |
|------|----------|-------|
| Page | 256 | 4 KB |
| Bank | 512 | 8 KB |
| World (4 banks) | 2048 | 32 KB |
| Cart max (8 worlds) | 16384 | **256 KB** CHR |

## 4. Sprites vs background

These are two fetch paths. Scroll only affects which nametable byte is under the beam.

- **BG:** scroll picks a pixel in the live nametable field. That byte is a tile index **0-255** into the **active BG pattern set** (the BG half of the BG bank on cart).
- **Sprites:** OAM (64 entries) holds tile indices into the **active sprite pattern set** (the sprite half of the sprite bank). OAM does not use scroll.
- Max **16 sprites per scanline**. Extras are dropped.
- Non-transparent sprite pixel wins over BG (compositor default). Sprite index 0 never wins, it is transparent.

## 5. Palettes and attributes

- **8 palettes** total: **4 background + 4 sprite**. Each palette has 4 entries because the art is 2bpp (pattern bits 00, 01, 10, 11).
- **Index 0 means two different things**, same as the NES:
  - **Background:** index 0 is a real color, the **shared backdrop**. All 4 BG palettes use the same color 0. A "transparent" hole in a BG tile just shows that backdrop. Normal screens are opaque.
  - **Sprites:** index 0 is **true transparency**. That sprite pixel is skipped. You see whatever BG (or backdrop) is behind it.
- **BG attributes are per tile:** each of the 960 nametable tiles has its own 2-bit palette select (which of the 4 BG palettes). NES instead shares one select across a 2x2 tile group.
- **Storage:** **240 bytes** per screen/slot. One byte covers a **2x2 cell of tiles** (16x15 cells on a 32x30 screen). Four 2-bit fields, one per tile:
  - bits 0-1: top-left tile
  - bits 2-3: top-right tile
  - bits 4-5: bottom-left tile
  - bits 6-7: bottom-right tile
  Index: `attrs[(ty / 2) * 16 + (tx / 2)]`, then shift by `((ty & 1) * 2 + (tx & 1)) * 2`.
- Sprite attributes: **NES-like OAM attr byte** (palette, flips, priority). Index 0 = transparent.
- **Master palette:** custom Retr01 ramp (not stock NES colors). **32 min / 64 likely**, RGB table TBD.

## 6. Live VRAM vs cartridge maps

**Cartridge MAP region** holds the world atlas and compressed screens (see section 2). CPU reads MAP only through the **`$FE90` MAP port**. PRG/CHR share the same ~2 MB flash.

**On-board VRAM (32 KB)** live set:

| Contents | Planning size |
|----------|----------------|
| 4 nametable slots x (960 tiles + 240 attrs), 2 KB-aligned | 4 x 2 KB = **8 KB** |
| Streaming / decompress scratch | ~4 KB |
| Reserved / future | remainder of 32 KB |

OAM lives in dedicated registers / `$FE2x`, not in the VRAM chip.

### Scrolling model

- `scroll_x` and `scroll_y` are **one byte each** (0-255, wrap). That is the pixel camera inside the live field.
- Live field: up to **four** nametable slots (2x2). Arrangement/mirroring chooses **1, 2, or 4** distinct screens. As the window crosses a slot boundary, those neighbor tiles **are** on screen. That is how pixel scrolling looks continuous.
- **Streaming cue (software, not a PPU register):** when the camera is within **2 tiles (16 px)** of a seam, look up the **neighbor** `(map_x±1, map_y)` or `(map_x, map_y±1)` in the MAP directory. If it exists, decompress that screen into the incoming slot. If it does not, fill that slot with the world's empty template (default black). Isolated screens still pixel-scroll. The empty neighbor is just black (or the per-world empty nametable).

## 7. Cartridge ROM budget (~2 MB)

| Region | Role | Ceiling |
|--------|------|---------|
| PRG-ROM | Game code | ~512 KB |
| CHR-ROM | Pattern banks | ~256 KB |
| MAP-ROM | Compressed nametables + attributes | ~1.17 MB |
| **Total** | Example parallel flash | **~2 MB** |

Uncompressed map upper bound: `8 x 64 x (960 + 240) = 600 KB` before RLE (tile plane + packed attr plane).
