# Retr01 System Overview

## 1. Vision

Retr01 is a modular 2D hardware ecosystem: discrete-logic 8-bit machines that share one execution model, one graphics contract, and one cartridge layout across form factors.

Form factors (A / C / H): [03_hardware_variants.md](03_hardware_variants.md).

Formerly GameNerd / Retr02. Those names are retired.

## 2. Core principles

1. **Unified CPU:** W65C02S on every variant, planning clock **8 MHz** (~4.5x a stock NES 6502).
2. **Interleaved VRAM only:** The CPU and PPU share Video SRAM on alternating clock phases. Because the 6502 only transfers data on the high phase (Phase 2), the PPU safely fetches graphics on the low phase (Phase 1). This grants the CPU 100% continuous logical access to VRAM with zero VBLANK lockout delays. System RAM remains entirely CPU-exclusive.
3. **Strict 2bpp:** 3 colors + transparency per draw unit, **8 palettes** (4 BG + 4 sprite), shared BG backdrop, **per-tile** BG palette select packed 4 tiles per attr byte (NES shares one select across a 2x2).
4. **Binary-first data:** Fixed-size layouts, with no dynamic allocation on target.
5. **Software collision:** AABB (or equivalent) in game code. No hardware sprite-vs-BG collision for gameplay. Beam timing uses a **raster compare / IRQ**, not NES sprite-0 hit.
6. **CHR from cartridge:** PPU reads pattern bytes from cart CHR-ROM (banked). VRAM holds live nametables/attrs only.

## 3. Shared capability snapshot

| Area | Spec |
|------|------|
| Resolution | 256x240 (32x30 x 8x8 tiles) |
| Color | 2bpp, 8 palettes, **per-tile** BG attrs (240 bytes/screen), **64-entry** master palette ([`retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt)) |
| Sprites | 64 OAM in **1284**, **16 / scanline** max, next-line line buffer |
| Worlds / screens | **8 worlds**, sparse grid up to **64 x 64**, **64 screens max** each |
| Banks / world | **4** (each 512 patterns: 256 BG + 256 sprites) |
| System RAM | **32 KB** CPU-only (`$0000-$7FFF`) |
| VRAM | **32 KB** interleaved, 4 x 2 KB nametable slots |
| Line buffer | Third **32 KB** SRAM (512 bytes used) |
| Cart | ~**2 MB** flash (PRG + CHR + MAP) |
| Input | **1 byte / player** (`$FE60`, `$FE61`) |
| Audio | ATmega328P, NES-style (2x pulse, triangle, noise, DMC) |
| CPU clock | **8.000 MHz** |
| Dot / frame | **5.369318 MHz**, 341x262, ~60.1 Hz |

Graphics: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md).  
Worlds: [04_worlds_and_screens.md](04_worlds_and_screens.md).  
Map: [08_memory_map.md](08_memory_map.md).

## 4. Near-term software focus

**Primary deliverable:** [16_simulation_and_bringup_plan.md](16_simulation_and_bringup_plan.md) — Digital circuit, then C emu rewrite. Schematic AIs are not the next step.

## 5. On-board memory (locked)

| Chip | Size | Role |
|------|------|------|
| System SRAM | 32 KB | Engine state, full chip at `$0000-$7FFF` |
| Video SRAM | 32 KB | Live nametables (up to 4), attrs, scratch, CPU<->PPU interleaved |
| Line-buffer SRAM | 32 KB chip, 512 B used | Sprite ping-pong (not OAM). OAM is in the 1284 |
| Cart flash | ~2 MB | PRG + CHR + MAP |
| Board EEPROM | 8 KB | High scores / operator settings (Retr01-A) |
