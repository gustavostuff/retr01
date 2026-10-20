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
  low | 3    22 | Q8          low | 3    22 | Q8
  low | 4    21 | Q7          low | 4    21 | Q7
  low | 5    20 | Q6          low | 5    20 | Q6
  low | 6    19 | Q5          low | 6    19 | Q5
  low | 7    18 | Q4          low | 7    18 | Q4
  low | 8    17 | Q3          low | 8    17 | Q3
  low | 9    16 | Q2          low | 9    16 | Q2
  low | 10   15 | Q1          low | 10   15 | Q1
  low | 11   14 | Q0          low | 11   14 | Q0
  GND | 12   13 | low         GND | 12   13 | low
      +----+----+                 +----+----+

wires

23 (Beam X) <--> 1 (Beam Y)
 2 (Beam X) <--> 2 (Beam Y)
TXD (CH340) <--> 1 (Beam X)

The LED wire lands on the anode.
Left to right in each color. Bit 0 is the rightmost LED.
22 (Beam X) <--> red bit 8
21 (Beam X) <--> red bit 7
20 (Beam X) <--> red bit 6
19 (Beam X) <--> red bit 5
18 (Beam X) <--> red bit 4
17 (Beam X) <--> red bit 3
16 (Beam X) <--> red bit 2
15 (Beam X) <--> red bit 1
14 (Beam X) <--> red bit 0
22 (Beam Y) <--> green bit 8
21 (Beam Y) <--> green bit 7
20 (Beam Y) <--> green bit 6
19 (Beam Y) <--> green bit 5
18 (Beam Y) <--> green bit 4
17 (Beam Y) <--> green bit 3
16 (Beam Y) <--> green bit 2
15 (Beam Y) <--> green bit 1
14 (Beam Y) <--> green bit 0
23 (Beam X) <--> yellow HSYNC
23 (Beam Y) <--> blue VSYNC

passives, rails, connectors

+5 V
  24 (Beam X), 24 (Beam Y), CH340 5V, CH340 VCC
GND
  12 (Beam X), 12 (Beam Y)
  3 through 11 and 13, both chips
  CH340 GND

100 nF from 24 to 12 on each chip.

10 kohm   2 (Beam X) <--> +5 V
switch    that RES wire <--> GND. Closed holds both counters at 0.
10 kohm   1 (Beam X) <--> +5 V

2.0 or 2.2 kohm in series on each LED wire. Cathode to GND. A lit LED is 1.
The yellow HSYNC LED shares the wire that already joins 23 (Beam X) and 1 (Beam Y).

CH340. Yellow jumper off. RXD and 3V3 open.
```

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
