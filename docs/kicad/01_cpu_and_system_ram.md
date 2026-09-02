# Lab 01: CPU and system RAM

**Goal:** Draw power, an 8 MHz clock, reset, the **W65C02S**, and **system SRAM**, then wire them so the CPU can read and write `$0000-$7FFF`.

**Pass:** Complete the **Success metrics** section (KiCad table required). Optional hardware comes later.

**Do not add yet:** cart, PLD, HC245, latches, video, AVRs.

**Related:** [`../hardware.md`](../hardware.md) island C. [`../../hw/md/W65C02S.md`](../../hw/md/W65C02S.md). [`../../hw/md/AS6C62256.md`](../../hw/md/AS6C62256.md). [`../passive_rf_etc.md`](../passive_rf_etc.md).

---

## 0. Ideas in plain language

The **6502** is your game process. It puts an address on `CPU_A*`, sets `RWB` (1 = read, 0 = write), and uses `CPU_D*` as a shared 8-bit bus.

**System RAM** is a 32 KB array at `$0000-$7FFF`. On Retr01, A15 = 0 means "this half of memory is RAM." A15 = 1 will later mean cart PRG / I/O (Lab 02+).

`CE#` on the SRAM means "chip enable, active when **low**." Think `if (!ce_n) device_selected`.

---

## 1. Create the KiCad project

1. Install KiCad 8.x.
2. **File -> New Project** in `hw/kicad/retr01_mobo/` (create folders as needed).
3. Open the root schematic.
4. **Page settings:** title `Retr01 Motherboard`, revision `lab-01`.
5. **Insert -> Hierarchical Sheet**.
   - Sheet name: `PowerCpuRam`
   - File: `sheets/01_power_cpu_ram.kicad_sch`
6. Double-click the sheet box to edit inside it. Do Lab 01 work **inside** this sheet.

Add a project note on the root sheet: `Labs: see docs/kicad/`.

---

## 2. Power and ground (minimum)

You only need a clean 5 V rail for this lab.

| What to place | Value / part | Nets |
|---------------|--------------|------|
| Power symbol | `+5V` | `+5V` |
| Power symbol | `GND` | `GND` |
| `PWR_FLAG` on `+5V` | (KiCad ERC) | after your supply |
| `PWR_FLAG` on `GND` | | |
| Barrel jack (optional now) | 2.1 mm | tip -> `+5V`, sleeve -> `GND` |
| Bulk cap | 100 uF | `+5V` to `GND` |
| Local ceramic | 100 nF | next to each IC VCC |

For a first schematic, a **+5V** PWR_FLAG is enough even if the barrel comes later. Real entry PPTC / ferrite land in a later polish pass ([`../passive_rf_etc.md`](../passive_rf_etc.md)).

---

## 3. Clock and reset (minimum)

| What | Net | Notes |
|------|-----|-------|
| 8.000 MHz canned oscillator | out -> series **33 ohm** -> `PHI2` | Feeds CPU pin 37 |
| Oscillator VCC / GND | `+5V` / `GND` | |
| Pull-up | 10 kohm `+5V` to `RESB` | |
| Reset pushbutton optional | `RESB` to `GND` when pressed | Or leave `RESB` high with pull-up for "always run" |

Also pull up for later (place now, even if unused):

| Net | Pull-up |
|-----|---------|
| `IRQB` | 10k to `+5V` |
| `NMIB` | 10k to `+5V` |
| `RDY` | 10k to `+5V` |

---

## 4. Place U1: W65C02S

Use symbol `CPU_6502` / `W65C02S` from a library, or create one from [`../../hw/md/W65C02S.md`](../../hw/md/W65C02S.md). Footprint: DIP-40.

### 4.1 Power and straps

| Pin | Name | Connect to |
|-----|------|------------|
| 8 | VDD | `+5V` + 100 nF to `GND` |
| 21 | VSS | `GND` |
| 36 | BE | `+5V` (bus drivers on) |
| 38 | SOB | `+5V` (unused feature held inactive) |
| 35 | NC | leave open |
| 3 | PHI1O | leave open |
| 39 | PHI2O | leave open |

### 4.2 Clock and control

| Pin | Name | Net |
|-----|------|-----|
| 37 | PHI2 | `PHI2` |
| 40 | RESB | `RESB` |
| 4 | IRQB | `IRQB` |
| 6 | NMIB | `NMIB` |
| 2 | RDY | `RDY` |
| 34 | RWB | `RWB` |

### 4.3 Address bus

| Pins | Names | Nets |
|------|-------|------|
| 9-20 | A0-A11 | `CPU_A0` .. `CPU_A11` |
| 22-25 | A12-A15 | `CPU_A12` .. `CPU_A15` |

In KiCad you can draw a bus `CPU_A[15..0]` and attach aliases.

### 4.4 Data bus (watch pin order)

On the PDIP, **D0 is pin 33**, then D1..D7 down to pin 26.

| Pin | Name | Net |
|-----|------|-----|
| 33 | D0 | `CPU_D0` |
| 32 | D1 | `CPU_D1` |
| 31 | D2 | `CPU_D2` |
| 30 | D3 | `CPU_D3` |
| 29 | D4 | `CPU_D4` |
| 28 | D5 | `CPU_D5` |
| 27 | D6 | `CPU_D6` |
| 26 | D7 | `CPU_D7` |

Optional test pads on `SYNC` (pin 7), `VPB` (1), `MLB` (5). Not required for Lab 01.

