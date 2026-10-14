# Tier A breadboard

Wire the flashed Beam X, Beam Y, and color PROM, then look for a solid field on the monitor. The three burn files are `firmware/a/beam_x.jed`, `firmware/a/beam_y.jed`, and `firmware/a/at27c256r_kit.bin`. Pin names on those burns are in [`docs/misc/irl_bring_up_log/firmware/a/README.md`](../misc/irl_bring_up_log/firmware/a/README.md).

Beam X is a dot counter and HSYNC. Beam Y is a line counter and VSYNC. Neither burn drives the PROM address. Jumpers on A5 through A0 pick one kit color. J2 pin 3 (CSYNC) stays open. The monitor uses HSYNC and VSYNC.

Power is off while a chip goes into a socket and while a chip comes out. One 5 V rail feeds every chip. The PROM programming voltage stays on the T48. On this board pin 1 (VPP) and pin 28 (VCC) share that same 5 V rail.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, Beam X image | 1 | Dot counter and HSYNC |
| ATF22V10C, PDIP-24, Beam Y image | 1 | Line counter and VSYNC |
| AT27C256R, PDIP-28, kit image | 1 | 64 colors |
| 74HCU04, PDIP-14 | 1 | Pierce oscillator and buffer. A 74HC04 is a different part. |
| 74HC74, PDIP-14 | 1 | Divides 21.47727 MHz by 4 |
| 21.47727 MHz crystal, HC-49 | 1 | Master clock |
| 1 Mohm | 1 | Pierce feedback |
| 18 pF to 20 pF | 2 | Crystal load capacitors |
| 33 ohm | 1 | Series resistor on the dot clock |
| 4 kohm, 2 kohm, 1 kohm | 2 of each | Red and green DAC weights |
| 2 kohm, 1 kohm | 1 of each | Blue DAC weights |
| 75 ohm | 3 | Gun loads |
| 100 nF | 5 | One across each IC |
| 220 uF | 1 | Bulk capacitor at the 5 V entry |
| 1x8 pin header, 2.54 mm | 1 | J2, to the monitor cable |

The 74HCU04, the 74HC74, and the two PLDs are 0.3 inch (300 mil) between pin rows, so each one straddles the center channel of a breadboard. The PROM is 0.6 inch (600 mil). Its rows do not land on opposite sides of that channel. A 28-pin socket of the same width, with one jumper per pin, keeps the pin numbers on the right holes.

Pin 1 is the pin next to the notch. Down the notched side the pins count upward. Up the far side they continue.

## 1. Rails

1. Connect the 5 V supply to the breadboard +5 V rail.
2. Connect the supply ground to the breadboard ground rail.
3. Connect the 220 uF capacitor from the +5 V rail to the ground rail. The marked side goes to ground.
4. Bridge +5 V to the rail on the far side of each breadboard in use. Bridge ground the same way.

## 2. Seat the five ICs

Power is still off.

1. Seat the 74HCU04 across the center channel, with the crystal on the next rows. Notch toward the row chosen as pin 1.
2. Seat the 74HC74 the same way, next to the 74HCU04. The 21 MHz wire between them stays short.
3. Seat Beam X across the center channel. Notch toward the row chosen as pin 1.
4. Seat Beam Y the same way, close to Beam X. HSYNC is a short jumper between them.
5. Seat the PROM in the wide socket. Notch marks pin 1.

## 3. Power on each IC

1. Connect 74HCU04 pin 14 to +5 V.
2. Connect 74HCU04 pin 7 to ground.
3. Connect a 100 nF capacitor from 74HCU04 pin 14 to 74HCU04 pin 7. Keep the leads short.
4. Connect 74HC74 pin 14 to +5 V.
5. Connect 74HC74 pin 7 to ground.
6. Connect a 100 nF capacitor from 74HC74 pin 14 to 74HC74 pin 7.
7. Connect Beam X pin 24 to +5 V.
8. Connect Beam X pin 12 to ground.
9. Connect a 100 nF capacitor from Beam X pin 24 to Beam X pin 12. Keep the leads short.
10. Connect Beam Y pin 24 to +5 V.
11. Connect Beam Y pin 12 to ground.
12. Connect a 100 nF capacitor from Beam Y pin 24 to Beam Y pin 12.
13. Connect PROM pin 28 (VCC) to +5 V.
14. Connect PROM pin 1 (VPP) to +5 V. Same rail as pin 28.
15. Connect PROM pin 14 (GND) to ground.
16. Connect a 100 nF capacitor from PROM pin 28 to PROM pin 14.

