# ATF22V10 Beam X

Used in Tiers A, B, and C.

The socket holds `beam_x.jed`. Pin 24 is VCC. Pin 12 is GND. Pins 3 to 11 and pin 13 tie to GND. Pin 2 (RES) ties to 5 V so the counter runs. Pin 1 is the dot clock, Si5351 CLK0 through 33 ohm.

This burn is a 9-bit counter plus HSYNC. Count runs 0 through 340, then returns to 0. HSYNC is high on counts 264 through 288. The color index is not on this chip. Q pins are the count, for a probe.

| Pin | Name |
| --- | --- |
| 14 | HSYNC |
| 15 | Q3 |
| 16 | Q2 |
| 17 | Q4 |
| 18 | Q7 |
| 19 | Q6 |
| 20 | Q5 |
| 21 | Q0 (least significant bit) |
| 22 | Q8 (most significant bit) |
| 23 | Q1 |

## Pass

Analyzer ground ties to breadboard ground. D0 is pin 1. D1 is pin 14. D2 is pin 21. Sample rate is 24 MHz.

| Probe | A good trace |
| --- | --- |
| Pin 1 | About 5.37 MHz |
| Pin 14 | About 15.7 kHz, high for about 4.7 us |
| Pin 21 | About 2.68 MHz across one line |

RES low clears the counter and holds it there. Pin 14 stays low while RES is low, because count 0 is outside the sync window. RES returns high before the next check.

The full pin story is in [`../firmware/a/README.md`](../firmware/a/README.md).
