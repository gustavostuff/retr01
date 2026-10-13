# Individual IC tests

One page per chip in the Tier A, B, and C labs. The burn is already done. Programming steps stay in [`../a/flashing.md`](../a/flashing.md).

Each socket gets 5 V, ground, and a 100 nF capacitor from its VCC pin to ground. The dot clock is the Si5351 on the flashing page. The 8-channel analyzer is the capture tool.

| Page | Chip | Tiers |
| --- | --- | --- |
| [`atf22v10-beam-x.md`](atf22v10-beam-x.md) | ATF22V10, Beam X | A, B, C |
| [`atf22v10-beam-y.md`](atf22v10-beam-y.md) | ATF22V10, Beam Y | A, B, C |
| [`at27c256r.md`](at27c256r.md) | AT27C256R color PROM | A, B, C |
| [`atf22v10-compositor.md`](atf22v10-compositor.md) | ATF22V10, Compositor | B, C |
| [`avr128db28.md`](avr128db28.md) | AVR128DB28, MCU-S1 | C |
| [`74hc573.md`](74hc573.md) | 74HC573 field address latch | C |
| [`as6c62256.md`](as6c62256.md) | AS6C62256 field SRAM | C |

The optional 74HC574 scroll latch stays off this list. Scroll stays at 0. The 21.47727 MHz Pierce pair is an alternate dot clock, and these pages use the Si5351 instead.
