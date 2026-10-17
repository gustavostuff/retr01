# Beam X and Beam Y

Two ATF22V10C parts, already burned, on a breadboard. Beam X counts dots 0 through 340. Beam Y counts lines 0 through 261. The images are [`beam_x.jed`](../../../apps/flasher_tool/firmware/a/beam_x.jed) and [`beam_y.jed`](../../../apps/flasher_tool/firmware/a/beam_y.jed).

The package is 300 mil. Each chip sits in a 24-pin socket of that width. The notch is pin 1. Power is off while a chip goes in or comes out.

One HiLetgo CH340 module clocks the counters and feeds 5 V. It is a 6-pin USB serial header: 5V, VCC, 3V3, TXD, RXD, GND. The yellow jumper bridges 5V and VCC, so TXD swings at 5V. An 8-channel USB logic analyzer reads the pins. The case label shows CH1, CH3, CH5, CH7, and CLK down the left of the header, and CH2, CH4, CH6, CH8, and GND down the right. PWR on that label is the power lamp beside the USB plug. A channel reading of 0 is a low pin. A reading of 1 is a high pin.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, Beam X image | 1 | Dot counter. HSYNC high on counts 264 through 288 |
| ATF22V10C, PDIP-24, Beam Y image | 1 | Line counter. VSYNC high on counts 243 through 245 |
| 24-pin socket, 300 mil | 2 | One per PLD |
| 100 nF | 2 | Bypass from VCC to ground, one beside each pin 24 |
| 10 kohm | 2 | Pull-up on RES, pull-up on Beam X CLK |
| Switch | 1 | RES to ground. Closed holds both counters at 0 |
| HiLetgo CH340 module | 1 | Clock and 5 V. TXD drives CK. RXD stays open |
| 8-channel USB logic analyzer, 24 MHz | 1 | Reads CK, RES, Q0, Q1, HSYNC, and VSYNC. GND joins the ground rail |

## Place

Beam X can be checked with Beam Y out of the socket. Beam Y pin 1 connects only after HSYNC is already moving.

| Mark | What | Where |
| --- | --- | --- |
| V+ | Both pin 24 | +5 V from the clock module 5V pin. That module's jumper bridges 5V and VCC. Its GND joins the ground rail. Its RXD stays open |
| C | 100 nF, one per chip | From that chip's pin 24 (VCC) to that chip's pin 12 (GND), across the package |
| GD | Both pin 12. Both pins 3 through 11. Both pin 13 | Ground, common with the CH340 GND and the analyzer GND |
| RS | Both pin 2 | One net. 10 kohm to +5 V, the switch to ground, and analyzer CH2 (D1) |
| CK | Beam X pin 1 | 10 kohm to +5 V, the clock module TXD, and analyzer CH1 (D0) |
| HS | Beam X pin 14 and Beam Y pin 1 | One wire. HSYNC out of Beam X, and the Beam Y clock. Analyzer CH5 (D4) lands on the Beam X pin |
| Q0 | Beam X pin 21. Beam Y pin 23 | Analyzer CH3 (D2) on the Beam X pin. Analyzer CH6 (D5) on the Beam Y pin |
| Q1 | Beam X pin 23. Beam Y pin 21 | Analyzer CH4 (D3) on the Beam X pin. Analyzer CH7 (D6) on the Beam Y pin |
| VS | Beam Y pin 14 | VSYNC. Analyzer CH8 (D7) |
| .. | The other Q pins | Open |

