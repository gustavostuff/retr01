# Main PCB: crystals and clock island

**Y1**, **Y2**, and **Y3** plus the compact analog loop around **U04** / **U74**. That cluster stays under about 20 mm.

Load-cap tables: [capacitors.md](capacitors.md). Pierce **R23/R24** and clock series **R12/R13**: [resistors.md](resistors.md). Zones: [README.md](README.md).

---

## Y2, 21.47727 MHz (DOT master)

HC-49/US vertical. Zone 2 clock island.

Load caps **C21/C22**. Feedback **R24** (1M across U04 pins **1** and **2**). Crystal pins tie to U04 pins **1** and **2** (Pierce gate 1). U74 divides by 4 to **DOT** (~5.37 MHz).

---

## Y1, 8.000 MHz (PHI2)

HC-49/US vertical. Zone 2 clock island.

Load caps **C25/C26**. Feedback **R23** (1M across U04 pins **5** and **6**). Crystal pins tie to U04 pins **5** and **6** (Pierce gate 3). Buffered PHI2 leaves through **R12** to U1 pin 37.

---

## Y3, 3.579545 MHz (AD724 FSC)

HC-49/US vertical. Zone 2 at **U725**.

Load caps **C23/C24**. Drives U725 FIN through the AD724 on-chip oscillator.

---

## U04, 74HCU04 (DIP-14)

Clock inverter. Bypass **C19** at pin 14.

| U04 pin | Role | Destination |
|--------:|------|-------------|
| 14 / 7 | VCC / GND | `+5V` / `GND` and **C19** |
| 1 to 2 | DOT Pierce | Y2 |
| 3 to 4 | DOT buffer | U74 clock input |
| 5 to 6 | PHI2 Pierce | Y1 |
| 9 to 8 | PHI2 buffer | **PHI2** net (then R12 to CPU) |
| 11,13 | spare inputs | `U04_SPARE_IN`, strapped to GND in the U04 footprint |
| 10,12 | spare outs | NC |

---

## U74, 74HC74 (DIP-14)

DOT divider. Bypass **C20** at pin 14.

| Stage | Pins | Net |
|-------|------|-----|
| VCC | 14 | `+5V` and **C20** |
| GND | 7 | `GND` |
| FF1 | CLK=3, Q=5, /Q=6 into D=2 | 21 MHz to 10.7 MHz |
| FF2 | CLK=11, Q=9, /Q=8 into D=12 | **DOT** (about 5.37 MHz) |
| PRE# / CLR# | 1, 4, 10, 13 | `U74_PRE_CLR#`, strapped to VCC in the U74 footprint |

DOT leaves through **R13** 33 ohm toward Beam X ([ics.md](ics.md) UPLDX).

---

## Load and feedback summary

| Crystal | Loads | Feedback | Logic |
|---------|-------|----------|-------|
| Y2 21.47727 MHz | C21, C22 | R24 on U04 1-2 | U04 gates 1-2, then U74 /4 |
| Y1 8.000 MHz | C25, C26 | R23 on U04 5-6 | U04 gates 3-4, then R12 |
| Y3 3.579545 MHz | C23, C24 | AD724 on-chip | U725 FIN |
