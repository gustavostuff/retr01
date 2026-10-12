# Tier A burn files

Images for two ATF22V10 PLDs and one AT27C256R color PROM.

| File | Chip | Load |
| --- | --- | --- |
| `beam_x.jed` | ATF22V10, Beam X | Device ATF22V10C. Dot counter and HSYNC. |
| `beam_y.jed` | ATF22V10, Beam Y | Device ATF22V10C. Line counter and VSYNC. |
| `at27c256r_kit.bin` | AT27C256R | 64 bytes at addresses 0 through 63. |

The `.pld` files are the PLD source. `gen_tier_a.py` rebuilds them, and the PROM image, from `apps/common/r01_kit_palette.c` and the Tier A raster (341 dots, 262 lines). GALasm 2.1 assembles each `.pld` into a `.jed`. The security fuse stays off.

## Beam X

CLK is the 5.369318 MHz dot clock, rising edge. Count runs 0 through 340, then returns to 0. HSYNC is high on counts 264 through 288 (25 dots, about 4.7 us). RES low clears the counter. Leave RES high while the clock runs.

| Pin | Name |
| --- | --- |
| 1 | CLK |
| 2 | RES |
| 3 to 11, 13 | NC. Tie each to GND. |
| 12 | GND |
| 14 | HSYNC |
| 15 | Q3 |
| 16 | Q2 |
| 17 | Q4 |
| 18 | Q7 |
| 19 | Q6 |
| 20 | Q5 |
| 21 | Q0 (least significant bit) |
| 22 | Q8 (most significant bit) |
| 23 | Q1 |
| 24 | VCC |

Beam Y pin 1 connects to this HSYNC pin. HSYNC rises once per line.

## Beam Y

Same RES behavior. Count runs 0 through 261, then returns to 0. VSYNC is high on counts 243, 244, and 245.

| Pin | Name |
| --- | --- |
| 1 | CLK (Beam X HSYNC) |
| 2 | RES |
| 3 to 11, 13 | NC. Tie each to GND. |
| 12 | GND |
| 14 | VSYNC |
| 15 | Q2 |
| 16 | Q3 |
| 17 | Q5 |
| 18 | Q7 |
| 19 | Q6 |
| 20 | Q4 |
| 21 | Q1 |
| 22 | Q8 (most significant bit) |
| 23 | Q0 (least significant bit) |
| 24 | VCC |

HSYNC and VSYNC are active high. Q pins are the count, for a probe. A picture does not need them wired onward.

## Color PROM

`at27c256r_kit.bin` is the locked 64-color kit, one packed R3G3B2 byte per index. Address pins A14 through A6 tie to GND. CE# and OE# stay low.

The Tier A sim also drives six color-index pins from Beam X. A 22V10 has ten macrocells. Nine hold this counter and the tenth is the sync pulse, so those index pins are not part of this burn. Jumpers on PROM A5 through A0 pick a solid kit color. That is the first picture.
