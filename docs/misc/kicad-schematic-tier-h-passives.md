# KiCad schematic Tier H: passives, power, clocks

Pin-level links match `apps/sim/tier-h/src/board_schematic.c` and [`schematic-netlist-tier-h.md`](../bringup/schematic-netlist-tier-h.md). Passive **values** match [`passive_bom.md`](../passive_bom.md).

Use **Device** symbols for **R** and **C**. Polarized **E1** uses an electrolytic symbol. Crystal **Y1-Y3** use `Device:Crystal` (HC-49/US). Clock logic is **U04** (74HCU04) and **U74** (74HC74).

---

## 1. Power entry

| Part | Pin / terminal | Net |
| --- | --- | --- |
| **J1** barrel | center / + | `+5V` rail (fuse/TBD per layout) |
| **J1** barrel | shell | `GND` |
| **E1** bulk | + | `+5V` |
| **E1** bulk | - | `GND` |

Star **GND** at the bulk cap return. Branch **+5V** to each IC bypass cluster.

---

## 2. Bypass capacitors (100 nF)

Pattern for each row: **C*n* pin 1** to IC **VCC/VDD**, **C*n* pin 2** to **GND**, **+5V** rail to the same VCC pin. Place each bypass capacitor within 5 mm of its designated IC power pin.

| Cap | IC RefDes | Device and Role | Package | Power Pin (VCC/VDD) | Ground Pin (GND/VSS) | Placement Zone |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | U1 | W65C02S Game CPU | DIP-40 | Pin 8 (VDD) | Pin 21 (VSS) | Central CPU (Top-Center) |
| C2 | U3 | AS6C62256 System RAM | DIP-28 | Pin 28 (VCC) | Pin 14 (VSS) | Zone 1 (Top-Left) |
| C3 | U6 | AS6C62256 Interleaved VRAM | DIP-28 | Pin 28 (VCC) | Pin 14 (VSS) | Zone 3 (Middle-Left) |
| C4 | U41 | AS6C62256 Field SRAM | DIP-28 | Pin 28 (VCC) | Pin 14 (VSS) | Zone 4 (Middle-Right) |
| C5 | UM | AVR128DB28 Master MCU (MCU-M) | SPDIP-28 | Pin 20 (VDD) | Pins 15, 21 (GND) | Middle-Center |
| C6 | US1 | AVR128DB28 Sprite Assist (MCU-S1) | SPDIP-28 | Pin 20 (VDD) | Pins 15, 21 (GND) | Zone 4 (Middle-Right) |
| C7 | US2 | AVR128DB28 Peripheral MCU (MCU-S2) | SPDIP-28 | Pin 20 (VDD) | Pins 15, 21 (GND) | Zone 5 (Bottom-Left) |
| C8 | UPLDX | ATF22V10 Beam X Timing PLD | DIP-24 | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) |
| C9 | UPLDY | ATF22V10 Beam Y Timing PLD | DIP-24 | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) |
| C10 | UPLDV | ATF22V10 Compositor PLD | DIP-24 | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) |
| C11 | U7A | 74HC157 Multiplexer A | DIP-16 | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) |
| C12 | U7B | 74HC157 Multiplexer B | DIP-16 | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) |
| C13 | U7C | 74HC157 Multiplexer C | DIP-16 | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) |
| C14 | U573 | 74HC573 Address Latch | DIP-20 | Pin 20 (VCC) | Pin 10 (GND) | Zone 4 (Middle-Right) |
| C15 | U574 | 74HC574 Scroll X Latch | DIP-20 | Pin 20 (VCC) | Pin 10 (GND) | Zone 3 (Middle-Left) |
| C16 | U24 | AT27C256R Color PROM | DIP-28 | Pin 28 (VCC) | Pin 14 (GND) | Zone 2 (Top-Right) |
| C17 | U130 | MCP130 Reset Supervisor | TO-92 | Pin 2 (VDD) | Pin 3 (VSS) | Zone 1 (Top-Left) |
| C18 | U725 | AD724 Composite Video Encoder | SOIC-16 | Pin 4 (APOS), pin 14 (DPOS) | Pin 2 (AGND), pin 13 (DGND) | Zone 2 (Top-Right) |
| C19 | U04 | 74HCU04 clock inverter | DIP-14 | Pin 14 (VCC) | Pin 7 (GND) | Zone 2 clock island |
| C20 | U74 | 74HC74 DOT divider | DIP-14 | Pin 14 (VCC) | Pin 7 (GND) | Zone 2 clock island |

Place each bypass capacitor adjacent to the IC symbol in the schematic and within 5 mm on the PCB layout. Cart flash, cart EEPROM, and pad ATtiny85 each have one 100 nF on those boards.

---

## 3. Ground ties (selected IC grounds)

Wire **GND** to each part ground pin (in addition to bypass return):

| IC | GND pin name |
| --- | --- |
| U1 | VSS |
| U3, U6, U41 | VSS (SRAM) |
| U24 | GND |
| UPLDY, UPLDV | GND |
| U40, U50 | VSS / GND |

Tie **U725** analog ground and **SCR1** / DAC return per video section in wiring doc.

---

## 4. Crystals and clock logic

Motherboard clocks are HC-49/US crystals plus **U04** (74HCU04) and **U74** (74HC74). Gate roles match [`docs/ic_behavior/74HCU04.md`](../ic_behavior/74HCU04.md) and [`docs/ic_behavior/74HC74.md`](../ic_behavior/74HC74.md). Load caps match [`pcb-component-placement-guide.md`](../bringup/pcb-component-placement-guide.md) (C21/C22 = Y2, C23/C24 = Y3, C25/C26 = Y1).

