# Retr01 Architecture Overview

This folder is the current architecture spec for **Retr01**.

Retr01 is a family of discrete-logic 2D machines that share one CPU model, one graphics model, one memory map, and one cartridge format across form factors.

## Scope

- **Retr01-A**: arcade motherboard, through-hole, first hardware target
- **Retr01-C**: home console, same architecture, different I/O shell
- **Retr01-H**: handheld, later SMD variant, same software contract

## Core principles

1. **Unified CPU**: W65C02S, planning **8.000 MHz**
2. **Interleaved VRAM only**: CPU and BG path share VRAM on alternating phases; system RAM is CPU-only
3. **CHR from cartridge**: tile art lives in cart CHR-ROM, not VRAM
4. **Software collision**: gameplay collision stays in game code, not hardware sprite-vs-BG hit logic
5. **Raster IRQ, not sprite-0**: mid-frame effects use scanline compare
6. **Binary-first data**: fixed layouts, no runtime allocation assumptions on target

## Canonical terminology

| Term | Meaning |
|------|---------|
| **World** | One cart chapter: sparse MAP atlas + **4 BG banks + 4 sprite banks** in CHR |
| **Screen** | One stored **32×30** tilemap (+ attrs) in MAP-ROM |
| **Grid position** | One `(col, row)` coordinate in a world's sparse virtual grid |
| **Camera nametable slots** | VRAM slots **0–3**: the live 2×2 playfield field |
| **Plane nametable slots** | VRAM slots **4–5**: optional parallax-only storage |
| **BG bank** | **256 BG tiles**, arranged as a **16×16** tile grid, **4 KB** |
| **Sprite bank** | **256 sprite tiles**, arranged as a **16×16** tile grid, **4 KB** |
| **BG bank latch** | Per-slot register selecting which BG bank CHR fetch uses |

## High-level hardware

| Block | Role |
|------|------|
| **W65C02S** | Game logic, streaming, register writes |
| **74HC BG path** | Beam counters, VRAM fetch, BG compositing |
| **ATmega1284P** | OAM storage, sprite evaluation, sprite line-buffer fill, controller bytes |
| **ATmega328P** | NES-style APU |
| **3× AS6C62256** | System RAM, VRAM, sprite line buffer |
| **ATF22V10 + 74HC glue** | Decode, timing, muxing, chip enables |

## Shared capability snapshot

| Area | Spec |
|------|------|
| Resolution | **256×240** |
| Tile size | **8×8** |
| Color | **2bpp**, **8 palettes** total (4 BG + 4 sprite) |
| Worlds | **8** max |
| Screens per world | **64** max on sparse **16×16** virtual grid |
| CHR per world | **4 BG banks + 4 sprite banks**, **256 tiles each** |
| Sprites | **64 OAM**, **16 per scanline** max |
| VRAM | **32 KB**, interleaved |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, **512 bytes** used |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Frame timing | **341×262**, about **60.098 Hz** |

## Variants

### Retr01-A

- Through-hole motherboard
- RGBS + S-Video + composite pads
- 20-pin IDC for cabinet controls
- 5 V barrel power

### Retr01-C

- Same core architecture
- 3-wire controllers with MCU in the pad
- Same `$FE60/$FE61` software contract

### Retr01-H

- Later SMD handheld
- Same memory map and cartridge model

## What stays stable for software

- RAM at `$0000-$7FFF`
- I/O page at `$FE00-$FEFF`
- PRG elsewhere, banked only through `$FE80`
- World/MAP streaming through `$FE90`
- BG banks per nametable slot
- Sprite bank independent of BG banks

## Where to look next

- Graphics, worlds, scrolling, banks, VRAM, and MAP: `02_graphics_worlds_memory.md`
- Board structure, buses, chips, and timing: `03_hardware_implementation.md`
- Emulator and bring-up path: `04_emulation_and_bringup.md`
- Costs and unresolved items: `05_costs_and_open_questions.md`
