# Retr01-A Hardware (32 IC)

**Status:** **Current** Retr01-A system BOM (motherboard + cart). This is the normative board architecture on this branch.

**Authority:**

| This doc owns | This doc does **not** own |
|---------------|---------------------------|
| Chip list, IC count, PCB size, silicon merges | Software-visible `$FExx` **logical** map, cart image, worlds/VRAM -- [`02`](02_graphics_worlds_memory.md) |
| HW pathways (PLD roles, bus HC245 split, PROM packaging) | Exact mailbox/I2C `$FExx` bit protocols -- land those in `02` when frozen |

Same **game-visible graphics model** as the older ~52 IC sketch on `main` (32 KB sys / VRAM / linebuf, 512 KB cart, interleaved VRAM, 341x262, `$FE4x` APU on 328P). Differences that matter for software: **`$FE70-$FE72` is a 1284 EEPROM handshake** (not parallel AT28C64B), packed Color PROM, bit-packed latches, cart I2C saves -- see [`02`](02_graphics_worlds_memory.md).

Target: **through-hole DIP**, compact **12 x 12 cm** 4-layer PCB.

**Related:** software [`02`](02_graphics_worlds_memory.md). Decisions [`05`](05_costs_and_open_questions.md). Overview [`01`](01_architecture_overview.md). Optional discrete island checklist [`03`](03_hardware_implementation.md) (legacy ~52 path / bench fallback).

---

## Summary

| Item | Spec |
|------|------|
| **System IC count** | **32** (31 motherboard + 1 cart save). Escape **+1 PLD** -> 33 if compositor overflow |
| CPU / RAM / VRAM / cart flash | W65C02S + 3x AS6C62256 + SST39SF040 |
| Video timing | 341x262, ~5.37 MHz dot |
| Audio MCU | **ATmega328P** (dedicated APU) |
| Sprite / pads / machine EEPROM | **ATmega1284P** (no APU time-share) |
| Board config storage | **1284P internal 4 KB EEPROM** (handshake; no AT28C64B) |
| Color PROM | **1x AT28C16** (or faster OTP): packed R3-G3-B2; 1-dot pipeline |
| Beam / raster | **2x ATF22V10** (X/Y state machines + compare) |
| Glue logic | Absorbed into PLDs |
| `$FExx` latches | **9x HC573** (bit-packed bytes) |
| Bus transceivers | **3x HC245** (CPU / video / cart-OAM) |
| Cart game saves | **1x I2C EEPROM on cart** (in the 32) |

vs the older ~52 IC architecture (preserved on `main` / documented in [`03`](03_hardware_implementation.md)): about **20 ICs** removed by packing latches, absorbing glue/beam into PLDs, consolidating Color PROM, and dropping parallel board EEPROM -- while keeping 328P and 3x HC245.

---

## Part 1: What changed vs the ~52 IC sketch

