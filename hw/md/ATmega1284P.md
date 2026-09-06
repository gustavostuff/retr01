# ATmega1284P: Sprite / OAM / pad MCU

**Package (Retr01 motherboard):** 40-pin PDIP.
**Qty:** 1.
**Clock (Retr01):** **20 MHz** (requires VCC 4.5-5.5 V for 20 MHz grade).

## Package dimensions

| | |
|--|--|
| Outline | PDIP-40, 600 mil |
| Body (nom.) | **53 x 14 mm** (length x width) |
| Sim @ 4 px/mm | **212 x 56 px** horizontal (default) |
| Reference | [`packages_dip.md`](packages_dip.md) |

## What it is

AVR 8-bit MCU: **128 KB Flash**, **16 KB SRAM**, **4 KB EEPROM**, 32 GPIO lines on four ports (A/B/C/D), timers with PWM, USART, SPI, TWI, ADC, JTAG. Retr01 uses it as a **dedicated helper MCU**, not the game CPU: it owns **OAM**, the **VBlank sprite field** + **HBlank BG0 line fill**, **controller bytes**, and **machine EEPROM**. APU stays on **328P** ([`hardware.md`](../../docs/hardware.md)). Program via 5 V ISP ([`programming.md`](../../docs/programming.md)).

## Retr01 role

| Port / duty | Retr01 map |
|-------------|------------|
| OAM storage + evaluate | CPU writes via `$FE20` (addr) / `$FE21` (data), auto-inc. 64 entries `Y,tile,attr,X` |
| Sprite field + BG0 | During **VBlank**, write full **120x128** sprite field. During **HBlank**, write next BG0 line (ping-pong) |
| Pads | Present `$FE60` / `$FE61`. Arcade GPIO or Retr01-C UART pads ([`controllers.md`](../../docs/controllers.md)) |
| Machine EEPROM | Internal 4 KB. CPU mailbox `$FE70`-`$FE72` + `RDY` ([`memory.md`](../../docs/memory.md)) |
| Cart save I2C | Master to cart 24C64 via `$FE22`-`$FE24` |
| CHR bus | May own cart CHR in **VBlank** (sprite field) and **HBlank** (BG0 line) while BG path is idle (do not share until island N proven) |
| Soft `$FExx` (HC573-zero) | `$FE00`, `$FE05`, `$FE06`/`$FE07`, `$FE08`, `$FE90`-`$FE92` soft registers. Decode SEL strobes + CPU D |
| MAP seek | Soft 24-bit seek. Cart `A14`-`A18` driven by UPLDV registered export (hard pin path) |

**Not** the BG beam path. **Not** the APU: that is ATmega328P.

### Soft `$FExx` (HC573-zero)

Firmware contract (schematic pinmap):

| Port | 1284 strobe pin | Notes |
|------|-----------------|-------|
| `$FE06` / `$FE00` | PD4 (pin 18) | Shared UPLDA SEL pin 14. Demux TBD or avoid dual use |
| `$FE07` | PD5 (pin 19) | Shares UPLDA SEL with `$FE02` (PLD load). Ignore strobe when not owning write |
| `$FE08` / `$FE05` | PD0 (pin 14) | Shared soft strobe |
| `$FE90`-`$FE92` | PD1 (pin 15) | Shared MAP family strobe. Sequence lo/mid/hi in firmware |
| Data | PC2-7 + PD6-7 | Same DQ as OAM |

On strobe + write: sample DQ into the soft register bank. On MAP mid/hi writes, UPLDV also loads CART_A14-A18 from CPU D (hardware). Keep soft `map_addr` in sync for `$FE93` auto-inc.

Sim: `r01s_board_peek_fe` / `r01s_board_poke_fe` on the board soft regs (no discrete HC573).

Cap: **16 sprites per logical scanline**. BG0 line is prepared one line ahead. Sprite field is full-playfield in VBlank. Not an RGB framebuffer.

## On-chip memory

| Space | Size | Notes |
|-------|------|-------|
| Flash | 128 KB | Firmware (OAM port, VBlank sprite field, HBlank BG0, pads, machine EEPROM) |
| SRAM | 16 KB | OAM shadow, line work, stacks |
| EEPROM | 4 KB | **Machine config** (not game saves: those are cart I2C) |

