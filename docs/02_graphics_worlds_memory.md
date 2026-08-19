# Retr01 Graphics, Worlds, and Memory

This doc is the merged source for the graphics model, world layout, VRAM layout, and CPU-visible memory map.

## Display and timing

| Item | Value |
|------|-------|
| Visible resolution | **256×240** |
| Tile size | **8×8** |
| Dot clock | **5.369318 MHz** |
| Dots per scanline | **341** |
| Scanlines per frame | **262** |
| Frame rate / NMI | about **60.098 Hz** |

CPU and dot clocks are **independent**. The CPU runs at **8.000 MHz**. The beam and BG path run on the dot clock.

## Worlds and screens

- A cart has up to **8 worlds**
- A world uses a sparse virtual grid up to **16×16**
- A world may store up to **64 screens**
- A screen is **32×30** tile indices plus packed attrs
- A screen is stored in MAP-ROM and identified by `(col, row)`

### What a world contains

- **4 BG banks**
- **4 sprite banks**
- MAP directory + compressed screen payloads

### What a screen stores

- tile plane: **960 bytes**
- attr plane: **240 bytes**
- flags with **BG bank 0–3**
- MAP payload offset

Parallax screens use the same screen format but are marked non-enterable.

## BG banks and sprite banks

| Term | Meaning |
|------|---------|
| **BG bank** | **256 BG tiles**, **16×16** tile grid, **4 KB** |
| **Sprite bank** | **256 sprite tiles**, **16×16** tile grid, **4 KB** |

Per world:

- **4 BG banks**
- **4 sprite banks**
- total CHR per world: **32 KB**

Across the full cart:

- max CHR budget: about **256 KB**

### Runtime bank rules

1. Each screen names an **authored BG bank** in MAP metadata.
2. When software loads or seam-streams a screen into a nametable slot, it also copies that screen's BG bank into that slot's **BG bank latch**.
3. **Camera slots 0–3** and **plane slots 4–5** each have their own BG bank latch.
4. **Sprite bank** is separate and global within the current world.
5. Changing a BG bank does **not** change the sprite bank, and vice versa.

## Scrolling and live VRAM

### Camera

- `scroll_x` and `scroll_y` are **one byte each** (`0–255`, wrap)
- The camera can sample **1, 2, or 4** live screens from VRAM slots **0–3**
- Smooth scrolling means multiple neighboring screens may be visible at once

### Why VRAM has six slots

- **Slots 0–3**: live **camera** field, up to four playfield screens
- **Slots 4–5**: optional **parallax plane** only

Slots 4–5 are **not** fifth and sixth playfield screens.

### VRAM layout

| VRAM offset | Size | Purpose |
|-------------|------|---------|
| `$0000-$07FF` | 2 KB | nametable slot 0 |
| `$0800-$0FFF` | 2 KB | nametable slot 1 |
| `$1000-$17FF` | 2 KB | nametable slot 2 |
| `$1800-$1FFF` | 2 KB | nametable slot 3 |
| `$2000-$2FFF` | 4 KB | scratch / streaming temp |
| `$3000-$37FF` | 2 KB | plane slot 4 |
| `$3800-$3FFF` | 2 KB | plane slot 5 |
| remainder | reserved | future |

Each 2 KB slot holds:

- tiles at `+0x000` (**960 bytes**)
- packed attrs at `+0x3C0` (**240 bytes**)

One attr byte is a **2×2 attr quadrant** with four 2-bit palette fields, one per tile.

## Palettes and compositing

- **8 palettes total**: 4 BG + 4 sprite
- BG color 0 is shared backdrop
- Sprite color 0 is transparent
- OAM attr stays NES-like: palette, flips, priority

Sprite priority rule:

1. transparent sprite pixel -> show BG
2. sprite-behind bit + opaque BG -> show BG
3. otherwise show sprite

## MAP-ROM model

MAP-ROM is **not** directly memory-mapped into the CPU space.

Software reads MAP through **`$FE90`**:

1. write 24-bit MAP address
2. read MAP data
3. hardware auto-increments

Recommended MAP directory row:

- `col`
- `row`
- `flags` (`bit0 = parallax`, `bits1-2 = BG bank`)
- `data_off` (24-bit)

## CPU memory map

| Range | Region | Notes |
|-------|--------|-------|
| `$0000-$7FFF` | System RAM | CPU-only |
| `$8000-$FDFF` | PRG window | banked through `$FE80` |
| `$FE00-$FEFF` | I/O page | PPU, VRAM port, OAM, CHR bank latches, APU, MAP, input |
| `$FF00-$FFFF` | PRG high | vectors included |

### Important I/O blocks

| Range | Purpose |
|-------|---------|
| `$FE00-$FE0F` | PPU control, scroll, raster compare |
| `$FE10-$FE1F` | VRAM port |
| `$FE20-$FE2F` | OAM port into 1284 |
| `$FE30-$FE3F` | world select + BG bank latches + sprite bank |
| `$FE40-$FE5F` | APU |
| `$FE60-$FE6F` | controllers / cabinet |
| `$FE80-$FE8F` | PRG bank |
| `$FE90-$FE9F` | MAP port |

## Raster and parallax

Retr01 uses **raster IRQ**, not NES sprite-0 hit.

- `raster_y`: target scanline
- `raster_hit`: status
- `raster_irq_enable`: IRQ gate

Parallax is implemented as a **separate scanline band** that points to plane slots **4–5**.

Rules:

- parallax forces a **1-axis camera**
- plane slots are not part of the 2×2 camera
- BG banks stay per slot
- sprite bank remains separate/global

## What software should remember

- nametable bytes are tile indices `0–255`
- tile bytes do **not** store a bank number
- the slot's BG bank latch chooses the CHR bank
- MAP chooses which screen to load
- the current world chooses which 32 KB CHR chapter is active
