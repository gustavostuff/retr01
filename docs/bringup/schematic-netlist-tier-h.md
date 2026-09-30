# Tier H schematic netlist (pin graph)

The Tier H sim builds a **schematic-complete pin netlist** (union-find over IC + passive pins) for wire overlay and KiCad/Skidl export. It does **not** model passive analog behavior or AD724 composite encoding.

**Source code:** `apps/sim/tier-h/src/board_schematic.c`, invoked from `r01s_board_netlist_rebuild()`.

**BOM counts:** [`docs/passive_bom.md`](../passive_bom.md).

## Bypass (`C1`-`C21`)

| Cap | IC RefDes | Device and Role | Power Pin | Ground Pin | Placement Zone |
| --- | --- | --- | --- | --- | --- |
| C1 | U1 | W65C02S Game CPU | Pin 8 (VDD) | Pin 21 (VSS) | Central CPU (Top-Center) |
| C2 | U3 | AS6C62256 System RAM | Pin 28 (VCC) | Pin 14 (VSS) | Zone 1 (Top-Left) |
| C3 | U6 | AS6C62256 Interleaved VRAM | Pin 28 (VCC) | Pin 14 (VSS) | Zone 3 (Middle-Left) |
| C4 | U41 | AS6C62256 Field SRAM | Pin 28 (VCC) | Pin 14 (VSS) | Zone 4 (Middle-Right) |
| C5 | UM | AVR128DB28 Master MCU (MCU-M) | Pin 10 (VDD) | Pin 11 (GND) | Zone 3 (Middle-Center) |
| C6 | US1 | AVR128DB28 Sprite Assist (MCU-S1) | Pin 10 (VDD) | Pin 11 (GND) | Zone 4 (Middle-Right) |
| C7 | US2 | AVR128DB28 Peripheral MCU (MCU-S2) | Pin 10 (VDD) | Pin 11 (GND) | Zone 5 (Bottom-Left) |
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
| C18 | U725 | AD724 Composite Video Encoder | Pin 10, Pin 16 (VCC) | Pin 2, Pin 8, Pin 15 (GND) | Zone 2 (Top-Right) |
| C19 | U40 | SST39SF040 Cartridge Flash | Pin 32 (VDD) | Pin 16 (VSS) | Cartridge Module (J36) |
| C20 | U50 | 24C64 Cartridge Save EEPROM | Pin 8 (VCC) | Pin 4 (GND) | Cartridge Module (J36) |
| C21 | UPAD1 | ATtiny85 Gamepad Controller MCU | Pin 8 (VCC) | Pin 4 (GND) | Controller Pad PCB (J3/J4) |

Each bypass: cap `1` to IC VCC, cap `2` to `PS1` GND, `PS1` VDD to IC VCC.

## Bulk and crystals

- **E1:** `+` to `+5V`, `-` to GND.
- **Y1/Y2/Y3** (passive BOM crystals): load **C22-C27**, with **Y1** and **Y2** also tied to functional **OSC8M** and **OSC_DOT** chips (same refdes as canned osc sprites, see sim UI).
- **Y3:** load caps only, with net **`FSC_XTAL`** on crystal pin until AD724 is modeled.

## Video DAC

Weighted **R1-R8** from **U24** `O7`..`O0` to **SCR1** `RIN`/`GIN`/`BIN` (tier-a pattern). Terminations **R9-R11** to GND. **SCR1** `AGND` to GND.

## Series 33 ohm

| R | Net |
| --- | --- |
| R12 | Y1 `PHI2` to U1 `PHI2` |
| R13 | Y2 `DOT` to UPLDX `DOT` |
| R14-R21 | U1 `D[n]` to U40 `DQ[n]` |
| R22 | `CART_OE#` to U40 `OE#` |
| R23 | `CART_WE#` to U40 `WE#` |
| R24-R25 | UM `SDA`/`SCL` to U50 (I2C) |

## Pull-ups

| R | Net |
| --- | --- |
| R26 | US2 `PAD_DATA` to UPAD1 `DATA` (+5V via R26) |
| R27-R28 | I2C SDA/SCL to +5V (MCU-M side) |
| R29 | CPU `RDY` (+5V, MCU-M `CPU_RDY` tied) |
| R30 | CPU `RESB` to +5V |

## Gaps (not schematic-complete on silicon)

- **AD724** RGB/composite path (only SCR1 DAC inputs + **FSC_XTAL** stub).
- **MCP130** reset supervisor.
- Cart **socket** vs **U40** edge (OE#/WE# named stubs on series resistors).
- Extra PLD helpers (`UPLDA`, `UPLDB`, `UPLDI`, ...) share the real BOM refdes where applicable but are not all in the locked-16 bypass table.

When AD724 and MCP130 land in sim, extend `board_schematic.c` rather than duplicating links in UI code.

**KiCad schematic (manual):** [`docs/misc/kicad-schematic-tier-h.md`](../misc/kicad-schematic-tier-h.md). **Skidl export (preliminary):** [`tier-h-skidl-export.md`](tier-h-skidl-export.md).
