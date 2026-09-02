# Lab 03: Decode PLD and CPU bus transceiver

**Goal:** Replace the **temporary** RAM/cart chip-select gates from Labs 01-02 with the real **ATF22V10 decode PLD** (U3) and add the **SN74HC245** CPU-domain transceiver (U4). Same CPU, RAM, and cart behavior. Cleaner selects. Safer bus sharing for later I/O chips.

**Pass:** Complete the **Success metrics** section (KiCad table required). U3 JEDEC programming is optional later.

**Depends on:** [Lab 01](01_cpu_and_system_ram.md), [Lab 02](02_cart_prg_read.md).

**Do not add yet:** the nine HC573 bodies (Lab 04). You may reserve `SEL_FE*` / `LE_FE*` net names now.

**Related:** [`../../hw/md/ATF22V10.md`](../../hw/md/ATF22V10.md). [`../../hw/md/SN74HC245.md`](../../hw/md/SN74HC245.md). [`../hardware.md`](../hardware.md) (bus rule).

---

## 0. Ideas in plain language

Until now, chip-select was a few discrete gates (like hard-coded `if` statements in `main`). The **decode PLD** is a tiny programmable chip that holds many of those `if`s in one 24-pin package.

The **HC245** is a controlled bridge between two 8-bit buses:

```text
CPU_D[7:0]  <--245-->  PERIPH_D[7:0]
```

`OE#` low = bridge on. `DIR` picks direction. When `OE#` is high, both sides disconnect (Hi-Z). That prevents bus fights when RAM, flash, and (later) latches share data wires.

**Bus rule** ([`../hardware.md`](../hardware.md)): one driver at a time per domain.

---

## 1. Where to draw

Keep U3 and U4 on the **PowerCpuRam** sheet (Lab 01) **or** split a sheet `sheets/03_decode_bus.kicad_sch`. Either is fine. Label hierarchical pins for `PERIPH_D0`..`PERIPH_D7`.

---

## 2. Place U4: SN74HC245 (CPU domain)

Stock KiCad symbol `74HC245` is fine. Footprint DIP-20.

| Pin | Name | Net |
|-----|------|-----|
| 1 | DIR | `CPU_245_DIR` (from U3) |
| 19 | OE | `CPU_245_OE#` (from U3, active low) |
| 2-9 | A1-A8 | `CPU_D0`..`CPU_D7` |
| 18-11 | B1-B8 | `PERIPH_D0`..`PERIPH_D7` |
| 20 | VCC | `+5V` + 100 nF |
| 10 | GND | `GND` |

### 2.1 What moves behind the transceiver

**Lab 03 migration:**

| Was on `CPU_D*` directly | Move to |
|--------------------------|---------|
| Future `$FExx` latch D inputs | `PERIPH_D*` (Lab 04) |
| Optional: cart data | either stay on CPU side for PRG, or use cart-domain HC245 later (U14). For Lab 03, **PRG flash may stay on `CPU_D*`** like NES-style ROM. Peripheral I/O uses `PERIPH_D*`. |

Practical split used by many 6502 boards:

- RAM + PRG flash share **CPU** data bus (no 245 between CPU and RAM/flash).
- I/O devices sit behind **U4** on `PERIPH_D*`.

If Lab 02 wired flash on `CPU_D*`, **leave it**. Do not force flash through U4 in this lab.

---

## 3. Place U3: ATF22V10 decode

Symbol: create `ATF22V10` from [`../../hw/md/ATF22V10.md`](../../hw/md/ATF22V10.md) or use a generic 22V10. Footprint DIP-24.

| Pin | Hard wire |
|-----|-----------|
| 24 | `+5V` + 100 nF |
| 12 | `GND` |
| 1 | `PHI2` (if you use registered equations) **or** treat as input |

### 3.1 Inputs to assign (worksheet)

Write pin numbers on your sheet as you lock CUPL later. For the schematic, name the pins by function:

| Function | Source net |
|----------|------------|
| A8 .. A15 | `CPU_A8`..`CPU_A15` |
| A0 .. A7 optional | only if you decode fine ports in the PLD |
| RWB | `RWB` |
| PHI2 | `PHI2` |
| BE optional | `BE` / `+5V` |
| RESB optional | `RESB` |

### 3.2 Outputs to assign (worksheet)

