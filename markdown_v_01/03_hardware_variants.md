# Retr01 Hardware Variants

One architecture (CPU, interleaved 32 KB VRAM, 32 KB system RAM, CHR from cart, 2bpp, shared memory map). Three shells. Shared rules: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md), [08_memory_map.md](08_memory_map.md).

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
| Glue | **ATF22V10** array (DIP-24). Lattice GAL22V10 is EOL. Same 22V10 idea |
| System RAM | **32 KB**, CPU-only |
| VRAM | **32 KB**, interleaved CPU<->PPU, CHR from cart |
| Mux / bus | **74HC157** / **74HC245**, plus **74HC573** latches and **74HC161** counters. GALs eat random gate chips only. Do not GAL-away wide buses |
| Board NVRAM | Parallel EEPROM (e.g. AT28C64B) |
| Cart | ~2 MB parallel flash (PRG + CHR + MAP) |
| Audio | ATmega APU, NES-style channels |

### Cabinet I/O

- 40-pin IDC header, **parallel switches** (no MCU in the stick)
- ~24 pins player inputs (2 sticks, up to 8 buttons each). Rest are power/ground/coin/start/reset
- Pinout TBD. CPU sees **four bytes** under `$FE60-$FE63` (see [08_memory_map.md](08_memory_map.md))

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

**Pinned:** 3-wire serial pads (N64-style idea), one small chip in each controller to shift bits. Goal is a tough, thin cable, not USB and not a first-pass DB-9.

Software still reads `$FE60-$FE63` (same four bytes as Retr01-A). Only the board-side shifter changes. Protocol and connector shell TBD.

### Power

- **Female 5.5 mm x 2.1 mm barrel**, 5 V. On-board regulator for 3.3 V if a chip needs it
- **Unpopulated solder pads** for a small USB-C female breakout PCB (builder wires 5 V / GND). Not USB-C PD on this board

---

## Retr01-H: Handheld

Portable SMD edition with architectural parity (same map, VRAM model, CHR-from-cart, 2bpp).

### Packaging

Dense CPU package, SMD SRAM/logic, 4-6 layer PCB. Decode may move from ATF22V10 to a denser CPLD, with the **same** CPU map.

### Power

Static-core clock halt, Li-Po + USB-C PMIC.

### Display & controls

Raw LCD/OLED, nearest-neighbor from 256x240. Analog out is **RGBS pads only** (no S-Video or composite on H). Thin platform layer for buttons, sleep, brightness.
