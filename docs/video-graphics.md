# Video and graphics

Logical pipeline for tiles, sprites, palettes, and the two BG layers. Scroll and VRAM windowing are in `world-scrolling.md`.

## Resolution

- Logical render size: 128x120.
- Hardware scale: 2x (chunky pixels).

## Tiles and sprites

- Tile based. 8x8 tiles and 8x8 sprites only. No 8x16 sprites.
- Up to 64 hardware sprites on screen. Up to 16 per scanline.
- Sprite overlay is drawn in vblank by one of the AVRs.
- Pixel format: 2bpp.
- Up to 25 simultaneous colors on screen (NES-style).

## Palette model

Do not mix these two ideas:

1. **64 RGB values** live in the board **AT27C256R** color PROM (packed R3G3B2). These are the real colors. Video reads them by 6-bit index. No CPU runtime poke path.
2. **64 individual palettes** live in the cartridge. Each individual palette is four numbers. Each number is a color index in 0..63 pointing into the PROM.

### Rows and selection

As an abstraction:

- 8 palette rows for BG and 8 for sprites.
- Each palette row holds 4 individual palettes.
- That gives 32 BG palettes and 32 sprite palettes on the cart (user defined).

Each pair of BG and sprite palette selection shares the same background color index. If BG palette row index N is selected, sprite palette row N is selected too, and that sprite row uses the same BG color index.

There is an 8-palette buffer that holds the selected BG and sprite palettes for active use.

## Background layers

Two BG layers:

- **BG0** at the bottom.
- **BG1** on top of BG0.

BG0 shows through transparent pixels of BG1. BG1 transparency is color index 0.

Software controls how much BG0 scrolls relative to BG1 to set depth. See `world-scrolling.md`.

### Compose order (conceptual)

1. VRAM holds a buffer for BG0 data and a buffer for BG1 data.
2. By default render BG1. For each transparent pixel (color index 0) on BG1, take the pixel from BG0 behind it.
3. BG0 and BG1 scroll can differ. There is a control for how much BG0 moves in X and Y relative to BG1.

## Attribute bytes

### BG attribute byte

| Bits | Meaning |
| --- | --- |
| 0-1 | Bank index (**0-3**) |
| 2-3 | Palette index (**0-3**) |
| 4 | H flip |
| 5 | V flip |
| 6 | Solid tile flag (software) |
| 7 | Animated tile (software). Pattern set iteration details TBD |

Valid bank and palette field values are **0-3** (two bits, four banks / four palettes per row). Do not document 0-4.

### Sprite attribute byte

Same layout as BG for bits 0-5. Bits 6 and 7 are reserved (usage TBD).
