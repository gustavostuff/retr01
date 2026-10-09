# Main PCB: crystals and clock island

Three HC-49/US cans in Zone 2 (top-right). **Y1** and **Y2** form analog Pierce loops on **U04**. **Y3** forms a second analog loop on **U725** FIN.

Each crystal pin copper run is a few millimeters. The whole loop (can, both load caps, feedback part if present, and the two IC pads) stays inside about **20 mm**. Those nets sit on **layer 4**. Digital clocks leave the island after the buffer.

Load-cap values: [capacitors.md](capacitors.md). Feedback **R23/R24** and series **R12/R13**: [resistors.md](resistors.md). Layer rules: [main-pcb-layers.md](../main-pcb-layers.md). Zones: [README.md](README.md).

---

## Copper rules

HC-49/US is a 2-pin can. Pin 1 and pin 2 are the quartz terminals.

A load cap sits on each crystal pin. Cap pin 1 lands on that crystal pin. Cap pin 2 is `GND`.

Y1 and Y2 also need a 1 M ohm across the same two U04 pads as the crystal. That resistor is the Pierce bias, not a series part.

| Loop | Crystal pin 1 copper | Crystal pin 2 copper | Length |
|------|----------------------|----------------------|--------|
| Y1 | U04 pin 5, **C25** pin 1, **R23** | U04 pin 6, **C26** pin 1, **R23** | Few mm per pin. Loop under 20 mm |
| Y2 | U04 pin 1, **C21** pin 1, **R24** | U04 pin 2, **C22** pin 1, **R24** | Few mm per pin. Loop under 20 mm |
| Y3 | U725 pin 3 (FIN), **C23** pin 1 | **C24** pin 1 only (`FSC_XTAL`) | Few mm per pin. Can beside U725 |

No via in a Pierce or FSC tank if the pads can share layer 4. No digital bus under or beside those pads. No long stub off a crystal pin.

PHI2 and DOT are not crystal nets. They start after U04 pin 8 and U74 pin 9, then pass **R12** and **R13**. Those series parts sit at the buffer outputs, not at the cans.

---

## Y1, 8.000 MHz (PHI2)

Can stands next to U04 pins **5** and **6** (gate 3).

```text
Y1 pin 1 ---- U04 pin 5 (3A) ---- C25 ---- GND
              |
              R23 1M
              |
Y1 pin 2 ---- U04 pin 6 (3Y) ---- C26 ---- GND
                    |
                    U04 pin 9 (4A)
                    U04 pin 8 (4Y) ---- R12 ---- U1 pin 37 PHI2
```

Gate 4 is the digital buffer. That copper may leave the 20 mm island. The crystal, **C25**, **C26**, and **R23** stay inside it.

---

## Y2, 21.47727 MHz (DOT master)

Can stands next to U04 pins **1** and **2** (gate 1).

```text
Y2 pin 1 ---- U04 pin 1 (1A) ---- C21 ---- GND
              |
              R24 1M
              |
Y2 pin 2 ---- U04 pin 2 (1Y) ---- C22 ---- GND
                    |
                    U04 pin 3 (2A)
                    U04 pin 4 (2Y) ---- U74 pin 3 (1CLK)
```

U74 divides by 4. **DOT** leaves U74 pin 9 through **R13** to UPLDX. That digital run may leave the island. The crystal, **C21**, **C22**, and **R24** stay inside the 20 mm loop.

---

## Y3, 3.579545 MHz (AD724 FSC)

Can stands next to **U725**, not next to U04. AD724 runs the oscillator on-chip.

```text
Y3 pin 1 ---- U725 pin 3 (FIN) ---- C23 ---- GND
Y3 pin 2 ---- C24 ---- GND
```

Y3 pin 2 has no second AD724 pad on this board. **C23** and **C24** still sit on the two crystal pins.

---

## U04, 74HCU04 (DIP-14)

Clock inverter. Bypass **C19** at pin 14.

| U04 pin | Role | Copper |
|--------:|------|--------|
| 14 / 7 | VCC / GND | `+5V` / `GND` and **C19** |
| 1 | 1A, Y2 Pierce | Y2 pin 1, **C21**, **R24** |
| 2 | 1Y, Y2 Pierce | Y2 pin 2, **C22**, **R24**, U04 pin 3 |
| 3 to 4 | DOT buffer | U74 pin 3 |
| 5 | 3A, Y1 Pierce | Y1 pin 1, **C25**, **R23** |
| 6 | 3Y, Y1 Pierce | Y1 pin 2, **C26**, **R23**, U04 pin 9 |
| 9 to 8 | PHI2 buffer | **R12** to U1 pin 37 |
| 11, 13 | spare inputs | `U04_SPARE_IN`, strapped to GND in the U04 footprint |
| 10, 12 | spare outs | NC |

---

## U74, 74HC74 (DIP-14)

DOT divider. Sits against U04 in the same Zone 2 cluster. Bypass **C20** at pin 14.

| Stage | Pins | Net |
|-------|------|-----|
| VCC | 14 | `+5V` and **C20** |
| GND | 7 | `GND` |
| FF1 | CLK=3, Q=5, /Q=6 into D=2 | 21 MHz to 10.7 MHz |
| FF2 | CLK=11, Q=9, /Q=8 into D=12 | **DOT** (about 5.37 MHz) |
| PRE# / CLR# | 1, 4, 10, 13 | `U74_PRE_CLR#`, strapped to VCC in the U74 footprint |

DOT leaves through **R13** 33 ohm toward Beam X ([ics.md](ics.md) UPLDX).
