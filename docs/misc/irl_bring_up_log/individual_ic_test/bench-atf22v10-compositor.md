# ATF22V10 Compositor

Used in Tiers B and C.

The socket holds the Tier B Compositor image. Beam X and Beam Y have already passed their own pages. The color PROM has already passed [`bench-at27c256r.md`](bench-at27c256r.md).

Pin 24 is VCC. Pin 12 is GND. Pin 1 is the same dot clock as Beam X pin 1. CPU-side inputs stay at the safe ties in [`docs/bringup/tier-b-video-lab.md`](../../../bringup/tier-b-video-lab.md). Unused outputs stay open.

The index bus is the only path into the PROM. Beam X no longer drives PROM A5 through A0. On the Tier A burn the horizontal count bits that feed this chip are Q pins: bit 5 is Beam X pin 20, bit 6 is pin 19, and bit 7 is pin 18.

## Pass

Watch PROM A5 through A0. The jumper block from the PROM page comes off those six pins before this check.

| Stimulus | PROM A5 through A0 |
| --- | --- |
| Sprite test off, backdrop at index 0, bars taken from Beam X bits 7 to 5 | The address steps through the bar colors across the line, and it is 0 while the line is in horizontal blank |
| Sprite test on, with a fixed sprite color | The address holds that sprite color and ignores the bar |
| Sprite test off and bar source off | The address matches the backdrop DIP |

A dark picture with a good Beam X HSYNC means the index is not reaching the PROM, or CE# or OE# is high. Sync that falls apart when this chip is added is a clock or power problem. The priority checks match the Tier B lab.
