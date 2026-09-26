# KiCad schematic Tier H: signal wiring

Digital and connector wiring for the motherboard schematic. Passives and power are in [`kicad-schematic-tier-h-passives.md`](kicad-schematic-tier-h-passives.md). MCU port names are in [`hardware.md`](../general/hardware.md) (Pin freeze essentials).

PLD **fuse equations** are not duplicated here. Schematic work uses **named nets** into **UPLDX**, **UPLDY**, and **UPLDV** that match the programmed JEDEC (beam, compositor, decode). Sim helpers **UPLDA** / **UPLDB** are not BOM mounts.

---

## 1. CPU **U1** (W65C02S)

### Address and data (parallel bus)

| U1 pin group | Connect to |
| --- | --- |
| A0-A15 | **U3** RAM A0-A15, **U4** PRG A0-A15, **U6** VRAM A0-A15, **UPLDV** / decode PLD address inputs, **J36** cart A0-A13 (Side A pins 4-17), **UM** sample inputs as needed |
| D0-D7 | **U3** DQ0-DQ7, **U4** DQ0-DQ7, **U6** DQ0-DQ7, **UM** CPU_D0-D7 (PA/PD), **US2** pad bus sampling, **R14-R21** toward cart D0-D7 |

### Control

| U1 signal | Connect to |
| --- | --- |
| PHI2 | **R12** -> clock tree (see passives) |
| RWB | Decode PLD (**UPLDV** or dedicated decode inputs) |
| BE | Decode PLD |
| RDY | **R29** pull-up, **UM** `CPU_RDY` (open-drain) |
| RESB | **R30** pull-up, reset switch / **J7** reset path |
| IRQB | **UPLDY** EQ / interrupt output |
| VDD / VSS | +5V / GND + **C1** |

---

## 2. System RAM **U3** (AS6C62256)

| SRAM signal | Connect to |
| --- | --- |
| A0-A14 | CPU A0-A14 (common low address bus) |
| A15 | Decode / PLD chip select mapping |
| DQ0-DQ7 | CPU D0-D7 |
| CE#, OE#, WE# | PLD decode outputs (mutually exclusive with cart and VRAM rules) |
| VCC, VSS | +5V, GND + **C2** |

---

## 3. PRG ROM **U4** (AT27C256R, fixed image)

| PROM signal | Connect to |
| --- | --- |
| A0-A14 | CPU address (same bundle as RAM low bits) |
| D0-D7 | CPU D0-D7 |
| CE#, OE# | Decode (PRG window `$8000-$FFFF`) |
| VCC, GND | +5V, GND |

---

## 4. VRAM **U6** (AS6C62256) and mux **U7A-U7C** (74HC157)

VRAM shares CPU address/data but uses **PHI2**-timed muxing ([`hardware.md`](../general/hardware.md)):

| VRAM A0-A3 | From mux **Y** outputs |
| --- | --- |
| A0 | **U7A** 1Y (select CPU vs beam address nibble) |
| A1 | **U7A** 2Y |
| A2 | **U7A** 3Y |
| A3 | **U7A** 4Y |

Repeat muxing pattern for higher address bits on **U7B** / **U7C** per layout (beam **A** vs CPU **A** on 157 **A/B** inputs). **G** (enable) on each **157** ties to PLD **SEL_VRAM** / beam phase (never float: **G** high forces Y low).

| VRAM DQ0-DQ7 | CPU D0-D7 |
| VRAM CE/OE/WE | PLD VRAM decode |

---

## 5. Beam PLDs **UPLDX** (Beam X) and **UPLDY** (Beam Y)

| Net | Role |
| --- | --- |
| **DOT** | **R13** from Y2 oscillator |
| **PHI2** | CPU clock reference |
| Scroll / raster latches | **U574**, hard regs `$7F02-$7F04` ([`memory.md`](../general/memory.md)) |
| **VBLANK**, **HBLANK**, **NMI** | **UM** `VBL`, CPU **IRQB** via **UPLDY** |
| **EQ#** / cascade | **UPLDY** to **UPLDX** (beam compare chain) |

Exact PLD pin numbers come from the JEDEC pinout PDF. Label nets in KiCad to match the fuse file (DOT, SY, Q, ...).

---

## 6. Compositor **UPLDV** (ATF22V10)

Inputs (representative, match JEDEC):

| Function | Source |
| --- | --- |
| Color index / priority | Beam position, sprite field, BG fetch |
| MAP address A14-A18 | To **J36** Side B pins 13-17 |
| **LE_7F02**, **LE_7F03**, **LE_7F04** | Strobe scroll/raster latches |
| **SEL_VRAM**, **SEL_SOFT0/1/2** | Soft vs hard port decode |
| **LE_MAP** | Cart MAP port timing |

Outputs drive **U24** address inputs (color PROM) and video path muxing. Tie unused PLD inputs to defined levels in ERC.

---

## 7. Scroll latch **U574** (74HC574)

| 574 pin | Net |
| --- | --- |
| D0-D7 | CPU D0-D7 (when `$7F02` write strobed) |
| CLK | PLD **LE_7F02** |
| Q0-Q7 | BG1 scroll X (`SX0-SX7`) to beam/compositor |
| OE# | GND (enabled) |
| VCC, GND | +5V + **C15** |

