# Open Questions & Decision Log

## A. Locked (do not regress)

| Topic | Decision |
|-------|----------|
| Name / order | **Retr01**; **A -> C -> H** |
| Worlds / screens / banks | **8 x 64**; **4 banks/world**; screen = **32x30** |
| Bank layout | Page0 BG + page1 sprites = **512** patterns |
| Authored vs runtime banks | Screen authored vs one BG bank; runtime BG/sprite banks independent; mid-frame OK |
| System RAM | **32 KB** full chip at `$0000-$7FFF` |
| VRAM | **32 KB**, interleaved; CHR from cart; OAM not in VRAM |
| Scroll | `scroll_x` / `scroll_y` one byte each (0-255 wrap); 1/2/4 screens via NT arrange |
| Stream margin | ~5-tile perimeter cue to refill seams |
| Palettes | **8** (4 BG + 4 sprite); **shared BG color 0** |
| BG attributes | **Per tile** (960 bytes/NT); low 2 bits = BG palette 0-3; finer than NES 2x2 |
| Sprite OAM attr | NES-like byte (palette / flip / priority) |
| Master palette | Custom Retr01; **32 min / 64 likely** - RGB table TBD |
| APU | NES-style: 2 pulse + triangle + noise + DMC |
| CPU clock | **8.000 MHz** |
| Dot clock / frame | **5.369318 MHz**; **341x262**; **~60.098 Hz** NMI |
| MAP access | **`$FE90` MAP port** |
| PRG mapper | **`$FE80` only** |
| CPU map | `$0000-$7FFF` RAM / `$FE00-$FEFF` I/O / PRG elsewhere |
| Near-term software | Low-level C emulator only |

## B. Still open

| # | Topic | Notes |
|---|--------|------|
| B1 | Exact master palette RGB table | 32 vs 64 entries + colors |
| B2 | Exact `$FExx` register bitfields | Block layout frozen |
| B3 | RGBS sync polarity / analog levels | Digital timing locked above |
| B4 | Arcade IDC pinout | |
| B5 | Retr01-C primary controller | DB-9 vs USB |
| B6 | OAM byte field order | NES-like Y,tile,attr,X default |
| B7 | Repo folder still named GameNerd | Rename when ready |

## C. Emulator defaults

| Topic | Default |
|-------|---------|
| Wrong-phase VRAM access | **Hard error** in debug |
| Sprite vs BG | Non-transparent sprite wins |
| Tile / page size | 8x8; 256 patterns/page |
| Attributes | Per-tile BG attr plane (960 bytes); NES-like sprite OAM attr |