Endurance (typical datasheet): Flash 10k, EEPROM 100k write cycles.

## 40-pin PDIP (signals)

Notch at top. Left = pins 1-20 top to bottom. Right = 21-40 bottom to top.

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| 1 | PB0 (XCK0/T0) | 40 | PA0 (ADC0) |
| 2 | PB1 (CLKO/T1) | 39 | PA1 |
| 3 | PB2 (INT2/AIN0) | 38 | PA2 |
| 4 | PB3 (OC0A/AIN1) | 37 | PA3 |
| 5 | PB4 (OC0B/SS) | 36 | PA4 |
| 6 | PB5 (ICP3/MOSI) | 35 | PA5 |
| 7 | PB6 (OC3A/MISO) | 34 | PA6 |
| 8 | PB7 (OC3B/SCK) | 33 | PA7 |
| 9 | RESET | 32 | AREF |
| 10 | VCC | 31 | GND |
| 11 | GND | 30 | AVCC |
| 12 | XTAL2 | 29 | PC7 (TOSC2) |
| 13 | XTAL1 | 28 | PC6 (TOSC1) |
| 14 | PD0 (RXD0/T3) | 27 | PC5 (TDI) |
| 15 | PD1 (TXD0) | 26 | PC4 (TDO) |
| 16 | PD2 (RXD1/INT0) | 25 | PC3 (TMS) |
| 17 | PD3 (TXD1/INT1) | 24 | PC2 (TCK) |
| 18 | PD4 (XCK1/OC1B) | 23 | PC1 (SDA) |
| 19 | PD5 (OC1A) | 22 | PC0 (SCL) |
| 20 | PD6 (OC2B/ICP) | 21 | PD7 (OC2A) |

Exact GPIO-to-`$FExx` bit wiring is **schematic TBD**. Sim should expose a **firmware contract**: which ports implement OAM bus, line-buffer address/data, pad inputs, and (later) PWM audio.

## How it works on the bus (behavioral)

The 6502 does **not** DMA into 1284. Pattern:

1. Decode PLD selects 1284 when CPU writes/reads `$FE20`/`$FE21`, `$FE60`/`$FE61`, or soft `$FExx` ports (`$FE00`/`$FE05`/`$FE06`-`$FE08`/`$FE90`-`$FE92`).
2. 1284 firmware treats those as register windows (addr latch + data with auto-inc for OAM, soft copies for MAP/palette/PPUCTRL).
3. Each **VBlank**, firmware plots the sprite field from OAM (+ CHR). Each **HBlank**, firmware fills the next BG0 line. Beam reads sprites from the field and BG0 from the prepared line (BG1 color-0 mask) during active display.
4. Cap: **16 sprites per logical scanline**. Not a RGB framebuffer.

### Expected CPU-visible behavior

| CPU action | Expect |
|------------|--------|
| Store to `$FE20` | OAM address pointer set |
| Store to `$FE21` | Byte written at pointer. Pointer++ |
| Load `$FE60` | Pad bitfield for player 1 |
| Load `$FE61` | Pad bitfield for player 2 |
| 16 sprites on one Y | First 16 evaluated. Extras dropped (or documented overflow rule) |

## Communication on Retr01

```text
W65C02S --$FE20/21-->  ATmega1284P  --addr/data-->  AS6C62256 linebuf
                    |                 --HBlank-->     SST39SF040 CHR (gated)
                    +--$FE60/61--> pads / IDC
Beam / PLD -------- HBlank / line sync ----------> 1284 IRQ or GPIO
```

## Unit-test focus (sim)

Full AVR cycle accuracy is **not** required on day one. Prefer:

1. **Contract model:** OAM RAM + `$FE20/$FE21` + line-buffer writer + pad bytes + machine-EEPROM mailbox
2. Later: fuller AVR ISA if sim needs it

Tests: OAM auto-inc wrap, sprite Y match, ping-pong half select, pad bit polarity.

## Current vs legacy

| Topic | Current (32 IC) | Earlier sketches |
|-------|-----------------|---------------------|
| Audio | Separate **328P** | Separate 328P |
| Machine EEPROM | Internal **4 KB** EEPROM | (1284 on-board) |
| Clock | 20 MHz | 20 MHz |
