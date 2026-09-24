# Retr01 passive BOM (full console)

Calculated passives for **one full Retr01 (non Nano)** bring-up set:

- main PCB (shared arcade/console motherboard, console I/O populated)
- **1** cart module
- **1** Retr01-C controller pad (ATtiny85 TRS, not arcade microswitch harness)

Active ICs stay in [`docs/general/hardware.md`](general/hardware.md). Connectors (`J*`, cart edge, TRS jacks) are listed there too. This file is resistors, capacitors, and crystals only.

Arcade-only series **47 ohm** button resistors and the second pad jack population are **out of scope**.

SoT values come from [`docs/general/hardware.md`](general/hardware.md) (Video out / passives, Controllers) and `hw/md/` when present.

---

## Why the counts look high

The Sim tray shows **many identical CCAP / R sprites**. That is one part per locked net, not one part per function.

**Ceramic capacitors (27 = 21 + 6)**

| Qty | What | Why |
|----:|------|-----|
| 21 | **100 nF** bypass | One per VCC site: 16 mobo ICs + **74HC14** + **AD724** + cart flash + cart EEPROM + pad ATtiny |
| 6 | Crystal load (~22 pF class) | **2 per crystal** on Y1 / Y2 / Y3. Same CCAP body as bypass, different value |

So most CCAPs are **decoupling**. The extras are **crystal loads**, not more bypass.

**Electrolytic (1)**

| Qty | What | Why |
|----:|------|-----|
| 1 | **220 uF** | Single 5 V entry bulk cap (ECAP sprite) |

**Resistors (30 = 11 + 14 + 5)**

Only **11** are the analog video DAC network. The rest are digital:

| Qty | What | Why |
|----:|------|-----|
| 11 | Color DAC (4k / 2k / 1k / 75 ohm) | Weighted R/G/B + 75 ohm terminations to ~0.7 Vpp ([`hardware.md`](general/hardware.md) Video out) |
| 14 | **33 ohm** series | **2** clocks (PHI2, DOT) + **12** cart edge (**D[7:0]**, OE#, WE#, SDA, SCL) |
| 5 | Pull-ups | Pad **DATA**, I2C **SDA/SCL**, CPU **RDY**, **RESB** |

Video alone does **not** need 30 resistors. Cart damping + pull-ups do.

---

## Scope counts (silicon that needs bypass)

| Domain | ICs with VCC | Notes |
|--------|-------------:|-------|
| Main PCB (locked 16) | 16 | CPU, 3x DB28, 3x SRAM, 3x PLD, 3x HC157, HC573, HC574, color PROM |
| Outside 18 on PCB | 2 | **74HC14** (optional), **AD724** |
| Cart module | 2 | SST39SF040 + 24C64 |
| Pad (x1) | 1 | ATtiny85 |
| **Total bypass sites** | **21** | one **100 nF** per VCC pin cluster |

---

## Crystals

| Qty | Ref / use | Value | Notes |
|----:|-----------|-------|-------|
| 1 | Y1 / CPU PHI2 | Abracon ACH **8.000 MHz** | Buffered via board clock path (HC14 / OSC) |
| 1 | Y2 / DOT | Abracon ACH **5.369318 MHz** | Beam / PPU clock |
| 1 | Y3 / AD724 FSC | Abracon ACH **14.31818 MHz** | NTSC encoder subcarrier |

**Load capacitors:** **2 per crystal** (**6** total). Exact pF from the Abracon CL rating (often ~18-22 pF each).

DB28 parts run on **internal HFOSC @ 24 MHz**. No extra MCU crystals on the locked BOM.

---

## Capacitors

| Qty | Value | Role |
|----:|-------|------|
| 21 | **100 nF** ceramic | Bypass: 18 PCB (16+HC14+AD724) + 2 cart + 1 pad |
| 1 | **220 uF** electrolytic (or polymer) | Entry bulk at 5 V input |
| 6 | Crystal load (see above) | Y1/Y2/Y3 |

AD724 may want extra datasheet filter / coupling caps beyond the single VCC bypass. Treat those as app-note add-ons, not locked here.

---

## Resistors

### Color DAC (AT27C256R, 1% metal film)

Packing `(R<<5)|(G<<2)|B`. LSB to MSB.

| Qty | Value | Gun |
|----:|-------|-----|
| 2 | **4.00k** | R LSB, G LSB |
| 3 | **2.00k** | R mid, G mid, B LSB |
| 3 | **1.00k** | R MSB, G MSB, B MSB |
| 3 | **75.0 ohm** | R/G/B to GND (~0.7 Vpp) |

**Subtotal DAC: 11**

### Series damping (**33 ohm**)

| Qty | Nets |
|----:|------|
| 2 | PHI2, DOT |
| 12 | Cart **D[7:0]**, **OE#**, **WE#**, **SDA**, **SCL** |

**Subtotal series 33 ohm: 14**

### Pull-ups

| Qty | Value | Net |
|----:|-------|-----|
| 1 | **4.7k** | Pad UART **DATA** (MCU-S2). Host side. OD half-duplex |
| 2 | **4.7k** | Cart I2C **SDA** / **SCL** (MCU-M OD) |
| 1 | **4.7k** (or **10k**) | CPU **RDY** idle high unless MCU stalls |
| 1 | **10k** | **RESB** / reset rail idle high (after HC14 conditioning) |

**Subtotal pull-ups: 5**

---

## Roll-up (this scope)

| Class | Qty |
|-------|----:|
| Crystals | 3 |
| Crystal load caps | 6 |
| 100 nF bypass | 21 |
| 220 uF bulk | 1 |
| DAC resistors | 11 |
| 33 ohm series | 14 |
| Pull-ups | 5 |
| **Passive line items (sum of qtys)** | **61** |

---

## Tier H sim netlist

Pin-level links for bypass, bulk, crystals, DAC, series **33 ohm**, and pull-ups live in `apps/sim/tier-h/src/board_schematic.c`. See [`docs/bringup/schematic-netlist-tier-h.md`](bringup/schematic-netlist-tier-h.md) for refdes mapping and known gaps (**AD724**, **74HC14** not seated in sim).

---

## Related

[`docs/general/hardware.md`](general/hardware.md) | [`apps/sim/README.md`](../apps/sim/README.md)
