# Video and graphics

Logical pipeline for tiles, sprites, palettes, and the two BG layers. Scroll and VRAM windowing are in `world-scrolling.md`.

## Resolution

- Logical render size: 128x120.
- Hardware scale: 2x (chunky pixels).

## Tiles and sprites

- Tile based. 8x8 tiles and 8x8 sprites only. No 8x16 sprites.
- Up to 64 hardware sprites on screen. Up to 16 per scanline.
- Entity **types** are capped at **32** cart-wide (one global catalog). How many entities may appear on screen is limited by that **64**-sprite OAM budget, not by type count (see `software-api.md`).
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

These two ideas stay separate:

1. **64 RGB values** live in the board **AT27C256R** color PROM (packed R3G3B2). These are the real colors. Video reads them by 6-bit index. No CPU runtime poke path. The kit table below is those 64 colors.
2. **64 individual palettes** live in the cartridge. Each individual palette is four numbers. Each number is a color index in 0..63 pointing into the PROM.

### Rows and selection

- 8 palette rows for BG and 8 for sprites.
- Each palette row holds 4 individual palettes.
- That gives 32 BG palettes and 32 sprite palettes on the cart (author defined).

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

### Motherboard color kit

The AT27C256R holds this **64-color** kit. Index **N** is PROM address **N**. Preview RGB is the **R**, **G**, and **B** columns (0..255). The PROM byte is packed **R3G3B2** (`{RRRGGGBB}`) of that triple. Studio, Emu, and the color-PROM burn use the same values from [`apps/common/r01_kit_palette.c`](../apps/common/r01_kit_palette.c).

Indices **0..15** are darkest, **16..31** mid-dark, **32..47** mid, **48..63** bright. Cart palette bytes are kit indices only.

Studio export may also write `<stem>_prom.bin` (64 packed R3G3B2 bytes) for OTP burners. That file matches this kit.

| Index | R | G | B | Hex |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | `000000` |
| 1 | 41 | 5 | 20 | `290514` |
| 2 | 42 | 5 | 7 | `2A0507` |
| 3 | 35 | 15 | 6 | `230F06` |
| 4 | 30 | 19 | 6 | `1E1306` |
| 5 | 26 | 22 | 5 | `1A1605` |
| 6 | 20 | 24 | 7 | `141807` |
| 7 | 6 | 26 | 7 | `061A07` |
| 8 | 5 | 26 | 19 | `051A13` |
| 9 | 7 | 25 | 24 | `071918` |
| 10 | 8 | 24 | 28 | `08181C` |
| 11 | 7 | 23 | 34 | `071722` |
| 12 | 3 | 11 | 61 | `030B3D` |
| 13 | 22 | 3 | 58 | `16033A` |
| 14 | 32 | 5 | 45 | `20052D` |
| 15 | 38 | 4 | 32 | `260420` |
| 16 | 54 | 54 | 54 | `363636` |
| 17 | 116 | 10 | 64 | `740A40` |
| 18 | 119 | 9 | 26 | `77091A` |
| 19 | 105 | 53 | 18 | `693512` |
| 20 | 93 | 63 | 14 | `5D3F0E` |
| 21 | 81 | 70 | 23 | `514617` |
| 22 | 66 | 76 | 25 | `424C19` |
| 23 | 19 | 81 | 26 | `13511A` |
| 24 | 22 | 80 | 63 | `16503F` |
| 25 | 17 | 78 | 77 | `114E4D` |
| 26 | 22 | 77 | 88 | `164D58` |
| 27 | 22 | 74 | 102 | `164A66` |
| 28 | 22 | 55 | 148 | `163794` |
| 29 | 71 | 41 | 144 | `472990` |
| 30 | 95 | 22 | 125 | `5F167D` |
| 31 | 108 | 17 | 95 | `6C115F` |
| 32 | 148 | 148 | 148 | `949494` |
| 33 | 192 | 74 | 122 | `C04A7A` |
| 34 | 197 | 74 | 77 | `C54A4D` |
| 35 | 184 | 96 | 27 | `B8601B` |
| 36 | 162 | 115 | 38 | `A27326` |
| 37 | 143 | 126 | 47 | `8F7E2F` |
| 38 | 119 | 135 | 45 | `77872D` |
| 39 | 32 | 144 | 48 | `209030` |
| 40 | 46 | 142 | 114 | `2E8E72` |
| 41 | 49 | 139 | 137 | `318B89` |
| 42 | 31 | 136 | 156 | `1F889C` |
| 43 | 36 | 131 | 181 | `2483B5` |
| 44 | 77 | 119 | 215 | `4D77D7` |
| 45 | 126 | 106 | 211 | `7E6AD3` |
| 46 | 157 | 93 | 191 | `9D5DBF` |
| 47 | 179 | 82 | 160 | `B352A0` |
| 48 | 255 | 255 | 255 | `FFFFFF` |
| 49 | 241 | 162 | 187 | `F1A2BB` |
| 50 | 241 | 166 | 161 | `F1A6A1` |
| 51 | 241 | 169 | 131 | `F1A983` |
| 52 | 238 | 172 | 68 | `EEAC44` |
| 53 | 212 | 186 | 51 | `D4BA33` |
| 54 | 176 | 200 | 65 | `B0C841` |
| 55 | 115 | 210 | 117 | `73D275` |
| 56 | 34 | 208 | 166 | `22D0A6` |
| 57 | 59 | 205 | 201 | `3BCDC9` |
| 58 | 72 | 201 | 228 | `48C9E4` |
| 59 | 136 | 196 | 237 | `88C4ED` |
| 60 | 164 | 189 | 239 | `A4BDEF` |
| 61 | 187 | 181 | 241 | `BBB5F1` |
| 62 | 213 | 169 | 239 | `D5A9EF` |
| 63 | 240 | 155 | 221 | `F09BDD` |

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

## Pattern banks (cart-wide)

CHR is one global pool: **16 banks**, 256 tiles each, 8x8 2bpp. Playfields, other screens, sprites, and the marked player all use this pool. A typical split is **8** banks for backgrounds and **8** for sprites and entities. That split is not a cap. See `memory.md`.

The **tile or sprite** has authority: each BG cell and each sprite names **bank 0-15** in its attr byte and may mix with any other bank on the same screen.

During a CHR fetch, bank bits **0-1** sit on cart **A12-A13** (inside a 16 KB page). Bits **2-3** sit on **A14-A15**. The CHR region base lives in MAP **A16-A18**. Same cart edge as `hardware.md`.

## Attribute bytes

Same pack for BG nametable attrs and sprite / OAM attrs. The whole byte is used.

### BG attr and sprite attr

| Bits | Meaning |
| --- | --- |
| 0-3 | Bank index (**0-15**) into global CHR |
| 4-5 | Palette index (**0-3**) |
| 6 | H flip |
| 7 | V flip |

Collision marks BG1 patterns by bank index and tile index. Palette and H/V flip do not affect solidity. Studio Set Solid stores those patterns in the project JSON. The packed list lives in system RAM (`$0200`, copied from PRG `$8700` at boot). Play samples the live 2x2 nametable RAM (filled with the VRAM window from MAP) against that list. Off-window cells read MAP. `r01_solid_pattern_add` is optional RAM extras. A BG1 cell is solid when its bank and tile match a marked pattern. See `software-api.md`.

The marked **player** uses the same bank field as every other entity (**0-15**, global CHR). Other screens use the same 16 banks. See `memory.md`.
