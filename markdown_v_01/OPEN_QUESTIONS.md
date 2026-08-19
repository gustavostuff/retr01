# Open Questions & Decision Log

Living snapshot. When hardware or software decisions change, **update this file** (and the specs it points at). It is not a freeze.

## A. Current locked decisions

| Topic | Decision |
|-------|----------|
| Name / order | **Retr01**, **A → C → H** |
| Worlds / screens / CHR cells | **8 worlds**. Each: **64 screens max** on a sparse virtual grid up to **16 × 16**. Each world has **4 BG cells + 4 sprite cells**. Screen = **32×30**. Spec: [04_worlds_and_screens.md](04_worlds_and_screens.md) |
| CHR layout | 4 BG cells + 4 sprite cells, **256** patterns each |
| Authored vs runtime cells | Each screen stores **BG cell 0-3** in MAP flags. Loader copies it into the destination slot's BG cell latch (slots 0-5). Sprite cell stays separate/global. Raster IRQ still available for mid-frame changes |
| Raster | **Scanline compare + IRQ**. `set_camera_axis(H/V/BOTH)`. `set_parallax` forces matching 1-axis camera. Spec: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) section 8 |
| System RAM | **32 KB** full chip at `$0000-$7FFF` |
| VRAM | **32 KB**, interleaved, CHR from cart. OAM is **not** in VRAM |
| OAM / sprites | 64 entries, **16 / scanline** max. OAM lives in the **ATmega1284P** (internal RAM). 6502 writes `$FE20/$FE21` (store loop, **no** hardware DMA). Line buffer is a third **AS6C62256**. Spec: [14_reduced_number_of_chips.md](14_reduced_number_of_chips.md) |
| Scroll | `scroll_x` / `scroll_y` one byte each (0–255 wrap), 1/2/4 screens via NT arrange. Always pixel-scroll, even with 0 stored neighbors |
| Stream margin | **2 tiles (16 px)** software cue to refill seams. Miss = empty template (default black, optional per-world nametable) |
| MAP offsets | **24-bit** (`world_base`, `data_off`, `empty_off`). Matches `$FE90`. MAP is ~1.17 MB, so 16-bit is not enough |
| Palettes | **8** (4 BG + 4 sprite), **shared BG color 0** |
| BG attributes | **Per tile**, packed **240 bytes/NT** (1 byte = 2×2 cell, 2 bits/tile). Not NES shared 2×2 |
| Sprite OAM attr | NES-like byte (palette / flip / priority) |
| Master palette | **64-entry RGB table** — [`retr01_world_studio/retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt) |
| APU | Separate **ATmega328P**. NES-style: 2 pulse + triangle + noise + DMC. Do not merge with the sprite MCU |
| CPU clock | **8.000 MHz** |
| Dot clock / frame | **5.369318 MHz**, **341×262**, **~60.098 Hz** NMI |
| MAP access | **`$FE90` MAP port**. Atlas + compressed screens in MAP-ROM |
| MAP RLE | **Byte RLE**, two sections per screen: **960** tile + **240** attr (**1200** decoded) |
| Controllers | **One byte per player**, all form factors. `$FE60` = P1, `$FE61` = P2. Bits: 0 Dpad Right, 1 Dpad Left, 2 Dpad Down, 3 Dpad Up, 4 X, 5 Y, 6 Coin (Select on console), 7 Start. No extra cabinet byte. A: parallel IDC. C: 3-wire + MCU **in the controller**, same two bytes on the board |
| PRG mapper | **`$FE80` only** |
| CPU map | `$0000-$7FFF` RAM / `$FE00-$FEFF` I/O / PRG elsewhere |
| Glue (Retr01-A v0) | **3× ATF22V10CQZ-20PU** (Microchip DIP, in production Aug 2026) + 74HC157/245/161/573. Planning **49** motherboard ICs. Glue 16 is frozen in [14](14_reduced_number_of_chips.md). Not Lattice GAL. Not ATF15xx. |
| Video out | **A/C:** RGBS + S-Video + composite pads. **H:** RGBS only. HDMI is an external converter |
| Power (A/C) | Female **5.5 mm × 2.1 mm** barrel, 5 V. Optional pads for a USB-C breakout PCB |
| Schematic prompt | [15](15_schematic_prompt_coprocessor.txt) later, after Digital. Current path: [16_simulation_and_bringup_plan.md](16_simulation_and_bringup_plan.md) |

## B. Still open

| # | Topic | Notes |
|---|--------|------|
| B1 | ~~Exact master palette RGB table~~ | **Resolved:** [`retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt) |
| B2 | Exact `$FExx` bitfields beyond the locked offsets | Block families frozen. `$FE0x` still needs raster_y, beam_y, raster_hit, enable, ack. APU `$FE4x` NES bit packing TBD. Pad **bytes and bit numbers** are locked in §A |
| B3 | RGBS sync polarity / analog levels | Digital timing locked. Proto assumes **negative** CSYNC |
| B4 | ~~Pad bitfields~~ | **Resolved:** 8 bits/player including Coin/Start as bits 6–7. Physical IDC pin numbers still free as long as they match that byte |
| B5 | Retr01-C 3-wire bit protocol | Pinned: 3 wires + MCU in the pad, **same `$FE60/$FE61` bytes**. Connector shell TBD |
| B6 | OAM byte field order | Default NES-like Y, tile, attr, X |
| B7 | Repo folder still named GameNerd | Rename when ready |

## C. Emulator defaults (when the C emu is rewritten)

| Topic | Default |
|-------|---------|
| Wrong-phase VRAM access | **Hard error** in debug |
| Sprite vs BG | Opaque sprite wins unless OAM priority puts it behind opaque BG |
| Sprite timing | **Next-line** eval (line *N* uses buffer built on line *N−1*), same as 1284 ping-pong |
| OAM upload | Guest loop to `$FE21`; **no** `$FE22` DMA steal |
| Pads | Two bytes `$FE60/$FE61`, bit layout in §A |
| Tile / cell size | 8×8, 256 patterns/cell |
| Attributes | Packed 240-byte BG attr plane (2 bits/tile), NES-like sprite OAM attr |
