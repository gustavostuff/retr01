# 74HC573 field address latch

Used in Tier C.

No image to burn. The chip is the latch between MCU-S1 AD7 to AD0 and field SRAM A7 to A0. This check uses jumpers, with the AVR and the SRAM off the board.

Pin 20 is VCC. Pin 10 is GND. Pin 1 (/OE) ties to GND so the Q pins drive. Pin 11 is LE.

One bit is enough to prove the latch. D0 is pin 3. Q0 is pin 2. The other pairs follow the same rule.

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

## Pass

| LE (pin 11) | D0 | Q0 |
| --- | --- | --- |
| High | High | High, and it follows D0 |
| High | Low | Low |
| Low | Changed after the fall | Holds the level it had when LE fell |
| High pulse, then low, D0 high during the pulse | Left low after the pulse | Stays high |

Pin 1 high releases every Q pin. The same ground resistor used on a PROM data pin sits low while /OE is high, and it shows Q0 again when pin 1 returns to ground.
