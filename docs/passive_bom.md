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

**Ceramic capacitors (29 = 23 + 6)**

| Qty | What | Why |
|----:|------|-----|
| 23 | **100 nF** bypass | One per VCC site: 20 on main PCB, 2 on cart, 1 on pad |
| 6 | Crystal load (~18-20 pF class) | **2 per crystal** on Y1 / Y2 / Y3. Same CCAP body as bypass, different value |

23 CCAPs are 100 nF bypass. Six are crystal loads.

**Electrolytic (1)**

| Qty | What | Why |
|----:|------|-----|
| 1 | **220 uF** | Single 5 V entry bulk cap (ECAP sprite) |

**Resistors (24 = 11 + 6 + 5 + 2)**

Only **11** are the analog video DAC network. The rest are digital:

| Qty | What | Why |
|----:|------|-----|
| 11 | Color DAC (4k / 2k / 1k / 75 ohm) | Weighted R/G/B + 75 ohm terminations to ~0.7 Vpp ([`hardware.md`](general/hardware.md) Video out) |
| 6 | **33 ohm** series | **2** clocks (PHI2, DOT) + **4** cart edge (**OE#**, **WE#**, **SDA**, **SCL**) |
| 5 | Pull-ups | Pad **DATA**, I2C **SDA/SCL**, CPU **RDY**, **RESB** |
| 2 | **1 M ohm** feedback | Biases the two 74HCU04 crystal inverter gates (Y1 and Y2) into linear active mode |

Video alone does **not** need 24 resistors. Clock and cart-control damping + pull-ups + crystal feedback do. Cart **D[7:0]** has no series parts.

---

## Scope counts (silicon that needs bypass)

| Domain | ICs with VCC | Notes |
|--------|-------------:|-------|
| Main PCB | 20 | Motherboard silicon footprints (19 counted ICs + MCP130 supervisor) |
| Cart module | 2 | Cart flash and save EEPROM |
| Controller pad | 1 | Pad ATtiny85 |
| **Total bypass sites** | **23** | One **100 nF** per VCC pin cluster |

---

## Crystals

| Qty | Ref / use | Value | Notes |
|----:|-----------|-------|-------|
| 1 | Y1 / CPU PHI2 | HC-49/US **8.000 MHz** | Driven by two 74HCU04 inverter gates |
| 1 | Y2 / DOT | HC-49/US **21.47727 MHz** | Driven by two 74HCU04 inverter gates, divided by 4 via 74HC74 to 5.369318 MHz |
| 1 | Y3 / AD724 FSC | HC-49/US **3.579545 MHz** | Connected to AD724 on-chip oscillator pins (NTSC subcarrier) |

**Load capacitors:** **2 per crystal** (**6** total). Exact pF from the crystal CL rating (typically 18 to 20 pF each).

DB28 parts run on **internal HFOSC @ 24 MHz**. No extra MCU crystals on the locked BOM.

### Discrete clock generation

The motherboard clock network uses standard through-hole quartz crystals and dedicated 74HC logic:
- **Y1 (8.000 MHz):** An 8.000 MHz crystal with an unbuffered **74HCU04** inverter gate and 1 M ohm feedback resistor forms the Pierce oscillator, buffered by a second inverter gate to drive the CPU PHI2 clock net.
- **Y2 (21.47727 MHz):** A 21.47727 MHz crystal with a second 74HCU04 inverter gate and 1 M ohm feedback resistor forms the master oscillator, buffered by an inverter gate, and divided by 4 using both stages of an **SN74HC74** dual D-type flip-flop. This produces a clean, symmetrical 50% duty-cycle **5.369318 MHz** dot clock.
- **Y3 (3.579545 MHz):** Directly drives the internal oscillator pins of the **AD724** composite video encoder with two load capacitors.

On solderless breadboards, an Si5351A breakout board or the same 21.47727 MHz 74HCU04/74HC74 circuit supplies the 5.369318 MHz clock directly.

---

## Capacitors

| Qty | Value | Role |
|----:|-------|------|
| 23 | **100 nF** ceramic | Bypass: 20 on main PCB, 2 on cart, 1 on pad |
| 1 | **220 uF** electrolytic (or polymer) | Entry bulk at 5 V input |
| 6 | Crystal load (see above) | Y1/Y2/Y3 (18 to 20 pF) |

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
| 4 | Cart **OE#**, **WE#**, **SDA**, **SCL** |

**Subtotal series 33 ohm: 6**

### Pull-ups

| Qty | Value | Net |
|----:|-------|-----|
| 1 | **4.7k** | Pad UART **DATA** (MCU-S2). Host side. OD half-duplex |
| 2 | **4.7k** | Cart I2C **SDA** / **SCL** (MCU-M OD) |
| 1 | **4.7k** (or **10k**) | CPU **RDY** idle high unless MCU stalls |
| 1 | **10k** | **RESB** / reset rail idle high |

**Subtotal pull-ups: 5**

### Crystal feedback

| Qty | Value | Net |
|----:|-------|-----|
| 2 | **1M** | Inverter input to output on Y1 and Y2 Pierce oscillator stages |

**Subtotal feedback: 2**

---

## Roll-up (this scope)

| Class | Qty |
|-------|----:|
| Crystals | 3 |
| Crystal load caps | 6 |
| 100 nF bypass | 23 |
| 220 uF bulk | 1 |
| DAC resistors | 11 |
| 33 ohm series | 6 |
| Pull-ups | 5 |
| Feedback resistors (1M) | 2 |
| **Passive line items (sum of qtys)** | **57** |

---

## Tier H sim netlist

Pin-level links for bypass, bulk, crystals, DAC, series **33 ohm**, and pull-ups live in `apps/sim/tier-h/src/board_schematic.c`. See [`docs/bringup/schematic-netlist-tier-h.md`](bringup/schematic-netlist-tier-h.md) for refdes mapping and known gaps (**AD724** not seated in sim).

---

## Related

[`docs/general/hardware.md`](general/hardware.md) | [`apps/sim/README.md`](../apps/sim/README.md)
