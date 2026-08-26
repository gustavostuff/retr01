# Retr01 Architecture Overview

## Sources of truth

When docs disagree, use this order.

| Concern | Authority | Notes |
|---------|-----------|-------|
| Software-visible behavior (CPU map, `$FExx` **logical** ports, cart image, worlds/VRAM/palettes) | [`02`](02_graphics_worlds_memory.md) | Current draft. Open bitfields/mailbox/I2C ports called out there |
| Locked decisions + open questions | [`05`](05_costs_and_open_questions.md) | Does not replace `02` for register text |
| Retr01-A **HW BOM** (current) | [`06`](06_hardware_v1_32ic.md) | **32 IC** system. Does not invent `$FExx` |
| Protoboard island bring-up | [`03`](03_hardware_implementation.md) | Bench checklist for the **32 IC** netlist |
| Studio Phase 1 (product) | [`retr01_studio/README.md`](../retr01_studio/README.md) | Authoring + Play + export. Short mirror: [`04`](04_retr01_studio.md) |
| Emulator Phase 1 | [`retr01_emu/README.md`](../retr01_emu/README.md) | Soft cart runtime matching Studio Play |
| Audio / APU protocol | [`09`](09_audio_architecture.md) | 6502 sequencer + 328P mixer; `$FE4x` bus bridge |
| Board IC simulator | [`08`](08_simulator.md) + [`retr01_sim/README.md`](../retr01_sim/README.md) | Pin/netlist models of the 32-IC BOM |
| IC pin/behavior detail | [`hw/md/`](../hw/md/) + datasheet PDFs | Sim and schematics |

**Current product board:** [`06`](06_hardware_v1_32ic.md), **32 ICs**, ~**12 x 12 cm** 4-layer THT.

```text
  Retr01-A (32 IC): roles:
  +------------------------------------------------------------------+
  | W65C02S          game CPU @ 8.000 MHz                            |
  | ATF22V10 x5      decode, interleave, beam X/Y, compositor        |
  | HC573 x9         packed $FExx latches                            |
  | HC157 x6         VRAM / line-buffer address mux                  |
  | HC245 x3         CPU / video / cart-OAM isolation                |
  | AS6C62256 x3     sys RAM, interleaved VRAM, sprite line buffer   |
  | SST39SF040       512 KB cart flash (PRG/CHR/MAP)                 |
  | AT28C16          Color PROM (6-bit index -> R3G3B2)              |
  | ATmega1284P      OAM / sprites / pads / machine EEPROM           |
  | ATmega328P       APU ($FE40-$FE5F)                               |
  | I2C EEPROM       cart game saves (in the 32)                     |
  +------------------------------------------------------------------+
```

This folder is the current architecture spec for **Retr01**.

Retr01 is a family of discrete-logic 2D machines that share one CPU model, one graphics model, one memory map, and one cartridge format across form factors.

## Scope

- **Retr01-A**: arcade motherboard, through-hole, first hardware target
- **Retr01-C**: home console, same architecture, different I/O shell
- **Retr01-H**: handheld, later SMD variant, same software contract

## Core principles

1. **Unified CPU**: W65C02S, planning **8.000 MHz**
2. **Interleaved VRAM only**: CPU and BG path share VRAM on alternating phases. System RAM is CPU-only
3. **CHR from cartridge**: tile art lives in cart CHR-ROM, not VRAM
4. **Software collision**: gameplay collision stays in game code, not hardware sprite-vs-BG hit logic
5. **Raster IRQ, not sprite-0**: mid-frame effects use scanline compare
6. **Binary-first data**: fixed layouts, no runtime allocation assumptions on target
7. **Master palette in Color PROM**: **64 indices** on the motherboard (packed **R3G3B2**), not in the cart
8. **Logical 128x120, fixed RGBS raster**: games use **16x15** screens. The RGBS path keeps a **256x240** active field. Board **SCALE** selects 1x or 2x mapping

## Canonical terminology

| Term | Meaning |
|------|---------|
| **World** | One cart chapter: sparse MAP atlas + **4 BG banks + 4 sprite banks** in CHR |
| **Screen** | One stored **16x15** tilemap (**128x120**) + **one attr byte per tile** in MAP-ROM |
| **Grid position** | One `(col, row)` coordinate in a world's sparse virtual grid |
| **Camera nametable slots** | VRAM slots **0-3**: the live 2x2 playfield field |
| **Plane nametable slots** | VRAM slots **4-5**: optional parallax-only storage |
| **BG bank** | **256 BG tiles**, arranged as a **16x16** tile grid, **4 KB** (CHR) |
| **Sprite bank** | **256 sprite tiles**, arranged as a **16x16** tile grid, **4 KB** (CHR) |
| **BG bank (runtime)** | Per **8x8 tile**: attr bits select CHR BG bank **0-3** (not per screen) |
| **Sprite bank (runtime)** | Per **OAM entry**: attr bits select CHR sprite bank **0-3** |
| **BG bank helper** | Optional `$FE31`-`$FE36` bulk stamp into slot attrs. Not the live fetch source |
| **Sprite bank helper** | Optional `$FE37` bulk stamp into OAM attrs. Not the live fetch source |
| **Global BG palettes** | Cart-wide store of **32 BG palettes** (**8 palette rows × 4 palettes**) |
| **Global sprite palettes** | Cart-wide store of **32 sprite palettes** (**8 palette rows × 4 palettes**) |
| **Palette row** | **4 palettes** in one plane, index **0-7**. BG row N and sprite row N are selected together |
| **Palette** | One 4-color set (**4 master indices** into the Color PROM) |
| **Active palette buffer** | **8 palettes** on screen: **4 BG + 4 sprite** from the currently selected palette row |
| **Color PROM** | Board-resident **64-entry** master palette (packed **R3G3B2** on current boards; not in cart) |
| **SCALE** | Board DIP: **2x** default (**128x120** -> **256x240**, fills CRT) or **1x** (centered **128x120**) |

