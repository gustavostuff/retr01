# Retr01 Architecture Overview

## Sources of truth

When docs disagree, use this order.

| Concern | Authority | Notes |
|---------|-----------|-------|
| Software-visible behavior (CPU map, `$FExx` **logical** ports, cart image, worlds/VRAM/palettes) | [`02`](02_graphics_worlds_memory.md) | Current draft. Open bitfields/mailbox/I2C ports called out there |
| Locked decisions + open questions | [`05`](05_costs_and_open_questions.md) | Does not replace `02` for register text |
| Retr01-A **HW BOM** (current) | [`06`](06_hardware_v1_32ic.md) | **32 IC** system. Does not invent `$FExx` |
| Optional discrete islands / legacy ~52 sketch | [`03`](03_hardware_implementation.md) | Bench fallback; full ~52 architecture also on `main` |
| Studio data model / phases | [`04`](04_retr01_studio.md) | Follows `02` |
| IC pin/behavior detail | [`hw/md/`](../hw/md/) + datasheet PDFs | Sim and schematics |

**Current product board:** [`06`](06_hardware_v1_32ic.md) -- **32 ICs**, ~**12 x 12 cm**. Diagram below is the **legacy ~52 IC** planning sketch (also on `main`) for orientation only.

**Legacy ~52 layout (not to scale).** Historical planning envelope ~160 x 220 mm. Each box is one IC; box width tracks DIP pin count.

