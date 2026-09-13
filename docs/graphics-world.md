# Retr01 Graphics and World

**Status:** Draft v1.2  
**Parent:** [overview.md](overview.md)

## 1. Tile format

- Size: 8x8 pixels
- Depth: 1 bpp
- Background (bit 0) is always **black** (`#000000`), not part of the FG palette
- Foreground (bit 1) is one of 8 colours (3-bit index into the Video MCU palette below)

## 2. Global FG palette

Source of truth: [`assets/global_palette.png`](../assets/global_palette.png) (8x1, left to right = index 0-7).

These values live in Video MCU firmware. They are technically changeable. Shipping games and tools should keep this stock palette.

| Index | Name | R | G | B | Hex |
|-------|------|---|---|---|-----|
| 0 | White | 255 | 255 | 255 | `#FFFFFF` |
| 1 | Mid grey | 127 | 127 | 127 | `#7F7F7F` |
| 2 | Dark grey | 64 | 64 | 64 | `#404040` |
| 3 | Red | 227 | 38 | 38 | `#E32626` |
| 4 | Yellow | 232 | 203 | 39 | `#E8CB27` |
| 5 | Green | 46 | 203 | 95 | `#2ECB5F` |
| 6 | Blue | 51 | 142 | 238 | `#338EEE` |
| 7 | Purple | 127 | 68 | 219 | `#7F44DB` |

Attribute bits 4-6 select this index.

## 3. Attribute byte (tiles and sprites)

```
Bits 0-1 : Bank index (0-3)
Bit  2   : Flip H
Bit  3   : Flip V
Bits 4-6 : Foreground colour (0-7)
Bit  7   : Reserved
```

## 4. Banks

Stored on the cartridge World / CHR chip:

- 4 global background tile banks
- 4 global sprite tile banks

Each bank holds 256 tiles of 8x8 at 1 bpp (256 bytes per bank if packed as 8 bytes per tile). Exact packing is fixed when the CHR format lands in tooling.

## 5. Sprites

- Size: 8x8 only (no 8x16 mode)
- Placement: pixel precise
- Limits:
  - **Max 32 sprites per frame**
  - **Max 16 sprites per scanline**
- Evaluation runs on the Video MCU

## 6. Nametable

One screen is 128x120 pixels, which is **16x15 = 240 tiles**.

Each frame transfer (or on change) sends:

- 240 tile-index bytes
- 240 attribute bytes

Total nametable payload: **480 bytes**, sent whole (no compression). See [hardware.md](hardware.md) for the SPI command.

## 7. World and screens

- **Screen:** one full 128x120 nametable (240 tiles + 240 attributes)
- **World:** up to **16 screens**
- Screens may sit on a virtual **16x16** grid (sparse placement is allowed)
- Supports linear maps (horizontal or vertical) and grid maps (Zelda 1 style)

### Transitions

- Instant screen switch, or
- Smooth 2D scrolling

A perimeter tile buffer around the visible area is kept so adjacent screen data can be prefetched for seamless scrolling.

