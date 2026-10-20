# Color bars on a CRT

This bench is not passed yet. Eight vertical bars on an RGBS CRT. Four perma-proto boards. Beam X, Beam Y, and the PROM are the burned chips from the passing notes. These boards have no LEDs. The compositor, the VRAM, the CPU, and the CXA2075M stay off these boards. Power is off while a chip goes in.

<img src="img/perma-proto.png" alt="Perma-proto board, top and bottom">

Each module is one board like the photo. 30 rows, columns A through J, power rails along both edges.

The +5 V rail on every board is CH340 5V and CH340 VCC. The GND rail on every board is CH340 GND. Yellow jumper off. RXD, 3V3, and TXD stay open.

Notch up. Pin 1 is the top pin on the left. A pin with no wire and no passive is open.

Headers are single-row pins soldered on the boards. The first name in a header list is the top pin with the notch up. The Kycon KMDGX-9S-AS is absent from this bench. A single-row header on the RGBS board carries the CRT wires.

```text
1. Clock

U04, 74HCU04

      +---\_/---+
   1A | 1    14 | VCC
   1Y | 2    13 | 6A
   2A | 3    12 | 6Y
   2Y | 4    11 | 5A
   3A | 5    10 | 5Y
   3Y | 6     9 | 4A
  GND | 7     8 | 4Y
      +----+----+

U74, SN74HC74

       +---\_/---+
 1CLR# | 1    14 | VCC
    1D | 2    13 | 2CLR#
  1CLK | 3    12 | 2D
 1PRE# | 4    11 | 2CLK
    1Q | 5    10 | 2PRE#
   1/Q | 6     9 | 2Q
   GND | 7     8 | 2/Q
       +----+----+

header

DOT
GND

wires

 2 (U04) <--> 3 (U04)
 4 (U04) <--> 3 (U74)
 2 (U74) <--> 6 (U74)
 5 (U74) <--> 11 (U74)
 8 (U74) <--> 12 (U74)
GND <--> 7 (U74)

passives, rails

+5 V
  14 (U04), 14 (U74)
  1 (U74), 4 (U74), 10 (U74), 13 (U74)
GND
  7 (U04), 5 (U04), 9 (U04), 11 (U04), 13 (U04)
  7 (U74)

100 nF from VCC to GND on U04 and U74.

Y2    21.47727 MHz   1 (U04) <--> 2 (U04)
R24   1 Mohm         1 (U04) <--> 2 (U04)
C21   22 pF          1 (U04) <--> GND
C22   22 pF          2 (U04) <--> GND
R13   33 ohm         9 (U74) <--> DOT
```

DOT is 2Q after R13. That pin is Beam X CLK.

```text
2. Beams

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

header

DOT
GND
XQ0
XQ1
XQ2
XQ3
XQ4
XQ5
XQ6
XQ7
XQ8
HSYNC
YQ0
YQ1
YQ2
YQ3
YQ4
YQ5
YQ6
YQ7
YQ8
VSYNC

wires

23 (Beam X) <--> 1 (Beam Y)
 2 (Beam X) <--> 2 (Beam Y)
DOT <--> 1 (Beam X)
GND <--> 12 (Beam X)
XQ0 <--> 14 (Beam X)
XQ1 <--> 15 (Beam X)
XQ2 <--> 16 (Beam X)
XQ3 <--> 17 (Beam X)
XQ4 <--> 18 (Beam X)
XQ5 <--> 19 (Beam X)
XQ6 <--> 20 (Beam X)
XQ7 <--> 21 (Beam X)
XQ8 <--> 22 (Beam X)
HSYNC <--> 23 (Beam X)
YQ0 <--> 14 (Beam Y)
YQ1 <--> 15 (Beam Y)
YQ2 <--> 16 (Beam Y)
YQ3 <--> 17 (Beam Y)
YQ4 <--> 18 (Beam Y)
YQ5 <--> 19 (Beam Y)
YQ6 <--> 20 (Beam Y)
YQ7 <--> 21 (Beam Y)
YQ8 <--> 22 (Beam Y)
VSYNC <--> 23 (Beam Y)

passives, rails

+5 V
  24 (Beam X), 24 (Beam Y)
GND
  12 (Beam X), 12 (Beam Y)
  3 (Beam X) through 11 (Beam X), 13 (Beam X)
  3 (Beam Y) through 11 (Beam Y), 13 (Beam Y)

100 nF from 24 to 12 on each chip.

10 kohm   2 (Beam X) <--> +5 V
switch    that RES wire <--> GND. Closed holds both counters at 0.
10 kohm   1 (Beam X) <--> +5 V
```

HSYNC is the same net as Beam Y CLK. The color-bar cable uses XQ7, XQ6, XQ5, HSYNC, and VSYNC.

