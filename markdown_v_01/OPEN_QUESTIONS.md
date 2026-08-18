# Open Questions & Decision Log

## A. Locked (do not regress)

| Topic | Decision |
|-------|----------|
| Name / order | **Retr01**, **A -> C -> H** |
| Worlds / screens / banks | **8 worlds**. Each: **64 screens max** on a sparse virtual grid up to **64 x 64**. 4 CHR banks/world. Screen = **32x30**. Spec: [04_worlds_and_screens.md](04_worlds_and_screens.md) |
| Bank layout | Page0 BG + page1 sprites = **512** patterns |
| Authored vs runtime banks | `load_screen` sets authored BG bank. Runtime BG/sprite banks independent. Mid-frame OK via raster IRQ |
| Raster | **Scanline compare + IRQ**. `set_camera_axis(H/V/BOTH)`. `set_parallax` forces matching 1-axis camera. Spec: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) section 8 |
| System RAM | **32 KB** full chip at `$0000-$7FFF` |
| VRAM | **32 KB**, interleaved, CHR from cart, OAM not in VRAM |
| Scroll | `scroll_x` / `scroll_y` one byte each (0-255 wrap), 1/2/4 screens via NT arrange. Always pixel-scroll, even with 0 stored neighbors |
| Stream margin | **2 tiles (16 px)** software cue to refill seams. Miss = empty template (default black, optional per-world nametable) |
| MAP offsets | **24-bit** (`world_base`, `data_off`, `empty_off`). Matches `$FE90`. MAP is ~1.17 MB, so 16-bit is not enough |
| Palettes | **8** (4 BG + 4 sprite), **shared BG color 0** |
| BG attributes | **Per tile**, packed **240 bytes/NT** (1 byte = 2x2 cell, 2 bits/tile). Not NES shared 2x2 |
| Sprite OAM attr | NES-like byte (palette / flip / priority) |
| Master palette | **64-entry RGB table** — [`retr01_world_studio/retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt). Studio doc: [09_master_palette.md](../retr01_world_studio/planning/09_master_palette.md) |
| APU | NES-style: 2 pulse + triangle + noise + DMC |
| CPU clock | **8.000 MHz** |
| Dot clock / frame | **5.369318 MHz**, **341x262**, **~60.098 Hz** NMI |
| MAP access | **`$FE90` MAP port**. Atlas + compressed screens in MAP-ROM. RAM `world`,`map_x`,`map_y` then `load_screen` |
| MAP RLE | **Byte RLE**, two sections per screen: **960** tile bytes + **240** attr bytes (**1200** decoded). Spec: [retr01_world_studio/planning/06_data_formats.md](../retr01_world_studio/planning/06_data_formats.md) |
| Controllers | CPU sees **`$FE60-$FE63`** (4 bytes, 2 players). A: parallel IDC. C: 3-wire pad (pinned) |
| PRG mapper | **`$FE80` only** |
| CPU map | `$0000-$7FFF` RAM / `$FE00-$FEFF` I/O / PRG elsewhere |
| Near-term software | Low-level C emulator only |
| Glue | **ATF22V10** for decode/timing. **74HC157/245/161/573** for buses, counters, latches. Cut gate chips only |
| Video out | **A/C:** RGBS + S-Video + composite pads. **H:** RGBS only. HDMI is an external converter, not on-board |
| Power (A/C) | Female **5.5 mm x 2.1 mm** barrel, 5 V. Optional pads for a USB-C breakout PCB |

## B. Still open

| # | Topic | Notes |
|---|--------|------|
| B1 | ~~Exact master palette RGB table~~ | **Resolved for v0.1:** 64 entries in [`retr01_world_studio/retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt) |
| B2 | Exact `$FExx` register bitfields | Block layout frozen. `$FE0x` must include `raster_y`, `beam_y`, `raster_hit`, `raster_irq_enable`, ack |
| B3 | RGBS sync polarity / analog levels | Digital timing locked above |
| B4 | Arcade IDC pinout | Parallel switches into `$FE60-$FE63`. Bitfields TBD |
| B5 | Retr01-C 3-wire pad protocol | Pinned: 3 wires + MCU in pad, same `$FE6x` bytes. Connector shell TBD |
| B6 | OAM byte field order | NES-like Y,tile,attr,X default |
| B7 | Repo folder still named GameNerd | Rename when ready |

## C. Emulator defaults

| Topic | Default |
|-------|---------|
| Wrong-phase VRAM access | **Hard error** in debug |
| Sprite vs BG | Opaque sprite wins unless OAM priority puts it behind opaque BG |
| Tile / page size | 8x8, 256 patterns/page |
| Attributes | Packed 240-byte BG attr plane (2 bits/tile), NES-like sprite OAM attr |
