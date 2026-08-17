# Retr01-A — Arcade Edition

Retr01-A is the foundation of the family: arcade motherboard, first hardware target.

## 1. Goals

- Through-hole (DIP) bring-up and repair.
- Shared Retr01 CPU / interleaved-VRAM / 2bpp / cart contract.
- Dense cabinet I/O header; analog RGBS; optional encoder pads.
- EEPROM for scores / operator settings.

## 2. Core silicon

| Block | Specification |
|-------|----------------|
| CPU | W65C02S (DIP-40) |
| Glue | GAL22V10 array (DIP-24) |
| System RAM | **32 KB** SRAM, CPU-only |
| VRAM | **32 KB** SRAM, **interleaved** CPU↔PPU; CHR from cart |
| Mux / bus | 74HC157 / 74HC245 (and latches/counters as needed) |
| Board NVRAM | Parallel EEPROM (e.g. AT28C64B) |
| Cart | ~2 MB parallel flash (PRG + CHR + MAP) |
| Audio | ATmega APU, NES-style channels |

Map: [08_memory_map.md](08_memory_map.md). Graphics: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md).

## 3. Cabinet I/O

- 40-pin IDC header.
- ~24 pins player inputs (2 sticks, up to 8 buttons each); rest power/ground/coin/start/reset.
- Pinout TBD in a later sheet; CPU sees them under `$7F60`.

## 4. Video & audio

- Primary: analog **RGBS**.
- Optional: S-Video / composite encoder pads.
- Audio: NES-style mono mix from ATmega via `$7F40–$7F5F`.

## 5. Validation path

Prove ROMs on the **low-level C emulator**, then flash cart / program EEPROM for cabinet install. Authoring toolchains come later.
