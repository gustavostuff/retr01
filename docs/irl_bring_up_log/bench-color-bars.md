# Color bars on a CRT

This bench is not passed yet.

It puts eight vertical bars on an RGBS CRT. The [Beam X / Beam Y](bench-beam-xy.md) note and the [color PROM](bench-at27c256r.md) note are the pin list for those chips. The sim pin netlist is the pin list for the dot clock, the DAC resistors, and **J2**. A pass updates the sim, the SKiDL netlist, and `v_07`. See [After a bench passes](README.md#after-a-bench-passes).

The compositor, the VRAM, the CPU, and the CXA2075M stay off this board.

## Already on the board

This bench has no LEDs. The chips and the rails come from the two passing benches. A wire in those notes that is not in a table below is absent here.

### Beam X and Beam Y

Both burned ATF22V10C chips stay in their sockets. The +5 V rail is the CH340 5V pin and the CH340 VCC pin. CH340 GND is the ground rail. RXD, 3V3, and TXD stay open. The yellow jumper stays off.

| Net | Pins | Where |
| --- | --- | --- |
| V+ | pin 24, both chips | +5 V rail. One 100 nF from each pin 24 to ground |
| GND | pin 12, both chips | Ground rail |
| Tied low | pins 3 through 11, and pin 13, both chips | Ground rail |
| RES | pin 2, both chips | One wire. 10 kohm to +5 V. Switch to ground |
| CLK pull-up | Beam X pin 1 | 10 kohm to +5 V. DOT arrives on this same pin through R13 |
| HSYNC | Beam X pin 23 and Beam Y pin 1 | One wire. That wire also meets 74HC02 pin 2 |

These Beam pins are open:

| Chip | Open pins | Bits |
| --- | --- | --- |
| Beam X | 14, 15, 16, 17, 18, 22 | Q0, Q1, Q2, Q3, Q4, Q8 |
| Beam Y | 14 through 22 | Q0 through Q8 |

Beam Y pin 23 is VSYNC. It meets 74HC02 pin 3 and nothing else.

### Color PROM

The burned AT27C256R stays in the 28-pin socket.

| Net | Pins | Name |
| --- | --- | --- |
| V+ | 1 and 28 | VPP and VCC. One 100 nF from V+ to ground |
| GND | 2, 3, 4, 14 | A12, A7, A6, GND |
| GND | 20, 21, 23, 24, 25, 26, 27 | CE#, A10, A11, A9, A8, A13, A14 |

Pin 22 (OE#) is not on that ground list. It meets 74HC02 pin 4.

### Wires from Beam X into the PROM

| From | To |
| --- | --- |
| Beam X pin 21 (Q7) | PROM pin 5 (A5), index weight 32 |
| Beam X pin 20 (Q6) | PROM pin 6 (A4), index weight 16 |
| Beam X pin 19 (Q5) | PROM pin 7 (A3), index weight 8 |
| Ground rail | PROM pin 8 (A2) |
| Ground rail | PROM pin 9 (A1) |
| Ground rail | PROM pin 10 (A0) |

PROM pins 19, 18, 17, 16, 15, 13, 12, and 11 are O7 through O0. Each one meets one DAC resistor in the DAC section. A5, A4, and A3 are the three Beam X wires. A2, A1, and A0 are the three ground wires.

## What the bars are

Visible dots are Beam X counts 0 through 255. Q7, Q6, and Q5 split that into eight bars of 32 dots. The open Q pins above stay open.

The address is `32*Q7 + 16*Q6 + 8*Q5`, so the eight bars are kit rows 0, 8, 16, 24, 32, 40, 48, and 56. Those rows run from black to white. Rows 0 through 7 are all dark, so Q7, Q6, and Q5 land on A5, A4, and A3 for this test.

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

Y2 and U04 pins 1 and 2 oscillate at 21.47727 MHz. U04 pin 4 is that clock buffered. U74 divides it by 4. U74 pin 9 is DOT, 5.369318 MHz. R13, 33 ohm, runs from that pin to Beam X pin 1. Beam Y clocks from Beam X pin 23.

The notch is at the top. Pin 1 is the top pin on the left. The note beside a pin is the wire on that pin. A note like "pin 6" means another pin on the same chip.

```text
74HCU04 (U04)
               notch
            +----U----+
 Y2 and R24 |  1   14 | +5 V
 Y2 and R24 |  2   13 | GND
 pin 2      |  3   12 | open
 U74 pin 3  |  4   11 | GND
 GND        |  5   10 | open
 open       |  6    9 | GND
 GND        |  7    8 | open
            +---------+

C21, 22 pF, from pin 1 to GND.
C22, 22 pF, from pin 2 to GND.
R24 is 1 M ohm, across pins 1 and 2, in parallel with Y2.
Gates 3 through 6 are unused. Inputs are pins 5, 9, 11, and 13.
```

```text
SN74HC74 (U74)
               notch
            +----U----+
 +5 V       |  1   14 | +5 V
 pin 6      |  2   13 | +5 V
 U04 pin 4  |  3   12 | pin 8
 +5 V       |  4   11 | pin 5
 pin 11     |  5   10 | +5 V
 pin 2      |  6    9 | R13, then Beam X pin 1
 GND        |  7    8 | pin 12
            +---------+
```

## Sync and blanking

Beam X pin 23 is high on counts 264 through 288. Beam Y pin 23 is high on counts 243 through 245. Both pulses are active high.

**J2** pin 5 is CSYNC, the same net as the CXA2075M SYNCIN pin. On this bench that net is Beam X pin 23 NOR Beam Y pin 23, so the pulse is low while either sync pin is high. A second NOR turns that over and drives PROM OE#. The outputs are off for the same interval. R9, R10, and R11 then hold the gun nodes at ground.

The notch is at the top. Pin 1 is the top pin on the left. Rs, 470 ohm, is in series between pin 1 and the cable sync wire.

```text
74HC02
             notch
          +----U----+
 CSYNC    |  1   14 | +5 V
 HSYNC    |  2   13 | open
 VSYNC    |  3   12 | GND
 PROM 22  |  4   11 | GND
 pin 1    |  5   10 | open
 GND      |  6    9 | GND
 GND      |  7    8 | GND
          +---------+
```

HSYNC is Beam X pin 23. VSYNC is Beam Y pin 23. Pin 5 ties to pin 1, on the chip side of Rs.

OE# is high only while HSYNC or VSYNC is high. Counts 256 through 263 still show a color, and so do Beam Y counts 240 through 242. That strip is this bench. Full-border blanking belongs to the compositor, which is not on this board.

Rs is breadboard only. `v_07` has no series resistor on CSYNC. The NOR pin is not a 75 ohm driver, and the TV sync input is about 75 ohm.

## DAC

Three nodes: Red, Green, and Blue. R1 through R8 each have one leg on a PROM pin and the other leg on a node. The three legs on a node are twisted together. R9, R10, and R11 each have one leg on that same twist and the other leg on the ground rail. The CRT wire is a third wire on that same twist. R1 through R8 do not reach the cable.

| One leg | Resistor | Other leg |
| --- | --- | --- |
| PROM pin 19 (O7) | R1, 1.00 kohm | Red node |
| PROM pin 18 (O6) | R2, 2.00 kohm | Red node |
| PROM pin 17 (O5) | R3, 4.00 kohm | Red node |
| Red node | R9, 75 ohm | Ground rail |
| Red node | CRT red | J2 pin 7 |
| PROM pin 16 (O4) | R4, 1.00 kohm | Green node |
| PROM pin 15 (O3) | R5, 2.00 kohm | Green node |
| PROM pin 13 (O2) | R6, 4.00 kohm | Green node |
| Green node | R10, 75 ohm | Ground rail |
| Green node | CRT green | J2 pin 3 |
| PROM pin 12 (O1) | R7, 1.00 kohm | Blue node |
| PROM pin 11 (O0) | R8, 2.00 kohm | Blue node |
| Blue node | R11, 75 ohm | Ground rail |
| Blue node | CRT blue | J2 pin 1 |

**J2** is the Kycon KMDGX-9S-AS pinout. A Mega Drive 2 cable uses the same numbers. The CRT sees four wires plus the shell:

| J2 pin | Cable | Bench |
| --- | --- | --- |
| 1 | Blue | Blue node |
| 3 | Green | Green node |
| 5 | CSYNC | Rs, from 74HC02 pin 1 |
| 7 | Red | Red node |
| S | Shell | Ground rail |

Pin 2 (+5 V), pin 4 (composite), and pins 6, 8, and 9 (audio) stay open.

A 75 ohm TV input sits in parallel with R9, R10, and R11, so the bars come out about half as tall as the 0.7 Vpp the motherboard note quotes. That is still this bench.

## Place

Power is off while a chip goes in. The notch is pin 1 on every DIP.

1. CH340 TXD is off Beam X pin 1. The 10 kohm from that pin to +5 V stays.
2. Beam X pins 14, 15, 16, 17, 18, and 22 are open. Beam Y pins 14 through 22 are open.
3. PROM pins 8, 9, and 10 are at ground. Pin 22 meets 74HC02 pin 4.
4. U04, U74, and the 74HC02 each have +5 V on pin 14, ground on pin 7, and a 100 nF from pin 14 to pin 7.
5. The crystal, R24, C21, and C22 follow the dot-clock drawing. U04 pins 5, 9, 11, and 13 are at ground.
6. The 74HC74 follows that same drawing. R13 runs from pin 9 to Beam X pin 1.
7. Beam X pin 21 meets PROM pin 5. Pin 20 meets pin 6. Pin 19 meets pin 7.
8. The 74HC02 follows the sync drawing. Rs runs from pin 1 to the cable sync wire.
9. R1, R2, and R3 meet at the Red node. R4, R5, and R6 meet at the Green node. R7 and R8 meet at the Blue node. R9, R10, and R11 run from those nodes to ground.
10. The CRT cable lands on those same nodes: J2 pin 7 on Red, pin 3 on Green, pin 1 on Blue, pin 5 on Rs, shell on ground.

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
