# ATF22V10 Beam X

The socket holds `beam_x.jed`. The package is 300 mil, so it sits across the breadboard center. The notch is pin 1. Power is off while the chip goes in or comes out.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, `beam_x.jed` | 1 | Dot counter and HSYNC |
| 74HCU04, PDIP-14 | 1 | Pierce oscillator and buffer |
| 74HC74, PDIP-14 | 1 | Divides 21.47727 MHz by 4 |
| 21.47727 MHz crystal, HC-49 | 1 | Master clock |
| 1 Mohm | 1 | Pierce feedback |
| 18 pF | 2 | Crystal load |
| 100 nF | 3 | One across each IC |
| 33 ohm | 1 | Series resistor on the dot clock |

The oscillator chip is a 74HCU04. A 74HC04 is a different part. The same stage is the console dot clock.

## Clock

The crystal, the 1 Mohm resistor, the two 18 pF capacitors, and 74HCU04 pins 1 and 2 sit on adjacent rows. Leads on that loop stay short. The crystal has no polarity. 18 pF is the starting load. 15 pF is the next value when 74HCU04 pin 4 stays quiet.

| What | Where |
| --- | --- |
| 74HCU04 pin 14 | +5 V |
| 74HCU04 pin 7 | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 14 |
| 74HCU04 pins 5, 9, 11, and 13 | Ground |
| 74HCU04 pins 6, 8, 10, and 12 | Open |
| Crystal | 74HCU04 pin 1 and pin 2 |
| 1 Mohm | 74HCU04 pin 1 to pin 2 |
| 18 pF | 74HCU04 pin 1 to ground |
| 18 pF | 74HCU04 pin 2 to ground |
| 74HCU04 pin 2 | 74HCU04 pin 3 |
| 74HCU04 pin 4 | 74HC74 pin 3 |
| 74HC74 pin 14 | +5 V |
| 74HC74 pin 7 | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 14 |
| 74HC74 pins 1, 4, 10, and 13 | +5 V |
| 74HC74 pin 6 | 74HC74 pin 2 |
| 74HC74 pin 5 | 74HC74 pin 11 |
| 74HC74 pin 8 | 74HC74 pin 12 |
| 74HC74 pin 9 | One end of the 33 ohm resistor |

74HC74 pin 9 is the 5.369318 MHz dot clock.

## Place

| What | Where |
| --- | --- |
| Pin 24 (VCC) | +5 V |
| Pin 12 (GND) | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 24 |
| Pins 3 through 11, and pin 13 | Ground |
| Pin 2 (RES) | +5 V |
| Pin 1 (CLK) | 74HC74 pin 9, through 33 ohm |
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

Analyzer ground ties to breadboard ground. D0 is Beam X pin 1. D1 is Beam X pin 14. D2 is Beam X pin 21. Sample rate is 24 MHz.

| Probe | A good trace |
| --- | --- |
| 74HCU04 pin 4 | About 21.48 MHz |
| 74HC74 pin 5 | About 10.74 MHz |
| 74HC74 pin 9, and Beam X pin 1 | About 5.37 MHz |
| Beam X pin 14 | About 15.7 kHz, high for about 4.7 us |
| Beam X pin 21 | About 2.68 MHz across one line |

RES low clears the counter and holds it there. Beam X pin 14 stays low while pin 2 is low, because count 0 is outside the sync window. Pin 2 returns high before the next check.

The full pin story is in [`../firmware/a/README.md`](../firmware/a/README.md).
