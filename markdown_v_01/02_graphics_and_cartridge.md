# Graphics & Cartridge Architecture

Canonical description of Retr01 tile graphics and cartridge layout.

## 1. Display geometry

| Item | Value |
|------|-------|
| Visible resolution | 256×240 |
| Tile size | 8×8 |
| Nametable | **32×30** tile indices (960 bytes) |
| Color depth | 2 bpp (3 colors + transparency) |

## 2. Worlds, screens, and banks

```
Cartridge
└── World 0..7                      (up to 8)
    ├── Pattern banks 0..3          (4 per world)
    │   └── bank: BG page (256) + Sprite page (256) = 512 patterns
    └── Screens 0..63               (up to 64)
        └── 32×30 nametable + per-tile BG attributes
            └── each screen authored against one of the world's 4 banks
```

### Rules

1. Up to **8** worlds; up to **64** screens each → **512** screens max per cart.
2. Each world has **4** pattern banks.
3. Each bank: **first page BG**, **second page sprites** → **512** patterns / bank (8 KiB @ 16 bytes/pattern).
4. A screen’s nametable is authored for one bank; at runtime the PPU may select **BG bank** and **sprite bank** independently, and either may change **mid-frame**.
5. PPU fetches CHR **from cartridge CHR-ROM** (not from VRAM).

## 3. Pattern math

| Unit | Patterns | Bytes |
|------|----------|-------|
| Page | 256 | 4 KiB |
| Bank | 512 | 8 KiB |
| World (4 banks) | 2048 | 32 KiB |
| Cart max (8 worlds) | 16384 | **256 KiB** CHR |

## 4. Sprites vs background

- BG: nametable + scroll → indices into **BG bank** page.
- Sprites: OAM (64) → indices into **sprite bank** page.
- Max **16 sprites per scanline**; extras dropped.
- Non-transparent sprite pixel wins over BG *(compositor default)*.

## 5. Palettes and attributes

- **BG palette is selected per tile** (not NES 2×2 attribute blocks).
- Each nametable slot in VRAM stores tile index **and** a per-tile attribute byte (planning default: 1 byte/tile; bitfield packing can shrink later).
- Exact palette count and master-palette index width: see [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md).

## 6. Live VRAM vs cartridge maps

**Cartridge MAP region** holds all screens (compressed, RLE-class), up to ~1.17 MiB with PRG/CHR in a ~2 MB flash.

**On-board VRAM (32 KB)** holds only what scrolling needs live:

| Contents | Planning size |
|----------|----------------|
| 4 nametable slots × (960 tiles + 960 attrs), 2 KiB-aligned | 4 × 2 KiB = **8 KiB** |
| OAM shadow / line eval scratch | ≲1 KiB |
| 5-tile streaming strip scratch | ≲2 KiB |
| Reserved / future | remainder of 32 KiB |

### Why four nametable slots

Smooth pixel scrolling can show tiles from **up to four screens** at once (camera on a 2×2 seam). Hardware keeps a **2×2 nametable grid** (64×60 tiles addressable via scroll). A **~5-tile-thick** perimeter around the 32×30 viewport is the CPU’s cue to stream the next strip from MAP-ROM into the slot that is scrolling into view — not an extra fifth nametable.

## 7. Cartridge ROM budget (~2 MB)

| Region | Role | Ceiling |
|--------|------|---------|
| PRG-ROM | Game code | ~512 KiB |
| CHR-ROM | Pattern banks | ~256 KiB |
| MAP-ROM | Compressed nametables + attributes | ~1.17 MiB |
| **Total** | Example parallel flash | **~2 MB** |