## 4. PLD pins that stay tied

Both PLDs use the same ties. RES high lets the counter run. The unused input pins sit at ground so they do not float.

1. Connect Beam X pin 2 (RES) to +5 V.
2. Connect Beam X pins 3, 4, 5, 6, 7, 8, 9, 10, 11, and 13 to ground.
3. Connect Beam Y pin 2 (RES) to +5 V.
4. Connect Beam Y pins 3, 4, 5, 6, 7, 8, 9, 10, 11, and 13 to ground.

Beam X pins 15 through 23, and Beam Y pins 15 through 23, are the count bits. Leave them open. A probe can use them later. They do not go to the PROM.

## 5. Dot clock

The crystal, the 1 Mohm resistor, the two load capacitors, and 74HCU04 pins 1 and 2 sit on adjacent rows. Leads on that loop stay short. The crystal has no polarity. A breadboard adds capacitance, so 18 pF is the starting load. 15 pF is the next value when pin 4 stays quiet.

Gates 3 through 6 are unused. Their inputs go to ground. Their outputs stay open.

1. Connect 74HCU04 pins 5, 9, 11, and 13 to ground.
2. Leave 74HCU04 pins 6, 8, 10, and 12 open.
3. Connect one crystal pin to 74HCU04 pin 1.
4. Connect the other crystal pin to 74HCU04 pin 2.
5. Connect the 1 Mohm resistor from 74HCU04 pin 1 to 74HCU04 pin 2.
6. Connect one 18 pF capacitor from 74HCU04 pin 1 to ground.
7. Connect the other 18 pF capacitor from 74HCU04 pin 2 to ground.
8. Connect 74HCU04 pin 2 to 74HCU04 pin 3.
9. Connect 74HCU04 pin 4 to 74HC74 pin 3.

Pin 4 is about 21.48 MHz. The 74HC74 divides that by 2, then by 2 again. Both stages toggle by feeding the inverted output back to D. PRE# and CLR# stay high so neither flip-flop is forced set or clear.

10. Connect 74HC74 pins 1, 4, 10, and 13 to +5 V.
11. Connect 74HC74 pin 6 to 74HC74 pin 2.
12. Connect 74HC74 pin 5 to 74HC74 pin 11.
13. Connect 74HC74 pin 8 to 74HC74 pin 12.
14. Connect 74HC74 pin 9 to one end of the 33 ohm resistor.
15. Connect the other end of the 33 ohm resistor to Beam X pin 1 (CLK).

Pin 5 is about 10.74 MHz. Pin 9 is the 5.369318 MHz dot clock.

## 6. Beam Y clock, and the sync pins

Beam Y advances once per line, on the rising edge of HSYNC.

1. Connect Beam X pin 14 (HSYNC) to Beam Y pin 1 (CLK).
2. Connect that same Beam X pin 14 node to J2 pin 5 (HSYNC).
3. Connect Beam Y pin 14 (VSYNC) to J2 pin 7 (VSYNC).
4. Leave J2 pin 3 (CSYNC) open.

## 7. PROM address pins that stay at ground

The kit occupies addresses 0 through 63. A high on any pin in this list selects some other address. Those locations are still `0xFF`.

1. Connect PROM pin 2 (A12) to ground.
2. Connect PROM pin 3 (A7) to ground.
3. Connect PROM pin 4 (A6) to ground.
4. Connect PROM pin 21 (A10) to ground.
5. Connect PROM pin 23 (A11) to ground.
6. Connect PROM pin 24 (A9) to ground.
7. Connect PROM pin 25 (A8) to ground.
8. Connect PROM pin 26 (A13) to ground.
9. Connect PROM pin 27 (A14) to ground. High on this pin selects the upper 16 KB.

## 8. PROM enables

Both enables are active low. A high on either one turns the data pins off.

