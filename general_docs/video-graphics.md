# Video and graphics

Logical pipeline for tiles, sprites, palettes, and the two BG layers. Scroll and VRAM windowing are in `world-scrolling.md`.

## Resolution

- Logical render size: 128x120.
- Hardware scale: 2x (chunky pixels).

## Tiles and sprites

- Tile based. 8x8 tiles and 8x8 sprites only. No 8x16 sprites.
- Up to 64 hardware sprites on screen. Up to 16 per scanline.
- Entity **types** per world are capped at **16** (catalog). How many entities may appear on screen is limited by that **64**-sprite OAM budget, not by type count (see `software-api.md`).
- Pixel format: 2bpp.
- Up to 25 simultaneous colors on screen (NES-style).

### When work runs (locked)

| Work | When | Who |
| --- | --- | --- |
| **Sprite field** | Entire overlay built in **VBlank**, one pass | MCU-S1 from OAM into field SRAM |
| **OAM SPI M->S1** | **Early VBlank**, or when **`S1_RDY`** is ready. Never in HBlank | MCU-M master |
| **BG0 next line** | **HBlank only** (ping-pong line buffer) | MCU-S1 |
| BG1 fetch / compose | Active display (beam + PLDs) | Video path |

HBlank is a small slice of time, so it only prepares the **next BG0 line**. That is intentional. Sprites are **not** line-ping-ponged. Doing all sprites in VBlank keeps HBlank free for BG0 show-through prep. OAM traffic must not steal that HBlank window (see `ic-comms-risks.md`).

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
| `$7F08` | `PAL_ROW`. Write **0-7** to select the active palette **row**. BG row N and sprite row N are always selected together. Both share the same backdrop color (color index **0** of that row pair). Writing `$7F08` also **resets** the `$7F09` fill cursor to the start of the active buffer. |
| `$7F09` | `PAL_DATA`. Auto-inc data window. PRG (or a helper) copies **32** master indices into the active buffer: **16** BG then **16** SPR for the selected row. |

Rules:

- Change `$7F08` only in **vblank** (or with video off). Mid-frame row swaps are undefined.
- Load / refresh the active buffer from the cart global palette planes in **vblank only**.
- Games do not poke individual PROM RGB values. They only pick rows and supply cart **indices**.
- Same vblank rule for scroll ports `$7F02` / `$7F03` (and raster `$7F04` unless a deliberate split-screen IRQ effect is defined). See `world-scrolling.md`.

Typical boot fill (matches Phase 1 PRG):

1. Seek MAP to the cart BG palette plane row (`$7F90`-`$7F92`).
2. `STA $7F08` with the row index (0-7). Cursor resets.
3. Copy 16 bytes from `$7F93` into `$7F09`.
4. Seek MAP to the SPR plane for the same row.
5. Copy 16 more bytes into `$7F09` (cursor continues at offset 16).

### Motherboard color kit (locked SoT)

The AT27C256R holds **this** 64-color kit (8-bit preview RGB). Studio and Emu use the same table. Burn tools write packed **R3G3B2** (`{RRRGGGBB}`) into PROM address `N` for kit index `N`.

Indices **0..15** are darkest, **16..31** mid-dark, **32..47** mid, **48..63** bright. Cart palette bytes are kit indices only.

```text
# idx   RRGGBB hex preview (not PROM bytes)
 0 000000  1 290514  2 2A0507  3 230F06  4 1E1306  5 1A1605  6 141807  7 061A07
 8 051A13  9 071918 10 08181C 11 071722 12 030B3D 13 16033A 14 20052D 15 260420
16 363636 17 740A40 18 77091A 19 693512 20 5D3F0E 21 514617 22 424C19 23 13511A
24 16503F 25 114E4D 26 164D58 27 164A66 28 163794 29 472990 30 5F167D 31 6C115F
32 949494 33 C04A7A 34 C54A4D 35 B8601B 36 A27326 37 8F7E2F 38 77872D 39 209030
40 2E8E72 41 318B89 42 1F889C 43 2483B5 44 4D77D7 45 7E6AD3 46 9D5DBF 47 B352A0
48 FFFFFF 49 F1A2BB 50 F1A6A1 51 F1A983 52 EEAC44 53 D4BA33 54 B0C841 55 73D275
56 22D0A6 57 3BCDC9 58 48C9E4 59 88C4ED 60 A4BDEF 61 BBB5F1 62 D5A9EF 63 F09BDD
```

Studio export may also write `<stem>_prom.bin` (64 packed R3G3B2 bytes) for OTP burners. That file must match this kit.

## Background layers

Two BG layers:

- **BG0** at the bottom.
- **BG1** on top of BG0.

BG0 shows through transparent pixels of BG1. BG1 transparency is color index 0. By default BG0 also fills the viewport where no BG1 screen is present in the camera window (missing / outside streamed slots). Optional cart flag **BG0 clip to BG1** (`r01_bg0_set_clip_to_bg1`) hides BG0 there and uses backdrop instead. Backdrop is the final fallback under BG0.

Software controls how much BG0 scrolls relative to BG1 to set depth. See `world-scrolling.md`.

### Compose order (conceptual)

1. VRAM holds BG1 nametable/attr data for the camera window.
2. During **HBlank**, MCU-S1 renders the **upcoming** BG0 line into one half of a **ping-pong** line buffer (the other half is what the active beam is reading for show-through).
3. During active display, the compositor draws BG1 where a present screen occupies the slot. Where BG1 is absent, or BG1's color index is **0**, it substitutes the pixel from the current BG0 ping-pong line (unless BG0 clip-to-BG1 is set, in which case absent BG1 slots use backdrop).
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

For entities, bank bits **0-1** index SPR banks of the **current world** (the world whose catalog owns the def). BG nametable attrs use that world's BG banks the same way.
