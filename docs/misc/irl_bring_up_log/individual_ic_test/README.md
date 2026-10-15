# Individual IC tests

One page per chip in the Tier A, B, and C labs. Each page lists the parts, where each pin goes, and the check. The burn is already done. Programming steps stay in [`../a/flashing.md`](../a/flashing.md).

Each socket gets 5 V, ground, and a 100 nF capacitor from its VCC pin to ground. The dot clock is the 21.47727 MHz Pierce stage on the Beam X page. The 8-channel analyzer is the capture tool.

| Page | Chip | Tiers |
| --- | --- | --- |
| [`bench-atf22v10-beam-x.md`](bench-atf22v10-beam-x.md) | ATF22V10, Beam X | A, B, C |
| [`bench-atf22v10-beam-y.md`](bench-atf22v10-beam-y.md) | ATF22V10, Beam Y | A, B, C |
| [`bench-at27c256r.md`](bench-at27c256r.md) | AT27C256R color PROM | A, B, C |
| [`bench-atf22v10-compositor.md`](bench-atf22v10-compositor.md) | ATF22V10, Compositor | B, C |
| [`bench-avr128db28.md`](bench-avr128db28.md) | AVR128DB28, MCU-S1 | C |
| [`bench-74hc573.md`](bench-74hc573.md) | 74HC573 field address latch | C |
| [`bench-as6c62256.md`](bench-as6c62256.md) | AS6C62256 field SRAM | C |

The optional 74HC574 scroll latch stays off this list. Scroll stays at 0.