---

## 8. Field path: **US1**, **U573**, **U41**

| Net | US1 port | U573 | U41 SRAM |
| --- | --- | --- | --- |
| AD0-AD7 | PA0-PA7 (bidirectional) | D0-D7 | DQ0-DQ7 (shared AD bus) |
| A8-A14 | PC0-PC3, PD1-PD3 | - | A8-A14 |
| A0-A7 (latched) | - | Q0-Q7 -> | A0-A7 |
| **ALE** | PF0 | LE | - |
| **/WE** | PF1 | - | WE# |
| **/OE**, **CE#** | - | - | PLD field read (beam) vs **/WE** (S1 write), mutually exclusive |

SPI to **UM**: **US1** PD4-PD7 (`SPI_*`, `/SS_S1`), **S1_RDY** PF6 <-> **UM** PF6.

---

## 9. MCU-M **UM** (AVR128DB28)

| Net | PORT (see hardware.md) |
| --- | --- |
| CPU_D0-D7 | PA0-1, PA4-7, PD6-7 (tri-state / sample) |
| I2C SDA/SCL | PA2-3 (+ **R27**, **R28**, **R24**, **R25** to cart) |
| SPI MOSI/MISO/SCK | PC0-2 |
| /SS_S1, /SS_S2 | PC3, PD5 |
| CPU_RDY | PD2 (OD) + **R29** |
| VBL, SEL_SOFT* | PD3, PD1, PF0-1 inputs |
| CPU_A_SAMPLE | PD4 |
| S1_RDY | PF6 in |
| UPDI | pin 19 -> program header + DIP switch pos 1 |

---

## 10. MCU-S2 **US2** (pads + audio)

| Net | PORT |
| --- | --- |
| P1 buttons | PA0-7 |
| P2 buttons | PC0-3, PD1-3, PF6 |
| SPI slave | PD4-7 with **UM** |
| PAD_DATA | PF0 OD + **R26** -> **J3/J4** ring |
| AUDIO_PWM | PF1 -> analog audio jack network |
| UPDI | pin 19 -> DIP pos 3 |

Arcade: **J5**/**J6** 1x10 pins 1-8 to PA0-7 / PC0-3 etc. per [`hardware.md`](../general/hardware.md) Controllers table.

---

## 11. Cart socket **J36** (2x18)

Map schematic nets to EDAC pins ([`hardware.md`](../general/hardware.md) cart table):

| J36 pin | Side A (typ) | Side B (typ) | Motherboard source |
| --- | --- | --- | --- |
| 1, 18 | GND | GND | GND |
| 2 | +5V | +5V | +5V |
| 3 | SDA | SCL | **UM** I2C via **R24/R25** |
| 4-11 | A0-A7 | D0-D7 | **U1** A0-A7, D0-D7 via series **R14-R21** on D |
| 12 | A8 | OE# | CPU A8, **CART_OE#** via **R22** |
| 13-17 | A9-A13 | A14-A18 (MAP) | CPU A9-A13, **UPLDV** MAP outputs |
| (WE#) | - | WE# | **CART_WE#** via **R23** |

**A14-A18** on the cart come from the compositor MAP port, not the CPU address pins.

---

## 12. Connectors (summary)

| Ref | Function | Schematic nets |
| --- | --- | --- |
| **J1** | 5 V in | +5V, GND |
| **J2** | RGB + sync out | DAC R/G/B, CSYNC or HSYNC/VSYNC from **UPLDX/Y** and mode jumper |
| **J3**, **J4** | TRS pads | Tip +5V, Ring **PAD_DATA**, Sleeve GND |
| **J5**, **J6** | Arcade buttons | GPIO from **US2**, GND on pins 9-10 |
| **J7** | Cab power/reset | +5V, GND, **RESB** / reset |
| **J8** | Audio RCA | PWM / mix from **US2** |
| **J9** | Composite RCA | **U725** output when populated |

Program header + 4-position DIP (UPDI / cart flash select) are defined in [`hardware.md`](../general/hardware.md) Console as programmer section. Add as symbol cluster (**J_PROG**, **SW_PROG**) wired to **UM** UPDI bridge and cart **WE#** path.

---

## 13. Optional **U725** (AD724)

When populated: RGB inputs from DAC nodes, **FSC_XTAL** from **Y3**, sync from **J2** mode, composite out to **J9**. Follow Analog Devices datasheet for remaining caps (beyond **C18** bypass). Tier H sim does not model full AD724 ([`schematic-netlist-tier-h.md`](../bringup/schematic-netlist-tier-h.md) gaps).

---

## 14. ERC and PCB sync

After wiring:

1. Every 74xx and PLD input has a defined level or net.
2. No duplicate drivers on **CPU_D** / **AD** buses without enable terms.
3. **Tools -> Update PCB from Schematic** (F8).
4. Compare ratsnest to Tier H sim wire overlay (optional sanity).

---

## Related

- [`kicad-schematic-tier-h.md`](kicad-schematic-tier-h.md): capture order
- [`docs/ic_behavior/`](../ic_behavior/README.md): per-chip pin behavior
- [`docs/general/ic-comms-risks.md`](../general/ic-comms-risks.md): bus timing rules
