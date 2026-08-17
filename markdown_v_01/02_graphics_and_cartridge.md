# Graphics & Cartridge Architecture

Canonical description of Retr01 tile graphics and cartridge layout.

## 1. Display geometry and timing

| Item | Value |
|------|-------|
| Visible resolution | 256x240 |
| Tile size | 8x8 |
| Nametable | **32x30** tile indices (960 bytes) + **960-byte** per-tile attribute plane |
| Color depth | 2 bpp (3 colors + transparency per draw unit) |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** (NTSC PPU-rate, same class as NES) |
| Dots per scanline | **341** (256 visible + 85 HBlank) |
| Scanlines per frame | **262** (240 visible + 22 VBlank) |
| Frame rate | **~60.098 Hz** (NMI metronome) |

CPU and dot clocks are **independent**. The W65C02S runs at 8 MHz for game logic and interleaved VRAM phases. The video beam advances on the 5.369318 MHz dot clock (standard NTSC PPU timing, friendly to arcade RGBS / encoders).

PPU VRAM fetches occur only on PPU-owned CPU phases (with line buffers / shift registers bridging the two domains as on real hardware). In other words, even though the PPU only accesses VRAM 50% of the time, the display never flickers. The hardware uses shift registers to cache the graphics data during the PPU's phase, continuously streaming pixels to the screen while the CPU executes game logic during its phase.

See also [08_memory_map.md](08_memory_map.md) for the VRAM port and interleave rules.

## 2. Worlds, screens, and banks

```
Cartridge
+-- World 0..7                      (up to 8)
    +-- Pattern banks 0..3          (4 per world)
    |   +-- bank: BG page (256) + Sprite page (256) = 512 patterns
    +-- Screens 0..63               (up to 64)
        +-- 32x30 nametable + per-tile attributes
            +-- each screen authored against one BG bank
```

### Rules

1. Up to **8** worlds, up to **64** screens each -> **512** screens max per cart.
2. Each world has **4** pattern banks.
3. Each bank: **first page BG**, **second page sprites** -> **512** patterns / bank (8 KB @ 16 bytes/pattern).
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

- BG: nametable + scroll -> indices into **BG bank** page.
- Sprites: OAM (64) -> indices into **sprite bank** page.
- Max **16 sprites per scanline**. Extras are dropped.
- Non-transparent sprite pixel wins over BG (compositor default).

## 5. Palettes and attributes

- **8 palettes** total: **4 background + 4 sprite** (each 4 entries at 2bpp: index 0 + 3 colors).
- **Shared backdrop / color 0** across BG palettes (NES rule), any master-palette index.
- **BG attributes are per tile:** each of the 960 nametable entries has its own palette select (which of the 4 BG palettes). This is **finer than NES** (NES shares one select across a 2x2 tile group).
- Storage: **960 attribute bytes** per screen/slot (1 byte per tile, low 2 bits = palette 0-3, upper bits reserved). Same byte width as the tile-index plane for simple addressing.
- Sprite attributes: **NES-like OAM attr byte** (palette, flips, priority). Index 0 = transparent.
- **Master palette:** custom Retr01 ramp (not stock NES colors). **32 min / 64 likely**, RGB table TBD.

## 6. Live VRAM vs cartridge maps

**Cartridge MAP region** holds all screens (compressed, RLE-class), up to ~1.17 MB with PRG/CHR in a ~2 MB flash. CPU reads MAP only through the **`$FE90` MAP port**.

**On-board VRAM (32 KB)** live set:

| Contents | Planning size |
|----------|----------------|
| 4 nametable slots x (960 tiles + 960 attrs), 2 KB-aligned | 4 x 2 KB = **8 KB** |
| Streaming / decompress scratch | ~4 KB |
| Reserved / future | remainder of 32 KB |

OAM lives in dedicated registers / `$FE2x`, not in the VRAM chip.

### Scrolling model

- `scroll_x` and `scroll_y` are **one byte each** (0-255, wrap).
- Live field: up to **four** nametable slots (2x2). Arrangement/mirroring chooses **1, 2, or 4** distinct screens.
- A **~5-tile-thick** perimeter cues streaming the next strip from MAP-ROM into the incoming slot.

## 7. Cartridge ROM budget (~2 MB)

| Region | Role | Ceiling |
|--------|------|---------|
| PRG-ROM | Game code | ~512 KB |
| CHR-ROM | Pattern banks | ~256 KB |
| MAP-ROM | Compressed nametables + attributes | ~1.17 MB |
| **Total** | Example parallel flash | **~2 MB** |

Uncompressed map upper bound: `8 x 64 x (960 + 960) = 960 KB` before RLE (tile plane + per-tile attr plane).
