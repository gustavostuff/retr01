# Beam X and Beam Y

Two ATF22V10C parts, already burned, on a breadboard. Beam X counts dots 0 through 340. Beam Y counts lines 0 through 261. The images are [`beam_x.jed`](../../../apps/flasher_tool/firmware/a/beam_x.jed) and [`beam_y.jed`](../../../apps/flasher_tool/firmware/a/beam_y.jed).

The package is 300 mil. Each chip sits in a 24-pin socket of that width. The notch is pin 1. Power is off while a chip goes in or comes out.

A HiLetgo CH340 module is a 6-pin USB serial header: 5V, VCC, 3V3, TXD, RXD, GND. The yellow jumper stays on 5V, so TXD and RXD swing at 5V. One module is the clock. Two or three more modules watch one counter bit each. RXD is the only input on the header, and a serial port does not print a steady level from it. A high pin leaves the line idle, so a short read returns nothing. A low pin is a break. The clock has to be slow for that to mean anything. These modules do not sample a 5.369318 MHz dot.

The LED on the same pin is the reading. Lit is 0. Dark is 1.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, Beam X image | 1 | Dot counter. HSYNC high on counts 264 through 288 |
| ATF22V10C, PDIP-24, Beam Y image | 1 | Line counter. VSYNC high on counts 243 through 245 |
| 24-pin socket, 300 mil | 2 | One per PLD |
| 100 nF | 2 | Bypass from VCC to ground, one beside each pin 24 |
| 10 kohm | 2 | Pull-up on RES, pull-up on Beam X CLK |
| Switch | 1 | RES to ground. Closed holds both counters at 0 |
| LED, red | 3 | The two or three bits under test |
| 2.2 kohm | 3 | LED series resistor |
| HiLetgo CH340 module | 1 | Clock and 5 V. TXD drives Beam X pin 1 |
| HiLetgo CH340 module | 2 or 3 | One watched bit each. RXD only |

## Place

Beam X can be checked with Beam Y out of the socket. Beam Y pin 1 connects only after HSYNC is already moving.

| What | Where |
| --- | --- |
| Both pin 24 (VCC) | +5 V from the clock module's 5V pin |
| Both pin 12 (GND) | Ground, common with every module GND |
| 100 nF | Pin 24 to pin 12 on that same chip |
| Both pin 2 (RES) | One net. 10 kohm to +5 V, and the switch to ground |
| Beam X pins 3 through 11, and pin 13 | Ground |
| Beam Y pins 3 through 11, and pin 13 | Ground |
| Beam X pin 1 (CLK) | 10 kohm to +5 V, and the clock module TXD |
| Beam Y pin 1 (CLK) | Beam X pin 14 (HSYNC) |
| Clock module | Jumper on 5V. 5V and GND feed the rail. TXD to Beam X pin 1. RXD open |
| Watcher module | Jumper on 5V. GND to the rail. RXD to the watched pin. 5V, VCC, 3V3, and TXD open |
| Watched pin | +5 V, then 2.2 kohm, then the LED anode. Cathode to the pin. RXD on that same pin |

Only the clock module feeds 5 V. A watcher 5V pin stays open so two USB supplies do not meet. 3V3 stays open on every module. VCC on the header stays open. TXD on a watcher stays open. It is an output.

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

RES closed (low) clears both counters, whatever CLK is doing. The clock module holds TXD low while the switch is closed, then the switch opens. Each later rising edge on TXD adds one Beam X count. `break_condition` true holds TXD low. Setting it false is the rising edge. Setting it true again parks the pin low for the next edge.

Three Beam X bits are enough. Pin 21 is Q0, pin 23 is Q1, pin 14 is HSYNC.

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

The same three LEDs and RXD wires land on Beam Y pins 21, 23, and 14 for the line counter. Q0 and Q1 swap pins between the two chips.
