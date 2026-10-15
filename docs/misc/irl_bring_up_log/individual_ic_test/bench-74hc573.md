# 74HC573 field address latch

The package is 300 mil, so it sits across the breadboard center. The notch is pin 1. Power is off while the chip goes in or comes out. No image to burn. The chip is the latch between MCU-S1 AD7 to AD0 and field SRAM A7 to A0. This check uses jumpers, with the AVR and the SRAM off the board.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| 74HC573, PDIP-20 | 1 | Transparent latch |
| 100 nF | 1 | Bypass from VCC to ground |

## Place

| What | Where |
| --- | --- |
| Pin 20 (VCC) | +5 V |
| Pin 10 (GND) | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 20 |
| Pin 1 (OE#) | Ground, so the Q pins drive |
| Pin 11 (LE) | Jumper to +5 V or ground |
| D pins | Jumpers to +5 V or ground |
| Q pins | Open. These are the probe points |

One bit is enough to prove the latch. The other pairs follow the same rule.

| Data | Q |
| --- | --- |
| D0 pin 3 | Q0 pin 2 |
| D1 pin 4 | Q1 pin 5 |
| D2 pin 7 | Q2 pin 6 |
| D3 pin 8 | Q3 pin 9 |
| D4 pin 14 | Q4 pin 12 |
| D5 pin 15 | Q5 pin 13 |
| D6 pin 17 | Q6 pin 16 |
| D7 pin 18 | Q7 pin 19 |

## Check

| LE (pin 11) | D0 | Q0 |
| --- | --- | --- |
| High | High | High, and it follows D0 |
| High | Low | Low |
| Low | Changed after the fall | Holds the level it had when LE fell |
| High pulse, then low, D0 high during the pulse | Left low after the pulse | Stays high |

Pin 1 high releases every Q pin. The same ground resistor used on a PROM data pin sits low while OE# is high, and it shows Q0 again when pin 1 returns to ground.
