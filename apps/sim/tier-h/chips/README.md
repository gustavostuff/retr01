# Chip models

Each IC is a struct that starts with an `R01sEntity`, plus an `R01sEntityVTable`. Parts follow `docs/general/hardware.md`. The AD724 is not modeled yet.

| Role | File | Part |
| --- | --- | --- |
| 5 V rail | `pwr5v.c` | PWR5V |
| PHI2 8 MHz | `osc8m.c` | OSC8M |
| DOT clock | `osc_dot.c` | OSC_DOT |
| Game CPU | `w65c02s.c` | W65C02S |
| No-cart boot image | `prg_rom.c` | PRG_ROM |
| SRAM (sys, VRAM, field) | `as6c62256.c` | AS6C62256 |
| Beam X / Beam Y / decode helper | `atf22v10.c`, `beam_xy.c` | ATF22V10 |
| Compositor | `compositor.c` | ATF22V10 role |
| BG fetch | `bg_fetch.c` | BG_FETCH |
| Color PROM | `at27c256r.c` | AT27C256R |
| VRAM address mux | `sn74hc157.c` | 74HC157 |
| Field address latch | `sn74hc573.c` | 74HC573 |
| BG1 scroll X | `sn74hc574.c` | 74HC574 |
| MCU-M / S1 / S2 | `avr128db28_m.c`, `avr128db28_s1.c`, `avr128db28_s2.c` | AVR128DB28 |
| S2 APU core | `atmega328p.c` | Behavioral core behind the S2 part name |
| Cart flash | `sst39sf040.c` | SST39SF040 |
| Cart save | `i2c_eeprom.c` | 24C64 |
| Pad MCU | `attiny85.c`, `pad_uart.c`, `pads.c` | ATtiny85 |
| Sprite field stats | `sprite_fetch.c` | SPRITE_FETCH |
| NMI / pad / video notes | `integration.c` | INTEGRATION |

`atmega328p.c` is the APU behavior MCU-S2 wraps. It is not a separate motherboard IC.
