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
- Dense cabinet I/O, analog RGBS, optional encoder pads
- EEPROM for scores / operator settings

### Core silicon

| Block | Specification |
|-------|----------------|
| CPU | W65C02S (DIP-40) |
| Glue | GAL22V10 array (DIP-24) |
| System RAM | **32 KB**, CPU-only |
| VRAM | **32 KB**, interleaved CPU<->PPU, CHR from cart |
| Mux / bus | 74HC157 / 74HC245 (+ latches/counters) |
| Board NVRAM | Parallel EEPROM (e.g. AT28C64B) |
| Cart | ~2 MB parallel flash (PRG + CHR + MAP) |
| Audio | ATmega APU, NES-style channels |

### Cabinet I/O

- 40-pin IDC header, **parallel switches** (no MCU in the stick)
- ~24 pins player inputs (2 sticks, up to 8 buttons each). Rest are power/ground/coin/start/reset
- Pinout TBD. CPU sees **four bytes** under `$FE60-$FE63` (see [08_memory_map.md](08_memory_map.md))

### Video & audio

- Primary: analog **RGBS**
- Optional: S-Video / composite encoder pads
- Audio: NES-style mix via `$FE40-$FE5F`

### Validation

Prove ROMs on the **low-level C emulator**, then flash cart / program EEPROM. Authoring toolchains come later.

---

## Retr01-C: Console

Living-room shell, same core as A. Differences are home displays and controllers.

### Board

Through-hole / socketed DIP for early revisions, 2-4 layer PCB, mini-ITX or custom shell.

### Video

Internal 256x240 2bpp. Nearest-neighbor upscale toward HDMI/DVI. Legacy analog paths as needed.

### Controllers

**Pinned:** 3-wire serial pads (N64-style idea), one small chip in each controller to shift bits. Goal is a tough, thin cable, not USB and not a first-pass DB-9.

Software still reads `$FE60-$FE63` (same four bytes as Retr01-A). Only the board-side shifter changes. Protocol and connector shell TBD.

### Power

Barrel jack and/or USB-C PD -> 5V / 3.3V rails.

---

## Retr01-H: Handheld

Portable SMD edition with architectural parity (same map, VRAM model, CHR-from-cart, 2bpp).

### Packaging

Dense CPU package, SMD SRAM/logic, 4-6 layer PCB. Decode may move from GAL22V10 to a denser CPLD, with the **same** CPU map.

### Power

Static-core clock halt, Li-Po + USB-C PMIC.

### Display & controls

Raw LCD/OLED, nearest-neighbor from 256x240. Thin platform layer for buttons, sleep, brightness.
