# Hardware reference (Retr01 motherboard)

Parts list and vendor datasheet notes for the shared Retr01 motherboard (arcade + console). This folder may hold local datasheet copies on disk. They are gitignored and are **not** Retr01 schematics.

**Markdown IC notes (sim / bring-up):** [`md/`](md/) - start with [`md/README.md`](md/README.md). Current BOM: [`../docs/hardware.md`](../docs/hardware.md). Simulator: [`../app/sim/README.md`](../app/sim/README.md).

Swap / alternate parts notes live under [`candidates/`](candidates/).

## Core BOM parts

| Part | Role |
|------|------|
| W65C02S | Game CPU |
| ATmega1284P | Sprites / OAM / pads / machine EEPROM |
| ATmega328P | APU |
| AS6C62256 | System / VRAM / line-buffer SRAM |
| SST39SF040 | Cart flash 512 KB |
| AT27C256R | Color PROM (packed R3G3B2, OTP) |
| ATF22V10 | Decode / beam / compositor PLDs |
| 74HC157 | Addr mux (3 VRAM + 3 linebuf) |
| 74HC245 | Bus isolation (x3) |
| 74HC14 | Reset / clocks (support. Not always in 32 count) |
| 74HC00 | Glue (often absorbed into PLD) |
| 74HC04 | Glue |
| 74HC08 | Glue |
| 74HC32 | Glue |
| 74HC86 | Glue |

Cart I2C save EEPROM (24C64-class): see [`md/24C64.md`](md/24C64.md) - pick a vendor when the part is frozen.

## candidates/

| Part | Replaces | Notes |
|------|----------|-------|
| AT27C256R | AT28C16 Color PROM | Faster (~70 ns), in production. OTP, DIP-28 |
| AVR32DA28 | ATmega328P (APU) | Modern AVR DA, 5 V OK, SPDIP-28. **32 KB Flash / 4 KB SRAM** - fine for APU, **not** enough for 1284 sprite MCU |
| AVR128DA28 / 64 | ATmega1284P class | Same DA family: **128 KB Flash / 16 KB SRAM**. Closer peer to 1284P. Check pin count / package (28-64 pin variants) |
