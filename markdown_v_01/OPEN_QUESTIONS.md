# Open Questions & Decision Log

## A. Locked (do not regress)

| Topic | Decision |
|-------|----------|
| Name / order | **Retr01**; **A -> C -> H** |
| Worlds / screens / banks | **8 x 64**; **4 banks/world**; screen = **32x30** |
| Bank layout | Page0 BG + page1 sprites = **512** patterns |
| System RAM | **32 KB**, CPU-only |
| VRAM | **32 KB**, interleaved; **CHR from cartridge** |
| Scroll live set | **4 nametable slots** (2x2) + ~5-tile stream margin |
| BG palettes | **Per-tile** (not 2x2) |
| Bank binding | **Separate** BG vs sprite banks; **mid-frame** changes allowed |
| APU | **NES-style**: 2 pulse + triangle + noise + DMC |
| Near-term software | **Low-level C emulator** only (no PPUX/cc65 focus yet) |
| CPU map | **`$0000-$7EFF` RAM / `$7Fxx` I/O / `$8000-$FFFF` PRG** - [08_memory_map.md](08_memory_map.md) |

## B. Still open (does not block emulator skeleton)

| # | Topic | Notes |
|---|--------|------|
| B1 | Attribute bitfield | Planning default = 1 attribute byte/tile; may pack later |
| B2 | Master palette size | How many global RGB entries; how many live BG/sprite palettes |
| B3 | Sprite attribute scheme | Mirror BG per-sprite palette bits? |
| B4 | MAP CPU access path | How MAP-ROM is banked for decompression into VRAM |
| B5 | Exact `$7Fxx` register bits | Block layout frozen; bitfields per reg still TBD |
| B6 | Clock / dot clock / RGBS timing sheet | ~8 MHz class called out historically |
| B7 | Arcade IDC pinout | |
| B8 | Retr01-C primary controller port | DB-9 vs USB |
| B9 | OAM byte format | NES-like 4 bytes/sprite is a reasonable default |
| B10 | Repo folder still named GameNerd | Rename when ready |

## C. Emulator defaults (change only deliberately)

| Topic | Default |
|-------|---------|
| Wrong-phase VRAM access | **Hard error** in debug |
| Sprite vs BG | Non-transparent sprite wins |
| Tile / page size | 8x8; 256 patterns/page |
