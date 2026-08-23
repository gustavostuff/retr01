# Chip models

Each IC is a struct that **starts with** an `R01sEntity`, plus an `R01sEntityVTable`.

| Island | File | Part | Notes |
|--------|------|------|--------|
| A | `pwr5v.c` | PWR5V | 5 V rail model (VIN/EN → VDD) |
| B | `osc8m.c` | OSC8M | 8 MHz PHI2 can oscillator |
| B | `sn74hc14.c` | SN74HC14 | Hex Schmitt (reset/clock cleanup) |
| C | `w65c02s.c` | W65C02S | Reset + vector fetch + fetch cycles (ISA later) |
| C | `as6c62256.c` | AS6C62256 | 32 KB SRAM truth table |
| C | `prg_rom.c` | PRG_ROM | Tiny 32 KB read-only PRG stub |
| — | `stub14.c` | STUB14 | UI scaffold demo only |

References: [`hw/md/`](../../hw/md/), islands in [`docs/03`](../../docs/03_hardware_implementation.md).
