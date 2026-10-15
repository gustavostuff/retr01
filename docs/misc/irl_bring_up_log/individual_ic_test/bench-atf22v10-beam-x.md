# ATF22V10 Beam X

The socket holds `beam_x.jed`. The package is 300 mil, so it sits across the breadboard center. The notch is pin 1. Power is off while the chip goes in or comes out.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, `beam_x.jed` | 1 | Dot counter and HSYNC |
| 100 nF | 1 | Bypass from VCC to ground |
| 33 ohm | 1 | Series resistor on the dot clock |

The dot clock is Si5351 CLK0 from [`../a/flashing.md`](../a/flashing.md).

## Place

| What | Where |
| --- | --- |
| Pin 24 (VCC) | +5 V |
| Pin 12 (GND) | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 24 |
| Pins 3 through 11, and pin 13 | Ground |
| Pin 2 (RES) | +5 V |
| Pin 1 (CLK) | Si5351 CLK0, through 33 ohm |
| Pins 14 through 23 | Open. These are the probe points |

Count runs 0 through 340, then returns to 0. HSYNC is high on counts 264 through 288. The color index is not on this chip. Q pins are the count.

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

## Check

Analyzer ground ties to breadboard ground. D0 is pin 1. D1 is pin 14. D2 is pin 21. Sample rate is 24 MHz.

| Probe | A good trace |
| --- | --- |
| Pin 1 | About 5.37 MHz |
| Pin 14 | About 15.7 kHz, high for about 4.7 us |
| Pin 21 | About 2.68 MHz across one line |

RES low clears the counter and holds it there. Pin 14 stays low while RES is low, because count 0 is outside the sync window. RES returns high before the next check.

The full pin story is in [`../firmware/a/README.md`](../firmware/a/README.md).
