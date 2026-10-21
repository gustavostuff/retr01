# Beam X and Beam Y

This bench passes. Red LEDs show the Beam X count and green LEDs show the Beam Y count. Q0 is pin 14 through Q8 on pin 22. Pin 23 is HSYNC on Beam X and VSYNC on Beam Y. In the photo, the GND and 5 V cables at the top-right are swapped.

<img src="img/ATF22V10C-bench-passed-both-x-and-y-beam-counters.jpg" alt="ATF22V10C breadboard, Beam X and Beam Y">

Two ATF22V10C parts, already burned. Beam X counts dots 0 through 340. Beam Y counts lines 0 through 261. One frame is 262 lines. The images are [`beam_x.jed`](../../../apps/flasher_tool/firmware/a/beam_x.jed) and [`beam_y.jed`](../../../apps/flasher_tool/firmware/a/beam_y.jed).

The package is 300 mil, in a 24-pin socket. Power is off while a chip goes in or comes out. Only the CH340 feeds 5 V.

Notch up. Pin 1 is the top pin on the left. A pin with no wire and no passive is open.

```text
ICs

Beam X, ATF22V10C          Beam Y, ATF22V10C

      +---\_/---+                 +---\_/---+
  CLK | 1    24 | VCC         CLK | 1    24 | VCC
  RES | 2    23 | HSYNC       RES | 2    23 | VSYNC
  GND | 3    22 | Q8          GND | 3    22 | Q8
  GND | 4    21 | Q7          GND | 4    21 | Q7
  GND | 5    20 | Q6          GND | 5    20 | Q6
  GND | 6    19 | Q5          GND | 6    19 | Q5
  GND | 7    18 | Q4          GND | 7    18 | Q4
  GND | 8    17 | Q3          GND | 8    17 | Q3
  GND | 9    16 | Q2          GND | 9    16 | Q2
  GND | 10   15 | Q1          GND | 10   15 | Q1
  GND | 11   14 | Q0          GND | 11   14 | Q0
  GND | 12   13 | GND         GND | 12   13 | GND
      +----+----+                 +----+----+

wires

23 (Beam X) goes to 1 (Beam Y)
2 (Beam X) goes to 2 (Beam Y)
TXD (CH340) goes to 1 (Beam X)

The LED wire lands on the anode.
Left to right in each color. Bit 0 is the rightmost LED.
22 (Beam X) goes to red LED bit 8
21 (Beam X) goes to red LED bit 7
20 (Beam X) goes to red LED bit 6
19 (Beam X) goes to red LED bit 5
18 (Beam X) goes to red LED bit 4
17 (Beam X) goes to red LED bit 3
16 (Beam X) goes to red LED bit 2
15 (Beam X) goes to red LED bit 1
14 (Beam X) goes to red LED bit 0
22 (Beam Y) goes to green LED bit 8
21 (Beam Y) goes to green LED bit 7
20 (Beam Y) goes to green LED bit 6
19 (Beam Y) goes to green LED bit 5
18 (Beam Y) goes to green LED bit 4
17 (Beam Y) goes to green LED bit 3
16 (Beam Y) goes to green LED bit 2
15 (Beam Y) goes to green LED bit 1
14 (Beam Y) goes to green LED bit 0
23 (Beam X) goes to yellow HSYNC LED
23 (Beam Y) goes to blue VSYNC LED

passives, rails, connectors

+5 V goes to 24 (Beam X), 24 (Beam Y), CH340 5V, CH340 VCC
GND goes to 12 (Beam X), 12 (Beam Y), 3 (Beam X) through 11 (Beam X), 13 (Beam X), 3 (Beam Y) through 11 (Beam Y), 13 (Beam Y), CH340 GND
(100 nF) goes to 24 (Beam X), 12 (Beam X)
(100 nF) goes to 24 (Beam Y), 12 (Beam Y)
(10 kohm) goes to 2 (Beam X), +5 V
switch goes to 2 (Beam X), GND. Closed holds both counters at 0.
(10 kohm) goes to 1 (Beam X), +5 V
(2.0 or 2.2 kohm) goes to each LED wire, GND. Cathode to GND. A lit LED is 1.
The yellow HSYNC LED shares the wire that already joins 23 (Beam X) and 1 (Beam Y).

CH340. Yellow jumper off. RXD and 3V3 open.
```

Pins 3 through 11 and pin 13 are unused inputs on each chip. They sit on GND.

## Check

`./ic_verifier 2 --clock` holds TXD low while the reset switch is closed. After the switch opens, TXD rises 10 times a second. `./ic_verifier 2 --clock --hz 2` is the same clock at another rate.

Each rising edge adds one Beam X count. Count 341 is count 0 again. One line is 341 edges. At 10 Hz that line takes 34.1 seconds. HSYNC rises on the edge that enters 264, and that rise adds one Beam Y count. Beam Y is already 1 while Beam X finishes the line and wraps to 0. The next HSYNC rise is 341 Beam X edges later.

A frame is 262 lines. At 10 Hz that is about 2.5 hours. Line 262 wraps Beam Y to 0.

A lit LED is 1. Beam X, in count order:

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

HSYNC is high on counts 264 through 288. Q8 through Q2 follow the same count. Count 256 is Q8 alone. Count 340 is Q8, Q6, Q4, and Q2.

Beam Y, counted in HSYNC rises:

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

VSYNC is high on counts 243 through 245. Count 256 is Q8 alone. Count 261 is Q8, Q2, and Q0.