---

## 5. Place U2: AS6C62256 system RAM

Same part family as VRAM later. This instance is **CPU-only** system RAM.

Footprint: DIP-28. Pinout: [`../../hw/md/AS6C62256.md`](../../hw/md/AS6C62256.md).

| SRAM pin | Connect to |
|----------|------------|
| A0-A14 | `CPU_A0` .. `CPU_A14` |
| DQ0-DQ7 | `CPU_D0` .. `CPU_D7` |
| VCC (28) | `+5V` + 100 nF |
| VSS (14) | `GND` |
| CE# (20) | `RAM_CE#` (see next section) |
| OE# (22) | `RAM_OE#` |
| WE# (27) | `RAM_WE#` |

---

## 6. Temporary chip-select logic (Lab 01 only)

You do not have the decode PLD yet (Lab 03). Use a **temporary** rule:

```text
RAM selected when A15 is 0
  RAM_CE# = A15          (active low enable when A15 low works if CE# = A15)
```

Wire:

| Net | Driven by | Meaning |
|-----|-----------|---------|
| `RAM_CE#` | `CPU_A15` | RAM on for `$0000-$7FFF` |
| `RAM_OE#` | active when read | see below |
| `RAM_WE#` | active when write | see below |

### 6.1 OE# and WE# without a PLD

SRAM truth table ([`../../hw/md/AS6C62256.md`](../../hw/md/AS6C62256.md)):

| Mode | CE# | OE# | WE# |
|------|-----|-----|-----|
| Read | L | L | H |
| Write | L | X (prefer H) | L |

**Temporary discrete (one 74HC00 or 74HC32 + inverter, or a spare 74HC14):**

Software view:

```text
if (ram_selected && cpu_read)  assert OE#
if (ram_selected && cpu_write) assert WE#
```

Practical Lab 01 wiring many people use on a bench:

| Net | Temporary equation |
|-----|--------------------|
| `RAM_CE#` | `CPU_A15` |
| `RAM_OE#` | low when `RWB=1` and RAM selected (gate), else high |
| `RAM_WE#` | low when `RWB=0` and RAM selected and PHI2 high enough for write, else high |

If gating feels heavy for day one, you may temporarily:

- Tie `RAM_OE#` to `GND` (always output-enabled when CE# low). **Only** if you never float-fight the CPU on writes. Safer: gate OE# with `RWB`.
- Drive `RAM_WE#` from a NAND of `not RWB` and `not RAM_CE#` (write when selected and RWB=0).

Place whatever small HC gate package you use. Label the sheet **TEMPORARY DECODE - REPLACE IN LAB 03**.

---

## 7. Success metrics

### In KiCad (required to pass Lab 01)

Do these in order. Check each box mentally or on paper.

| # | Check | What you should see |
|---|--------|---------------------|
| 1 | **Annotate** schematic | U1 = W65C02S, U2 = AS6C62256 (or your chosen refdes). No `?` refdes left |
| 2 | Run **Electrical Rules Checker** | **0 errors**. Fix power-pin / PWR_FLAG issues first. Warnings about unused CPU pins (SYNC, etc.) are OK if those pins are intentionally open |
| 3 | Highlight net `CPU_A0` | Highlight shows **U1 A0** and **U2 A0** (and bus entry if used). Repeat spot-check `CPU_A7` and `CPU_A14` |
| 4 | Highlight net `CPU_A15` | Shows **U1 A15** and the temporary `RAM_CE#` driver input / net join |
| 5 | Highlight net `CPU_D0` | Shows **U1 D0 (pin 33)** and **U2 DQ0**. Spot-check `CPU_D7` |
| 6 | Highlight `PHI2` | Oscillator output path (through series R if drawn) and **U1 pin 37** |
| 7 | Highlight `RESB` | Pull-up to `+5V`, **U1 pin 40**, button if present |
| 8 | Highlight `RAM_CE#` | Reaches **U2 CE#**. Driven only by the temporary A15 rule (not floating) |
| 9 | Highlight `+5V` and `GND` | Both U1 and U2 power pins connected. 100 nF visible next to each VCC |
| 10 | Sheet note | Text on sheet says temporary decode will be replaced in Lab 03 |

**Lab 01 KiCad pass** = rows 1-10 all true. Export a PDF of the sheet for your notes.

### Optional later (not required to start Lab 02)

KiCad does not run the 6502. Functional R/W needs something to fetch (Lab 02) or a fixture.

| Test | Expect |
|------|--------|
| Breadboard island with CPU in reset, bit-bang SRAM A/D/CE/OE/WE from a MCU | Write `$55` to an address, read back `$55` |
| After Lab 02 + programmed flash | Tiny PRG writes `$AA` to `$0200`, reads back `$AA` |
| Sim | Island C smoke assumes PRG exists ([`../../retr01/sim/README.md`](../../retr01/sim/README.md)). Use after cart path exists |

---

## 8. Done / not done

**Done**

- Project + hierarchical sheet
- `+5V` / `GND` / `PHI2` / `RESB`
- U1 W65C02S fully pinned for Lab 01
- U2 system RAM on `CPU_A*` / `CPU_D*`
- Temporary `RAM_CE#` from `CPU_A15`

**Not done (on purpose)**

- Cart flash / 36-pin connector
- ATF22V10 decode
- HC245 isolation
- Any `$FExx` latch

---

## Next

[Lab 02: Cart PRG read](02_cart_prg_read.md)
