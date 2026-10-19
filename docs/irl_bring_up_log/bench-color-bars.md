# Color bars on a CRT

This bench is not passed yet.

It puts eight vertical bars on an RGBS CRT. The [Beam X / Beam Y](bench-beam-xy.md) note and the [color PROM](bench-at27c256r.md) note are the pin list for those chips. The sim pin netlist is the pin list for the dot clock, the DAC resistors, and **J2**. A pass updates the sim, the SKiDL netlist, and `v_07`. See [After a bench passes](README.md#after-a-bench-passes).

The compositor, the VRAM, the CPU, and the CXA2075M stay off this board.

## Already on the board

The Beam X / Beam Y setup stays: both ATF22V10C chips, the 20 count LEDs, shared **RES** on pin 2, unused inputs tied to ground, and Beam X pin 23 wired to Beam Y pin 1.

The color PROM socket stays, with the burned AT27C256R, +5 V on pins 1 and 28, and ground on pins 2, 3, 4, 14, 20, 21, 23, 24, 25, 26, and 27. Those grounded pins are A12, A7, A6, GND, CE#, A10, A11, A9, A8, A13, and A14.

These wires are absent on this bench:

- The CH340 TXD wire on Beam X pin 1. The CRT uses the dot clock.
- The six address switches. A5, A4, and A3 come from Beam X. A2, A1, and A0 sit at ground.
- The PROM data LEDs, and the ground wire on OE# (pin 22). The data pins drive the DAC. OE# blanks the guns during sync.

## What the bars are

Visible dots are Beam X counts 0 through 255. Q7, Q6, and Q5 split that into eight bars of 32 dots. The other Beam X outputs, and every Beam Y output, stay on their LEDs.

| Beam X | Pin | PROM | Pin | Index weight |
| --- | --- | --- | --- | --- |
| Q7 | 21 | A5 | 5 | 32 |
| Q6 | 20 | A4 | 6 | 16 |
| Q5 | 19 | A3 | 7 | 8 |

A2 (pin 8), A1 (pin 9), and A0 (pin 10) stay at ground. The address is `32*Q7 + 16*Q6 + 8*Q5`, so the eight bars are kit rows 0, 8, 16, 24, 32, 40, 48, and 56. Those rows run from black to white. Rows 0 through 7 are all dark, so Q7, Q6, and Q5 land on A5, A4, and A3 for this test.

These three jumpers are this bench. They are not compositor pins, and they do not go onto `v_07` as the color-index wiring.

## Parts to add

| Mark | Part | Role |
| --- | --- | --- |
| Y2 | 21.47727 MHz crystal, HC-49 | Dot-clock crystal |
| U04 | 74HCU04 | Pierce gate and buffer |
| U74 | SN74HC74 | Divide by 4 |
| R24 | 1 M ohm | Feedback across the Pierce gate |
| R13 | 33 ohm | Series from DOT into Beam X pin 1 |
| C21, C22 | 22 pF | One on each crystal leg |
| C04, C74, CNOR | 100 nF | One across each new chip |
| NOR | 74HC02 | CSYNC and PROM blanking |
| R1 | 1.00 k ohm | Red MSB, PROM O7 |
| R2 | 2.00 k ohm | Red, PROM O6 |
| R3 | 4.00 k ohm | Red LSB, PROM O5 |
| R4 | 1.00 k ohm | Green MSB, PROM O4 |
| R5 | 2.00 k ohm | Green, PROM O3 |
| R6 | 4.00 k ohm | Green LSB, PROM O2 |
| R7 | 1.00 k ohm | Blue MSB, PROM O1 |
| R8 | 2.00 k ohm | Blue LSB, PROM O0 |
| R9, R10, R11 | 75.0 ohm | One from each gun node to ground |
| Rs | 470 ohm | Series into the cable sync wire. Breadboard only |

R1 through R8 and R9 through R11 are the DAC in `docs/hardware.md`. The sim ties PROM O7 through O0 to R1 through R8. R9, R10, and R11 sit from the gun nodes to ground.

## Dot clock

This is the motherboard DOT path. U04 gates 3 through 6 are unused. Their inputs sit on ground so they do not float: pins 5, 9, 11, and 13.

```
 74HCU04                          SN74HC74
 1  1A  o---- Y2 ----o  1Y  2     1  1CLR#  -- +5 V
 3  2A  o----------- 1Y          2  1D     -- pin 6 (1/Q)
 4  2Y  o----------------------  3  1CLK
 5  3A  -- GND                  4  1PRE#  -- +5 V
 7  GND -- GND                  5  1Q     -- pin 11 (2CLK)
 9  4A  -- GND                  6  1/Q    -- pin 2 (1D)
11  5A  -- GND                  7  GND    -- GND
13  6A  -- GND                  8  2/Q    -- pin 12 (2D)
14  VCC -- +5 V                 9  2Q     -- R13 -- Beam X pin 1
                                10  2PRE# -- +5 V
R24, 1 M ohm, from U04 pin 1 to pin 2.
C21, 22 pF, from U04 pin 1 to GND.
C22, 22 pF, from U04 pin 2 to GND.
                                11  2CLK   -- pin 5 (1Q)
                                12  2D     -- pin 8 (2/Q)
                                13  2CLR#  -- +5 V
                                14  VCC    -- +5 V
```

U74 pin 9 is DOT, 5.369318 MHz. R13 is the 33 ohm series resistor between that pin and Beam X CLK. Beam Y clocks from Beam X pin 23.

## Sync and blanking

Beam X pin 23 is high on counts 264 through 288. Beam Y pin 23 is high on counts 243 through 245. Both pulses are active high.

**J2** pin 5 is CSYNC, the same net as the CXA2075M SYNCIN pin. On this bench that net is Beam X pin 23 NOR Beam Y pin 23, so the pulse is low while either sync pin is high. A second NOR turns that over and drives PROM OE#. The outputs are off for the same interval. R9, R10, and R11 then hold the gun nodes at ground.

```
 74HC02
 1  1Y   CSYNC, through Rs (470 ohm) to the cable sync wire
 2  1A   Beam X pin 23 (HSYNC)
 3  1B   Beam Y pin 23 (VSYNC)
 4  2Y   PROM pin 22 (OE#)
 5  2A   pin 1 (CSYNC, before Rs)
 6  2B   GND
 7  GND
 8  3A   GND
 9  3B   GND
11  4A   GND
12  4B   GND
14  VCC  +5 V
```

OE# is high only while HSYNC or VSYNC is high. Counts 256 through 263 still show a color, and so do Beam Y counts 240 through 242. That strip is this bench. Full-border blanking belongs to the compositor, which is not on this board.

Rs is breadboard only. `v_07` has no series resistor on CSYNC. The NOR pin is not a 75 ohm driver, and the TV sync input is about 75 ohm.

## DAC

Each PROM data pin feeds one resistor. The three resistors of a gun meet at one node. A 75 ohm resistor goes from that node to ground. The cable takes the same node.

| PROM | Pin | Resistor | Gun node | Cable |
| --- | --- | --- | --- | --- |
| O7 | 19 | R1, 1.00 k | Red | J2 pin 7 |
| O6 | 18 | R2, 2.00 k | Red | J2 pin 7 |
| O5 | 17 | R3, 4.00 k | Red | J2 pin 7 |
| O4 | 16 | R4, 1.00 k | Green | J2 pin 3 |
| O3 | 15 | R5, 2.00 k | Green | J2 pin 3 |
| O2 | 13 | R6, 4.00 k | Green | J2 pin 3 |
| O1 | 12 | R7, 1.00 k | Blue | J2 pin 1 |
| O0 | 11 | R8, 2.00 k | Blue | J2 pin 1 |

| Gun node | To ground | Cable |
| --- | --- | --- |
| Red | R9, 75 ohm | J2 pin 7 |
| Green | R10, 75 ohm | J2 pin 3 |
| Blue | R11, 75 ohm | J2 pin 1 |

**J2** is the Kycon KMDGX-9S-AS pinout: pin 1 blue, pin 3 green, pin 5 CSYNC, pin 7 red, shell ground. Pin 2 (+5 V) and the audio pins stay open. A Mega Drive 2 cable uses those same pins. The shell, or pin S, is the only ground back to the bench ground.

A 75 ohm TV input sits in parallel with R9, R10, and R11, so the bars come out about half as tall as the 0.7 Vpp the motherboard note quotes. That is still this bench.

## Place

Power is off while a chip goes in. The notch is pin 1 on every DIP.

1. The CH340 TXD wire is off Beam X pin 1.
2. The PROM address switches are off. Pins 8, 9, and 10 are at ground.
3. The PROM data LEDs are off. Pin 22 is not tied to ground.
4. U04, U74, and the 74HC02 each have +5 V on pin 14, ground on pin 7, and a 100 nF from pin 14 to pin 7.
5. The crystal, R24, C21, and C22 follow the dot-clock drawing. U04 pins 5, 9, 11, and 13 are at ground.
6. The 74HC74 follows that same drawing. R13 runs from pin 9 to Beam X pin 1. The 10 kohm from that pin to +5 V stays.
7. Beam X pin 21 meets PROM pin 5. Pin 20 meets pin 6. Pin 19 meets pin 7. Each wire lands on the PLD pin, the same node as that LED's resistor.
8. The 74HC02 follows the sync drawing. Rs runs from pin 1 to the cable sync wire.
9. R1 through R8 run from the PROM data pins to the three gun nodes. R9 through R11 run from those nodes to ground.
10. The cable is pin 7 red, pin 3 green, pin 1 blue, pin 5 sync, shell ground.

## Check

Power is on. RES, pin 2 of both PLDs, is high.

| Check | Passes when |
| --- | --- |
| U74 pin 9 | About 5.37 MHz. A scope shows a period near 186 ns. |
| Beam X pin 23 | High for about 4.7 us, once per 63.5 us. |
| Beam Y pin 23 | High for three of those lines, once per frame. |
| CRT | Eight vertical bars, stable, near 60 Hz. Left to right: black, dark, gray, blue, gray, cyan, white, light. |
| Sync interval | Guns dark while pin 23 of either PLD is high. |

The white bar is the seventh, kit row 48. The black bar is the first, kit row 0.

A locked picture has Rs at 470 ohm and shell ground on bench ground. The CH340 wire is off pin 1 for that check.

## Scope

- The three address jumpers are this bench. They are not the motherboard color-index wiring.
- CSYNC on this bench is the 74HC02 NOR.
- Rs is this bench. It has no motherboard refdes.