```text
3. PROM, AT27C256R

      +---\_/---+
  VPP | 1    28 | VCC
  A12 | 2    27 | A14
   A7 | 3    26 | A13
   A6 | 4    25 | A8
   A5 | 5    24 | A9
   A4 | 6    23 | A11
   A3 | 7    22 | OE#
   A2 | 8    21 | A10
   A1 | 9    20 | CE#
   A0 | 10   19 | O7
   O0 | 11   18 | O6
   O1 | 12   17 | O5
   O2 | 13   16 | O4
  GND | 14   15 | O3
      +----+----+

header

A5
A4
A3
OE#
GND
O7
O6
O5
O4
O3
O2
O1
O0

wires

A5 <--> 5 (PROM)
A4 <--> 6 (PROM)
A3 <--> 7 (PROM)
OE# <--> 22 (PROM)
GND <--> 14 (PROM)
O7 <--> 19 (PROM)
O6 <--> 18 (PROM)
O5 <--> 17 (PROM)
O4 <--> 16 (PROM)
O3 <--> 15 (PROM)
O2 <--> 13 (PROM)
O1 <--> 12 (PROM)
O0 <--> 11 (PROM)

passives, rails

+5 V
  1 (PROM), 28 (PROM)
GND
  2 (PROM), 3 (PROM), 4 (PROM), 14 (PROM)
  8 (PROM), 9 (PROM), 10 (PROM)
  20 (PROM), 21 (PROM), 23 (PROM), 24 (PROM), 25 (PROM), 26 (PROM), 27 (PROM)

100 nF from VCC to GND.
```

```text
4. RGBS

NOR, 74HC02

      +---\_/---+
   1Y | 1    14 | VCC
   1A | 2    13 | 4Y
   1B | 3    12 | 4B
   2Y | 4    11 | 4A
   2A | 5    10 | 3Y
   2B | 6     9 | 3B
  GND | 7     8 | 3A
      +----+----+

header

O7
O6
O5
O4
O3
O2
O1
O0
HSYNC
VSYNC
OE#
GND

CRT header

Red
Green
Blue
CSYNC
GND

wires

HSYNC <--> 2 (NOR)
VSYNC <--> 3 (NOR)
 1 (NOR) <--> 5 (NOR)
 4 (NOR) <--> OE#
GND <--> 7 (NOR)
CRT GND <--> 7 (NOR)

passives, rails

+5 V
  14 (NOR)
GND
  7 (NOR), 6 (NOR), 8 (NOR), 9 (NOR), 11 (NOR), 12 (NOR)

100 nF from VCC to GND on NOR.

R1    1.00 kohm      O7 <--> Red
R2    2.00 kohm      O6 <--> Red
R3    4.00 kohm      O5 <--> Red
R9    75 ohm         Red <--> GND
R4    1.00 kohm      O4 <--> Green
R5    2.00 kohm      O3 <--> Green
R6    4.00 kohm      O2 <--> Green
R10   75 ohm         Green <--> GND
R7    1.00 kohm      O1 <--> Blue
R8    2.00 kohm      O0 <--> Blue
R11   75 ohm         Blue <--> GND

Rs    470 ohm        1 (NOR) <--> CSYNC
```

Red, Green, Blue, CSYNC, and GND are pins 7, 3, 1, 5, and the shell of a Mega Drive 2 cable on the Kycon KMDGX-9S-AS. Pins 2, 4, 6, 8, and 9 of that connector have no wire on this bench.

```text
cables

DOT (Clock) <--> DOT (Beam)           9 (U74) through R13, and 1 (Beam X)
XQ7 (Beam) <--> A5 (PROM)             21 (Beam X) and 5 (PROM)
XQ6 (Beam) <--> A4 (PROM)             20 (Beam X) and 6 (PROM)
XQ5 (Beam) <--> A3 (PROM)             19 (Beam X) and 7 (PROM)
HSYNC (Beam) <--> HSYNC (RGBS)        23 (Beam X) and 2 (NOR)
VSYNC (Beam) <--> VSYNC (RGBS)        23 (Beam Y) and 3 (NOR)
O7 (PROM) <--> O7 (RGBS)              19 (PROM)
O6 (PROM) <--> O6 (RGBS)              18 (PROM)
O5 (PROM) <--> O5 (RGBS)              17 (PROM)
O4 (PROM) <--> O4 (RGBS)              16 (PROM)
O3 (PROM) <--> O3 (RGBS)              15 (PROM)
O2 (PROM) <--> O2 (RGBS)              13 (PROM)
O1 (PROM) <--> O1 (RGBS)              12 (PROM)
O0 (PROM) <--> O0 (RGBS)              11 (PROM)
OE# (RGBS) <--> OE# (PROM)            4 (NOR) and 22 (PROM)
GND on each header <--> CH340 GND
```

Visible counts are Beam X 0 through 255. Q7, Q6, and Q5 make eight bars of 32 dots. The address is `32*Q7 + 16*Q6 + 8*Q5`, kit rows 0, 8, 16, 24, 32, 40, 48, and 56. Those rows run from black to white. Rows 0 through 7 are all dark, which is why those bits land on A5, A4, and A3.

OE# is high only while HSYNC or VSYNC is high. Counts 256 through 263 still show a color, and so do Beam Y counts 240 through 242. Full-border blanking belongs to the compositor.

A 75 ohm TV input sits in parallel with R9, R10, and R11, so the bars are about half of the 0.7 Vpp in the motherboard note.

The three address wires, the NOR, and Rs are this bench. They are not motherboard color-index wiring. `v_07` has no series resistor on CSYNC.

## Check

Power is on. RES, pin 2 of both PLDs, is high. CH340 TXD is open.

| Check | Passes when |
| --- | --- |
| U74 pin 9 | About 5.37 MHz. A scope shows a period near 186 ns. |
| Beam X pin 23 | High for about 4.7 us, once per 63.5 us. |
| Beam Y pin 23 | High for three of those lines, once per frame. |
| CRT | Eight vertical bars, stable, near 60 Hz. Left to right: black, dark, gray, blue, gray, cyan, white, light. |
| Sync interval | Guns dark while pin 23 of either PLD is high. |

The white bar is the seventh, kit row 48. The black bar is the first, kit row 0.
