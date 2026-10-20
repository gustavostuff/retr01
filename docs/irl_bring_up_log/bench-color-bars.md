# Color bars on a CRT

This bench is not passed yet. Eight vertical bars on an RGBS CRT. Beam X, Beam Y, and the PROM are the burned chips from the passing notes. This bench has no LEDs. The compositor, the VRAM, the CPU, and the CXA2075M stay off this board. Power is off while a chip goes in.

The order is the PROM, then Beam X, then Beam Y, then the clock, the NOR, and the CRT. The +5 V rail is CH340 5V and CH340 VCC. The GND rail is CH340 GND. Yellow jumper off. RXD, 3V3, and TXD stay open.

Notch up. Pin 1 is the top pin on the left. A pin with no wire and no passive is open.

```text
1. PROM, AT27C256R

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

wires

none. The address wires arrive with Beam X.

passives, rails

+5 V
  1 (PROM), 28 (PROM)
GND
  2 (PROM), 3 (PROM), 4 (PROM), 14 (PROM)
  8 (PROM), 9 (PROM), 10 (PROM)
  20 (PROM), 21 (PROM), 23 (PROM), 24 (PROM), 25 (PROM), 26 (PROM), 27 (PROM)

100 nF from VCC to GND.

2. Beam X, ATF22V10C

      +---\_/---+
  CLK | 1    24 | VCC
  RES | 2    23 | HSYNC
  low | 3    22 | Q8
  low | 4    21 | Q7
  low | 5    20 | Q6
  low | 6    19 | Q5
  low | 7    18 | Q4
  low | 8    17 | Q3
  low | 9    16 | Q2
  low | 10   15 | Q1
  low | 11   14 | Q0
  GND | 12   13 | low
      +----+----+

wires

21 (Beam X) <--> 5 (PROM)
20 (Beam X) <--> 6 (PROM)
19 (Beam X) <--> 7 (PROM)

passives, rails

+5 V
  24 (Beam X)
GND
  12 (Beam X)
  3 (Beam X) through 11 (Beam X), 13 (Beam X)

100 nF from 24 to 12.

10 kohm   2 (Beam X) <--> +5 V
switch    that RES wire <--> GND. Closed holds the counter at 0.
10 kohm   1 (Beam X) <--> +5 V

3. Beam Y, ATF22V10C

      +---\_/---+
  CLK | 1    24 | VCC
  RES | 2    23 | VSYNC
  low | 3    22 | Q8
  low | 4    21 | Q7
  low | 5    20 | Q6
  low | 6    19 | Q5
  low | 7    18 | Q4
  low | 8    17 | Q3
  low | 9    16 | Q2
  low | 10   15 | Q1
  low | 11   14 | Q0
  GND | 12   13 | low
      +----+----+

wires

23 (Beam X) <--> 1 (Beam Y)
 2 (Beam X) <--> 2 (Beam Y)

passives, rails

+5 V
  24 (Beam Y)
GND
  12 (Beam Y)
  3 (Beam Y) through 11 (Beam Y), 13 (Beam Y)

100 nF from 24 to 12.

The switch on the Beam X RES wire holds both counters at 0.

4. Clock, NOR, and CRT

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

wires

 2 (U04) <--> 3 (U04)
 4 (U04) <--> 3 (U74)
 2 (U74) <--> 6 (U74)
 5 (U74) <--> 11 (U74)
 8 (U74) <--> 12 (U74)
23 (Beam X) <--> 2 (NOR)
23 (Beam Y) <--> 3 (NOR)
 1 (NOR) <--> 5 (NOR)
 4 (NOR) <--> 22 (PROM)

passives, rails, connectors

+5 V
  14 (U04), 14 (U74), 14 (NOR)
  1 (U74), 4 (U74), 10 (U74), 13 (U74)
GND
  7 (U04), 5 (U04), 9 (U04), 11 (U04), 13 (U04)
  7 (U74)
  7 (NOR), 6 (NOR), 8 (NOR), 9 (NOR), 11 (NOR), 12 (NOR)
  J2 shell

100 nF from VCC to GND on U04, U74, and NOR.

Y2    21.47727 MHz   1 (U04) <--> 2 (U04)
R24   1 Mohm         1 (U04) <--> 2 (U04)
C21   22 pF          1 (U04) <--> GND
C22   22 pF          2 (U04) <--> GND
R13   33 ohm         9 (U74) <--> 1 (Beam X)

R1    1.00 kohm      19 (PROM) <--> Red
R2    2.00 kohm      18 (PROM) <--> Red
R3    4.00 kohm      17 (PROM) <--> Red
R9    75 ohm         Red <--> GND
R4    1.00 kohm      16 (PROM) <--> Green
R5    2.00 kohm      15 (PROM) <--> Green
R6    4.00 kohm      13 (PROM) <--> Green
R10   75 ohm         Green <--> GND
R7    1.00 kohm      12 (PROM) <--> Blue
R8    2.00 kohm      11 (PROM) <--> Blue
R11   75 ohm         Blue <--> GND

Rs    470 ohm        1 (NOR) <--> 5 (J2)

J2, Kycon KMDGX-9S-AS. A Mega Drive 2 cable uses these numbers.
  7   Red
  3   Green
  1   Blue
  5   Rs
  S   GND
  2, 4, 6, 8, 9 open
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
