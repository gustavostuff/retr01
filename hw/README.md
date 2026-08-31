# Hardware datasheets (Retr01 motherboard)

Official / vendor datasheets for the shared Retr01 motherboard (arcade + console). These are reference PDFs (pinouts, AC timing, package drawings). They are **not** Retr01 schematics.

**Markdown IC notes (sim / bring-up):** [`md/`](md/) - start with [`md/README.md`](md/README.md). Current BOM: [`../docs/hardware.md`](../docs/hardware.md). Simulator: [`../retr01_sim/README.md`](../retr01_sim/README.md).

Swap / alternate parts live in [`candidates/`](candidates/).

| File | Part | Role |
|------|------|------|
| `W65C02S_cpu.pdf` | W65C02S | Game CPU |
| `ATmega1284P_mcu.pdf` | ATmega1284P | Sprites / OAM / pads / machine EEPROM |
| `ATmega328P_mcu.pdf` | ATmega328P | APU |
| `AS6C62256_sram_32kx8.pdf` | AS6C62256 | System / VRAM / line-buffer SRAM |
| `SST39SF040_flash_512KB.pdf` | SST39SF040 | Cart flash 512 KB |
| `AT28C16_color_prom.pdf` | AT28C16 | Color PROM (packed R3G3B2) |
| `ATF22V10_pld.pdf` | ATF22V10 | Decode / beam / compositor PLDs |
| `SN74HC157_mux.pdf` | 74HC157 | Addr mux (3 VRAM + 3 linebuf) |
| `SN74HC245_bus_transceiver.pdf` | 74HC245 | Bus isolation (x3) |
| `SN74HC573_latch.pdf` | 74HC573 | `$FExx` latches (x9) |
| `SN74HC14_schmitt.pdf` | 74HC14 | Reset / clocks (support. Not always in 32 count) |
| `SN74HC00_nand.pdf` | 74HC00 | Glue (often absorbed into PLD) |
| `SN74HC04_inverter.pdf` | 74HC04 | Glue |
| `SN74HC08_and.pdf` | 74HC08 | Glue |
| `SN74HC32_or.pdf` | 74HC32 | Glue |
| `SN74HC86_xor.pdf` | 74HC86 | Glue |

Cart I2C save EEPROM (24C64-class): see [`md/24C64.md`](md/24C64.md) - pick a vendor PDF when the part is frozen.

## candidates/

| File | Replaces | Notes |
|------|----------|-------|
| `AT27C256R_otp_eprom_candidate.pdf` | AT28C16 Color PROM | Faster (~70 ns), in production. OTP, DIP-28 |
| `AVR32DA28_mcu_candidate.pdf` | ATmega328P (APU) | Modern AVR DA, 5 V OK, SPDIP-28. **32 KB Flash / 4 KB SRAM** - fine for APU, **not** enough for 1284 sprite MCU |
| `AVR128DA28_64_mcu_candidate.pdf` | ATmega1284P class | Same DA family: **128 KB Flash / 16 KB SRAM**. Closer peer to 1284P. Check pin count / package (28-64 pin variants) |