```text
  NORTH (I/O edge - cabinet / bench)   ~160 x 220 mm planning envelope (not to scale)
  +----------------------------------------------------------------------------------------------------------+
  | [5V IN] (o) PWR LED     +-------------------- IDC-20 CABINET --------------------+     [ISP 6-pin]       |
  |                         | START COIN P1 P2 ... -> ATmega1284P pad glue            |     SCALE DIP 2x/1x  |
  +-------------------------+---------------------------------------------------------+----------------------+
  |                                                                                                          |
  |  +-- CPU + PLD DECODE ------------------------------------+  +-- SRAM TRIO (AS6C62256 x3) ------------+  |
  |  |                                                        |  |                                        |  |
  |  |  +------------------------+                            |  |  +---------------+  +---------------+  |  |
  |  |  |      W65C02S    [40]   |   8.000 MHz PHI2           |  |  | SYS RAM [28]  |  |  VRAM  [28]   |  |  |
  |  |  |      game CPU          |                            |  |  | AS6C62256     |  | AS6C62256     |  |  |
  |  |  +------------------------+                            |  |  | $0000-$7FFF   |  | interleaved   |  |  |
  |  |                                                        |  |  +---------------+  +---------------+  |  |
  |  |  +-------------+ +-------------+ +-------------+       |  |                                        |  |
  |  |  | ATF22V10    | | ATF22V10    | | ATF22V10    |       |  |         +---------------+              |  |
  |  |  | DEC   [24]  | | TIM   [24]  | | PPU   [24]  |       |  |         | LINEBUF [28]  |              |  |
  |  |  +-------------+ +-------------+ +-------------+       |  |         | AS6C62256     |              |  |
  |  |                                                        |  |         | sprite ping   |              |  |
  |  |  +-----------+ +-----------+ +-----------+             |  |         +---------------+              |  |
  |  |  | HC573[20] | | HC573[20] | | HC688[20] | raster cmp  |  +----------------------------------------+  |
  |  |  | decode/LE | | wrap/hit  | | compare   |             |                                              |
  |  |  +-----------+ +-----------+ +-----------+             |  +-- MCU DOMAIN --------------------------+  |
  |  +--------------------------------------------------------+  |                                        |  |
  |                                                              |  +------------------------+            |  |
  |  +-- VRAM INTERLEAVE + CPU BUS ---------------------------+  |  |   ATmega1284P   [40]   | 20 MHz     |  |
  |  |                                                        |  |  |   OAM / sprites / pads |            |  |
  |  |  addr mux CPU-phase vs BG-fetch (HC157 x4):            |  |  +------------------------+            |  |
  |  |  +--------+ +--------+ +--------+ +--------+           |  |                                        |  |
  |  |  |HC157   | |HC157   | |HC157   | |HC157   |           |  |  +-----------------+                   |  |
  |  |  |[16]    | |[16]    | |[16]    | |[16]    |           |  |  | ATmega328P [28] | 16 MHz APU        |  |
  |  |  +--------+ +--------+ +--------+ +--------+           |  |  +-----------------+                   |  |
  |  |                                                        |  |                                        |  |
  |  |  +-----------+ +-----------+ +-----------+             |  |  line-buffer addr mux (HC157 x2):      |  |
  |  |  | HC245[20] | | HC573[20] | | HC573[20] |             |  |  +--------+ +--------+                 |  |
  |  |  | CPU bus   | | VRAM addr | | VRAM data  |            |  |  |HC157   | |HC157   | 1284 vs beam    |  |
  |  |  +-----------+ +-----------+ +-----------+             |  |  |[16]    | |[16]    |                 |  |
  |  +--------------------------------------------------------+  |  +--------+ +--------+                 |  |
  |                                                              +----------------------------------------+  |
  |                                                                                                          |
  |  +-- BG BEAM + $FExx LATCHES + GLUE ------------------------------------------------------------------+  |
  |  |                                                                                                    |  |
  |  |  beam X/Y counters (HC161 x4) -> 341x262 @ ~5.37 MHz dot                                           |  |
  |  |  +-------+ +-------+ +-------+ +-------+                                                           |  |
  |  |  |HC161  | |HC161  | |HC161  | |HC161  |                                                           |  |
  |  |  |[14]   | |[14]   | |[14]   | |[14]   |                                                           |  |
  |  |  +-------+ +-------+ +-------+ +-------+                                                           |  |
  |  |                                                                                                    |  |
  |  |  $FExx latches HC573 (14 total in legend; boxes below are representative)                          |  |
  |  |  +-----------+ +-----------+ +-----------+ +-----------+                                           |  |
  |  |  | HC573[20] | | HC573[20] | | HC573[20] | | HC573[20] |                                           |  |
  |  |  +-----------+ +-----------+ +-----------+ +-----------+                                           |  |
  |  |  +-----------+ +-----------+ +-----------+ +-----------+                                           |  |
  |  |  | HC573[20] | | HC573[20] | | HC573[20] | | HC573[20] |                                           |  |
  |  |  +-----------+ +-----------+ +-----------+ +-----------+                                           |  |
  |  |                                                                                                    |  |
  |  |  more $FExx / path latches (HC573) + OAM path                                                      |  |
  |  |  +-----------+ +-----------+ +-----------+                                                         |  |
  |  |  | HC573[20] | | HC573[20] | | HC245[20] |                                                         |  |
  |  |  | (latches) | | (latches) | | OAM path  |                                                         |  |
  |  |  +-----------+ +-----------+ +-----------+                                                         |  |
  |  |                                                                                                    |  |
  |  |  glue + reset (DIP-14)                                                                             |  |
  |  |  +-------+ +-------+ +-------+ +-------+ +-------+ +-------+ +-------+ +-------+ +-------+         |  |
  |  |  |HC14   | |HC00   | |HC00   | |HC04   | |HC04   | |HC08   | |HC08   | |HC32   | |HC32   |         |  |
  |  |  |[14]rst| |[14]   | |[14]   | |[14]   | |[14]   | |[14]   | |[14]   | |[14]   | |[14]   |         |  |
  |  |  +-------+ +-------+ +-------+ +-------+ +-------+ +-------+ +-------+ +-------+ +-------+         |  |
  |  |  +-------+                                                                                         |  |
  |  |  |HC86   |                                                                                         |  |
  |  |  |[14]   |                                                                                         |  |
  |  |  +-------+                                                                                         |  |
  |  +----------------------------------------------------------------------------------------------------+  |
  |                                                                                                          |
  |  +-- CART + BOARD EEPROM ---------------------+  +-- VIDEO OUT (Color PROM + pads) -------------------+  |
  |  |                                            |  |                                                    |  |
  |  |  +-------------------+                     |  |  +-------------+ +-------------+ +-------------+   |  |
  |  |  | SST39SF040  [32]  |  512 KB NOR         |  |  | AT28C16[24] | | AT28C16[24] | | AT28C16[24] |   |  |
  |  |  | PRG/CHR/MAP gated |  v0 on-board socket |  |  | Color PROM R| | Color PROM G| | Color PROM B|   |  |
  |  |  +-------------------+                     |  |  +-------------+ +-------------+ +-------------+   |  |
  |  |                                            |  |         |               |               |          |  |
  |  |  +---------------+  +-----------+          |  |      R-2R+75R        R-2R+75R        R-2R+75R      |  |
  |  |  | AT28C64B [28] |  | HC245[20] |          |  |         +---------------+---------------+          |  |
  |  |  | board EEPROM  |  | cart isol |          |  |         J RGBS / S-VIDEO / COMPOSITE pads          |  |
  |  |  +---------------+  +-----------+          |  |                                                    |  |
  |  |                                            |  +----------------------------------------------------+  |
  |  |  +-- CART EDGE (planning) ---------------+ |                                                          |
  |  |  | 32-pin ROM socket / future cart IDC   | |                                                          |
  |  |  +---------------------------------------+ |                                                          |
  |  +--------------------------------------------+                                                          |
  |                                                                                                          |
  SOUTH (component side - tallest sockets ~15 mm clearance below board)                                      |
  +----------------------------------------------------------------------------------------------------------+

  LEGEND (legacy ~52 planning count - see 03; current BOM is 06)
  Box width ~ pin count (DIP-40 widest, DIP-14 narrowest). Each box = one IC.

  Package | Count | Parts
  --------+-------+------------------------------------------------------------------
  [40]    |   2   | W65C02S, ATmega1284P
  [32]    |   1   | SST39SF040 cart flash (512 KB)
  [28]    |   5   | AS6C62256 x3, ATmega328P, AT28C64B
  [24]    |   6   | ATF22V10 x3 (DEC/TIM/PPU), AT28C16 x3 (Color PROM R/G/B)
  [20]    |  18   | HC573 x14, HC245 x3, HC688 x1
  [16]    |   6   | HC157 x4 (VRAM mux) + HC157 x2 (line-buffer mux)
  [14]    |  14   | HC161 x4, HC14 x1, HC00 x2, HC04 x2, HC08 x2, HC32 x2, HC86 x1
  --------+-------+------------------------------------------------------------------
          |  52   | legacy sketch (optional +1 ATF22V10). Current: 32 IC in 06
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
| **BG palette bank** | Cartridge store of up to **32 BG palettes** (**8 palette rows x 4 palettes**) |
| **Sprite palette bank** | Cartridge store of up to **32 sprite palettes** (**8 palette rows x 4 palettes**) |
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
| Color | **2bpp**, **64-entry Color PROM** on board (packed **R3G3B2**), cart holds indices only, **one synced palette row active** (4 BG + 4 sprite) |
| Worlds | **8** max |
| Screens per world | **32** max on sparse **8x8** virtual grid |
| Cart / PRG | **512 KB** flash. **32 KB** PRG (one region, no paging). ~**414 KB** full fill |
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

- Through-hole motherboard, **~32 IC** system ([`06`](06_hardware_v1_32ic.md)), ~**12 x 12 cm** target
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

**Retr01 Studio** is the only tool in active development. See `04_retr01_studio.md` for UI layout, Phase 1 scope, and roadmap.

Studio authors worlds, screens, palettes, and CHR. Full `.retr01` cart compile (PRG + CHR + MAP) lands in later Studio phases - see `04_retr01_studio.md`.

## Future software (planned)

Two validation tracks (not Studio):

- **Board IC simulator** ([`08`](08_simulator.md)) -- pin/netlist models of the 32-IC BOM; islands then full board
- **Optional later cycle-level cart check** -- tighter `$FExx` / timing fidelity for game images (may share code with the board sim or stay separate; not started)

## Where to look next

- **Sources of truth:** table at top of this doc
- Product pitch and NES comparison: `07_pitch.md`
- Graphics, worlds, `$FExx`, cart image (software SoT): `02_graphics_worlds_memory.md`
- Current 32-IC HW BOM: `06_hardware_v1_32ic.md`
- Optional islands / legacy ~52: `03_hardware_implementation.md`
- Retr01 Studio: `04_retr01_studio.md`
- Locked decisions + open Qs: `05_costs_and_open_questions.md`
- Board simulator: `08_simulator.md`
- IC markdown notes: `hw/md/`
