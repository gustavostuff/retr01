# Retr01-A Hardware v1 (32-IC Revision)

**Status:** Proposed system BOM (motherboard + cart). Does **not** replace the v0 (~52 IC) reference in [`03_hardware_implementation.md`](03_hardware_implementation.md) until bench validation passes the gates in [Bring-up strategy](#bring-up-strategy-v0-islands-vs-v1-pcb).

This doc captures the compact Retr01-A bill of materials: programmable logic and MCU silicon absorb discrete glue, counters, and comparators, while **keeping** a dedicated APU MCU and full bus isolation. Target: **through-hole DIP**, compact **12 x 12 cm** 4-layer PCB, **same software-visible memory and video model** as v0 (32 KB sys / VRAM / linebuf, 512 KB cart, interleaved VRAM, 341x262 timing).

**Related:** graphics and `$FExx` contract in [`02`](02_graphics_worlds_memory.md). Costs and open questions in [`05`](05_costs_and_open_questions.md).

---

## Summary

| Item | v0 (~52 IC) | v1 (this doc) |
|------|-------------|---------------|
| **System IC count** | ~52 | **32** (31 motherboard + 1 cart save) |
| CPU / RAM / VRAM / cart flash | W65C02S + 3x AS6C62256 + SST39SF040 | **Unchanged** |
| Video timing | 341x262, ~5.37 MHz dot | **Unchanged** |
| Audio MCU | ATmega328P | **ATmega328P retained** (dedicated APU) |
| Sprite / pads / machine EEPROM | ATmega1284P | **ATmega1284P** (no APU time-share) |
| Board config storage | AT28C64B @ `$FE70` | **1284P internal 4 KB EEPROM** (handshake) |
| Color PROM | 3x AT28C16 (R/G/B) | **1x AT28C16** (packed R3-G3-B2; 1-dot pipeline) |
| Beam / raster | 4x HC161 + HC688 | **2x ATF22V10** (X/Y state machines + compare) |
| Glue logic | ~10x 74HC DIP-14 | **Absorbed into PLDs** |
| `$FExx` latches | 14x HC573 | **9x HC573** (bit-packed bytes) |
| Bus transceivers | 3x HC245 | **3x HC245 retained** (CPU / video / cart-OAM paths) |
| Cart game saves | (not in v0 mobo spec) | **1x I2C EEPROM on cart** (in the 32) |

Net: **~20 ICs removed** vs v0. Hope: **32 is enough**; escape hatch is **+1 ATF22V10** if compositor equations overflow (same as v0).

---

## Part 1: Optimization path (52 ICs toward 32)

How ~20 ICs come off compared to v0, and what stays for risk mitigation.

### 1. Board EEPROM eliminated (-1 IC)

| | |
|--|--|
| **Removed** | 1x AT28C64B (28-pin DIP) |
| **Replaced by** | ATmega1284P **internal 4 KB EEPROM** |
| **Reason** | Cabinet config and high scores are low-frequency writes. A 28-pin parallel EEPROM is oversized for that role. |
| **Caveat** | **4 KB** (not 8 KB). Frequent **game** saves belong on **cart EEPROM**, not 1284 wear. CPU writes via a **software handshake** to 1284 firmware (see [Register map deltas](#register-map-deltas-vs-v0)). |

### 2. Audio MCU retained (0 IC delta vs v0)

| | |
|--|--|
| **Kept** | 1x ATmega328P (28-pin DIP) |
| **Role** | NES-style APU @ 16 MHz; owns `$FE40-$FE5F` as in v0 |
| **Reason** | Merging APU into the 1284 (VBlank synth + sprites) is the highest MCU scheduling risk. v1 keeps a **dedicated** audio MCU so the 1284 stays on sprites, OAM, pads, and machine EEPROM. |
| **Not done** | Do **not** plan 1284 APU time-share for the first PCB spin. |

### 3. Color PROMs consolidated (-2 ICs)

| | |
|--|--|
| **Removed** | 2x AT28C16 (of 3) |
| **Replaced by** | 1x AT28C16: **6-bit index in**, **8-bit packed out** `{RRRGGGBB}` (R3, G3, B2) to three R-2R ladders |
| **Reason** | 64-color master palette does not need three parallel 8-bit-wide PROM chips. |
| **Timing** | **1-dot pipeline** (latch index on pixel N, PROM drives DACs on N+1). Prefer **faster OTP** (e.g. AT27C256R-70 class) if 150 ns is tight at ~5.37 MHz. |
| **Quality** | Blue is **2 bits** (4 levels). Studio Color PROM preview must match packed layout. Revert to 3x PROM only if art quality forces it (+2 ICs). |

### 4. Discrete glue logic absorbed (-9 ICs net)

| | |
|--|--|
| **Removed** | ~10x 74HC glue (HC00/04/08/32/86, etc.) |
| **Replaced by** | Equations in existing / added **ATF22V10** devices (decode, `/CE`, clock gating, direction) |
| **Caveat** | Watch **product-term** limits per PLD; v0 already allowed +1 PLD on overflow. v1 uses **5 PLDs** total (hope); **6th** only if fit fails. |

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
| **Caveat** | **Logical** map can stay 6502-friendly (ORA/AND + Zero Page shadows). **Spec revision** vs frozen v0 `$FExx` table in `02` -- freeze a v1 map + thin PRG HAL for saves/APU before shipping carts. |

### 7. Bus transceivers retained (0 IC delta vs v0)

| | |
|--|--|
| **Kept** | **3x HC245**: CPU vs video data path, cart isolation, OAM / helper path (same roles as v0) |
| **Reason** | Cutting to 1x HC245 and relying only on PLD `/OE` is a high contention risk. v1 keeps full isolation within the 32-IC budget. |

### 8. Compositor moved to PLD (v0 used HC573 + glue)

| | |
|--|--|
| **Added** | Dedicated **compositor ATF22V10** (5th PLD; part of the +2 PLDs vs v0's 3) |
| **Scope (recommended)** | **Late-stage priority mux only**: pick final **6-bit Color PROM index** from pre-resolved BG vs sprite indices + priority / transparency. **Not** full CHR fetch, palette row mux, and tile pipeline in one PLD. |
| **Caveat** | Highest PLD integration risk. Define I/O and pipeline stages before locking PCB. Escape: **+1 ATF22V10** (33 ICs). |

### 9. Cart save EEPROM (+1 IC vs v0 mobo, in the 32)

| | |
|--|--|
| **Added** | 1x I2C EEPROM on cartridge (e.g. 24C64) |
| **Reason** | Game saves must not wear 1284 internal EEPROM; `$FExx` / save map churn is mitigated by a **frozen register map + HAL**, not by bringing AT28C64B back. |

### Net tally (approx.)

| Change | Delta |
|--------|------:|
| Drop AT28C64B | -1 |
| Drop 2x Color PROM | -2 |
| Absorb glue into PLDs | -9 |
| Beam PLDs replace HC161/HC688 | -5 |
| Pack HC573 | -5 |
| Keep 328P, 3x HC245 | 0 |
| Add beam + compositor PLDs (vs v0's 3) | +2 |
| Add cart I2C EEPROM | +1 |
| **Net vs ~52** | **~-20 -> 32** |

---

## Part 2: v1 bill of materials

### Processors and MCUs (3 ICs)

| Qty | Part | Role |
|-----|------|------|
| 1 | W65C02S (DIP-40) | 8 MHz game CPU: logic, MAP streaming, latch writes |
| 1 | ATmega1284P (DIP-40) | 20 MHz: sprite/OAM line buffer, pads, **machine EEPROM** |
| 1 | ATmega328P (DIP-28) | 16 MHz: **NES-style APU**, `$FE40-$FE5F` |

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

**Note:** A **6th PLD** is **not** in the 32; add only if equation fit fails (system becomes 33).

### Registers and latches (9 ICs)

| Qty | Part | Role |
|-----|------|------|
| 9 | 74HC573 (DIP-20) | Packed `$FExx` state: scroll, PPU, raster, plane bands, MAP addr, etc. |

### Video mux, bus, and output (10 ICs)

| Qty | Part | Role |
|-----|------|------|
| 6 | 74HC157 (DIP-16) | VRAM / line-buffer address mux (CPU phase vs video phase) |
| 1 | AT28C16 (DIP-24) | Consolidated Color PROM (6-bit index -> 8-bit R3-G3-B2) |
| 3 | 74HC245 (DIP-20) | CPU / video / cart-OAM bus isolation |

### Cart save (+1 IC, on cartridge)

| Qty | Part | Role |
|-----|------|------|
| 1 | I2C EEPROM (e.g. 24C64, SOIC-8 or DIP-8) | **Per-game saves** (4 wires: VCC, GND, SDA, SCL) |

Interface: 6502 bit-bang or 1284 as I2C master behind a `$FExx` window (TBD).

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

**Split:** **31** on motherboard (including cart flash in socket) + **1** cart save IC. If cart flash lives only on the cartridge PCB, count is still **32** system ICs (30 soldered mobo + flash + EEPROM).

**Not in the 32:** reset / clock conditioner (**74HC14**) -- absorb into PLD / discrete passives if possible; add only if bring-up needs it (+1 -> 33).

---

## Part 3: Inter-chip pathways (v1)

### APU path (328P)

1. W65C02S writes sound triggers to **`$FE40-$FE5F`** (same address band as v0).
2. ATmega328P services the APU map continuously; PWM / analog out as in v0 planning.
3. ATmega1284P does **not** synthesize audio; it stays on sprites, OAM, pads, and machine EEPROM.

### PLD beam and raster IRQ

1. W65C02S writes split scanline to **`$FE04`** (latched in HC573).
2. Y-beam PLD compares registered Y counter to latched value **combinationally**.
3. Match drives **IRQB** (same idea as v0 HC688, no external comparator IC).

### 8-bit color DAC path

1. Compositor PLD outputs **6-bit master palette index** (pipelined one dot if needed).
2. AT28C16 (or faster OTP substitute) outputs **8-bit** `{RRRGGGBB}`.
3. Three R-2R ladders (3 + 3 + 2 pins) -> **RGBS** pads (sync generation unchanged from v0 planning).

### Bus isolation

Three HC245s keep CPU, video-fetch, and cart/OAM domains from fighting. PLD `/OE` still enforces **one driver at a time** within each domain.

---

## Register map deltas vs v0

| Topic | v0 | v1 |
|-------|----|----|
| `$FE40-$FE5F` APU | ATmega328P hardware | **Unchanged** -- still 328P |
| `$FE70-$FE72` board EEPROM | AT28C64B parallel port | **Removed** -- machine config via **1284 EEPROM handshake** (new `$FExx` or mailbox -- TBD) |
| `$FE00` etc. | One flag per latch byte (14x HC573) | **Bit-packed** bytes (9x HC573) -- freeze bitfield table in `02` when adopted |
| Cart saves | Not specified on mobo | **Cart I2C EEPROM** + `$FExx` access helper / HAL |

Games targeting **v0** and **v1** can share the **same cart image** (PRG/CHR/MAP) if PRG uses a HAL for saves and machine EEPROM; APU port stays binary-compatible with v0. Machine EEPROM layout is **not** binary compatible with AT28C64B without a migration tool.

---

## Validation gates (v1 adoption)

Do **not** treat v1 as production-ready until:

| Gate | Pass criteria |
|------|----------------|
| **G1 CPU + RAM** | Same as v0 island C |
| **G2 VRAM interleave** | Same as v0 island G |
| **G3 Beam PLDs** | Stable 341x262; HBlank/VBlank/NMI stubs; raster IRQ on `$FE04` |
| **G4 Compositor PLD** | Correct BG/sprite priority @ dot rate with pipelined PROM |
| **G5 328P APU** | Stable output; APU register smoke tests (same bar as v0 island K) |
| **G6 Bus** | No fights with 3x HC245 + PLD `/OE`; cart + CPU + video never clash |
| **G7 Color** | 64-entry PROM programmed; RGBS levels tuned; 1-dot pipeline verified (extends v0 Q2) |
| **G8 Saves** | Machine config in 1284 EEPROM survives power cycle; cart EEPROM R/W |

---

## Bring-up strategy: v0 islands vs v1 PCB

### Do you need to build the full 52-IC v0 motherboard?

**No -- not as a product goal.** The ~52 IC plan in `03` is a **de-risking reference** and **island bring-up checklist**, not a requirement that the first shipped board populate every discrete chip.

### Recommended path

```text
Phase A -- Prove behavior (v0 island order from 03)
  Use breadboard / sim islands for: CPU+RAM, VRAM interleave, beam, BG fetch,
  1284 sprites, 328P APU, Color PROM + RGBS (pipelined).

Phase B -- Prove v1-specific merges (before 32-IC PCB)
  - Compositor PLD (priority mux only) at dot rate
  - Packed HC573 / `$FExx` map in emulator or FPGA/CPLD fixture
  - 1284 machine-EEPROM handshake (no AT28C64B)
  - Cart I2C save path

Phase C -- First integrated PCB
  Target **v1 32 IC** system (12 x 12 cm mobo + cart) once Phase A core + Phase B merges pass.
```

### When v0 discrete chips still help

Use **discrete** HC161 + HC688 on the bench when a beam PLD fails -- swap one island back to v0 parts to localize bugs. You do not need all 52 ICs soldered at once; `03` explicitly says **do not breadboard all 52 at once**.

### When to skip straight to v1 PCB

Only if **sim + CPLD fixtures** already pass gates G3-G4 and G6-G7. Otherwise you debug compositor and packed `$FExx` on a 4-layer board with less fallback.

---

## Risk summary

| Area | Risk | Mitigation in 32-IC BOM |
|------|------|-------------------------|
| 1284 APU time-share | VBlank budget, underrun | **Not used** -- 328P kept |
| Compositor PLD | I/O and timing fit | Priority-mux-only scope; escape +1 PLD |
| Bus contention | Driver fights | **3x HC245** retained |
| Color PROM | 150 ns / R3G3B2 | 1-dot pipeline; faster OTP swap; Studio match |
| `$FExx` / saves | Spec drift | Freeze map in `02`; PRG HAL; cart I2C for game saves |
| IC count | Overflow | Hope **32**; +1 PLD or +HC14 only if bring-up demands |

---

## Where to look next

| Doc | Content |
|-----|---------|
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | v0 ~52 IC reference, island bring-up order |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Worlds, VRAM, `$FExx` (update when v1 map freezes) |
| [`05_costs_and_open_questions.md`](05_costs_and_open_questions.md) | Locked decisions; add v1 entries when adopted |
| [`01_architecture_overview.md`](01_architecture_overview.md) | Capability snapshot (unchanged for games) |
