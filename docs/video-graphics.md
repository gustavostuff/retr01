# Video and graphics

Logical pipeline for tiles, sprites, palettes, and the two BG layers. Scroll and VRAM windowing are in `world-scrolling.md`.

## Resolution

- Logical render size: 128x120.
- Hardware scale: 2x (chunky pixels).

## Tiles and sprites

- Tile based. 8x8 tiles and 8x8 sprites only. No 8x16 sprites.
- Up to 64 hardware sprites on screen. Up to 16 per scanline.
- Pixel format: 2bpp.
- Up to 25 simultaneous colors on screen (NES-style).

### When work runs (locked)

| Work | When | Who |
| --- | --- | --- |
| **Sprite field** | Entire overlay built in **VBlank**, one pass | MCU-S1 from OAM into field SRAM |
| **BG0 next line** | **HBlank only** (ping-pong line buffer) | MCU-S1 |
| BG1 fetch / compose | Active display (beam + PLDs) | Video path |

HBlank is a small slice of time, so it only prepares the **next BG0 line**. That is intentional. Sprites are **not** line-ping-ponged. Doing all sprites in VBlank keeps HBlank free for BG0 show-through prep.

If more than 16 sprites land on one scanline, **later OAM entries on that line are not drawn** (priority by OAM order, no flicker mode in v1).

## Palette model

Do not mix these two ideas:

1. **64 RGB values** live in the board **AT27C256R** color PROM (packed R3G3B2). These are the real colors. Video reads them by 6-bit index. No CPU runtime poke path.
2. **64 individual palettes** live in the cartridge. Each individual palette is four numbers. Each number is a color index in 0..63 pointing into the PROM.

### Rows and selection

- 8 palette rows for BG and 8 for sprites.
- Each palette row holds 4 individual palettes.
- That gives 32 BG palettes and 32 sprite palettes on the cart (user defined).

Each pair of BG and sprite palette selection shares the same background color index. If BG palette row index N is selected, sprite palette row N is selected too, and that sprite row uses the same BG color index.

There is an **active palette buffer** that holds the selected BG and sprite palettes for the current frame.

### Active row select (locked)

| Port | Role |
| --- | --- |
| `$7F08` | `PAL_ROW`. Write **0-7** to select the active palette **row**. BG row N and sprite row N are always selected together. Both share the same backdrop color (color index **0** of that row pair). |
| `$7F09` | `PAL_DATA`. Auto-inc data window used while PRG (or a helper) copies the chosen cart palette plane bytes into the active buffer |

Rules:

- Change `$7F08` only in **vblank** (or with video off). Mid-frame row swaps are undefined.
- Load / refresh the active buffer from the cart global palette planes in **vblank only**.
- Games do not poke individual PROM RGB values. They only pick rows and supply cart **indices**.

## Background layers

Two BG layers:

- **BG0** at the bottom.
- **BG1** on top of BG0.

BG0 shows through transparent pixels of BG1. BG1 transparency is color index 0.

Software controls how much BG0 scrolls relative to BG1 to set depth. See `world-scrolling.md`.

### Compose order (conceptual)

1. VRAM holds BG1 nametable/attr data for the camera window.
2. During **HBlank**, MCU-S1 renders the **upcoming** BG0 line into one half of a **ping-pong** line buffer (the other half is what the active beam is reading for show-through).
3. During active display, the compositor draws BG1. Where BG1's color index is **0**, it substitutes the pixel from the current BG0 ping-pong line.
4. Sprite field (from VBlank) sits in the priority stack per compositor rules.

### BG0 ping-pong (HBlank only)

```text
  line N active  --> read buffer A
  HBlank         --> S1 fills buffer B for line N+1
  line N+1       --> read buffer B
  HBlank         --> S1 fills buffer A for line N+2
  ...
```

No other MCU-S1 video jobs run in HBlank.

## Attribute bytes

### BG attribute byte

| Bits | Meaning |
| --- | --- |
| 0-1 | Bank index (**0-3**) |
| 2-3 | Palette index (**0-3**) |
| 4 | H flip |
| 5 | V flip |
| 6 | Solid tile flag (software / physics) |
| 7 | **Animated tile** (locked below) |

### Animated tiles (bit 7 locked)

If bit **7** is **1**, that cell's pattern index is animated in hardware (or a dedicated helper path) as a 4-beat cycle:

| Step | Pattern index used |
| --- | --- |
| 0 | `base` |
| 1 | `base + 1` |
| 2 | `base + 2` |
| 3 | `base + 3` |
| next | wrap to `base` |

- `base` is the tile index stored in the nametable byte (0..255 within the selected bank).
- Addition wraps **inside the bank** at hardware level: `(base + k) & 0xFF`. Authors must place the four frames in consecutive indices (with wrap from 255 -> 0 if they cross the end).
- Default period is **6** display frames per step. PRG or system RAM holds a configurable delay (one global setting for v1 is enough).
- Bit 7 = 0 means a static tile (`base` only).

### Sprite attribute byte

Same layout as BG for bits 0-5. Bits 6 and 7 are reserved (leave **0** until a real need appears).
