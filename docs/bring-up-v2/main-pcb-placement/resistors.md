# Main PCB: resistors

All **R1-R24** on the motherboard. Axial THT, vertical 2.54 mm pitch. Pin 1 is the source side in the tables below.

Values match [`docs/passive_bom.md`](../../passive_bom.md) and `retr01_kicad/tier_h_map.py`. Zones: [README.md](README.md). Pin-level wiring: [`docs/misc/kicad-schematic-tier-h-passives.md`](../../misc/kicad-schematic-tier-h-passives.md).

Cabinet microswitch series **47 ohm** lives on the cabinet harness.

---

## Color DAC R1-R11 (Z2, between U24 and J2)

1% metal. Packing is `{RRRGGGBB}`. PROM `O7`..`O0` feed **R1**..**R8**. **R9-R11** terminate the three gun nodes.

| Ref | Value | From | To |
|-----|-------|------|----|
| R1 | 4.00k | U24 `O7` | Red gun (J2 pin 1, U725 RIN) |
| R2 | 4.00k | U24 `O6` | Red gun |
| R3 | 2.00k | U24 `O5` | Red gun |
| R4 | 4.00k | U24 `O4` | Green gun (J2 pin 2, U725 GIN) |
| R5 | 2.00k | U24 `O3` | Green gun |
| R6 | 2.00k | U24 `O2` | Green gun |
| R7 | 1.00k | U24 `O1` | Blue gun (J2 pin 3, U725 BIN) |
| R8 | 1.00k | U24 `O0` | Blue gun |
| R9 | 75 ohm | Red gun | `GND` |
| R10 | 75 ohm | Green gun | `GND` |
| R11 | 75 ohm | Blue gun | `GND` |

Loads **R9-R11** set about 0.7 Vpp into 75 ohm. This analog string stays short. Digital buses stay out of the island.

---

## Series 33 ohm R12-R17

| Ref | From | To | Zone |
|-----|------|----|------|
| R12 | U04 pin 8 (PHI2 buffer) | U1 pin 37 PHI2 | Z2 clock island |
| R13 | U74 pin 9 (DOT) | UPLDX pin 1 DOT | Z2 clock island |
| R14 | `CART_OE#` (compositor / MAP) | J36 OE# | Cart spine |
| R15 | `CART_WE#` | J36 WE# | Cart spine |
| R16 | UM SDA | J36 SDA (A3) | Hub, toward J36 |
| R17 | UM SCL | J36 SCL (B3) | Hub, toward J36 |

Cart **D0-D7** tie straight from U1 to J36. Clock dampers sit at the buffer outputs. **R14-R17** sit in line on cart OE#, WE#, SDA, and SCL.

---

## Pull-ups R18-R22

| Ref | Value | From | To | Zone |
|-----|-------|------|----|------|
| R18 | 4.7k | `+5V` | US2 `PAD_DATA` / J3 and J4 ring | Z5 at the TRS jacks |
| R19 | 4.7k | `+5V` | UM SDA | Hub at UM |
| R20 | 4.7k | `+5V` | UM SCL | Hub at UM |
| R21 | 4.7k | `+5V` | U1 RDY | CPU at pin 2 |
| R22 | 10k | `+5V` | U1 RESB | Z1 at U130 / U1 pin 40 |

`CART_WE#` idles high through a pull-up in play ([`hardware.md`](../../general/hardware.md)). **R15** is the series damper on that net.

---

## Pierce feedback R23-R24 (1M)

Sit in the [crystal](crystals.md) clock island.

| Ref | Across | Crystal | Zone |
|-----|--------|---------|------|
| R23 | U04 pin 5 (3A) and pin 6 (3Y) | Y1 8.000 MHz | Z2 clock island |
| R24 | U04 pin 1 (1A) and pin 2 (1Y) | Y2 21.47727 MHz | Z2 clock island |
