# Main PCB: resistors

All **R1-R32** on the motherboard. Axial THT, vertical 2.54 mm pitch. Pin 1 is the source side in the tables below.

Values match [`docs/passive_bom.md`](../../passive_bom.md) and `retr01_kicad/tier_h_map.py`. Zones: [README.md](README.md). Pin-level wiring: [`docs/misc/kicad-schematic-tier-h-passives.md`](../../misc/kicad-schematic-tier-h-passives.md).

Arcade **47 ohm** button series is not on this BOM.

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

## Series 33 ohm R12-R25

| Ref | From | To | Zone |
|-----|------|----|------|
| R12 | U04 pin 8 (PHI2 buffer) | U1 pin 37 PHI2 | Z2 clock island |
| R13 | U74 pin 9 (DOT) | UPLDX pin 1 DOT | Z2 clock island |
| R14 | U1 D0 | J36 D0 | Cart spine |
| R15 | U1 D1 | J36 D1 | Cart spine |
| R16 | U1 D2 | J36 D2 | Cart spine |
| R17 | U1 D3 | J36 D3 | Cart spine |
| R18 | U1 D4 | J36 D4 | Cart spine |
| R19 | U1 D5 | J36 D5 | Cart spine |
| R20 | U1 D6 | J36 D6 | Cart spine |
| R21 | U1 D7 | J36 D7 | Cart spine |
| R22 | `CART_OE#` (compositor / MAP) | J36 OE# | Cart spine |
| R23 | `CART_WE#` | J36 WE# | Cart spine |
| R24 | UM SDA | J36 SDA (A3) | Hub, toward J36 |
| R25 | UM SCL | J36 SCL (B3) | Hub, toward J36 |

Cart dampers sit in line between the motherboard buses and [J36](connectors.md).

---

## Pull-ups R26-R30

| Ref | Value | From | To | Zone |
|-----|-------|------|----|------|
| R26 | 4.7k | `+5V` | US2 `PAD_DATA` / J3 and J4 ring | Z5 at the TRS jacks |
| R27 | 4.7k | `+5V` | UM SDA | Hub at UM |
| R28 | 4.7k | `+5V` | UM SCL | Hub at UM |
| R29 | 4.7k | `+5V` | U1 RDY | CPU at pin 2 |
| R30 | 10k | `+5V` | U1 RESB | Z1 at U130 / U1 pin 40 |

`CART_WE#` idles high through a pull-up in play ([`hardware.md`](../../general/hardware.md)). **R23** is the series damper on that net. The pull-up has no extra refdes on this BOM.

---

## Pierce feedback R31-R32 (1M)

Sit in the [crystal](crystals.md) clock island.

| Ref | Across | Crystal | Zone |
|-----|--------|---------|------|
| R31 | U04 pin 5 (3A) and pin 6 (3Y) | Y1 8.000 MHz | Z2 clock island |
| R32 | U04 pin 1 (1A) and pin 2 (1Y) | Y2 21.47727 MHz | Z2 clock island |