| Function | Net | Replaces |
|----------|-----|----------|
| RAM chip enable | `RAM_CE#` | Lab 01 temporary from A15 |
| Cart OE gate | `CART_OE#` | Lab 02 temporary OE logic |
| CPU 245 OE | `CPU_245_OE#` | new |
| CPU 245 DIR | `CPU_245_DIR` | new |
| I/O strobes | `SEL_FE00`, `SEL_FE02`, ... | Lab 04 uses these |
| APU / 1284 selects | stubs OK | later labs |

You do **not** need working JEDEC firmware in KiCad. Place the symbol, wire nets, and add a text box:

```text
U3 CUPL/JEDEC: decode equations TBD
Pin map must match the .PLD file when programmed
```

### 3.3 Minimum equation intent (for when you program U3)

```text
RAM_CE#   active when addr in $0000-$7FFF
CART_OE#  active when PRG read and not $FExx and RWB=1
SEL_FExx  pulse / level for each I/O port write (Lab 04)
CPU_245_OE# / DIR
  enable peripheral bridge on $FExx access (and direction from RWB)
```

Delete the Lab 01/02 **TEMPORARY** gate chips only after these nets are driven by U3 (or by labeled hierarchical pins from U3).

---

## 4. Re-home Lab 01 / Lab 02 selects

1. Disconnect temporary gate outputs from `RAM_CE#` and `CART_OE#`.
2. Connect those nets to U3 outputs.
3. Remove temporary gate ICs from the schematic (or leave DNP with a big X note).
4. Confirm RAM still only for A15=0. Flash OE only for PRG reads. `$FExx` selects neither RAM nor flash.

---

## 5. DIR / OE cheat sheet

| CPU access | `CPU_245_OE#` | `CPU_245_DIR` (A=CPU side) | Meaning |
|------------|---------------|----------------------------|---------|
| Idle / RAM / PRG only | H | x | I/O bridge off |
| Write `$FExx` | L | H (A->B) | CPU drives `PERIPH_D*` |
| Read `$FExx` | L | L (B->A) | Device drives toward CPU |

Exact polarity must match how you wired A vs B. If your DIR definition differs, fix the table on the sheet. Do not leave DIR floating.

---

## 6. Success metrics

### In KiCad (required to pass Lab 03)

| # | Check | What you should see |
|---|--------|---------------------|
| 1 | Annotate | U3 = ATF22V10 (decode), U4 = 74HC245. Temporary gate ICs removed or marked **DNP** with a replace note |
| 2 | ERC | **0 errors**. U3/U4 VCC/GND connected. No floating DIR or OE on U4 |
| 3 | Highlight `RAM_CE#` | Driven from **U3** (not from bare `CPU_A15` gate logic unless that gate is gone). Ends at U2 CE# |
| 4 | Highlight `CART_OE#` | Driven from **U3**. Ends at flash OE# |
| 5 | Highlight `CPU_D0` | Still on CPU/RAM/flash as in Lab 02 |
| 6 | Highlight `PERIPH_D0` | On **U4 B-side** (or A-side if you swapped). Does **not** need to touch flash for Lab 03 |
| 7 | Highlight `CPU_245_OE#` | U3 -> U4 pin 19 |
| 8 | Highlight `CPU_245_DIR` | U3 -> U4 pin 1 |
| 9 | Decoupling | 100 nF at U3 pin 24 and U4 pin 20 |
| 10 | Worksheet text | On sheet: U3 pin functions listed (even if CUPL pin numbers still TBD). Note JEDEC not required for schematic pass |

**Lab 03 KiCad pass** = rows 1-10 all true. Hand-check still: RAM and flash never both drive `CPU_D*` for the same access.

### Optional later (hardware / Sim)

| Test | Expect |
|------|--------|
| Same RAM marker write/read as after Lab 02 | Still works with U3 programmed for `RAM_CE#` |
| Same PRG blink / marker | Still works with U3 programmed for `CART_OE#` |
| `$FExx` write after Lab 04 | Scope `LE_FE*` / `CPU_245_OE#` pulse |
| Sim island C | Decode + CPU HC245 present ([`../../retr01/sim/README.md`](../../retr01/sim/README.md)) |

---

## 7. Done / not done

**Done**

- U3 decode PLD on the netlist
- U4 CPU HC245
- Temporary CE/OE logic retired
- `PERIPH_D*` ready for latches

**Not done**

- Programming the PLD JEDEC
- U5A-U5I HC573 (Lab 04)
- Cart-domain HC245 U14, video HC245
- Other four ATF22V10 roles (VRAM, beam, compositor)

---

## Next

[Lab 04: `$FExx` latches](04_fexx_latches.md)
