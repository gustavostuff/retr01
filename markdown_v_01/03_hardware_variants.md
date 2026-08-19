# Retr01 Hardware Variants

One architecture (CPU, interleaved 32 KB VRAM, 32 KB system RAM, sprite line-buffer SRAM, 1284 sprite/input coprocessor, CHR from cart, 2bpp, shared memory map). Three shells. Shared rules: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md), [08_memory_map.md](08_memory_map.md). Coprocessor / chip count: [14_reduced_number_of_chips.md](14_reduced_number_of_chips.md).

| Variant | Role | Packaging | Status |
|---------|------|-----------|--------|
| **Retr01-A** | Arcade motherboard | Through-hole (DIP) | **Initial / primary** |
| **Retr01-C** | Home console | Through-hole (DIP) | After A proves the core |
| **Retr01-H** | Handheld | SMD | Later, do not block A |

---

## Retr01-A: Arcade

Foundation of the family: cabinet-ready board, first silicon target.

### Goals

- Through-hole bring-up and repair
- Dense cabinet I/O, analog outs (RGBS, S-Video, composite pads)
- EEPROM for scores / operator settings

### Core silicon

| Block | Specification |
|-------|----------------|
| CPU | W65C02S (DIP-40) |
| Glue | **3× ATF22V10CQZ-20PU** (DIP-24, Microchip, in production as of Aug 2026). Lattice **GAL16V8 / GAL20V8 / GAL22V10** are discontinued. Overflow → 4th **ATF22V10CQZ-20PU**, not a Lattice GAL and not ATF15xx |
| System RAM | **32 KB**, CPU-only |
| VRAM | **32 KB**, interleaved CPU<->PPU, CHR from cart |
| Line buffer | Third **32 KB** SRAM (512 B used). OAM is in the 1284 |
| Sprite + input | **ATmega1284P-PU** (DIP-40) |
| Mux / bus | **74HC157** / **74HC245**, plus **74HC573** latches and **74HC161** counters. GALs eat random gate chips only. Do not GAL-away wide buses |
| Board NVRAM | Parallel EEPROM (e.g. AT28C64B) |
| Cart | ~2 MB parallel flash (PRG + CHR + MAP) |
| Audio | **ATmega328P-PU**, NES-style channels. Do not merge with the 1284 |

### Cabinet I/O

- 20-pin IDC header (2×10), **parallel switches** for controller inputs (no MCU in the stick)
- Two players: UDLR + A + B + Coin + Start each. Rest of the header is power/ground/reset/speaker/optional CSYNC
- Pinout TBD. CPU sees **two bytes**: `$FE60` = P1, `$FE61` = P2 (see [08_memory_map.md](08_memory_map.md))

### Video & audio

- Analog pads on the board: **RGBS**, **S-Video**, and **composite** (15.7 kHz class, see [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md))
- No on-board HDMI. The RGBS pads can feed an external analog-to-HDMI converter
- Audio: NES-style mix via `$FE40-$FE5F`

### Power (bench / cabinet)

- **Female 5.5 mm x 2.1 mm barrel**, 5 V (same jack as Retr01-C, handy on the bench)
- In a cabinet, 5 V may instead come from the cab PSU
- **Unpopulated solder pads** for a small off-the-shelf USB-C female breakout PCB, if the builder wants that jack. Not a USB-C PD design on our board

### Validation

Prove ROMs on the **low-level C emulator**, then flash cart / program EEPROM. Authoring toolchains come later.

---

## Retr01-C: Console

Living-room shell, same core as A. Differences are home displays and controllers.

### Board

Through-hole / socketed DIP for early revisions, 2-4 layer PCB, mini-ITX or custom shell.

### Video

Internal 256x240 2bpp (same PPU as A). Analog pads: **RGBS**, **S-Video**, and **composite**. No on-board HDMI or DVI. RGBS pads can feed an external analog-to-HDMI converter.

### Controllers

**Pinned:** 3-wire serial pads (clock / data / latch or equivalent), MCU **inside each controller** serializes 8 bits. Goal is a tough, thin cable, not USB and not a first-pass DB-9.

Software still reads `$FE60` / `$FE61` (same two bytes as Retr01-A). The board 1284 reconstructs those bytes. Bit protocol and connector shell TBD (`B5`).

### Power

- **Female 5.5 mm x 2.1 mm barrel**, 5 V. On-board regulator for 3.3 V if a chip needs it
- **Unpopulated solder pads** for a small USB-C female breakout PCB (builder wires 5 V / GND). Not USB-C PD on this board

---

## Retr01-H: Handheld

Portable SMD edition with architectural parity (same map, VRAM model, CHR-from-cart, 2bpp).

### Packaging

Dense CPU package, SMD SRAM/logic, 4-6 layer PCB. Decode may move from ATF22V10 to a denser **in-production** CPLD, with the **same** CPU map. Do not plan H around ATF1508/1504 (EOL).

### Power

Static-core clock halt, Li-Po + USB-C PMIC.

### Display & controls

Raw LCD/OLED, nearest-neighbor from 256x240. Analog out is **RGBS pads only** (no S-Video or composite on H). Buttons still present as `$FE60` / `$FE61` (same 8-bit layout). Thin platform layer for sleep and brightness.
