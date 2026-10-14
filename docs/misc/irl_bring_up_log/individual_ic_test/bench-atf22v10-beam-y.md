# ATF22V10 Beam Y

Used in Tiers A, B, and C.

The socket holds `beam_y.jed`. Power, ground, the NC ties, and RES match Beam X. Pin 24 is VCC. Pin 12 is GND. Pins 3 to 11 and pin 13 tie to GND. Pin 2 ties to 5 V.

Pin 1 is not the dot clock. It is Beam X pin 14, one rising edge per line. Beam X has already passed [`bench-atf22v10-beam-x.md`](bench-atf22v10-beam-x.md).

Count runs 0 through 261, then returns to 0. VSYNC is high on counts 243, 244, and 245.

| Pin | Name |
| --- | --- |
| 14 | VSYNC |
| 15 | Q2 |
| 16 | Q3 |
| 17 | Q5 |
| 18 | Q7 |
| 19 | Q6 |
| 20 | Q4 |
| 21 | Q1 |
| 22 | Q8 (most significant bit) |
| 23 | Q0 (least significant bit) |

Q0 on this chip is pin 23. On Beam X, Q0 is pin 21.

## Pass

D0 is pin 1 (the line pulse from Beam X). D1 is pin 14. D2 is pin 23.

| Probe | A good trace |
| --- | --- |
| Pin 1 | About 15.7 kHz |
| Pin 14 | About 60.1 Hz, high for 3 lines |
| Pin 23 | Toggles once per line, so about 7.87 kHz across one frame |

RES low clears this counter too. Pin 14 stays low while RES is low. The line pulse on pin 1 can keep running.
