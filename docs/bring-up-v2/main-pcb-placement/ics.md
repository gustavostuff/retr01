# Main PCB: ICs

Silicon placement by zone. Bypass caps: [capacitors.md](capacitors.md). Clocks: [crystals.md](crystals.md). Connectors: [connectors.md](connectors.md). Zones: [README.md](README.md).

---

## Central CPU

### U1, W65C02S (DIP-40)

Location: top-center, above J36. Bypass **C1** at pin 8.

| U1 pin | Name | Destination |
|-------:|------|-------------|
| 8 | VDD | `+5V` and **C1** |
| 21 | VSS | `GND` |
| 9-20 | A0-A11 | SysRAM U3, cart A0-A11, mux A-side |
| 22-25 | A12-A15 | SysRAM U3. A0-A13 also drop into J36 A4-A17 |
| 33-26 | D0-D7 | SysRAM U3, cart D, UM data, U574 D |
| 34 | RWB | Decode / PLDs |
| 37 | PHI2 | Clock island (buffered 8 MHz) through **R12** 33 ohm |
| 40 | RESB | U130 RESET#, **R22** 10k to `+5V`, J7 pin 3 |
| 4 | IRQB | Beam Y EQ / IRQ path |
| 2 | RDY | UM `CPU_RDY` (OD) plus **R21** 4.7k to `+5V` |

Orientation: pin-1 notch per silkscreen. Address and data edge toward J36 so A/D drop straight down.

---

## Zone 1, system RAM and reset (top-left)

### U3, AS6C62256 system RAM (DIP-28)

Location: left of CPU. Bypass **C2** at pin 28.

| U3 pin | Name | Destination |
|-------:|------|-------------|
| 28 | VCC | `+5V` and **C2** |
| 14 | GND | `GND` |
| 10,9,8,7,6,5,4,3,25,24,21,23,2,26,1 | A0-A14 | CPU A0-A14 |
| 11-13,15-19 | DQ0-DQ7 | CPU D0-D7 |
| 20 | CE# | Decode (system RAM select) |
| 22 | OE# | Decode |
| 27 | WE# | CPU RWB / write decode |

### U130, MCP130 (TO-92)

Location: next to U1 pin 40. Bypass **C17** at pin 2.

| Pin | Name | Destination |
|----:|------|-------------|
| 1 | RESET# | CPU RESB |
| 2 | VDD | `+5V` and **C17** |
| 3 | VSS | `GND` |

---

## Zone 2, video engine (top-right)

Signal flow toward J2: clocks, Beam X/Y, Compositor, Color PROM, R-2R, J2.

Clock logic **U04** / **U74** sits in the crystal cluster. Pin tables are in [crystals.md](crystals.md).

### UPLDX, ATF22V10 Beam X (DIP-24)

Bypass **C8** at pin 24. Owns H timing, HBlank/VBlank/NMI, and scroll Y load `$7F03`.

| Group | Destination |
|-------|-------------|
| CLK (pin 1) | **DOT** |
| VCC 24 / GND 12 | rails and **C8** |
| Scroll-Y data | CPU D (load) |
| LE `$7F03` | Compositor decode |
| Beam outs | UPLDY, Compositor, sync to J2 / U725 |

### UPLDY, ATF22V10 Beam Y (DIP-24)

Bypass **C9** at pin 24. Owns V timing, raster `$7F04`, and EQ into CPU IRQB.

| Group | Destination |
|-------|-------------|
| Beam X inputs | UPLDX |
| Raster load | CPU D and LE `$7F04` |
| EQ / IRQ | CPU IRQB |
| VCC/GND | rails and **C9** |

### UPLDV, ATF22V10 Compositor (DIP-24)

Bypass **C10** at pin 24. Owns color index, MAP A14-A18, `SEL_VRAM`, `SEL_SOFT*`, LEs, and cart OE windows.

| Group | Destination |
|-------|-------------|
| Index A0-A5 | **U24** A0-A5 (trace length under 25 mm) |
| CART_A14-A18 | J36 B13-B17 |
| LE `$7F02` | U574 CLK |
| Soft SELs | UM |
| VRAM select / mux G | U7A-C enable and select |
| VCC/GND | rails and **C10** |

### U24, AT27C256R color PROM (DIP-28)

Bypass **C16** at pin 28.

| U24 pin | Name | Destination |
|-------:|------|-------------|
| 28 / 14 | VCC / GND | rails and **C16** |
| 10,9,8,7,6,5 | A0-A5 | Compositor index |
| 4,3,25,24,21,23,2,26 | A6-A13 | `GND` (unused) |
| 11-13,15-19 | D0-D7 | DAC ladder R1-R8 |
| 20 CE# | CE# | `GND` (always on) |
| 22 OE# | OE# | `GND` (always on) |
| 1 | VPP | Idle per datasheet |

DAC **R1-R11** sit between U24 and J2 ([resistors.md](resistors.md)).

### U725, AD724 (SOIC-16)

Bypass **C18** near APOS/DPOS. Crystal **Y3** with **C23/C24** ([crystals.md](crystals.md)).

| U725 net | Destination |
|----------|-------------|
| RIN/GIN/BIN | DAC sums |
| HSYNC / VSYNC / CSYNC | J2 pins 5 / 6 / 4. AD724 may use CSYNC on its HSYNC pin |
| FIN + FSC | Y3 3.579545 MHz |
| COMP | J9 center |
| APOS/DPOS | `+5V` and **C18** |
| AGND/DGND | `GND` |

This corner stays clear of CPU and cart buses.

---

## Zone 3, VRAM and muxes (middle-left)

