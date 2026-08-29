# Retr01 Architecture Overview

## Sources of truth

When docs disagree, use this order.

| Concern | Authority | Notes |
|---------|-----------|-------|
| Software-visible behavior (CPU map, `$FExx` **logical** ports, cart image, worlds/VRAM/palettes) | [`02`](02_graphics_worlds_memory.md) | Current draft. Open bitfields/mailbox/I2C ports called out there |
| Locked decisions + open questions | [`04`](04_costs_and_open_questions.md) | Does not replace `02` for register text |
| Retr01-A **HW BOM** (current) | [`05`](05_hardware_v1_32ic.md) | **32 IC** system. Does not invent `$FExx` |
| Protoboard island bring-up | [`03`](03_hardware_implementation.md) | Bench checklist for the **32 IC** netlist |
| Studio Phase 4 (product) | [`retr01_studio/README.md`](../retr01_studio/README.md) | Authoring + Play + export. Marked player, names/ids, JSON v7, cart `format_ver` 2 |
| Emulator Phase 1 | [`retr01_emu/README.md`](../retr01_emu/README.md) | Soft cart runtime matching Studio Play |
| Audio / APU protocol | [`06`](06_audio_architecture.md) | 6502 sequencer + 328P mixer, `$FE4x` bus bridge |
| Board IC simulator | [`retr01_sim/README.md`](../retr01_sim/README.md) | Pin/netlist models of the 32-IC BOM |
| Studio game modules (movement, camera, entities, collision budgets) | [`07`](07_game_modules.md) | Attachable gameplay profiles. Studio phases implement subsets later |
| IC pin/behavior detail | [`hw/md/`](../hw/md/) + datasheet PDFs | Sim and schematics |

**Current product board:** [`05`](05_hardware_v1_32ic.md), **32 ICs**, ~**12x12 cm** 4-layer THT (chip roles and netlist there).

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
| **Plane nametable slots** | VRAM slots **4-5**: live parallax storage (**2** slots). Cart may hold up to **8** parallax screens/world. Optional **slices** (**1..120** bands, variable thickness, H or V offsets) bend the plane without moving playfield camera (curved roads in racing games) |
| **BG bank** | **256 BG tiles**, arranged as a **16x16** tile grid, **4 KB** (CHR) |
| **Sprite bank** | **256 sprite tiles**, arranged as a **16x16** tile grid, **4 KB** (CHR) |
| **BG bank (runtime)** | Per **8x8 tile**: attr bits select CHR BG bank **0-3** (not per screen) |
| **Sprite bank (runtime)** | Per **OAM entry**: attr bits select CHR sprite bank **0-3** |
| **BG bank helper** | Optional `$FE31`-`$FE36` bulk stamp into slot attrs. Not the live fetch source |
| **Sprite bank helper** | Optional `$FE37` bulk stamp into OAM attrs. Not the live fetch source |
| **Global BG palettes** | Cart-wide store of **32 BG palettes** (**8 palette rows x 4 palettes**) |
| **Global sprite palettes** | Cart-wide store of **32 sprite palettes** (**8 palette rows x 4 palettes**) |
| **Palette row** | **4 palettes** in one plane, index **0-7**. BG row N and sprite row N are selected together |
| **Palette** | One 4-color set (**4 master indices** into the Color PROM) |
| **Active palette buffer** | **8 palettes** on screen: **4 BG + 4 sprite** from the currently selected palette row |
| **Color PROM** | Board-resident **64-entry** master palette (packed **R3G3B2** on current boards, not in cart) |
| **SCALE** | Board DIP: **2x** default (**128x120** -> **256x240**, fills CRT) or **1x** (centered **128x120**) |

## High-level hardware

Current chip list: [`05`](05_hardware_v1_32ic.md) (**32 IC**). Roles:

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
| Worlds | **8** max (indices 0-7) |
| Screens per world | **32 present** max on sparse **8x8** virtual grid (playfield only). **0..8** parallax screens/world. Optional **1..120** plane slices (variable thickness) |
| Global UI ROM | **Other screens** (max **48**): title (**0**), interstitial (**1**), credits pages (**2+**, **0..46**). Payloads raw **480 B** or **RLE**. PRG owns scroll/fade/hold. See [`02`](02_graphics_worlds_memory.md#other-screens-global-rom) |
| Cart / PRG | **512 KB** flash. Cart `format_ver` **2** only (8 worlds, other screens). **32 KB PRG** (no banking). Full caps (incl. **8** parallax/world) fill **~442 KB**. **~70 KB** free |
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

- Through-hole motherboard, **32 IC** system ([`05`](05_hardware_v1_32ic.md)), ~**12 x 12 cm** target
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
- PRG as **one global 32 KB** section in the cart at `$8000` (I/O hole at `$FE00-$FEFF`). No PRG banking. See [`02`](02_graphics_worlds_memory.md). Not split per world.
- World/MAP streaming through `$FE90`
- BG banks per **8x8 tile** (attr `BANK`). Screens may only stamp a default at load
- Sprite bank independent of BG banks (per OAM attr `BANK`, `$FE37` optional stamp)

## Near-term software focus

**Retr01 Studio** (Phase 2 authoring) and **Retr01 Emulator** (Phase 1 cart runtime) are the active tools. See [`retr01_studio/README.md`](../retr01_studio/README.md) and [`retr01_emu/README.md`](../retr01_emu/README.md). **Board IC simulator** ([`retr01_sim/`](../retr01_sim/)) validates the 32-IC netlist.

## Doc index (`docs/`)

| # | File | Topic |
|---|------|--------|
| 01 | `01_architecture_overview.md` | This file. Sources of truth, terminology |
| 02 | `02_graphics_worlds_memory.md` | Software SoT: VRAM, cart, `$FExx` |
| 03 | `03_hardware_implementation.md` | Protoboard island bring-up |
| 04 | `04_costs_and_open_questions.md` | Locked decisions, open questions |
| 05 | `05_hardware_v1_32ic.md` | 32-IC BOM |
| 06 | `06_audio_architecture.md` | APU / bytecode |
| 07 | `07_game_modules.md` | Game module contract + budgets |
