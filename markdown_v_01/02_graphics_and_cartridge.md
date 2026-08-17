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

CPU and dot clocks are **independent**. The W65C02S runs at 8 MHz for game logic and interleaved VRAM phases. The video beam advances on the 5.369318 MHz dot clock (standard NTSC PPU timing, friendly to arcade RGBS / encoders).

PPU VRAM fetches occur only on PPU-owned CPU phases (with line buffers / shift registers bridging the two domains as on real hardware). In other words, even though the PPU only accesses VRAM 50% of the time, the display never flickers. The hardware uses shift registers to cache the graphics data during the PPU's phase, continuously streaming pixels to the screen while the CPU executes game logic during its phase.

See also [08_memory_map.md](08_memory_map.md) for the VRAM port and interleave rules.

## 2. Worlds, screens, scroll, and banks

Three different ideas. Do not mix them.

| Word | What it is | Where it lives |
|------|------------|----------------|
| **World** | A cart chapter: its CHR banks and its MAP screens. Up to **8** per cart. `$FE30` world select picks which chapter the PPU is drawing art from. | Cartridge |
| **Screen** | One **32x30** nametable plus attrs. Up to **64** per world. | MAP-ROM on cart. Up to **4** copies live in VRAM slots |
| **Scroll** | `scroll_x` / `scroll_y`, one byte each (0-255). Pixel offset of the 256x240 window across the live VRAM slots. | PPU latches (`$FE0x`) |
| **CHR bank** | 512 patterns (256 BG + 256 sprites). 4 banks per world. | CHR-ROM on cart. `$FE30` picks BG bank and sprite bank separately |

**Smooth scrolling** is pixel scroll over the live slots, plus software copying the next **screen** into the incoming slot before the camera reaches the seam. The player can walk a large map. The hardware never has a 16-bit "world camera." When a scroll byte wraps, you refill a slot. You do **not** bump the world index unless the game changes chapter (different art set).

How screens sit next to each other inside a world (the map grid) is **not locked yet**. Authoring sketch only, labels in PRG/MAP, not hardware:

```
world_01:
    ; Import this world's CHR and MAP (whole world, or per-screen binaries).
    ; Neighbor graph / grid comes later.

world_02:
    ; Same idea.
```

```
Cartridge
+-- World 0..7                      (up to 8)
    +-- Pattern banks 0..3          (4 per world)
    |   +-- bank: BG patterns (256) + sprite patterns (256) = 512
    +-- Screens 0..63               (up to 64)
        +-- 32x30 nametable + 240-byte attrs
            +-- each screen authored against one BG bank
```

### Rules

1. Up to **8** worlds, up to **64** screens each -> **512** screens max per cart.
2. Each world has **4** pattern banks.
3. Each bank: **first 256 patterns BG**, **second 256 sprites** -> **512** patterns / bank (8 KB @ 16 bytes/pattern).
4. **Authored bank:** each screen's nametable tile indices assume one of the world's 4 banks.
5. **Runtime banks:** PPU may select **BG bank** and **sprite bank** independently. Either may change **mid-frame**.
6. PPU fetches CHR **from cartridge CHR-ROM** (not from VRAM).

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
- Non-transparent sprite pixel wins over BG (compositor default).

## 5. Palettes and attributes

- **8 palettes** total: **4 background + 4 sprite** (each 4 entries at 2bpp: index 0 + 3 colors).
- **Shared backdrop / color 0** across BG palettes (NES rule), any master-palette index.
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

**Cartridge MAP region** holds all screens (compressed, RLE-class), up to ~1.17 MB with PRG/CHR in a ~2 MB flash. CPU reads MAP only through the **`$FE90` MAP port**.

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
- **Streaming cue (software, not a PPU register):** when the camera is within **2 tiles (16 px)** of a seam, copy the next screen strip from MAP-ROM into the incoming slot. At 8 MHz with interleaved VRAM a 2-tile strip is cheap. 1 tile is tight if game logic is busy that frame. 5 tiles is more time than we need. The emulator does not enforce the cue. Miss it and the player sees stale tiles at the edge.

## 7. Cartridge ROM budget (~2 MB)

| Region | Role | Ceiling |
|--------|------|---------|
| PRG-ROM | Game code | ~512 KB |
| CHR-ROM | Pattern banks | ~256 KB |
| MAP-ROM | Compressed nametables + attributes | ~1.17 MB |
| **Total** | Example parallel flash | **~2 MB** |

Uncompressed map upper bound: `8 x 64 x (960 + 240) = 600 KB` before RLE (tile plane + packed attr plane).
