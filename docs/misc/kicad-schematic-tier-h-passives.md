# KiCad schematic Tier H: passives, power, clocks

Pin-level links match `apps/sim/tier-h/src/board_schematic.c` and [`schematic-netlist-tier-h.md`](../bringup/schematic-netlist-tier-h.md). Passive **values** match [`passive_bom.md`](../passive_bom.md).

Use **Device** symbols for **R** and **C**. Polarized **E1** uses an electrolytic symbol. Crystal **Y1-Y3** use `Device:Crystal` or the canned oscillator symbol that matches the footprint.

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

Pattern for each row: **C*n* pin 1** to IC **VCC/VDD**, **C*n* pin 2** to **GND**, **+5V** rail to the same VCC pin.

| Cap | IC | VCC pin name |
| --- | --- | --- |
| C1 | U1 W65C02S | VDD |
| C2 | U3 sys RAM | VCC |
| C3 | U6 VRAM | VCC |
| C4 | U41 field SRAM | VCC |
| C5 | UM MCU-M | VCC |
| C6 | US1 MCU-S1 | VCC |
| C7 | US2 MCU-S2 | VCC |
| C8 | UPLDX | VCC |
| C9 | UPLDY | VCC |
| C10 | UPLDV | VCC |
| C11 | U7A | VCC |
| C12 | U7B | VCC |
| C13 | U7C | VCC |
| C14 | U573 | VCC |
| C15 | U574 | VCC |
| C16 | U24 color PROM | VCC |
| C17 | (optional 74HC14) | +5V only if populated |
| C18 | (AD724 **U725**) | +5V only if populated |
| C19 | U40 cart flash | VDD (cart sheet or J36 stub) |
| C20 | U50 24C64 | VCC (cart sheet or J36 stub) |
| C21 | UPAD1 pad MCU | VCC (pad PCB, not mobo) |

Place each bypass **adjacent** to the IC symbol in the schematic (same rule on PCB).

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

## 4. Crystals and load caps

Sim uses **Y1/Y2** crystals plus functional oscillator symbols for PHI2/DOT paths. On schematic, either draw **crystal + loads** or **canned osc** symbols with the same net names.

### Y1 (8 MHz, CPU PHI2 path)

| From | To |
| --- | --- |
| Y1 pin 14 (or osc VCC) | +5V |
| Y1 pin 7 | GND |
| C22 pin 1 | Y1 pin 1 |
| C22 pin 2 | GND |
| C23 pin 1 | Y1 pin 8 |
| C23 pin 2 | GND |
| Osc **PHI2** out | **R12** pin 1 (see series section) |

### Y2 (DOT, ~5.369318 MHz)

| From | To |
| --- | --- |
| Y2 pin 14 | +5V |
| Y2 pin 7 | GND |
| C24, C25 | load pins 1/8 of Y2 to GND (same pattern as C22/C23) |
| Osc **DOT** out | **R13** pin 1 |

### Y3 (14.31818 MHz, AD724 FSC)

| From | To |
| --- | --- |
| Y3 pin 7 | GND |
| C26, C27 | Y3 pins 1 and 8 to GND |
| Y3 pin 8 (or osc out) | net **`FSC_XTAL`** -> **U725** FSC input |

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
| R12 pin 1 | Y1/osc **PHI2** | R12 pin 2 -> **U1** PHI2 |
| R13 pin 1 | Y2/osc **DOT** | R13 pin 2 -> **UPLDX** DOT |
| R14-R21 | **U1** D0-D7 | **J36** cart D0-D7 (via flash symbol or socket net names) |
| R22 pin 1 | net **CART_OE#** (from PLD decode) | R22 pin 2 -> cart **OE#** |
| R23 pin 1 | net **CART_WE#** (MCU-M bridge) | R23 pin 2 -> cart **WE#** |
| R24 | **UM** I2C **SDA** | **J36** SDA / **U50** SDA |
| R25 | **UM** I2C **SCL** | **J36** SCL / **U50** SCL |

Cart data series (**R14-R21**) sit on the motherboard between **U1** and **J36** side-B D0-D7 pins ([`hardware.md`](../general/hardware.md) cart table).

---

## 7. Pull-ups

| Resistor | Value | From | To |
| --- | --- | --- | --- |
| R29 | 4.7k (typ) | +5V | **U1** RDY (also **UM** `CPU_RDY` open-drain tie) |
| R30 | 10k (typ) | +5V | **U1** RESB |
| R26 | 4.7k | +5V | **US2** `PAD_DATA` (and pad **DATA** net toward **J3/J4**) |
| R27 | 4.7k | +5V | **UM** I2C **SDA** |
| R28 | 4.7k | +5V | **UM** I2C **SCL** |

**CART_WE#** also needs a board **pull-up** to idle-high in play ([`hardware.md`](../general/hardware.md), [`ic-comms-risks.md`](../general/ic-comms-risks.md)).

---

## Related

- [`kicad-schematic-tier-h-wiring.md`](kicad-schematic-tier-h-wiring.md): digital interconnect
- [`kicad-schematic-tier-h-symbols.md`](kicad-schematic-tier-h-symbols.md): footprints and refdes
