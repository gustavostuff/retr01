# Retr01 Architecture Overview

**Retr01-A motherboard - estimated PCB layout (not to scale).** 
52 through-hole ICs, ~160 x 220 mm planning envelope, DIP widths shown as pin count. 
Detail: [`03_hardware_implementation.md`](03_hardware_implementation.md).

```text
  NORTH (I/O edge — cabinet/bench)
  ┌───────────────────────────────────────────────────────────────────────────────────────┐
  │ [5V IN]  (o) PWR LED   ┌──────────────── IDC-20 CABINET ────────────────┐  [ISP 6]    │
  │      │                 │ START COIN P1-P2 … (to 1284 pad glue)          │  328P APU   │
  ├──────┴─────────────────┴────────────────────────────────────────────────┴─────────────┤
  │                                                                                       │
  │   ┌─ CPU + DECODE ──────────────────────┐    ┌─ SRAM TRIO (32 KB each) ─────────────┐ │
  │   │                                     │    │                                      │ │
  │   │   ┌────────────────────────────┐    │    │  ┌──────────────┐  ┌──────────────┐  │ │
  │   │   │      W65C02S      [40]     │    │    │  │  SYS RAM     │  │    VRAM      │  │ │
  │   │   │      8 MHz PHI2            │    │    │  │  62256 [28]  │  │  62256 [28]  │  │ │
  │   │   └────────────────────────────┘    │    │  └──────────────┘  └──────────────┘  │ │
  │   │                                     │    │                                      │ │
  │   │  ┌─────────┐ ┌─────────┐ ┌─────────┐│    │         ┌──────────────┐             │ │
  │   │  │ ATF22V10│ │ ATF22V10│ │ ATF22V10││    │         │  LINE BUFFER │             │ │
  │   │  │ GAL-DEC │ │ GAL-TIM │ │ GAL-PPU ││    │         │  62256  [28] │             │ │
  │   │  │   [24]  │ │   [24]  │ │   [24]  ││    │         └──────────────┘             │ │
  │   │  └─────────┘ └─────────┘ └─────────┘│    └──────────────────────────────────────┘ │
  │   │                                     │                                             │
  │   │  ┌──────┐ ┌──────┐  HC688 [20]      │    ┌─ MCU DOMAIN ─────────────────────────┐ │
  │   │  │HC573 │ │HC573 │  compare/wrap    │    │                                      │ │
  │   │  │ [20] │ │ [20] │  (beam + decode) │    │  ┌────────────────────────────┐      │ │
  │   │  └──────┘ └──────┘                  │    │  │   ATmega1284P      [40]    │      │ │
  │   └─────────────────────────────────────┘    │  │   sprites · OAM · pads     │      │ │
  │                                              │  └────────────────────────────┘      │ │
  │   ┌─ VRAM INTERLEAVE + BUS ────────────────┐ │                                      │ │
  │   │  HC157 HC157 HC157 HC157   [16] ×4     │ │  ┌──────────────┐  20 MHz xtal       │ │
  │   │  addr mux (CPU latched vs BG fetch)    │ │  │ ATmega328P   │  16 MHz xtal       │ │
  │   │  ┌──────────┐  HC573 [20] latch        │ │  │ APU    [28]  │                    │ │
  │   │  │ HC245    │  HC573 [20] data path    │ │  └──────────────┘                    │ │
  │   │  │ trcv [20]│                          │ └──────────────────────────────────────┘ │
  │   │  └──────────┘                          │                                          │
  │   └────────────────────────────────────────┘                                          │
  │                                                                                       │
  │   ┌─ BG BEAM + FETCH GLUE ────────────────────────────────────────────────────────┐   │
  │   │  HC161 HC161 HC161 HC161  [16] ×4  → 341×262 dot clock ~5.37 MHz              │   │
  │   │                                                                               │   │
  │   │  HC573×6  scroll · slot banks · MAP addr  [20]     HC14 [14] reset Schmitt    │   │
  │   │  HC573    palette/compositor latches     [20]     HC00·04·08·32·86  [14] glue │   │
  │   └───────────────────────────────────────────────────────────────────────────────┘   │
  │                                                                                       │
  │   ┌─ CART + CONFIG ──────────────────────┐   ┌─ VIDEO OUT (analog pads) ──────────┐   │
  │   │  ┌────────────────────────────┐      │   │  Color PROM AT28C16 x3 + R-2R      │   │
  │   │  │  SST39SF040 parallel  [32] │      │   │  ┌────┐ ┌────┐ ┌────┐  J RGBS      │   │
  │   │  │  PRG · CHR · MAP (gated)   │      │   │  │ 75Ω│ │ 75Ω│ │ 75Ω│  J S-VIDEO   │   │
  │   │  └────────────────────────────┘      │   │  └────┘ └────┘ └────┘  J COMPOSITE │   │
  │   │  ┌──────────────┐  HC245 [20]        │   └────────────────────────────────────┘   │
  │   │  │ AT28C64B     │  cart data isol    │                                            │
  │   │  │ board E2 [28]│                    │   HC157x2 [16]  line-buffer addr mux       │
  │   │  └──────────────┘                    │   (1284 writer vs beam reader)             │
  │   │         ┌── CART EDGE (planning) ────┤                                            │
  │   │         │ 32-pin ROM socket/IDC      │                                            │
  │   └─────────┴────────────────────────────┘                                            │
  │                                                                                       │
  SOUTH (component side — tallest sockets ~15 mm clearance below board)                   │
  └───────────────────────────────────────────────────────────────────────────────────────┘

  LEGEND (planning totals — see 03_hardware_implementation.md)
  ───────────────────────────────────────────────────────────
  Package │ Count │ Parts
  ────────┼───────┼────────────────────────────────────────────────────────────
  [40]    │   2   │ W65C02S, ATmega1284P
  [32]    │   1   │ SST39SF040 (v0 on-board cart-flash socket, moves to cart later)
  [28]    │   5   │ 3x AS6C62256, ATmega328P, AT28C64B
  [24]    │   6   │ 3x ATF22V10 (decode/timing/PPU) + 3x AT28C16 Color PROM (R/G/B)
  [20]    │  ~18  │ HC573 latches, HC245 transceivers, HC688 compare
  [16]    │   6   │ HC157 mux (4x VRAM + 2x line buffer)
  [14]    │  ~14  │ HC161x4, HC14, HC00/04/08/32/86 glue
  ────────┼───────┼────────────────────────────────────────────────────────────
          │  52   │ motherboard ICs (v0 frozen plan, +4th PLD if equations overflow)
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
7. **Master palette in Color PROM**: 64 RGB colors live on the motherboard (AT28C16), not in the cart
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
| **BG bank helper** | Optional `$FE31`-`$FE36` bulk stamp into slot attrs; not the live fetch source |
| **BG palette bank** | Cartridge store of up to **32 BG palettes** (**8 palette rows x 4 palettes**) |
| **Sprite palette bank** | Cartridge store of up to **32 sprite palettes** (**8 palette rows x 4 palettes**) |
| **Palette row** | **4 palettes** in one plane, index **0-7**. BG row N and sprite row N are selected together |
| **Palette** | One 4-color set (**4 master indices** into the Color PROM) |
| **Active palette buffer** | **8 palettes** on screen: **4 BG + 4 sprite** from the currently selected palette row |
| **Color PROM** | Board-resident **64-color** master RGB table (not in cart) |
| **SCALE** | Board DIP: **2x** default (**128x120** -> **256x240**, fills CRT) or **1x** (centered **128x120**) |

## High-level hardware

| Block | Role |
|------|------|
| **W65C02S** | Game logic, streaming, register writes |
| **74HC BG path** | Beam counters, VRAM fetch, BG compositing, scale/border |
| **ATmega1284P** | OAM storage, sprite evaluation, sprite line-buffer fill, controller bytes |
| **ATmega328P** | NES-style APU |
| **3x AS6C62256** | System RAM, VRAM, sprite line buffer |
| **3x AT28C16** | Color PROM (R/G/B master palette -> DACs) |
| **ATF22V10 + 74HC glue** | Decode, timing, muxing, chip enables |

## Shared capability snapshot

| Area | Spec |
|------|------|
| Logical resolution | **128x120** (**16x15** tiles, **16:15**) |
| RGBS active field | **256x240** (SCALE **2x** fills field; **1x** = centered 128x120) |
| Tile size | **8x8** |
| Color | **2bpp**, **64-color Color PROM** on board, **BG/sprite palette banks** in cart (indices only), **one synced palette row active** (4 BG + 4 sprite) |
| Worlds | **16** max |
| Screens per world | **64** max on sparse **16x16** virtual grid |
| CHR per world | **4 BG banks + 4 sprite banks**, **256 tiles each** |
| Sprites | **64 OAM**, **16 per logical scanline** max |
| VRAM | **32 KB**, interleaved |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, **128 px**/half used |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Frame timing | **341x262**, about **60.098 Hz** |

## Variants

### Retr01-A

- Through-hole motherboard
- RGBS + S-Video + composite pads
- **SCALE** DIP (1x default / 2x)
- 20-pin IDC for cabinet controls
- 5 V barrel power

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
- PRG elsewhere, banked only through `$FE80`
- World/MAP streaming through `$FE90`
- BG banks per **8x8 tile** (attr `BANK`); screens may only stamp a default at load
- Sprite bank independent of BG banks (global `$FE37`)

## Near-term software focus

**Retr01 Studio** is the only tool in active development. See `04_retr01_studio.md` for UI layout, game constraints, phased delivery, ASM optimization pipeline, and testing policy.

Studio authors worlds, screens, palettes, and CHR. Full `.retr01` cart compile (PRG + CHR + MAP) lands in later Studio phases - see `04_retr01_studio.md`.

## Future software (planned)

A **low-level hardware emulator** is planned later, not built now. It should simulate cycles, memory decode, and `$FExx` hardware behavior faithfully enough to validate carts before silicon exists.

## Where to look next

- Product pitch and NES comparison: `07_pitch.md`
- Graphics, worlds, scrolling, banks, VRAM, MAP, BG attrs: `02_graphics_worlds_memory.md`
- Board structure, buses, chips, and timing: `03_hardware_implementation.md` (includes **sprite line buffer**)
- Protoboard module tests (island bring-up): `06_protoboard_module_tests.md`
- Retr01 Studio (current tool): `04_retr01_studio.md`
- Costs and unresolved items: `05_costs_and_open_questions.md`
