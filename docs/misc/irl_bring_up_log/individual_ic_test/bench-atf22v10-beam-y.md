# ATF22V10 Beam Y

The socket holds `beam_y.jed`. The package is 300 mil, so it sits across the breadboard center. The notch is pin 1. Power is off while the chip goes in or comes out. Beam X has already passed [`bench-atf22v10-beam-x.md`](bench-atf22v10-beam-x.md).

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, `beam_y.jed` | 1 | Line counter and VSYNC |
| 100 nF | 1 | Bypass from VCC to ground |

Pin 1 is Beam X pin 14, one rising edge per line.

## Place

| What | Where |
| --- | --- |
| Pin 24 (VCC) | +5 V |
| Pin 12 (GND) | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 24 |
| Pins 3 through 11, and pin 13 | Ground |
| Pin 2 (RES) | +5 V |
| Pin 1 (CLK) | Beam X pin 14 (HSYNC) |
| Pins 14 through 23 | Open. These are the probe points |

Count runs 0 through 261, then returns to 0. VSYNC is high on counts 243, 244, and 245. Q0 on this chip is pin 23. On Beam X, Q0 is pin 21.

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

## Check

Analyzer ground ties to breadboard ground. D0 is pin 1. D1 is pin 14. D2 is pin 23.

| Probe | A good trace |
| --- | --- |
| Pin 1 | About 15.7 kHz |
| Pin 14 | About 60.1 Hz, high for 3 lines |
| Pin 23 | Toggles once per line, so about 7.87 kHz across one frame |

RES low clears this counter too. Pin 14 stays low while RES is low. The line pulse on pin 1 can keep running.
