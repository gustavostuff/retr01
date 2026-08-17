# Graphics & Cartridge Architecture

Canonical description of Retr01 tile graphics, pattern banks, and cartridge ROM budget. Worlds, the sparse screen atlas, and `load_screen` live in [04_worlds_and_screens.md](04_worlds_and_screens.md).

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

## 2. Scroll, banks, and CHR

World / grid / screen atlas: [04_worlds_and_screens.md](04_worlds_and_screens.md). Do not mix those with the PPU camera.

| Word | What it is here |
|------|-----------------|
| **Scroll** | `scroll_x` / `scroll_y`, one byte each (0-255). Pixel offset of the 256x240 window across the live VRAM slots. PPU latches (`$FE0x`). |
| **CHR bank** | 512 patterns (256 BG + 256 sprites). 4 banks per world. `$FE30` picks BG bank and sprite bank separately. |

### Bank rules

1. Each world has **4** pattern banks.
2. Each bank: **first 256 patterns BG**, **second 256 sprites** -> **512** patterns / bank (8 KB @ 16 bytes/pattern).
3. **Authored bank:** `load_screen` sets the BG bank that screen was drawn against. That is the default at the start of the frame. Software may still switch banks mid-frame (see section 8).
4. **Runtime banks:** PPU may select **BG bank** and **sprite bank** independently. Either may change **mid-frame**. Nametable bytes stay 0-255 into whichever BG page is **currently** latched.
5. PPU fetches CHR **from cartridge CHR-ROM** (not from VRAM).

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
- Compositor: if the sprite pixel is pattern color 0, skip it (transparent). Else if the OAM **priority** bit is set, opaque BG wins. Else the sprite wins. Pattern color 0 is **not** OAM sprite #0. OAM entry 0 is a normal sprite.

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
- Sprite attributes: **NES-like OAM attr byte** (palette, flips, priority). Pattern color 0 = transparent. The priority bit puts the sprite behind opaque BG.
- **Master palette:** custom Retr01 ramp (not stock NES colors). **32 min / 64 likely**, RGB table TBD.

## 6. Live VRAM vs cartridge maps

**Cartridge MAP region** holds the world atlas and compressed screens. See [04_worlds_and_screens.md](04_worlds_and_screens.md). CPU reads MAP only through the **`$FE90` MAP port**. PRG/CHR share the same ~2 MB flash.

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
- **Streaming cue:** software, **2 tiles (16 px)** before a seam. Neighbor lookup, empty-template fill, and `load_screen` are in [04_worlds_and_screens.md](04_worlds_and_screens.md).

## 7. Cartridge ROM budget (~2 MB)

| Region | Role | Ceiling |
|--------|------|---------|
| PRG-ROM | Game code | ~512 KB |
| CHR-ROM | Pattern banks | ~256 KB |
| MAP-ROM | Compressed nametables + attributes | ~1.17 MB |
| **Total** | Example parallel flash | **~2 MB** |

Uncompressed map upper bound: `8 x 64 x (960 + 240) = 600 KB` before RLE (tile plane + packed attr plane).

## 8. Mid-frame banks, parallax, and raster IRQ

Nametable indices are always **one byte** (0-255). You do not store a bank number per tile. More unique tiles on screen, parallax, and status-bar splits are the same class of trick as the NES: change a latch **while the beam is running**.

Retr01 makes that latch change **easier** than the NES. We do **not** use sprite-0 hit.

### Why not NES sprite-0

NES games wait until a non-transparent pixel of OAM sprite 0 overlaps opaque BG, then spin until that flag, then write scroll or CHR banks. That burns a sprite, depends on art, and races the PPU. Retr01 also cannot copy NES cycle-counted waits: CPU clock (8.000 MHz) and dot clock (5.369318 MHz) are **independent**, not a 3:1 pair.

Gameplay collision stays AABB in PRG ([01_system_overview.md](01_system_overview.md) principle 5). Raster timing is a beam compare, not a compositor collision.

### Raster compare (the sprite-0 replacement)

In `$FE0x` (exact bytes still `B2`):

| Field | Role |
|-------|------|
| `raster_y` | Scanline to match (**0-255**). Visible splits are 0-239. Write this. |
| `beam_y` | Live beam Y. Read-only. Fine for debug. Do not spin on this for splits. |
| `raster_hit` | Status bit. Sets when `beam_y == raster_y` at **start of that scanline** (dot 0). Sticky until software acks. |
| `raster_irq_enable` | If set, that match asserts **IRQ** (W65C02S `IRQB`), not NMI. |

NMI stays the VBlank metronome (start of line 240). IRQ is optional and only for raster. Ack the hit in the IRQ handler, then write the **next** `raster_y` if you have another split this frame.

Hardware is a compare of the existing Y counters (74HC161) against one latch. A GAL or a 74HC688 is enough. No extra sprite.

### When a write shows up on screen

`$FE30` (banks / world) and scroll latches are live. The **next PPU pattern fetch** uses the new value. Shift registers already hold the current tile, so expect up to **8 px (one tile)** of delay. For a clean split, write during **HBlank** (85 dots, ~126 CPU cycles at 8 MHz). The IRQ at dot 0 of line N is early enough to prepare the next line, or fire the compare on line N-1 and write in that HBlank.

### More than 256 unique BG tiles

One nametable still names tiles 0-255. The active BG page is whichever bank `$FE30` currently selects.

- At `load_screen`, set the **authored** BG bank (the set that nametable was drawn against).
- In a raster IRQ, switch to another of the world's **4** BG banks. From that scanline down, the same 0-255 indices are a **different** 256 pictures.
- Four horizontal bands => up to **1024** unique BG tiles on one frame, still one nametable.
- Status bar vs playfield is the one-split version of the same trick.
- This is **not** MMC3 1 KB CHR granules. A bank switch replaces the whole 256-tile BG page. Splits are horizontal bands, not per-column banks. Per-tile bank IDs would need another attr plane. We are not adding that.

Sprite bank may switch on the same IRQ (boss art, HUD icons) independently of BG.

`$FE30` world select may also change mid-frame (legal, next CHR fetch). Usually leave it. It is a whole chapter of CHR, not a small tile set.

### Parallax

The four VRAM nametable slots are the **2x2 camera field**, not four background layers. Parallax is software:

1. **Scroll split:** write a new `scroll_x` / `scroll_y` in HBlank at a raster line (sky vs ground, status bar locked, etc.).
2. **Several bands:** re-arm `raster_y` in each IRQ (near / mid / far).
3. **Sprite layer:** 16 sprites per line can carry a foreground strip. OAM does not use scroll, so it already parallaxes against the BG camera if you move it differently.

You may also rewrite nametable bytes during the visible frame (interleaved VRAM). NES generally could not. That updates **which** of the 256 tiles are shown. It does not add a new CHR page. Combine with a bank switch if you need new art.

Do not steal a nametable slot as a second layer. That breaks 2x2 scrolling.