Historical reduction notes (why 32, not 52). Product BOM is [Part 2](#part-2-bill-of-materials).

### 1. Board EEPROM eliminated (-1 IC)

| | |
|--|--|
| **Removed** | 1x AT28C64B (28-pin DIP) |
| **Replaced by** | ATmega1284P **internal 4 KB EEPROM** |
| **Reason** | Cabinet config and high scores are low-frequency writes. |
| **Caveat** | **4 KB**. Game saves on **cart EEPROM**. CPU handshake TBD in `02`. |

### 2. Audio MCU retained

| | |
|--|--|
| **Kept** | 1x ATmega328P |
| **Role** | NES-style APU @ 16 MHz; `$FE40-$FE5F` |
| **Reason** | Dedicated APU; 1284 stays on sprites, OAM, pads, machine EEPROM. |

### 3. Color PROMs consolidated (-2 ICs)

| | |
|--|--|
| **Removed** | 2x AT28C16 (of 3) |
| **Replaced by** | 1x AT28C16: **6-bit index in**, **8-bit** `{RRRGGGBB}` out |
| **Timing** | **1-dot pipeline**. Prefer faster OTP (e.g. AT27C256R-70) if 150 ns is tight. |
| **Quality** | Blue is **2 bits**. Studio must quantize to R3G3B2. |

### 4. Discrete glue absorbed (-9 ICs net)

| | |
|--|--|
| **Removed** | ~10x 74HC glue |
| **Replaced by** | Equations in **ATF22V10** devices |
| **Caveat** | **5 PLDs** total; **6th** only if fit fails. |

### 5. Beam counters and raster compare (-5 ICs net)

| | |
|--|--|
| **Removed** | 4x HC161 + 1x HC688 |
| **Replaced by** | 2x ATF22V10: X (0-340), Y (0-261) + compare vs `$FE04` -> IRQ |

### 6. Hardware latches packed (-5 ICs)

| | |
|--|--|
| **Replaced by** | **9x HC573** bit-packed `$FExx` bytes |
| **Caveat** | Logical addresses in `02`; **bitfield packing table** still open (Q21). |

### 7. Bus transceivers retained

| | |
|--|--|
| **Kept** | **3x HC245** |

### 8. Compositor in PLD

| | |
|--|--|
| **Added** | Compositor ATF22V10 (5th PLD): **priority mux only** -> 6-bit Color PROM index |
| **Escape** | +1 ATF22V10 (33 ICs) |

### 9. Cart save EEPROM (+1 IC, in the 32)

| | |
|--|--|
| **Added** | 1x I2C EEPROM on cartridge (e.g. 24C64) |

### Net tally (approx.)

Authoritative count is the [BOM table](#ic-count) (**32**). Approx deltas vs ~52: -1 EEPROM -2 PROM -9 glue -5 beam -5 latches +2 PLDs +1 cart save, with HC14 absorbed.

---

## Part 2: Bill of materials

### Processors and MCUs (3 ICs)

| Qty | Part | Role |
|-----|------|------|
| 1 | W65C02S (DIP-40) | 8 MHz game CPU |
| 1 | ATmega1284P (DIP-40) | 20 MHz: sprites/OAM, pads, **machine EEPROM** |
| 1 | ATmega328P (DIP-28) | 16 MHz: **APU**, `$FE40-$FE5F` |

### Memory and storage (4 ICs)

| Qty | Part | Role |
|-----|------|------|
| 1 | AS6C62256 (DIP-28) | 32 KB system RAM (`$0000-$7FFF`) |
| 1 | AS6C62256 (DIP-28) | 32 KB interleaved VRAM |
| 1 | AS6C62256 (DIP-28) | 32 KB sprite line-buffer SRAM |
| 1 | SST39SF040 (DIP-32) | 512 KB cart flash (PRG / CHR / MAP) |

### Programmable logic (5 ICs)

| Qty | Part | Role |
|-----|------|------|
| 1 | ATF22V10 | Address decode, PHI2 / CPU bus gating |
| 1 | ATF22V10 | VRAM interleave mux + absorbed glue |
| 1 | ATF22V10 | X-beam state machine (0-340) |
| 1 | ATF22V10 | Y-beam state machine (0-261) + raster IRQ compare |
| 1 | ATF22V10 | BG/sprite **priority mux** -> 6-bit Color PROM address |

### Registers and latches (9 ICs)

| Qty | Part | Role |
|-----|------|------|
| 9 | 74HC573 (DIP-20) | Packed `$FExx` state |

### Video mux, bus, and output (10 ICs)

| Qty | Part | Role |
|-----|------|------|
| 6 | 74HC157 (DIP-16) | VRAM / line-buffer address mux |
| 1 | AT28C16 (DIP-24) | Color PROM (6-bit index -> R3-G3-B2) |
| 3 | 74HC245 (DIP-20) | CPU / video / cart-OAM isolation |

### Cart save (+1 IC, on cartridge)

| Qty | Part | Role |
|-----|------|------|
| 1 | I2C EEPROM (e.g. 24C64) | Per-game saves (VCC, GND, SDA, SCL) |

Interface: 6502 bit-bang or 1284 as I2C master behind a `$FExx` window (TBD in `02`).

### IC count

| Block | Count |
|-------|------:|
| CPUs / MCUs | 3 |
| SRAM + flash | 4 |
| PLDs | 5 |
| HC573 | 9 |
| HC157 + PROM + HC245 | 10 |
| Cart I2C EEPROM | 1 |
| **System total** | **32** |

**Split:** **31** motherboard (incl. cart flash in socket) + **1** cart save. If flash lives only on the cart PCB: still **32** (30 soldered mobo + flash + EEPROM).

**Not in the 32:** **74HC14** reset/clock -- absorb if possible; else +1 -> 33.

---

## Part 3: Inter-chip pathways

### APU path (328P)

1. W65C02S writes `$FE40-$FE5F`.
2. ATmega328P services APU continuously.
3. ATmega1284P does **not** synthesize audio.

### PLD beam and raster IRQ

1. `$FE04` latched in HC573.
2. Y-beam PLD compares to Y counter.
3. Match drives **IRQB**.

### 8-bit color DAC path

1. Compositor PLD -> 6-bit palette index (pipelined one dot if needed).
2. PROM/OTP -> `{RRRGGGBB}`.
3. R-2R ladders -> **RGBS**.

### Bus isolation

Three HC245s + PLD `/OE`: one driver at a time per domain.

---

## Software map notes

Detail in [`02`](02_graphics_worlds_memory.md). Short:

| Topic | Norm |
|-------|------|
| `$FE40-$FE5F` APU | ATmega328P |
| Machine config | 1284 internal EEPROM via **`$FE70-$FE72` handshake** (same address band as legacy parallel EEPROM; **not** AT28C64B silicon) |
| Latch silicon | 9x HC573 bit-packed (bitfields open in `02`) |
| Cart saves | Cart I2C EEPROM + HAL |

PRG should use `machine_eeprom_*` / `cart_save_*` helpers. APU may use direct `$FE4x` stores.

---

## Validation gates

Bench/sim gates before locking schematics / first PCB spin:

| Gate | Pass criteria |
|------|----------------|
| **G1 CPU + RAM** | Island C style |
| **G2 VRAM interleave** | Island G style |
| **G3 Beam PLDs** | Stable 341x262; HBlank/VBlank/NMI stubs; raster IRQ |
| **G4 Compositor PLD** | BG/sprite priority @ dot rate; pipelined PROM |
| **G5 328P APU** | Stable output; `$FE4x` smoke |
| **G6 Bus** | No fights with 3x HC245 + PLD `/OE` |
| **G7 Color** | 64-entry PROM; RGBS tuned; 1-dot pipeline |
| **G8 Saves** | 1284 machine EEPROM + cart EEPROM R/W |

---

## Bring-up strategy

```text
Phase A -- Prove behavior (island order from 03, adapted to this BOM)
  CPU+RAM, VRAM interleave, beam PLDs, BG fetch, 1284 sprites,
  328P APU, Color PROM + RGBS (pipelined).

Phase B -- Board-specific merges
  - Compositor PLD at dot rate
  - Packed HC573 / $FExx (as bitfields land in 02)
  - 1284 machine-EEPROM handshake
  - Cart I2C save path

Phase C -- First integrated PCB
  32 IC system (12 x 12 cm mobo + cart)
```

**Optional:** use discrete HC161 + HC688 on the bench if a beam PLD fails (legacy parts from the ~52 path). Do not breadboard 52 ICs as a product goal. Full ~52 IC architecture remains on `main` / [`03`](03_hardware_implementation.md).

---

## Risk summary

| Area | Risk | Mitigation |
|------|------|------------|
| Compositor PLD | I/O and timing fit | Priority-mux-only; escape +1 PLD |
| Bus contention | Driver fights | 3x HC245 |
| Color PROM | 150 ns / R3G3B2 | 1-dot pipeline; faster OTP; Studio match |
| `$FExx` / saves | Spec gaps | Freeze mailbox + I2C + bitfields in `02` |
| IC count | Overflow | Norm **32**; +1 PLD or +HC14 only if needed |

---

## Where to look next

| Doc | Content |
|-----|---------|
| [`02`](02_graphics_worlds_memory.md) | Software SoT |
| [`03`](03_hardware_implementation.md) | Legacy ~52 / optional island checklist |
| [`05`](05_costs_and_open_questions.md) | Locked decisions + open Qs |
| [`01`](01_architecture_overview.md) | Sources of truth + snapshot |
