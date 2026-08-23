# Chip models

Each IC is a struct that **starts with** an `R01sEntity`, plus an `R01sEntityVTable`.

| Island / role | File | Part | Notes |
|---------------|------|------|--------|
| A | `pwr5v.c` | PWR5V | 5 V rail model (VIN/EN → VDD) |
| B | `osc8m.c` | OSC8M | 8 MHz PHI2 can oscillator |
| B | `sn74hc14.c` | SN74HC14 | Hex Schmitt (reset/clock cleanup) |
| C | `w65c02s.c` | W65C02S | Reset + vector fetch + fetch cycles (ISA later) |
| C | `as6c62256.c` | AS6C62256 | 32 KB SRAM truth table |
| C | `prg_rom.c` | PRG_ROM | Tiny 32 KB read-only PRG stub |
| D | `sn74hc573.c` | SN74HC573 | Octal latch (`$FE02` on board) |
| E | `pads.c` | PADS | `$FE60`/`$FE61` stub (1=pressed) |
| G | `as6c62256.c` | AS6C62256 | 2nd instance — interleaved VRAM |
| G | `sn74hc157.c` | SN74HC157 | PHI2 CPU/PPU addr mux (low nybble) |
| H | `osc_dot.c` | OSC_DOT | ~5.37 MHz dot can (independent of PHI2) |
| H | `beam_xy.c` | BEAM_XY | ATF22V10 X/Y beam stub (341×262, H/VBlank, NMI#) |
| H | `sn74hc161.c` | SN74HC161 | Discrete counter (layer-1; optional bench path) |
| H | `sn74hc688.c` | SN74HC688 | Raster Y vs `$FE04` |
| I | `bg_fetch.c` | BG_FETCH | Nametable VA from beam+scroll; tile/attr latch |
| O | `compositor.c` | COMPOSITOR | BG/sprite priority mux PLD stub |
| O | `at28c16.c` | AT28C16 | Color PROM (kit R3G3B2, 64 entries) |
| O | `video_sink.c` | LCD_SINK | 128×120 RGBS playfield preview |
| J | `sst39sf040.c` | SST39SF040 | 512 KB flash, **read-only stub** |
| K | `atmega328p.c` | ATmega328P | APU regs `$FE40–$FE5F` + PWM square stub |
| L | `atmega1284p.c` | ATmega1284P | OAM `$FE20/$FE21` + 20 MHz tick stub |
| M | `as6c62256.c` + `sn74hc157.c` | AS6C62256 + HC157 | Line-buffer ping-pong + addr mux |
| bus | `sn74hc245.c` | SN74HC245 | Octal transceiver (DIR + OE) |
| glue | `sn74hc00.c` | SN74HC00 | Quad NAND |
| glue | `sn74hc08.c` | SN74HC08 | Quad AND |
| glue | `sn74hc32.c` | SN74HC32 | Quad OR |
| glue | `sn74hc04.c` | SN74HC04 | Hex inverter |
| — | `stub14.c` | STUB14 | UI scaffold demo only |

References: [`hw/md/`](../../hw/md/), islands in [`docs/03`](../../docs/03_hardware_implementation.md).
