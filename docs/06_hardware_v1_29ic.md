# Retr01-A Hardware v1 (29-IC Revision)

**Status:** Proposed motherboard BOM. Does **not** replace the v0 (~52 IC) reference in [`03_hardware_implementation.md`](03_hardware_implementation.md) until bench validation passes the gates in [Bring-up strategy](#bring-up-strategy-v0-islands-vs-v1-pcb).

This doc captures the ultra-optimized Retr01-A bill of materials: programmable logic and MCU silicon absorb discrete glue, counters, comparators, and peripheral MCUs. Target: **through-hole DIP**, compact **12 x 12 cm** 4-layer PCB, **same software-visible memory and video model** as v0 (32 KB sys / VRAM / linebuf, 512 KB cart, interleaved VRAM, 341x262 timing).

**Related:** graphics and `$FExx` contract in [`02`](02_graphics_worlds_memory.md). Costs and open questions in [`05`](05_costs_and_open_questions.md).

---

## Summary

| Item | v0 (~52 IC) | v1 (this doc) |
|------|-------------|---------------|
| Motherboard IC count | ~52 | **28-29** (+ optional cart save IC) |
| CPU / RAM / VRAM / cart | W65C02S + 3x AS6C62256 + SST39SF040 | **Unchanged** |
| Video timing | 341x262, ~5.37 MHz dot | **Unchanged** |
| Audio MCU | ATmega328P | **ATmega1284P** (VBlank synth + PWM) |
| Board config storage | AT28C64B @ `$FE70` | **1284P internal 4 KB EEPROM** (handshake) |
| Color PROM | 3x AT28C16 (R/G/B) | **1x AT28C16** (packed R3-G3-B2 byte) |
| Beam / raster | 4x HC161 + HC688 | **2x ATF22V10** (X/Y state machines + compare) |
| Glue logic | ~10x 74HC DIP-14 | **Absorbed into PLDs** |
| `$FExx` latches | 14x HC573 | **9x HC573** (bit-packed bytes) |
| Bus transceivers | 3x HC245 | **1x HC245** (CPU vs video path; rest = PLD `/OE`) |
| Cart game saves | (not in v0 mobo spec) | **Optional 4-pin I2C EEPROM on cart** |

---

## Part 1: Optimization path (52 ICs toward ~29)

How ~23 ICs come off the motherboard compared to v0.

### 1. Board EEPROM eliminated (-1 IC)

| | |
|--|--|
| **Removed** | 1x AT28C64B (28-pin DIP) |
| **Replaced by** | ATmega1284P **internal 4 KB EEPROM** |
| **Reason** | Cabinet config and high scores are low-frequency writes. A 28-pin parallel EEPROM is oversized for that role. |
| **Caveat** | **4 KB** (not 8 KB). Frequent **game** saves belong on **cart EEPROM**, not 1284 wear. CPU writes via a **software handshake** to 1284 firmware (see [Register map deltas](#register-map-deltas-vs-v0)). |

### 2. Audio MCU merged (-1 IC)

| | |
|--|--|
| **Removed** | 1x ATmega328P (28-pin DIP) |
| **Replaced by** | ATmega1284P during **VBlank** + **hardware PWM** during active video |
| **Reason** | 1284 @ 20 MHz is idle enough in the vertical blank window to synthesize NES-style audio into a ring buffer; timers output PWM while the core fills sprite line buffers on active lines. |
| **Caveat** | Retr01 VBlank is **~22 scanlines**, not 38. Budget is ~**28k cycles** at 20 MHz per frame, not ~48k. 1284 still services **OAM port**, **pads**, and latch reads in VBlank. **Prototype audio before dropping 328P** on the first PCB spin. |

### 3. Color PROMs consolidated (-2 ICs)

| | |
|--|--|
| **Removed** | 2x AT28C16 (of 3) |
| **Replaced by** | 1x AT28C16: **6-bit index in**, **8-bit packed out** `{RRRGGGBB}` (R3, G3, B2) to three R-2R ladders |
| **Reason** | 64-color master palette does not need three parallel 8-bit-wide PROM chips. |
| **Caveat** | Blue is **2 bits** (4 levels). Same **150 ns** class access as v0; plan **1-dot pipeline** (index latch -> PROM out). Update Studio Color PROM preview to packed layout. |

### 4. Discrete glue logic absorbed (-9 ICs net)

| | |
|--|--|
| **Removed** | ~10x 74HC glue (HC00/04/08/32/86, etc.) |
| **Replaced by** | Equations in existing / added **ATF22V10** devices (decode, `/CE`, clock gating, direction) |
| **Caveat** | Watch **product-term** limits per PLD; v0 already allowed +1 PLD on overflow. v1 uses **5 PLDs** total. |

### 5. Beam counters and raster compare (-5 ICs net)

| | |
|--|--|
| **Removed** | 4x HC161 + 1x HC688 |
| **Replaced by** | 2x ATF22V10: **X machine** (0-340, 9 bits), **Y machine** (0-261, 9 bits) + **combinatorial compare** vs `$FE04` raster latch -> IRQ |
| **Caveat** | **9 FFs + wrap** per axis fits one ATF22V10 (10 macrocells). Must clock at **dot rate** (~5.37 MHz). ATF22V10 speed grade is adequate. |

### 6. Hardware latches packed (-5 ICs)

| | |
|--|--|
| **Removed** | 5x HC573 |
| **Replaced by** | **9x HC573** with **bit-packed** `$FExx` bytes (PPUCTRL, plane, flags share bytes) |
| **Caveat** | **Logical** map can stay 6502-friendly (ORA/AND + Zero Page shadows). **Spec revision** vs frozen v0 `$FExx` table in `02` -- update docs and emulator when v1 is adopted. |

### 7. Bus transceivers reduced (-2 ICs)

| | |
|--|--|
| **Removed** | 2x HC245 (of 3) |
| **Kept** | 1x HC245: CPU vs video-fetch data path |
| **Caveat** | Cart, OAM, and SRAM drivers need a documented **one-driver-at-a-time** plan via PLD `/OE`. **Schematic review required** before PCB. |

### 8. Compositor moved to PLD (v0 used HC573 + glue)

| | |
|--|--|
| **Added** | Dedicated **compositor ATF22V10** (5th PLD) |
| **Scope (recommended)** | **Late-stage priority mux only**: pick final **6-bit Color PROM index** from pre-resolved BG vs sprite indices + priority / transparency. **Not** full CHR fetch, palette row mux, and tile pipeline in one PLD. |
| **Caveat** | **Highest integration risk** in v1. Define PLD I/O and pipeline stages before locking BOM. |

---

## Part 2: v1 bill of materials

### Processors and MCUs (2 ICs)

| Qty | Part | Role |
|-----|------|------|
| 1 | W65C02S (DIP-40) | 8 MHz game CPU: logic, MAP streaming, latch writes |
| 1 | ATmega1284P (DIP-40) | 20 MHz: sprite/OAM line buffer (active frame), **audio synth (VBlank)**, PWM out, pads, **machine EEPROM** |

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

**Note:** Header drafts sometimes say "6 PLDs"; the counted BOM here is **5**. A 6th PLD is **not** planned unless equation fit fails (same escape hatch as v0).

### Registers and latches (9 ICs)

| Qty | Part | Role |
|-----|------|------|
| 9 | 74HC573 (DIP-20) | Packed `$FExx` state: scroll, PPU, raster, plane bands, MAP addr, etc. |

### Video mux and output (8 ICs)

| Qty | Part | Role |
|-----|------|------|
| 6 | 74HC157 (DIP-16) | VRAM / line-buffer address mux (CPU phase vs video phase) |
| 1 | AT28C16 (DIP-24) | Consolidated Color PROM (6-bit index -> 8-bit R3-G3-B2) |
| 1 | 74HC245 (DIP-20) | CPU <-> video data path isolation |

### IC count

| Block | Count |
|-------|------:|
| CPUs / MCUs | 2 |
| SRAM + flash | 4 |
| PLDs | 5 |
| HC573 | 9 |
| HC157 + PROM + HC245 | 8 |
| **Motherboard total** | **28** |

**29th motherboard IC (optional):** reset / clock conditioner (v0 used **74HC14**) or dedicated supervisor -- not listed above; add if not absorbed into PLD oscillators.

### Cart add-on (+1 IC, not on motherboard)

| Qty | Part | Role |
|-----|------|------|
| 1 | I2C EEPROM (e.g. 24C64, SOIC-8 or DIP-8) | **Per-game saves** on cartridge (4 wires: VCC, GND, SDA, SCL) |

Keeps wear off 1284 internal EEPROM. Interface: 6502 bit-bang or 1284 as I2C master behind a `$FExx` window (TBD).

---

## Part 3: Inter-chip pathways (v1)

### VBlank audio handshake

1. W65C02S writes sound triggers to **`$FE40-$FE5F`** (same address band as v0 APU map).
2. During **VBlank**, 1284 reads those latches, updates APU state, fills a **PCM / mixed sample ring buffer**.
3. During **active video**, a **hardware timer + PWM** drains the buffer; 1284 core continues **sprite evaluation** and line-buffer fills.

Underrun = click/pop. Size buffer conservatively; validate on scope before removing 328P from any production spin.

### PLD beam and raster IRQ

1. W65C02S writes split scanline to **`$FE04`** (latched in HC573).
2. Y-beam PLD compares registered Y counter to latched value **combinationally**.
3. Match drives **IRQB** (same idea as v0 HC688, no external comparator IC).

### 8-bit color DAC path

1. Compositor PLD outputs **6-bit master palette index**.
2. AT28C16 outputs **8-bit** `{RRRGGGBB}`.
3. Three R-2R ladders (3 + 3 + 2 pins) -> **RGBS** pads (sync generation unchanged from v0 planning).

---

## Register map deltas vs v0

| Topic | v0 | v1 |
|-------|----|----|
| `$FE40-$FE5F` APU | ATmega328P hardware | **1284 firmware** + same CPU port addresses (behavior doc TBD) |
| `$FE70-$FE72` board EEPROM | AT28C64B parallel port | **Removed** -- machine config via **1284 EEPROM handshake** (new `$FExx` or mailbox -- TBD) |
| `$FE00` etc. | One flag per latch byte (14x HC573) | **Bit-packed** bytes (9x HC573) -- update bitfield table in `02` when frozen |
| Cart saves | Not specified on mobo | **Cart I2C EEPROM** + optional `$FExx` access helper |

Games targeting **v0** and **v1** can share the **same cart image** (PRG/CHR/MAP) if PRG uses a HAL for saves and APU; machine EEPROM layout is **not** binary compatible without a migration tool.

---

## Validation gates (v1 adoption)

Do **not** treat v1 as production-ready until:

| Gate | Pass criteria |
|------|----------------|
| **G1 CPU + RAM** | Same as v0 island C |
| **G2 VRAM interleave** | Same as v0 island G |
| **G3 Beam PLDs** | Stable 341x262; HBlank/VBlank/NMI stubs; raster IRQ on `$FE04` |
| **G4 Compositor PLD** | Correct BG/sprite priority @ dot rate with pipelined PROM |
| **G5 1284 audio** | Stable PWM output; no underrun over 30 min; APU register smoke tests |
| **G6 Bus** | No fights with one HC245 + PLD `/OE`; cart + CPU + video never clash |
| **G7 Color** | 64-entry PROM programmed; RGBS levels tuned (extends v0 Q2) |
| **G8 Saves** | Machine config in 1284 EEPROM survives power cycle; cart EEPROM R/W |

---

## Bring-up strategy: v0 islands vs v1 PCB

### Do you need to build the full 52-IC v0 motherboard?

**No -- not as a product goal.** The ~52 IC plan in `03` is a **de-risking reference** and **island bring-up checklist**, not a requirement that the first shipped board populate every discrete chip.

### Recommended path

```text
Phase A -- Prove behavior (v0 island order from 03)
  Use breadboard / sim islands for: CPU+RAM, VRAM interleave, beam, BG fetch,
  1284 sprites, 328P OR 1284 audio (pick one audio path to validate early),
  Color PROM + RGBS.

Phase B -- Prove v1-specific merges (before 29-IC PCB)
  - 1284 APU + sprites time-sharing (scope + logic analyzer)
  - Compositor PLD (priority mux only) at dot rate
  - Single HC245 bus plan
  - Packed HC573 / `$FExx` map in emulator or FPGA/CPLD fixture

Phase C -- First integrated PCB
  Target **v1 28-29 IC** layout (12 x 12 cm) once Phase A core + Phase B merges pass.
  Optional: 328P socket or jumper **audio fallback** on first spin.
```

### When v0 discrete chips still help

Use **discrete** HC161 + HC688 and **separate 328P** on the bench when a v1 block fails -- swap one island back to v0 parts to localize bugs. You do not need all 52 ICs soldered at once; `03` explicitly says **do not breadboard all 52 at once**.

### When to skip straight to v1 PCB

Only if **sim + CPLD fixtures** already pass gates G3-G6. Otherwise you debug **compositor + bus + merged audio** on a 4-layer board with no fallback.

---

## Risk summary

| Area | Risk | Mitigation |
|------|------|------------|
| 1284 APU | VBlank time, underrun | Prototype early; PWM buffer; optional 328P on rev A PCB |
| Compositor PLD | I/O and timing fit | Narrow scope to priority mux; pipeline PROM |
| 1x HC245 | Bus contention | Explicit tri-state table; island tests |
| 9x HC573 pack | Spec drift | Update `02` bitfields; emulator parity |
| 1284 EEPROM | Endurance / 4 KB | Machine-only data; cart EEPROM for game saves |
| IC count doc | 28 vs 29 naming | 28 counted above + reset/supervisor or cart IC counted separately |

---

## Where to look next

| Doc | Content |
|-----|---------|
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | v0 ~52 IC reference, island bring-up order |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Worlds, VRAM, `$FExx` (update when v1 map freezes) |
| [`05_costs_and_open_questions.md`](05_costs_and_open_questions.md) | Locked decisions; add v1 entries when adopted |
| [`01_architecture_overview.md`](01_architecture_overview.md) | Capability snapshot (unchanged for games) |
