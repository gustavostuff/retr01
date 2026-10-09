# Tier H schematic netlist (pin graph)

The Tier H sim builds a **schematic-complete pin netlist** (union-find over IC + passive pins) for wire overlay and KiCad/Skidl export. It does **not** model passive analog behavior or AD724 composite encoding.

**Source code:** `apps/sim/tier-h/src/board_schematic.c`, invoked from `r01s_board_netlist_rebuild()`.

**Downstream:** KiCad/Skidl ([`tier-h-skidl-export.md`](tier-h-skidl-export.md)) reads the JSON from this graph. Fix connectivity and **physical pad numbers** here first (chip `r01s_entity_add_pin`, `board_schematic.c`, `board_netlist.c`). Skidl should map and name nets, not invent links. Export-only rail merges or renames are a temporary bridge until the sim netlist is complete (e.g. all GND ties, PLD package VCC/GND pins).

**BOM counts:** [`docs/passive_bom.md`](../passive_bom.md).

## Bypass (`C1`-`C20` on the motherboard)

| Cap | IC RefDes | Device and Role | Power Pin | Ground Pin | Placement Zone |
| --- | --- | --- | --- | --- | --- |
| C1 | U1 | W65C02S Game CPU | Pin 8 (VDD) | Pin 21 (VSS) | Central CPU (Top-Center) |
| C2 | U3 | AS6C62256 System RAM | Pin 28 (VCC) | Pin 14 (VSS) | Zone 1 (Top-Left) |
| C3 | U6 | AS6C62256 Interleaved VRAM | Pin 28 (VCC) | Pin 14 (VSS) | Zone 3 (Middle-Left) |
| C4 | U41 | AS6C62256 Field SRAM | Pin 28 (VCC) | Pin 14 (VSS) | Zone 4 (Middle-Right) |
| C5 | UM | AVR128DB28 Master MCU (MCU-M) | Pin 20 (VDD) | Pins 15, 21 (GND) | Middle-Center |
| C6 | US1 | AVR128DB28 Sprite Assist (MCU-S1) | Pin 20 (VDD) | Pins 15, 21 (GND) | Zone 4 (Middle-Right) |
| C7 | US2 | AVR128DB28 Peripheral MCU (MCU-S2) | Pin 20 (VDD) | Pins 15, 21 (GND) | Zone 5 (Bottom-Left) |
| C8 | UPLDX | ATF22V10 Beam X Timing PLD | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) |
| C9 | UPLDY | ATF22V10 Beam Y Timing PLD | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) |
| C10 | UPLDV | ATF22V10 Compositor PLD | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) |
| C11 | U7A | 74HC157 Multiplexer A | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) |
| C12 | U7B | 74HC157 Multiplexer B | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) |
| C13 | U7C | 74HC157 Multiplexer C | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) |
| C14 | U573 | 74HC573 Address Latch | Pin 20 (VCC) | Pin 10 (GND) | Zone 4 (Middle-Right) |
| C15 | U574 | 74HC574 Scroll X Latch | Pin 20 (VCC) | Pin 10 (GND) | Zone 3 (Middle-Left) |
| C16 | U24 | AT27C256R Color PROM | Pin 28 (VCC) | Pin 14 (GND) | Zone 2 (Top-Right) |
| C17 | U130 | MCP130 Reset Supervisor | Pin 2 (VDD) | Pin 3 (VSS) | Zone 1 (Top-Left) |
| C18 | U725 | AD724 Composite Video Encoder | Pin 4 (APOS), pin 14 (DPOS) | Pin 2 (AGND), pin 13 (DGND) | Zone 2 (Top-Right) |
| C19 | U04 | 74HCU04 clock inverter | Pin 14 (VCC) | Pin 7 (GND) | Zone 2 clock island |
| C20 | U74 | 74HC74 DOT divider | Pin 14 (VCC) | Pin 7 (GND) | Zone 2 clock island |

Cart flash, cart EEPROM, and the pad ATtiny85 each carry one 100 nF on those boards. Crystal loads are C21-C26.

Each bypass: cap `1` to IC VCC, cap `2` to `PS1` GND, `PS1` VDD to IC VCC.

## Bulk and crystals

- **E1:** `+` to `+5V`, `-` to GND.
- **Y1/Y2/Y3** (HC-49/US crystals): load **C21-C26**. **Y1** and **Y2** Pierce loops sit on **U04**. **U74** divides Y2 by 4 for DOT. Sim time steps canned **OSC8M** / **OSC_DOT** engines. Export carries Y1/Y2/Y3 plus U04/U74.
- **Y3:** load caps plus **`FSC_XTAL`** into AD724 FIN when that path is modeled.

## Video DAC

Weighted **R1-R8** from **U24** `O7`..`O0` to **SCR1** `RIN`/`GIN`/`BIN` (tier-a pattern). Terminations **R9-R11** to GND. **SCR1** `AGND` to GND.

## Series 33 ohm

| R | Net |
| --- | --- |
| R12 | U04 pin 8 to U1 `PHI2` |
| R13 | U74 pin 9 to UPLDX `DOT` |
| R14 | `CART_OE#` to U40 `OE#` |
| R15 | `CART_WE#` to U40 `WE#` |
| R16-R17 | UM `SDA`/`SCL` to U50 (I2C) |

## Pull-ups

| R | Net |
| --- | --- |
| R18 | US2 `PAD_DATA` to UPAD1 `DATA` (+5V via R18) |
| R19-R20 | I2C SDA/SCL to +5V (MCU-M side) |
| R21 | CPU `RDY` (+5V, MCU-M `CPU_RDY` tied) |
| R22 | CPU `RESB` to +5V |

## Clock feedback (1 M ohm)

| R | Net |
| --- | --- |
| R23 | U04 Y1 Pierce (pins 5-6) |
| R24 | U04 Y2 Pierce (pins 1-2) |

## Cart program header

**J10** 2x2. Pin 1 PWR is named `J10_PWR_NC`. Pins 2 and 4 are GND. Pin 3 DATA is UM PC1 / `SPI_MISO`. **CART_ARM** is compositor MAP latch D7, not a board net. See [`hardware.md`](../general/hardware.md).

## Gaps (not schematic-complete on silicon)

- **PLD signal pins** use DIP-24 legs so each air wire leaves a pin. Those numbers are a placement stand-in until a JEDEC map exists. Pin 12 is GND and pin 24 is VCC.
- **AD724** analog NTSC encode (COMP is a logic video-present flag; RGB and J9 are netlisted).
- VRAM mux **B** inputs and the **A/B** select wait on the PLD phase decode. **A** inputs are the CPU address and **Y** outputs are VRAM A[11:0].
- Cart MAP **A14-A18** on **J36** wait on the compositor fuse map.
- Extra PLD helpers (`UPLDA`, `UPLDB`, `UPLDI`, ...) are not motherboard placements.

**KiCad schematic (manual):** [`docs/misc/kicad-schematic-tier-h.md`](../misc/kicad-schematic-tier-h.md). **Skidl export (preliminary):** [`tier-h-skidl-export.md`](tier-h-skidl-export.md).
