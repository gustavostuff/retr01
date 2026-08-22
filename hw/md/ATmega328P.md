# ATmega328P -- APU MCU (v0)

**PDF:** [`../ATmega328P_mcu.pdf`](../ATmega328P_mcu.pdf) (family sheet includes 48/88/168/328).  
**Package (Retr01-A):** 28-pin PDIP.  
**Qty:** 1 (v0). **Removed in proposed v1** (audio merges into ATmega1284P).  
**Clock (Retr01):** **16 MHz** (5 V).

## What it is

AVR 8-bit MCU. For **ATmega328P** specifically: **32 KB Flash**, **2 KB SRAM**, **1 KB EEPROM**, 23 I/O lines, timers/PWM, USART, SPI, TWI, ADC. Retr01 v0 dedicates it to a **NES-style APU**: the 6502 writes register-like bytes in `$FE40-$FE5F`; this chip synthesizes audio (PWM or similar analog-friendly output).

## Retr01 role (v0)

| Duty | Detail |
|------|--------|
| APU registers | Appear at `$FE40-$FE5F` on the 6502 bus (decode + latch or MCU port) |
| Synthesis | Square / noise / etc. modeled after NES APU; exact channel map TBD in firmware |
| Output | Timer PWM (or dual PWM) to RC / amp -- board analog TBD |
| Isolation | Own time domain; may be developed in **sim first** (island **K**) while CPU video islands proceed |

v1: same **CPU address band** may remain, but **1284 firmware** services it during VBlank ([`06`](../../docs/06_hardware_v1_29ic.md)).

## On-chip memory (328P)

| Space | Size |
|-------|------|
| Flash | 32 KB |
| SRAM | 2 KB |
| EEPROM | 1 KB |

Speed grade at 5 V: up to **20 MHz**; Retr01 plans **16 MHz**.

## 28-pin PDIP (standard 328P)

```text
         +-----\/-----+
  RESET  | 1       28 | PC5 (ADC5/SCL)
    RXD  | 2       27 | PC4 (ADC4/SDA)
    TXD  | 3       26 | PC3 (ADC3)
   INT0  | 4       25 | PC2 (ADC2)
   INT1  | 5       24 | PC1 (ADC1)
     T0  | 6       23 | PC0 (ADC0)
    VCC  | 7       22 | GND
    GND  | 8       21 | AREF
  XTAL1  | 9       20 | AVCC
  XTAL2  |10       19 | PB5 (SCK)
    T1   |11       18 | PB4 (MISO)
     B0  |12       17 | PB3 (MOSI/OC2A)
     B1  |13       16 | PB2 (SS/OC1B)
     B2  |14       15 | PB1 (OC1A)
         +------------+
```

(Pin 1 = PC6/RESET, 2 = PD0/RXD, ... 13 = PD7, 14 = PB0, ... classic Arduino-compatible map.)

GPIO-to-`$FE4x` wiring is **schematic TBD**. Sim contract: **32-byte APU window** + PWM sample stream.

## How it works (behavioral)

1. 6502 `STA $FE4x` (or block) -> decode asserts chip select / strobes data into 328P (parallel port or latched bus).
2. Firmware updates channel state (period, volume, enable, length, ...).
3. ISR or main loop mixes channels; **hardware timer** outputs PWM continuously.
4. CPU is not blocked on audio sample rate beyond the register write.

### Expected CPU-visible behavior

| CPU action | Expect |
|------------|--------|
| Write APU enable / period regs | Audible (or digital) tone changes within firmware latency |
| Silence / disable | PWM idle or DC mid-level per design |
| Rapid register spam | Last write wins; no bus timeout on 6502 side |

## Communication on Retr01

```text
W65C02S --$FE40-$FE5F--> decode --> ATmega328P --> PWM --> analog out
```

No connection to VRAM or sprite line buffer. Optional shared reset with system RESB.

## Unit-test focus (sim)

1. Register file + NES-like channel math (deterministic samples)
2. PWM duty from mixer output
3. Optional: full AVR core later

Island **K** pass: independent tone without the rest of the video board.

## v0 vs v1

| | v0 | v1 |
|--|----|----|
| Chip present | Yes | No (logic on 1284) |
| `$FE40-$FE5F` | 328P | 1284 firmware |
| Bring-up | Island K | Merged with sprite MCU timing budget |
