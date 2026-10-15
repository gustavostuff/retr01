# AS6C62256 field SRAM

The package is 600 mil, so it sits in a 28-pin socket of that width, one jumper per pin. The notch is pin 1. Power is off while the chip goes in or comes out. No image to burn. This socket is the field chip. System RAM and VRAM are later sockets of the same part.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| AS6C62256, PDIP-28 | 1 | Field SRAM |
| 28-pin socket, 600 mil | 1 | Wide socket for the SRAM |
| 100 nF | 1 | Bypass from VCC to ground |

## Place

| What | Where |
| --- | --- |
| Pin 28 (VCC) | +5 V |
| Pin 14 (VSS) | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 28 |
| Pin 20 (CE#) | Ground for this check |
| Address pins other than the one under test | Ground |
| Pin 22 (OE#) and pin 27 (WE#) | Jumpers. Both sit high between steps |
| DQ pins | Driven only during a write. Jumpers off for a read |

| Pin | Name | Pin | Name |
| --- | --- | --- | --- |
| 10 | A0 | 11 | DQ0 |
| 9 | A1 | 12 | DQ1 |
| 8 | A2 | 13 | DQ2 |
| 7 | A3 | 15 | DQ3 |
| 6 | A4 | 16 | DQ4 |
| 5 | A5 | 17 | DQ5 |
| 4 | A6 | 18 | DQ6 |
| 3 | A7 | 19 | DQ7 |
| 25 | A8 | 22 | OE# |
| 24 | A9 | 27 | WE# |
| 21 | A10 | 20 | CE# |
| 23 | A11 | 2 | A12 |
| 26 | A13 | 1 | A14 |

OE# and WE# are never low at the same time while the bench is driving the data pins. The latch in [`bench-74hc573.md`](bench-74hc573.md) drives A7 to A0 once MCU-S1 is on the board. This page ties those address pins directly.

## Check

| Step | Address | Control | Data pins |
| --- | --- | --- | --- |
| Write | A0 low (address 0) | OE# high, pulse WE# low, then high | Driven to `0xA5`. DQ7, DQ5, DQ2, and DQ0 high |
| Read | address 0 | WE# high, OE# low, data jumpers off | Same four pins high, the others low |
| Write | A0 high (address 1) | OE# high, pulse WE# | Driven to `0x5A`. DQ6, DQ4, DQ3, and DQ1 high |
| Read | address 1 | WE# high, OE# low | That pattern |
| Read | address 0 again | WE# high, OE# low | `0xA5` still |

CE# high releases the data pins on the next read. The byte at address 0 is still there when CE# returns low.