U04 pin 14 is VCC. U04 pin 7 is GND. U74 pin 14 is VCC. U74 pin 7 is GND.

### Y2 (21.47727 MHz) and DOT

| From | To |
| --- | --- |
| R24 (1 M ohm) | U04 pin 1 (1A) and pin 2 (1Y) |
| Y2 pins 1 and 2 | U04 pin 1 and pin 2 (Pierce tank) |
| C21 pin 1 / pin 2 | Y2 pin 1 / GND |
| C22 pin 1 / pin 2 | Y2 pin 2 / GND |
| U04 pin 3 (2A) | U04 pin 2 (1Y) |
| U04 pin 4 (2Y) | U74 pin 3 (1CLK) |
| U74 pin 6 (1/Q) | U74 pin 2 (1D) |
| U74 pin 5 (1Q) | U74 pin 11 (2CLK) |
| U74 pin 8 (2/Q) | U74 pin 12 (2D) |
| U74 pins 1, 4, 10, 13 (CLR/PRE) | +5V |
| U74 pin 9 (2Q) | **R13** pin 1 (DOT to UPLDX) |

### Y1 (8.000 MHz) and PHI2

| From | To |
| --- | --- |
| R23 (1 M ohm) | U04 pin 5 (3A) and pin 6 (3Y) |
| Y1 pins 1 and 2 | U04 pin 5 and pin 6 (Pierce tank) |
| C25 pin 1 / pin 2 | Y1 pin 1 / GND |
| C26 pin 1 / pin 2 | Y1 pin 2 / GND |
| U04 pin 9 (4A) | U04 pin 6 (3Y) |
| U04 pin 8 (4Y) | **R12** pin 1 (PHI2 to U1) |

Unused U04 inputs pin 11 (5A) and pin 13 (6A) sit on `U04_SPARE_IN`, strapped to GND in the U04 footprint.

### Y3 (3.579545 MHz, AD724 FSC)

| From | To |
| --- | --- |
| C23 pin 1 / pin 2 | Y3 pin 1 / GND |
| C24 pin 1 / pin 2 | Y3 pin 2 / GND |
| Y3 | U725 FIN (pin 3) per Analog Devices crystal recipe |

---

## 5. Color DAC (U24 -> resistors -> video)

PROM outputs **O7** (MSB) .. **O0** (LSB). Resistor values from [`passive_bom.md`](../passive_bom.md).

| PROM pin | Resistor | Value | To |
| --- | --- | --- | --- |
| O7 | R1 | 4.00k | analog **R** gun node |
| O6 | R2 | 4.00k | **R** gun |
| O5 | R3 | 2.00k | **R** gun |
| O4 | R4 | 4.00k | **G** gun node |
| O3 | R5 | 2.00k | **G** gun |
| O2 | R6 | 2.00k | **G** gun |
| O1 | R7 | 1.00k | **B** gun node |
| O0 | R8 | 1.00k | **B** gun |

Termination to GND (75 ohm, ~0.7 Vpp):

| Resistor | Value | From | To |
| --- | --- | --- | --- |
| R9 | 75 ohm | **R** gun node | GND |
| R10 | 75 ohm | **G** gun node | GND |
| R11 | 75 ohm | **B** gun node | GND |

**J2** pins 1-3 tie to **R/G/B** gun nodes (after resistors, per [`hardware.md`](../general/hardware.md) J2 table). **U725** RGB inputs share the same analog nets when populated.

Unused **U24** address pins: tie to **GND** on schematic ([`hardware.md`](../general/hardware.md)).

---

## 6. Series 33 ohm

| Resistor | From | To |
| --- | --- | --- |
| R12 pin 1 | U04 pin 8 (PHI2 buffer) | R12 pin 2 -> **U1** PHI2 |
| R13 pin 1 | U74 pin 9 (DOT, 5.369318 MHz) | R13 pin 2 -> **UPLDX** DOT |
| R14 pin 1 | net **CART_OE#** (from PLD decode) | R14 pin 2 -> cart **OE#** |
| R15 pin 1 | net **CART_WE#** (compositor, gated by CART_ARM) | R15 pin 2 -> cart **WE#** |
| R16 | **UM** I2C **SDA** | **J36** SDA / **U50** SDA |
| R17 | **UM** I2C **SCL** | **J36** SCL / **U50** SCL |

Cart **D0-D7** tie **U1** to **J36** side-B with no series parts ([`hardware.md`](../general/hardware.md) cart table).

---

## 7. Pull-ups

| Resistor | Value | From | To |
| --- | --- | --- | --- |
| R21 | 4.7k (typ) | +5V | **U1** RDY (also **UM** `CPU_RDY` open-drain tie) |
| R22 | 10k (typ) | +5V | **U1** RESB |
| R18 | 4.7k | +5V | **US2** `PAD_DATA` (and pad **DATA** net toward **J3/J4** and **J11/J12**) |
| R19 | 4.7k | +5V | **UM** I2C **SDA** |
| R20 | 4.7k | +5V | **UM** I2C **SCL** |

**CART_WE#** also needs a board **pull-up** to idle-high in play. **CART_ARM** is compositor MAP latch D7, cleared by **RESB** ([`hardware.md`](../general/hardware.md), [`ic-comms-risks.md`](../general/ic-comms-risks.md)).

---

## Related

- [`kicad-schematic-tier-h-wiring.md`](kicad-schematic-tier-h-wiring.md): digital interconnect
- [`kicad-schematic-tier-h-symbols.md`](kicad-schematic-tier-h-symbols.md): footprints and refdes
