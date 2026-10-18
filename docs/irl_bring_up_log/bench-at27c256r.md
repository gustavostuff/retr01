# AT27C256R color PROM

This bench passes. The switch code selects a kit row, and the LEDs show that packed byte. Several tests have been performed.

<img src="img/AT27C256R-bench-passed.jpg" alt="AT27C256R breadboard, row 0">

One kit chip in the socket at a time. The package is 600 mil, so it sits in a 28-pin socket of that width, one jumper per pin. The notch is pin 1. Power is off while the chip goes in or comes out.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| AT27C256R, PDIP-28, kit image | 2 | One chip in the socket at a time |
| 28-pin socket, 600 mil | 1 | Wide socket for the PROM |
| 100 nF | 1 | From V+ to the ground rail |
| Switch | 6 | A5 through A0. One 6-position DIP switch covers the same pins |
| 10 kohm | 6 | Hold an open address switch at 0 |
| LED, red | 8 | One per data bit |
| 2.2 kohm | 8 | LED series resistor. Output pin sinks 2.1 mA |

## Place

| Mark | Pins | Where |
| --- | --- | --- |
| V+ | 1 and 28 | +5 V rail. Pin 1 is VPP and pin 28 is VCC |
| GD | 2 through 4, 14, and 20 through 27 | Ground rail. This holds CE#, OE#, and the higher address pins low, including A14 |
| C | one cap | From V+ to the ground rail |
| S5 to S0 | 5 through 10 | Switch to +5 V, and 10 kohm to ground. S5 is pin 5. S0 is pin 10 |
| D7 to D0 | 19 through 15, and 13 through 11 | LED cathode. +5 V, then 2.2 kohm, then the LED anode. D7 is pin 19. D0 is pin 11 |

```text
 +5V ------------------------------------------------
                         notch
                    +------U------+
                 V+ |  1       28 | V+
                 GD |  2       27 | GD
                 GD |  3       26 | GD
                 GD |  4       25 | GD
                 S5 |  5       24 | GD
                 S4 |  6       23 | GD
                 S3 |  7       22 | GD
                 S2 |  8       21 | GD
                 S1 |  9       20 | GD
                 S0 | 10       19 | D7
                 D0 | 11       18 | D6
                 D1 | 12       17 | D5
                 D2 | 13       16 | D4
                 GD | 14       15 | D3
                    +-------------+
                 C from V+ to the ground rail
 GND ------------------------------------------------
```

A high on A14 selects the upper 16 KB. Those bytes are `0xFF`. CE# or OE# high turns the data pins off. Closed is 1. A lit LED is a 0. A dark LED is a 1.

## Check

The switch code, left to right, is the kit row in [`video-graphics.md`](../video-graphics.md), starting at 0. Switch 5 is the left bit.

The output byte is read left to right. Bit 7 is the left bit.

| Pin | Byte bit |
| --- | --- |
| 19 | 7 |
| 18 | 6 |
| 17 | 5 |
| 16 | 4 |
| 15 | 3 |
| 13 | 2 |
| 12 | 1 |
| 11 | 0 |

| A5 to A0 | Row | O7 to O0 |
| --- | --- | --- |
| 000000 | 0 | 00000000 |
| 000001 | 1 | 00100000 |
| 000100 | 4 | 00100100 |
| 001100 | 12 | 00000001 |
| 010000 | 16 | 00100101 |
| 100000 | 32 | 10010010 |
| 110000 | 48 | 11111111 |
| 111111 | 63 | 11110011 |

Power off, then the second chip goes in with the notch at the same end. The same switches light the same LEDs.
