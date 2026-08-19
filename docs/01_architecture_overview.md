# Retr01 Architecture Overview

**Retr01-A motherboard — estimated PCB layout (not to scale).**  
49 through-hole ICs · ~160 × 220 mm planning envelope · DIP widths shown as pin count.  
Detail: [`03_hardware_implementation.md`](03_hardware_implementation.md).

```text
  NORTH (I/O edge — cabinet / bench)
  ┌──────────────────────────────────────────────────────────────────────────────────────┐
  │ [5V IN]  (o) PWR LED   ┌──────────────── IDC-20 CABINET ────────────────┐  [ISP 6]   │
  │      │                 │ START COIN P1-P2 … (to 1284 pad glue)          │  328P APU  │
  ├──────┴─────────────────┴────────────────────────────────────────────────┴────────────┤
  │                                                                                      │
  │   ┌─ CPU + DECODE ─────────────────────┐    ┌─ SRAM TRIO (32 KB each) ─────────────┐ │
  │   │                                    │    │                                      │ │
  │   │   ┌────────────────────────────┐   │    │  ┌──────────────┐  ┌──────────────┐  │ │
  │   │   │      W65C02S      [40]   │   │    │  │  SYS RAM     │  │    VRAM      │  │ │
  │   │   │      8 MHz PHI2          │   │    │  │  62256 [28]  │  │  62256 [28]  │  │ │
  │   │   └────────────────────────────┘   │    │  └──────────────┘  └──────────────┘  │ │
  │   │                                    │    │                                      │ │
  │   │  ┌─────────┐ ┌─────────┐ ┌─────────┐│    │         ┌──────────────┐            │ │
  │   │  │ ATF22V10│ │ ATF22V10│ │ ATF22V10││    │         │  LINE BUFFER │            │ │
  │   │  │ GAL-DEC │ │ GAL-TIM │ │ GAL-PPU ││    │         │  62256  [28] │            │ │
  │   │  │   [24]  │ │   [24]  │ │   [24]  ││    │         └──────────────┘            │ │
  │   │  └─────────┘ └─────────┘ └─────────┘│    └──────────────────────────────────────┘ │
  │   │                                    │                                             │
  │   │  ┌──────┐ ┌──────┐  HC688 [20]     │    ┌─ MCU DOMAIN ─────────────────────────┐ │
  │   │  │HC573 │ │HC573 │  compare/wrap    │    │                                      │ │
  │   │  │ [20] │ │ [20] │  (beam + decode) │    │  ┌────────────────────────────┐      │ │
  │   │  └──────┘ └──────┘                  │    │  │   ATmega1284P      [40]    │      │ │
  │   └────────────────────────────────────┘    │  │   sprites · OAM · pads     │      │ │
  │                                              │  └────────────────────────────┘      │ │
  │   ┌─ VRAM INTERLEAVE + BUS ────────────────┐ │                                      │ │
  │   │  HC157 HC157 HC157 HC157   [16] ×4    │ │  ┌──────────────┐  20 MHz xtal       │ │
  │   │  addr mux (CPU latched vs BG fetch)    │ │  │ ATmega328P   │  16 MHz xtal       │ │
  │   │  ┌──────────┐  HC573 [20] latch      │ │  │ APU    [28]  │                    │ │
  │   │  │ HC245    │  HC573 [20] data path   │ │  └──────────────┘                    │ │
  │   │  │ trcv [20]│                          │ └──────────────────────────────────────┘ │
  │   │  └──────────┘                          │                                         │
  │   └────────────────────────────────────────┘                                         │
  │                                                                                      │
  │   ┌─ BG BEAM + FETCH GLUE ────────────────────────────────────────────────────────┐  │
  │   │  HC161 HC161 HC161 HC161  [16] ×4  → 341×262 dot clock ~5.37 MHz             │  │
  │   │                                                                               │  │
  │   │  HC573×6  scroll · slot banks · MAP addr  [20]     HC14 [14] reset Schmitt    │  │
  │   │  HC573    palette / compositor latches   [20]     HC00·04·08·32·86  [14] glue │  │
  │   └───────────────────────────────────────────────────────────────────────────────┘  │
  │                                                                                      │
  │   ┌─ CART + CONFIG ──────────────────────┐   ┌─ VIDEO OUT (analog pads) ──────────┐  │
  │   │  ┌────────────────────────────┐    │   │  R-2R DAC ×3 (RGB) + CSYNC           │  │
  │   │  │  SST39SF040 parallel  [32] │    │   │  ┌────┐ ┌────┐ ┌────┐  J RGBS      │  │
  │   │  │  PRG · CHR · MAP (gated)   │    │   │  │ 75Ω│ │ 75Ω│ │ 75Ω│  J S-VIDEO   │  │
  │   │  └────────────────────────────┘    │   │  └────┘ └────┘ └────┘  J COMPOSITE  │  │
  │   │  ┌──────────────┐  HC245 [20]     │   └──────────────────────────────────────┘  │
  │   │  │ AT28C64B     │  cart data isol  │                                            │
  │   │  │ board E² [28]│                  │   HC157×2 [16]  line-buffer addr mux       │
  │   │  └──────────────┘                  │   (1284 writer vs beam reader)             │
  │   │         ┌── CART EDGE (planning) ──┤                                            │
  │   │         │ 32-pin ROM socket / IDC  │                                            │
  │   └─────────┴──────────────────────────┘                                            │
  │                                                                                      │
  SOUTH (component side — tallest sockets ~15 mm clearance below board)
  └──────────────────────────────────────────────────────────────────────────────────────┘

  LEGEND (planning totals — see 03_hardware_implementation.md)
  ───────────────────────────────────────────────────────────
  Package │ Count │ Parts
  ────────┼───────┼────────────────────────────────────────────────────────────
  [40]    │   2   │ W65C02S, ATmega1284P
  [32]    │   1   │ SST39SF040 (cart flash on board)
  [28]    │   5   │ 3× AS6C62256, ATmega328P, AT28C64B
  [24]    │   3   │ ATF22V10 (decode · timing · PPU/CHR gating)
  [20]    │  ~18  │ HC573 latches, HC245 transceivers, HC688 compare
  [16]    │   6   │ HC157 mux (4× VRAM + 2× line buffer)
  [14]    │  ~14  │ HC161×4, HC14, HC00/04/08/32/86 glue
  ────────┼───────┼────────────────────────────────────────────────────────────
          │  49   │ motherboard ICs (v0 frozen plan; +4th PLD if equations overflow)
```

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

**Retr01 Studio** is the only tool in active development. See `04_retr01_studio.md` for UI layout, game constraints, phased delivery, ASM optimization pipeline, and testing policy.

Studio authors worlds, screens, palettes, and CHR, then compiles them into a `.retr01` cart image.

## Future software (planned)

A **low-level hardware emulator** is planned later, not built now. It should simulate cycles, memory decode, and `$FExx` hardware behavior faithfully enough to validate carts before silicon exists.

## Where to look next

- Graphics, worlds, scrolling, banks, VRAM, and MAP: `02_graphics_worlds_memory.md`
- Board structure, buses, chips, and timing: `03_hardware_implementation.md`
- Protoboard module tests (island bring-up): `06_protoboard_module_tests.md`
- Retr01 Studio (current tool): `04_retr01_studio.md`
- Costs and unresolved items: `05_costs_and_open_questions.md`
