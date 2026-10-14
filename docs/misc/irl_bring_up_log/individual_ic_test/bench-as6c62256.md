# AS6C62256 field SRAM

Used in Tier C.

No image to burn. This socket is the field chip. System RAM and VRAM are later sockets of the same part.

Pin 28 is VCC. Pin 14 is VSS. Pin 20 (CE#) ties to GND for this check. Address pins other than the one under test tie to GND.

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

## Pass

OE# and WE# are never low at the same time while the bench is driving the data pins. Between steps, both enables sit high.

| Step | Address | Control | Data pins |
| --- | --- | --- | --- |
| Write | A0 low (address 0) | OE# high, pulse WE# low, then high | Driven to 0xA5. DQ7, DQ5, DQ2, and DQ0 high |
| Read | address 0 | WE# high, OE# low, data jumpers off | Same four pins high, the others low |
| Write | A0 high (address 1) | OE# high, pulse WE# | Driven to 0x5A. DQ6, DQ4, DQ3, and DQ1 high |
| Read | address 1 | WE# high, OE# low | That pattern |
| Read | address 0 again | WE# high, OE# low | 0xA5 still |

CE# high releases the data pins on the next read. The byte at address 0 is still there when CE# returns low.

The latch in [`bench-74hc573.md`](bench-74hc573.md) is what drives A7 to A0 once MCU-S1 is on the board. This page ties those address pins directly.
