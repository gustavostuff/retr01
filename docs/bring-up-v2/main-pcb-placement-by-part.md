# Main PCB: part placement by zone and pin

Floor-plan checklist for the motherboard under `apps/sim/tier-h/kicad/main-pcb/`.
Board outline is **160 x 100 mm** Eurocard. Skidl netlist is `apps/sim/tier-h/skidl/retr01_prelim.net`.

Design sources: [`docs/general/hardware.md`](../general/hardware.md), floor plan [`docs/bringup/pcb-component-placement-guide.md`](../bringup/pcb-component-placement-guide.md), DIP pin numbers in `apps/sim/tier-h/skidl/retr01_kicad/pinmap.py`.

---

## Quick map

```
TOP (rear I/O)     J1 power | J8 audio | J9 composite | J2 RGBS
----------------------------------------------------------------
 Zone1 SYSTEM RAM  |  CPU U1 (above cart)  |  Zone2 VIDEO / DAC
                   |                       |
-------------------+------ J36 CART -------+--------------------
 Zone3 VRAM+MUX    |  MCU-M (UM)           |  Zone4 S1 + field
 Zone5 CTRL+AUDIO  |  (open / mounting)    |
----------------------------------------------------------------
BOTTOM (front)     J3/J4 pads | J5 arcade | J7 cab
```

| Zone | Name | Parts |
|------|------|-------|
| **Z1** | Top-left | U3, U130, E1, C2, C17 |
| **CPU** | Top-center | U1, C1, directly above J36 |
| **Z2** | Top-right | Clock island, PLDs, U24, DAC Rs, J2, U725, J9 |
| **CART** | Center | J36 |
| **Z3** | Middle-left | U6, U7A/B/C, U574 and their Cs |
| **HUB** | Middle-center | UM and C5 |
| **Z4** | Middle-right | US1, U573, U41 and Cs |
| **Z5** | Bottom-left | US2, J3, J4, J5, J7 and C7 |

---

## Connectors (board edges)

### J1, DC barrel (rear, top-left)

| Pin | Destination |
|-----|-------------|
| Center (+) | `+5V` rail, then E1(+) |
| Sleeve / shunt | `GND` |

### E1, 220 uF bulk (next to J1)

| Pin | Destination |
|-----|-------------|
| + | `+5V` |
| - | `GND` |

### J36, cart edge 2x18 (board center, under CPU)

View into the socket: **A** is one face, **B** is the opposite face.

| Edge | Net | Source |
|------|-----|--------|
| A1, B1, A18 | `GND` | Plane |
| A2, B2 | `+5V` | Rail |
| A3 | `SDA` | UM TWI (33 ohm series + 4.7k pull-up) |
| B3 | `SCL` | UM TWI (33 ohm series + 4.7k pull-up) |
| A4-A17 | `A0`-`A13` | CPU `A0`-`A13` |
| B4-B11 | `D0`-`D7` | CPU `D0`-`D7` (33 ohm series) |
| B12 | `OE#` | Compositor / MAP decode (33 ohm series) |
| B13-B17 | `A14`-`A18` | Compositor MAP (UPLDV) |
| B18 | `WE#` | Flash path / pull-up idle (33 ohm series) |

**R14-R23** (33 ohm cart dampers) sit in line between the motherboard buses and the J36 pins.

### J2, RGBS header (rear, top-right), Zone 2

| Pin | Net |
|-----|-----|
| 1 | Red (DAC) |
| 2 | Green (DAC) |
| 3 | Blue (DAC) |
| 4 | CSYNC or HSYNC (mode jumper) |
| 5 | GND or VSYNC |
| 6 | `GND` |

### J8, audio RCA (rear), driven from Zone 5

| Pin | Destination |
|-----|-------------|
| Center | US2 `AUDIO_PWM` (PF1) |
| Shell | `GND` |

### J9, composite RCA (rear), Zone 2

| Pin | Destination |
|-----|-------------|
| Center | U725 `COMP` |
| Shell | `GND` |

### J3 / J4, TRS pads (front, bottom-left), Zone 5

| Contact | Net |
|---------|-----|
| Tip (T) | `+5V` |
| Ring (R) | `PAD_DATA` (US2 PF0, open-drain) plus 4.7k pull-up |
| Sleeve (S) | `GND` |

### J5, arcade 2x10 (bottom-left), Zone 5

| Pins | Destination |
|------|-------------|
| Odd 1-15 | P1 bits to US2 `PA0`-`PA7` (47 ohm series) |
| Even 2-16 | P2 bits to US2 `PC0`-`PC3`, `PD1`-`PD3`, `PF6` |
| 17-20 | `GND` |

### J7, cab power/reset 2x2 (near J5)

Cab harness brings `+5V`, `GND`, and reset. No onboard power slide or reset tactile.

| Pin | Net |
|-----|-----|
| 1 | `+5V` |
| 2 | `GND` |
| 3 | `RESET_N` / RESB path |
| 4 | `GND` |

Layout (top view, pin 1 at top-left):

```text
1 (+5V)   2 (GND)
3 (RESB)  4 (GND)
```

Note: cab-side reset debounce / RC on the J7 RESB pin is deferred. U130 still holds the supervisor path.
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
| 33-26 | D0-D7 | SysRAM U3, cart D (33 ohm), UM data, U574 D |
| 34 | RWB | Decode / PLDs |
| 37 | PHI2 | Clock island (buffered 8 MHz) through **R12** 33 ohm |
| 40 | RESB | U130 RESET#, R30 10k to `+5V`, J7 pin 3 |
| 4 | IRQB | Beam Y EQ / IRQ path |
| 2 | RDY | UM `CPU_RDY` (OD) plus pull-up |

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

