# ATmega1284P -- Sprite / OAM / pad MCU

**PDF:** [`../ATmega1284P_mcu.pdf`](../ATmega1284P_mcu.pdf).  
**Package (Retr01-A):** 40-pin PDIP.  
**Qty:** 1.  
**Clock (Retr01):** **20 MHz** (requires VCC 4.5-5.5 V for 20 MHz grade).

## What it is

AVR 8-bit MCU: **128 KB Flash**, **16 KB SRAM**, **4 KB EEPROM**, 32 GPIO lines on four ports (A/B/C/D), timers with PWM, USART, SPI, TWI, ADC, JTAG. Retr01 uses it as a **dedicated helper MCU**, not the game CPU: it owns **OAM**, **sprite line-buffer fill**, **controller bytes**, and **machine EEPROM**. APU stays on **328P** ([`06`](../../docs/06_hardware_v1_32ic.md)).

## Retr01 role

| Port / duty | Retr01 map |
|-------------|------------|
| OAM storage + evaluate | CPU writes via `$FE20` (addr) / `$FE21` (data), auto-inc; 64 entries `Y,tile,attr,X` |
| Sprite line buffer | During **HBlank**, write next line into AS6C62256 line-buffer SRAM (ping-pong 128 px halves) |
| Pads | Present `$FE60` / `$FE61` (R L D U X Y coin start, **1 = pressed**) |
| Machine EEPROM | Internal 4 KB; CPU handshake via `$FE70` band (protocol TBD in `02`) |
| CHR in HBlank | May own cart CHR bus while BG path is idle (do not share until island N proven) |

**Not** the BG beam path. **Not** the APU -- that is ATmega328P.

Cap: **16 sprites per logical scanline**. Pipeline is one line ahead; not a framebuffer.

## On-chip memory

| Space | Size | Notes |
|-------|------|-------|
| Flash | 128 KB | Firmware (sprite eval, OAM port, pads, machine EEPROM) |
| SRAM | 16 KB | OAM shadow, line work, stacks |
| EEPROM | 4 KB | **Machine config** (not game saves -- those are cart I2C) |

Endurance (typical datasheet): Flash 10k, EEPROM 100k write cycles.

## 40-pin PDIP (signals)

Notch at top. Left = pins 1-20 top to bottom; right = 21-40 bottom to top.

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

Exact GPIO-to-`$FExx` bit wiring is **schematic TBD**; sim should expose a **firmware contract**: which ports implement OAM bus, line-buffer address/data, pad inputs, and (later) PWM audio.

## How it works on the bus (behavioral)

The 6502 does **not** DMA into 1284. Pattern:

1. Decode PLD selects 1284 when CPU writes/reads `$FE20`/`$FE21` or `$FE60`/`$FE61`.
2. 1284 firmware treats those as register windows (addr latch + data with auto-inc for OAM).
3. Each HBlank (or line IRQ from beam logic), firmware:
   - Scans OAM for sprites on the next logical Y
   - Fetches CHR tiles (when bus granted)
   - Writes **128 bytes** into the **next** line-buffer half
4. Beam hardware reads the **current** half while 1284 fills the other.

```text
Line N:   half A SHOW (beam)   | half B FILL N+1 (1284)
Line N+1: half A FILL N+2      | half B SHOW
```

### Expected CPU-visible behavior

| CPU action | Expect |
|------------|--------|
| Store to `$FE20` | OAM address pointer set |
| Store to `$FE21` | Byte written at pointer; pointer++ |
| Load `$FE60` | Pad bitfield for player 1 |
| Load `$FE61` | Pad bitfield for player 2 |
| 16 sprites on one Y | First 16 evaluated; extras dropped (or documented overflow rule) |

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

| Topic | Current (32 IC) | Legacy (~52 / main) |
|-------|-----------------|---------------------|
| Audio | Separate **328P** | Separate 328P |
| Machine EEPROM | Internal **4 KB** EEPROM | AT28C64B parallel |
| Clock | 20 MHz | 20 MHz |
