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
| **BG bank** | **256 BG tiles**, arranged as a **16×16** tile grid, **4 KB** (CHR) |
| **Sprite bank** | **256 sprite tiles**, arranged as a **16×16** tile grid, **4 KB** (CHR) |
| **BG bank latch** | Per-slot register selecting which CHR BG bank fetch uses |
| **BG palette bank** | Cartridge store of up to **32 BG palettes** (**8 palette rows x 4 palettes**) |
| **Sprite palette bank** | Cartridge store of up to **32 sprite palettes** (**8 palette rows x 4 palettes**) |
| **Palette row** | **4 palettes** in one plane; index **0-7**; BG row N and sprite row N are selected together |
| **Palette** | One 4-color set (**4 master indices**) |
| **Active palette buffer** | **8 palettes** on screen: **4 BG + 4 sprite** from the currently selected palette row |

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
| Color | **2bpp**, **64-color master palette**, **BG/sprite palette banks** (8 rows x 4 palettes each, sparse), **one synced palette row active** (4 BG + 4 sprite) |
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

## Near-term software focus

**Retr01 Studio** is the only tool in active development. See `04_retr01_studio.md`.

Studio authors worlds, screens, palettes, and CHR, then compiles them into a `.retr01` cart image.

## Future software (planned)

A **low-level hardware emulator** is planned later, not built now. It should simulate cycles, memory decode, and `$FExx` hardware behavior faithfully enough to validate carts before silicon exists.

## Where to look next

- Graphics, worlds, scrolling, banks, VRAM, and MAP: `02_graphics_worlds_memory.md`
- Board structure, buses, chips, and timing: `03_hardware_implementation.md`
- Retr01 Studio (current tool): `04_retr01_studio.md`
- Costs and unresolved items: `05_costs_and_open_questions.md`