### Clock island (cluster under about 20 mm)

#### Y2, 21.47727 MHz

Load caps **C22/C23**, feedback **R32** (1M). Crystal pins tie to U04 pins **1** and **2** (Pierce gate 1).

#### Y1, 8.000 MHz

Load caps **C26/C27**, feedback **R31** (1M). Crystal pins tie to U04 pins **5** and **6** (Pierce gate 3).

#### U04, 74HCU04 (DIP-14)

Bypass **C19** at pin 14.

| U04 pin | Role | Destination |
|--------:|------|-------------|
| 14 / 7 | VCC / GND | `+5V` / `GND` and **C19** |
| 1 to 2 | DOT Pierce | Y2 |
| 3 to 4 | DOT buffer | U74 clock input |
| 5 to 6 | PHI2 Pierce | Y1 |
| 9 to 8 | PHI2 buffer | **PHI2** net (then R12 to CPU) |
| 11,13 | spare inputs | `GND` |
| 10,12 | spare outs | NC |

#### U74, 74HC74 (DIP-14)

Bypass **C20** at pin 14.

| Stage | Pins | Net |
|-------|------|-----|
| FF1 | CLK=3, Q=5, /Q=6 into D=2 | 21 MHz to 10.7 MHz |
| FF2 | CLK=11, Q=9, /Q=8 into D=12 | **DOT** (about 5.37 MHz) |
| PRE#/CLR# | 1,4,10,13 | `+5V` |

DOT leaves through **R13** 33 ohm toward Beam X.

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

### DAC resistors R1-R11 (1% metal)

Location: between U24 and J2. Packing is `{RRRGGGBB}`, LSB to MSB.

| Values | Guns |
|--------|------|
| 4.00k x2 | R LSB, G LSB |
| 2.00k x3 | R mid, G mid, B LSB |
| 1.00k x3 | R/G/B MSB |
| 75 ohm x3 (**R9-R11**) | R/G/B to `GND` (about 0.7 Vpp) |

Sum nodes feed J2 pins 1-3 and U725 R/G/B inputs.

### U725, AD724 (SOIC-16)

Bypass **C18** near APOS/DPOS. Crystal **Y3** with **C24/C25**.

| U725 net | Destination |
|----------|-------------|
| RIN/GIN/BIN | DAC sums |
| HSYNC/VSYNC or CSYNC | Beam sync (mode select) |
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
| I2C SDA/SCL | PA2-3 | Cart EEPROM (OD) |
| CPU D2-D5 | PA4-7 | Soft bus |
| SPI MOSI/MISO/SCK | PC0-2 | Master to S1/S2 |
| `/SS_S1` | PC3 | Slave select S1 |
| `SEL_SOFT0` | PD1 | Soft page decode |
| `CPU_RDY` | PD2 | OD stall |
| `VBL` | PD3 | VBlank |
| `CPU_A_SAMPLE` | PD4 | Latched A[7:0] |
| `/SS_S2` | PD5 | Slave select S2 |
| CPU D6-D7 | PD6-7 | Soft bus |
| `SEL_SOFT1` / `2` | PF0-1 | Soft page decode |
| `S1_RDY` | PF6 | Handshake from S1 |
| UPDI | pin 19 | Program header / DIP |

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
| UPDI | pin 19 | Program header / DIP |

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
| `PAD_DATA` | PF0 OD | J3/J4 ring plus 4.7k |
| `AUDIO_PWM` | PF1 | J8 |
| P2 START | PF6 | J5 |
| UPDI | pin 19 | Program header / DIP |

---

## Bypass capacitors (one per IC, within 5 mm of VCC)

| Cap | IC | Power pin |
|-----|-----|-----------|
| C1 | U1 | 8 |
| C2 | U3 | 28 |
| C3 | U6 | 28 |
| C4 | U41 | 28 |
| C5 | UM | 20 |
| C6 | US1 | 20 |
| C7 | US2 | 20 |
| C8 | UPLDX | 24 |
| C9 | UPLDY | 24 |
| C10 | UPLDV | 24 |
| C11-C13 | U7A-C | 16 |
| C14 | U573 | 20 |
| C15 | U574 | 20 |
| C16 | U24 | 28 |
| C17 | U130 | 2 |
| C18 | U725 | 4 and 14 |
| C19 | U04 | 14 |
| C20 | U74 | 14 |

Crystal loads **C22/C23** (Y2), **C24/C25** (Y3), and **C26/C27** (Y1) sit at the crystal, not across the board.

---

## Placement sequence

1. Board outline and mounting holes
2. J36 at center, then U1 above it
3. J1 / E1 at top-left. J2 / J8 / J9 on the top edge. J3-J5 / J7 at bottom-left
4. Zone 2 clock island, then PLDs, U24, DAC, encoder
5. Zone 1 U3 and reset
6. Zone 3 U6, three HC157, U574
7. UM hub
8. Zone 4 US1, U573, U41
9. Zone 5 US2
10. All 100 nF caps, crystal loads, series 33 ohm, and pull-ups

Ratsnest refresh from `retr01_prelim.net` follows. Short nets route first: bypass, clocks, color index, field AD, cart drop.

---

## Related

- [`docs/bringup/pcb-component-placement-guide.md`](../bringup/pcb-component-placement-guide.md): zone rationale
- [`docs/bringup/pcb-netlist-verification-guide.md`](../bringup/pcb-netlist-verification-guide.md): PCB vs JSON check
- [`docs/bringup/tier-h-skidl-export.md`](../bringup/tier-h-skidl-export.md): `.net` export
- [`temp/motherboard-placement-notes.md`](../../temp/motherboard-placement-notes.md): routing and parasitics notes
