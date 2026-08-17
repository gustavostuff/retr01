# Retr01 System Overview

## 1. Vision

Retr01 is a modular 2D hardware ecosystem: discrete-logic 8-bit machines that share one execution model, one graphics contract, and one cartridge layout across form factors.

Form factors (A / C / H): [03_hardware_variants.md](03_hardware_variants.md).

Formerly GameNerd / Retr02 - those names are retired.

## 2. Core principles

1. **Unified CPU:** W65C02S on every variant.
2. **Interleaved VRAM only:** CPU and PPU share the **video SRAM** on alternating clock phases. System RAM is CPU-exclusive.
3. **Strict 2bpp:** 3 colors + transparency per BG tile / sprite draw unit.
4. **Binary-first data:** Fixed-size layouts; no dynamic allocation on target.
5. **Software collision:** AABB (or equivalent) in game code - no hardware collision flags.
6. **CHR from cartridge:** PPU reads pattern bytes from cart CHR-ROM (banked); VRAM holds live nametables/attrs/OAM-related state, not a full CHR copy.

## 3. Shared capability snapshot

| Area | Spec |
|------|------|
| Resolution | 256x240 (32x30 x 8x8 tiles) |
| Color | 2bpp; **per-tile** BG palette select |
| Sprites | 64 OAM; **16 / scanline** max |
| Worlds / screens | **8 x 64** |
| Banks / world | **4** (each 512 patterns: 256 BG + 256 sprites) |
| System RAM | **32 KB** CPU-only |
| VRAM | **32 KB** interleaved; sized for **4 live nametables** + attrs + headroom |
| Cart | ~**2 MB** flash class (PRG + CHR + compressed maps) |
| Audio | ATmega, **NES-style** (2x pulse, triangle, noise, DMC) |

Graphics detail: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md).  
CPU/VRAM map: [08_memory_map.md](08_memory_map.md).

## 4. Near-term software focus

**Primary deliverable:** a hardware-faithful **low-level emulator written in C** that enforces interleaved VRAM phases, GAL-style decode, sprite caps, and the memory map.

Asset editors and 6502 game toolchains (PPUX, cc65, Rust->6502) are **out of scope for now**; the emulator can ingest hand-built or script-generated ROM images for bring-up.

## 5. On-board memory (locked)

| Chip | Size | Role |
|------|------|------|
| System SRAM | 32 KB | Engine state; CPU-only |
| Video SRAM | 32 KB | Live nametables (up to 4), attributes, scratch; **CPU<->PPU interleaved** |
| Cart flash | ~2 MB | PRG + CHR + MAP |
| Board EEPROM | small | High scores / operator settings (Retr01-A) |