```text
 Clock
  5V ---- VCC jumper
  5V ---- +5V rail, and V+ on both pin 24
  TXD ---- CK, Beam X pin 1
  GND ---- GND rail
  RXD open
  3V3 open

 Logic analyzer
  CH1 (D0) ---- CK, Beam X pin 1      CH2 (D1) ---- RS, both pin 2
  CH3 (D2) ---- Q0, Beam X pin 21     CH4 (D3) ---- Q1, Beam X pin 23
  CH5 (D4) ---- HS, Beam X pin 14     CH6 (D5) ---- Q0, Beam Y pin 23
  CH7 (D6) ---- Q1, Beam Y pin 21     CH8 (D7) ---- VS, Beam Y pin 14
  CLK open                            GND ---- GND rail

 +5V ---- Clock 5V
              Beam X                              Beam Y
               notch                               notch
            +----U-----+                        +----U-----+
         CK |  1    24 | V+                  HS |  1    24 | V+
         RS |  2    23 | Q1                  RS |  2    23 | Q0
         GD |  3    22 | ..                  GD |  3    22 | ..
         GD |  4    21 | Q0                  GD |  4    21 | Q1
         GD |  5    20 | ..                  GD |  5    20 | ..
         GD |  6    19 | ..                  GD |  6    19 | ..
         GD |  7    18 | ..                  GD |  7    18 | ..
         GD |  8    17 | ..                  GD |  8    17 | ..
         GD |  9    16 | ..                  GD |  9    16 | ..
         GD | 10    15 | ..                  GD | 10    15 | ..
         GD | 11    14 | HS                  GD | 11    14 | VS
         GD | 12    13 | GD                  GD | 12    13 | GD
            +----------+                        +----------+
         C beside each V+, from pin 24 (VCC) to pin 12 (GND) on that chip
 GND ---- Clock GND, analyzer GND
```

Only the clock module feeds 5 V. Analyzer GND joins that ground. CLK on the analyzer is an output and stays open. On the CH340, RXD and 3V3 stay open.

Beam X pins that the bench uses:

| Pin | Name |
| --- | --- |
| 1 | CLK |
| 2 | RES |
| 12 | GND |
| 14 | HSYNC |
| 21 | Q0 |
| 23 | Q1 |
| 24 | VCC |

Beam Y uses the same CLK, RES, VCC, and GND pins. Q0 and Q1 swap:

| Pin | Name |
| --- | --- |
| 14 | VSYNC |
| 21 | Q1 |
| 23 | Q0 |

Q0 is the least significant bit on both chips. The other Q pins can stay open.

## Check

The host opens the analyzer with the fx2lafw driver. The capture runs at 20 kHz. Sticker CH1 through CH8 are D0 through D7, in that order.

RES closed (low) clears both counters. The clock module holds TXD low while the switch is closed, then the switch opens. Each later rising edge on TXD adds one Beam X count. D0 shows that edge.

Beam X uses D2, D3, and D4. Those are Q0, Q1, and HSYNC.

| Edges | Count | Q1 | Q0 | HSYNC |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 |
| 1 | 1 | 0 | 1 | 0 |
| 2 | 2 | 1 | 0 | 0 |
| 3 | 3 | 1 | 1 | 0 |
| 4 | 4 | 0 | 0 | 0 |
| 264 | 264 | 0 | 0 | 1 |
| 288 | 288 | 0 | 0 | 1 |
| 289 | 289 | 0 | 1 | 0 |
| 341 | 0 | 0 | 0 | 0 |

Count 341 is count 0 again. HSYNC rises on the edge that enters 264, and that rise is the Beam Y clock. The next rise is 341 Beam X edges later.

Beam Y, counted in HSYNC rises. D5, D6, and D7 are Q0, Q1, and VSYNC. Q0 and Q1 swap pins between the two chips.

| HSYNC rises | Count | Q1 | Q0 | VSYNC |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 |
| 1 | 1 | 0 | 1 | 0 |
| 2 | 2 | 1 | 0 | 0 |
| 3 | 3 | 1 | 1 | 0 |
| 243 | 243 | 1 | 1 | 1 |
| 245 | 245 | 0 | 1 | 1 |
| 246 | 246 | 1 | 0 | 0 |
| 262 | 0 | 0 | 0 | 0 |