## High-level hardware

Current chip list: [`06`](06_hardware_v1_32ic.md) (**32 IC**). Roles:

| Block | Role |
|------|------|
| **W65C02S** | Game logic, streaming, register writes |
| **BG / beam / compositor (PLD + HC157)** | Beam, VRAM fetch, BG pixels, priority mux, scale/border |
| **ATmega1284P** | OAM, sprite line-buffer fill, pads, **machine EEPROM** |
| **ATmega328P** | NES-style APU (`$FE40-$FE5F`) |
| **3x AS6C62256** | System RAM, VRAM, sprite line buffer |
| **Color PROM** | **1x** packed R3G3B2 (64 indices) -> DACs |
| **5x ATF22V10** | Decode, interleave, beam X/Y, compositor |
| **9x HC573 + 3x HC245** | `$FExx` latches + bus isolation |
| **Cart** | SST39SF040 flash + I2C save EEPROM |

## Shared capability snapshot

| Area | Spec |
|------|------|
| Logical resolution | **128x120** (**16x15** tiles, **16:15**) |
| RGBS active field | **256x240** (SCALE **2x** fills field, **1x** = centered 128x120) |
| Tile size | **8x8** |
| Color | **2bpp**, **64-entry Color PROM** on board (packed **R3G3B2**), cart holds **8 global BG rows + 8 global sprite rows** (indices only), **one synced row active** (4 BG + 4 sprite) |
| Worlds | **8** max |
| Screens per world | **32** max on sparse **8x8** virtual grid |
| Cart / PRG | **512 KB** flash. **32 KB** PRG (one region, no paging). ~**420 KB** full fill |
| CHR per world | **4 BG banks + 4 sprite banks**, **256 tiles each**, **32 KB** |
| Sprites | **64 OAM**, **16 per logical scanline** max |
| VRAM | **32 KB**, interleaved |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, **128 px**/half used |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Frame timing | **341x262**, about **60.098 Hz** |

## Variants

### Retr01-A

- Through-hole motherboard, **32 IC** system ([`06`](06_hardware_v1_32ic.md)), ~**12 x 12 cm** target
- RGBS + S-Video + composite pads
- **SCALE** DIP (**2x** default / **1x** optional)
- 20-pin IDC for cabinet controls
- 5 V barrel power
- Cart: 512 KB flash + I2C game-save EEPROM

### Retr01-C

- Same core architecture
- 3-wire controllers with **ATtiny85** (draft) in the pad -> `$FE60/$FE61`
- Same software contract

### Retr01-H

- Later SMD handheld
- Same memory map and cartridge model

## What stays stable for software

- RAM at `$0000-$7FFF`
- I/O page at `$FE00-$FEFF`
- PRG as **one global section** in the cart (**32 KB** max). It maps at `$8000` with the classic I/O hole at `$FE00-$FEFF` (see [`02`](02_graphics_worlds_memory.md)); not split per world.
- World/MAP streaming through `$FE90`
- BG banks per **8x8 tile** (attr `BANK`); screens may only stamp a default at load
- Sprite bank independent of BG banks (per OAM attr `BANK`; `$FE37` optional stamp)

## Near-term software focus

**Retr01 Studio** Phase 1 and **Retr01 Emulator** Phase 1 are the active tools. See [`retr01_studio/README.md`](../retr01_studio/README.md) and [`retr01_emu/README.md`](../retr01_emu/README.md). Later Studio/Emu phases are not documented until they are defined.

Studio Phase 1: PNG atlas import, Play preview, `.retr01` export. Emulator Phase 1: load that cart and run the same Play rules on host.

## Validation tools

- **Board IC simulator** ([`08`](08_simulator.md)): pin/netlist models of the 32-IC BOM, islands then full board
- **Software emulator** ([`retr01_emu/`](../retr01_emu/)): Phase 1 cart + Play parity with Studio

## Where to look next

- **Sources of truth:** table at top of this doc
- Product pitch and NES comparison: `07_pitch.md`
- Graphics, worlds, `$FExx`, cart image (software SoT): `02_graphics_worlds_memory.md`
- Current 32-IC HW BOM: `06_hardware_v1_32ic.md`
- Audio / APU bytecode + bus bridge: `09_audio_architecture.md`
- Protoboard islands: `03_hardware_implementation.md`
- Retr01 Studio Phase 1: `04_retr01_studio.md` / `retr01_studio/README.md`
- Locked decisions + open Qs: `05_costs_and_open_questions.md`
- Board simulator: `08_simulator.md`
- IC markdown notes: `hw/md/`