### U6, AS6C62256 VRAM (DIP-28)

Bypass **C3** at pin 28. Same pinout as U3. Address comes from mux **Y**. Data rides the CPU D bus during the PHI2-high CPU window.

| Control | Destination |
|---------|-------------|
| CE# / OE# / WE# | Compositor / VRAM decode (`SEL_VRAM`) |

### U7A / U7B / U7C, 74HC157 (DIP-16)

Location: next to U6. Bypass **C11/C12/C13** at pin 16.

Shared controls: pin **1** is S (PHI2 phase select), pin **15** is G# (must not float), pins 16/8 are VCC/GND.

| Mux | I0 (A = CPU) | I1 (B = beam) | Y into U6 |
|-----|--------------|---------------|-----------|
| **U7A** | CPU A0-A3 | Beam addr 0-3 | VRAM A0-A3 |
| **U7B** | CPU A4-A7 | Beam addr 4-7 | VRAM A4-A7 |
| **U7C** | CPU A8-A11 | Beam addr 8-11 | VRAM A8-A11 |

HC157 channel pins (same on each chip):

| Chan | I0 | I1 | Y |
|------|----|----|---|
| a | 2 | 3 | 4 |
| b | 5 | 6 | 7 |
| c | 11 | 10 | 9 |
| d | 14 | 13 | 12 |

### U574, 74HC574 scroll X (DIP-20)

Location: near the muxes. Bypass **C15** at pin 20.

| Pin | Destination |
|----:|-------------|
| 2-9 | D0-D7 from CPU D |
| 12-19 | Q0-Q7 into scroll X for beam/compositor |
| 11 | CLK from `LE_7F02` (Compositor) |
| 1 | OE# to `GND` |
| 20 / 10 | VCC / GND and **C15** |

---

## Hub, MCU-M (middle-center)

### UM, AVR128DB28 MCU-M (SPDIP-28)

Location: junction of CPU D, soft decode, SPI to S1/S2, and cart I2C. Bypass **C5** at pin 20.

| Net | Port | Role |
|-----|------|------|
| CPU D0-D1 | PA0-1 | Soft bus (Z / in) |
| I2C SDA/SCL | PA2-3 | Cart EEPROM (OD). Series **R16/R17** 33 ohm to J36. Pull-ups **R19/R20** 4.7k to `+5V` |
| CPU D2-D5 | PA4-7 | Soft bus |
| SPI MOSI/MISO/SCK | PC0-2 | Master to S1/S2. PC1 also J10 DATA |
| `/SS_S1` | PC3 | Slave select S1 |
| `SEL_SOFT0` | PD1 | Soft page decode |
| `CPU_RDY` | PD2 | OD stall |
| `VBL` | PD3 | VBlank |
| `CPU_A_SAMPLE` | PD4 | Latched A[7:0] |
| `/SS_S2` | PD5 | Slave select S2 |
| CPU D6-D7 | PD6-7 | Soft bus |
| `SEL_SOFT1` / `2` | PF0-1 | Soft page decode |
| `S1_RDY` | PF6 | Handshake from S1 |
| UPDI | pin 19 | Off-board SerialUPDI. Not on J10 |

Power: VDD **20**, GND **15** and **21**, VDDIO2 **6**, AVDD **14** to `+5V` / `GND`. **C5** sits within 5 mm of pin 20.

---

## Zone 4, S1 sprite and field (middle-right)

Compact cluster: US1, U573, and U41.

### US1, AVR128DB28 MCU-S1

Bypass **C6** at pin 20.

| Net | Port | Destination |
|-----|------|-------------|
| AD0-AD7 | PA0-7 | U573 D and U41 DQ |
| A8-A11 | PC0-3 | U41 A8-A11 |
| A12-A14 | PD1-3 | U41 A12-A14 |
| SPI and `/SS` | PD4-7 | from UM |
| ALE | PF0 | U573 LE |
| `/WE` | PF1 | U41 WE# (local pull-up) |
| `S1_RDY` | PF6 | to UM |
| UPDI | pin 19 | Off-board SerialUPDI. Not on J10 |

### U573, 74HC573 (DIP-20)

Bypass **C14** at pin 20.

| Pin | Destination |
|----:|-------------|
| 2-9 | D0-D7 from AD[7:0] |
| 12-19 | Q0-Q7 into field A0-A7 |
| 11 | LE from US1 ALE |
| 1 | OE# to `GND` |
| 20 / 10 | VCC / GND and **C14** |

### U41, AS6C62256 field SRAM (DIP-28)

Bypass **C4** at pin 28.

| Group | Destination |
|-------|-------------|
| A0-A7 | U573 Q |
| A8-A14 | US1 direct |
| DQ0-DQ7 | AD bus |
| WE# | US1 `/WE` |
| CE# / OE# | PLD (field window) |
| VCC/GND | rails and **C4** |

---

## Zone 5, controllers and audio (bottom-left)

### US2, AVR128DB28 MCU-S2

Location: behind J3/J4. Bypass **C7** at pin 20.

| Net | Port | Destination |
|-----|------|-------------|
| P1 RIGHT...START | PA0-7 | J5 odds / pad path |
| P2 RIGHT...UP | PC0-3 | J5 evens |
| P2 X/Y/COIN | PD1-3 | J5 |
| SPI and `/SS_S2` | PD4-7 | from UM |
| `PAD_DATA` | PF0 OD | J3/J4 ring plus **R18** 4.7k |
| `AUDIO_PWM` | PF1 | J8 |
| P2 START | PF6 | J5 |
| UPDI | pin 19 | Off-board SerialUPDI. Not on J10 |
