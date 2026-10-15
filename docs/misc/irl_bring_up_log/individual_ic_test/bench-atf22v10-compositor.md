# ATF22V10 Compositor

The socket holds the Tier B Compositor image. The package is 300 mil, so it sits across the breadboard center. The notch is pin 1. Power is off while the chip goes in or comes out. Beam X, Beam Y, and the color PROM have already passed their own pages.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, Tier B Compositor image | 1 | Color index for the PROM |
| 100 nF | 1 | Bypass from VCC to ground |

## Place

| What | Where |
| --- | --- |
| Pin 24 (VCC) | +5 V |
| Pin 12 (GND) | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 24 |
| Pin 1 (CLK) | The same dot clock as Beam X pin 1 |
| CPU-side inputs | The safe ties in [`docs/bringup/tier-b-video-lab.md`](../../../bringup/tier-b-video-lab.md) |
| Unused outputs | Open |
| Index outputs | PROM A5 through A0 |

PROM A5 through A0 come from this chip. The jumper block on the PROM page is off those six pins for this check. Beam X pin 20 is count bit 5, pin 19 is bit 6, and pin 18 is bit 7.

## Check

Watch PROM A5 through A0.

| Stimulus | PROM A5 through A0 |
| --- | --- |
| Sprite test off, backdrop at index 0, bars taken from Beam X bits 7 to 5 | The address steps through the bar colors across the line, and it is 0 while the line is in horizontal blank |
| Sprite test on, with a fixed sprite color | The address holds that sprite color and ignores the bar |
| Sprite test off and bar source off | The address matches the backdrop DIP |

A dark picture with a good Beam X HSYNC means the index is not reaching the PROM, or CE# or OE# is high. Sync that falls apart when this chip is added is a clock or power problem. The priority checks match the Tier B lab.
