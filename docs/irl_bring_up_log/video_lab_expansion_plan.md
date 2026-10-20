# Video lab expansion

This plan adds a compositor board, then an MCU-S1 board, on top of the four modules in [bench-color-bars.md](bench-color-bars.md). It is a plan. It is not a passed bench. The color-bar note stays the pin netlist. The sim and `v_07` stay on the passing notes.

Priority and the two background layers are in [video-graphics.md](../video-graphics.md). Motherboard compositor jobs are in [hardware.md](../hardware.md).

## Modules that stay

The clock board stays. Y2, U04, U74, and R13 stay. DOT still enters Beam X pin 1.

The beam board stays. Beam X pin 23 still clocks Beam Y pin 1. Both RES pins stay tied. Unused inputs stay at GND. The reset switch stays. The header already carries both counters and both sync pins, so the compositor cable adds no solder there.

Three cables leave the PROM. Those are XQ7, XQ6, and XQ5, Beam X pins 21, 20, and 19. HSYNC and VSYNC stay on the RGBS NOR. The same header pins also feed the compositor. One header pin is one net, shared by Beam Y CLK, the NOR, and the new board.

The RGBS board stays. The DAC, the 75 ohm loads, Rs, the NOR, and the CRT header stay. CSYNC stays active low, NOR pin 1 through Rs. OE# stays high while HSYNC or VSYNC is high, NOR pin 4 to PROM pin 22. Later border blanking can move to the compositor. The NOR still makes CSYNC.

PROM data pins stay on the DAC. O7 through O0 are pins 19, 18, 17, 16, 15, 13, 12, and 11. Pin 1 and pin 28 stay at +5 V. Pins 2, 3, 4, 14, 20, 21, 23, 24, 25, 26, and 27 stay at GND. CE# stays low. Addresses above A5 stay at GND, because the kit is 64 colors.

Beam X, Beam Y, and the PROM keep the burned images from the passing notes.

## Address pins that change

The color-bar address is `32*Q7 + 16*Q6 + 8*Q5`. That uses A5, A4, and A3. A2, A1, and A0 sit at GND, so the bars are kit rows 0, 8, 16, 24, 32, 40, 48, and 56. A compositor index is six bits, A5 through A0, pins 5 through 10.

| Pin | Color-bar bench | Compositor bench |
| --- | --- | --- |
| 5, A5 | Beam X Q7 | Index bit 5 |
| 6, A4 | Beam X Q6 | Index bit 4 |
| 7, A3 | Beam X Q5 | Index bit 3 |
| 8, A2 | GND | Index bit 2 |
| 9, A1 | GND | Index bit 1 |
| 10, A0 | GND | Index bit 0 |
| 22, OE# | NOR pin 4 | NOR pin 4 |

The color-bar note solders pins 8, 9, and 10 to GND. Those three joints are the desolder. Header pins for A2, A1, and A0, with jumpers to GND on the color-bar bench, turn that step into a cable change. The six index wires then come from the compositor.

Putting XQ7, XQ6, and XQ5 back on A5, A4, and A3, and returning A2, A1, and A0 to GND, brings the eight bars back. OE# stays on the NOR.

## Compositor board

One new perma-proto. One ATF22V10C. Inputs are the beam header, both counters plus HSYNC and VSYNC, and the pixel inputs below. Outputs are the six index bits on PROM pins 5 through 10.

Each dot picks one kit index:

1. A sprite pixel whose index is not 0 covers both backgrounds.
2. A BG1 pixel whose index is not 0 shows next.
3. BG0 shows where the BG1 index is 0.
4. Index 0 is the fallback.

The PLD does not hold the pictures. A lab fuse map can build the two backgrounds from the beam counts, such as stripes, a checker, or a split field. That burn is a bench image.

The motherboard compositor also owns MAP A14 through A18, the soft selects, and the latch enables. That is a different burn. The lab image stays off the motherboard part.

## MCU-S1 board

One new perma-proto, after the compositor. One AVR128DB28 in the MCU-S1 role. The firmware holds the pixel art. The AVR writes buffers. The compositor keeps driving the PROM address.

DOT is about 5.37 MHz, about 186 ns per dot. The AVR runs at 24 MHz, under five cycles per dot. That is too few cycles to fetch BG1, read BG0, and test a sprite on every dot.

HBlank is Beam X counts 256 through 340. That is about 85 dots, about 16 us, about 380 cycles. The job in that window is the next BG0 line, 128 pixels. The HSYNC pulse itself stays the passing check, high for about 4.7 us on counts 264 through 288.

VBlank is Beam Y counts 240 through 261. That is about 1.4 ms, about 33000 cycles. The job in that window is a few sprites. VSYNC itself stays high for three lines, counts 243 through 245.

BG1 pixel art from the same firmware needs a buffer the compositor can read during counts 0 through 255. The HBlank window is already the BG0 line, so the BG1 buffer fills in VBlank, or a line ahead of the beam. The lab part for that buffer is a small SRAM. On the motherboard, one AS6C62256 holds the sprite field and the BG0 ping-pong. The AVR writes the lab SRAM. The compositor reads it at the dot clock.

That SRAM is new. The clock board, the beam board, the PROM socket, and the DAC stay as they are.
