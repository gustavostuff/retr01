# Hardware datasheets (Retr01-A)

Official / vendor datasheets for the v0 IC plan in `docs/03_hardware_implementation.md`.
These are reference PDFs (pinouts, AC timing, package drawings). They are **not** Retr01 schematics.

**Markdown IC notes (sim / bring-up):** [`md/`](md/) -- start with CPU/MCU. Simulator overview: [`../docs/08_simulator.md`](../docs/08_simulator.md).

Swap / alternate parts live in [`candidates/`](candidates/).

| File | Part | Role |
|------|------|------|
| `W65C02S_cpu.pdf` | W65C02S | Game CPU |
| `ATmega1284P_mcu.pdf` | ATmega1284P | Sprites / OAM / pads |
| `ATmega328P_mcu.pdf` | ATmega328P | APU |
| `AS6C62256_sram_32kx8.pdf` | AS6C62256 | System / VRAM / line-buffer SRAM |
| `SST39SF040_flash_512KB.pdf` | SST39SF040 | Cart flash 512 KB |
| `AT28C64B_eeprom.pdf` | AT28C64B | Board EEPROM |
| `AT28C16_color_prom.pdf` | AT28C16 | Color PROM R/G/B |
| `ATF22V10_pld.pdf` | ATF22V10 | Decode / timing PLD |
| `SN74HC157_mux.pdf` | 74HC157 | Addr mux |
| `SN74HC245_bus_transceiver.pdf` | 74HC245 | Bus isolation |
| `SN74HC573_latch.pdf` | 74HC573 | `$FExx` latches |
| `SN74HC688_comparator.pdf` | 74HC688 | Raster compare |
| `SN74HC161_counter.pdf` | 74HC161 | Beam counters |
| `SN74HC14_schmitt.pdf` | 74HC14 | Reset / clocks |
| `SN74HC00_nand.pdf` | 74HC00 | Glue |
| `SN74HC04_inverter.pdf` | 74HC04 | Glue |
| `SN74HC08_and.pdf` | 74HC08 | Glue |
| `SN74HC32_or.pdf` | 74HC32 | Glue |
| `SN74HC86_xor.pdf` | 74HC86 | Glue |

## candidates/

| File | Replaces | Notes |
|------|----------|-------|
| `AT27C256R_otp_eprom_candidate.pdf` | AT28C16 Color PROM | Faster (~70 ns), in production. OTP, DIP-28 |
| `AVR32DA28_mcu_candidate.pdf` | ATmega328P (APU) | Modern AVR DA, 5 V OK, SPDIP-28. **32 KB Flash / 4 KB SRAM** - fine for APU, **not** enough for 1284 sprite MCU |
| `AVR128DA28_64_mcu_candidate.pdf` | ATmega1284P class | Same DA family: **128 KB Flash / 16 KB SRAM**. Closer peer to 1284P. Check pin count / package (28-64 pin variants) |