1. Connect PROM pin 20 (CE#) to ground.
2. Connect PROM pin 22 (OE#) to ground.

## 9. Color index

A5 is the 32s bit. A0 is the 1s bit. The first check uses index 48, byte `0xFF`, every gun at full scale. That field is white when the DAC and the header match the list below.

1. Connect PROM pin 5 (A5) to +5 V.
2. Connect PROM pin 6 (A4) to +5 V.
3. Connect PROM pin 7 (A3) to ground.
4. Connect PROM pin 8 (A2) to ground.
5. Connect PROM pin 9 (A1) to ground.
6. Connect PROM pin 10 (A0) to ground.

Another index is the same six pins, from [`docs/misc/irl_bring_up_log/individual_ic_test/at27c256r.md`](../misc/irl_bring_up_log/individual_ic_test/at27c256r.md). Index 0 is black in the kit, so a dark screen at that index is the kit and not a dead DAC.

## 10. Red gun

PROM pin 19 is O7 (red weight 4), pin 18 is O6 (weight 2), pin 17 is O5 (weight 1). The three resistors meet at one node. That node is the red output.

1. Connect PROM pin 19 to one end of a 4 kohm resistor (R1).
2. Connect PROM pin 18 to one end of a 2 kohm resistor (R2).
3. Connect PROM pin 17 to one end of a 1 kohm resistor (R3).
4. Connect the free ends of R1, R2, and R3 together. That joint is the red node.
5. Connect the red node to one end of a 75 ohm resistor (R9).
6. Connect the other end of R9 to ground.
7. Connect the red node to J2 pin 8 (R).

## 11. Green gun

PROM pin 16 is O4 (green weight 4), pin 15 is O3 (weight 2), pin 13 is O2 (weight 1). Pin 14 is ground, not a data pin.

1. Connect PROM pin 16 to one end of a 4 kohm resistor (R4).
2. Connect PROM pin 15 to one end of a 2 kohm resistor (R5).
3. Connect PROM pin 13 to one end of a 1 kohm resistor (R6).
4. Connect the free ends of R4, R5, and R6 together. That joint is the green node.
5. Connect the green node to one end of a 75 ohm resistor (R10).
6. Connect the other end of R10 to ground.
7. Connect the green node to J2 pin 6 (G).

## 12. Blue gun

PROM pin 12 is O1 (blue weight 2). PROM pin 11 is O0 (blue weight 1).

1. Connect PROM pin 12 to one end of a 2 kohm resistor (R7).
2. Connect PROM pin 11 to one end of a 1 kohm resistor (R8).
3. Connect the free ends of R7 and R8 together. That joint is the blue node.
4. Connect the blue node to one end of a 75 ohm resistor (R11).
5. Connect the other end of R11 to ground.
6. Connect the blue node to J2 pin 4 (B).

The PROM data pins reach the header only through those resistors. A direct wire from a data pin to J2 puts a 5 V swing on the monitor input.

## 13. Header ground

1. Connect J2 pin 1 to ground.
2. Connect J2 pin 2 to ground.

J2, left to right, pin 1 at the left:

```text
1 GND,  2 GND,  3 CSYNC (open),  4 B,  5 HSYNC,  6 G,  7 VSYNC,  8 R
```

## 14. Power on, then check

Turn the 5 V supply on. The oscillator runs as soon as power is up.

| Probe | A good result |
| --- | --- |
| 74HCU04 pin 4 | About 21.48 MHz |
| 74HC74 pin 5 | About 10.74 MHz |
| 74HC74 pin 9, and Beam X pin 1 | About 5.37 MHz |
| Beam X pin 14 | About 15.7 kHz, high for about 4.7 us |
| Beam Y pin 14 | About 60.1 Hz, high for 3 lines |

A monitor set for separate HSYNC and VSYNC, on J2 pins 5 and 7, locks to a solid white field at index 48. RGBS wants CSYNC on J2 pin 3. This burn does not drive that pin.

RES low on a PLD clears that counter and holds it. Beam X pin 14 stays low while Beam X pin 2 is low. Return pin 2 to +5 V before the next check. The same rule applies to Beam Y pin 2.

## Related

- Analyzer capture: [`docs/misc/irl_bring_up_log/a/flashing.md`](../misc/irl_bring_up_log/a/flashing.md)
- Per-chip probe points: [`docs/misc/irl_bring_up_log/individual_ic_test/`](../misc/irl_bring_up_log/individual_ic_test/)
- Tier B adds the Compositor on this same layout: [`tier-b-breadboard-placement.md`](tier-b-breadboard-placement.md)
