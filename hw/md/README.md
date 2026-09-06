# Retr01 IC reference (markdown)

Keyboard-friendly notes for each motherboard IC: what it is, package/pins, speed and memory, Retr01 role, pin behavior, and how it talks to the rest of the board. Written for **simulator authors** and island bring-up. Confirm timing and pinouts against the vendor datasheet for the frozen part.

**Simulator:** [`app/sim/README.md`](../../app/sim/README.md).
**PDIP body sizes (mm + sim px):** [`packages_dip.md`](packages_dip.md).

## Doc batches

| Batch | Status | Parts |
|-------|--------|-------|
| **0. Packages** | Done | [`packages_dip.md`](packages_dip.md) (JEDEC / Microchip PDIP outlines + 4 px/mm canvas scale) |
| **1. CPU / MCU** | Done | [`W65C02S.md`](W65C02S.md), [`ATmega1284P.md`](ATmega1284P.md), [`ATmega328P.md`](ATmega328P.md) |
| **2. Memory** | Done | [`AS6C62256.md`](AS6C62256.md), [`SST39SF040.md`](SST39SF040.md), [`AT27C256R.md`](AT27C256R.md), [`AT28C16.md`](AT28C16.md) (legacy note), [`24C64.md`](24C64.md) (cart I2C save) |
| **3. Logic / video glue** | Done | [`ATF22V10.md`](ATF22V10.md), [`SN74HC157.md`](SN74HC157.md), [`SN74HC245.md`](SN74HC245.md), [`SN74HC573.md`](SN74HC573.md) (qty 0), [`SN74HC_glue.md`](SN74HC_glue.md) (HC00/04/08/14/32/86) |
| **3b. PLD equations** | Stubs | [`../pld/`](../pld/) CUPL drafts for scroll/raster/MAP (HC573-zero) |

## How to use these notes

- Prefer these markdown notes for sim / bring-up intent.
- **Authority:** vendor datasheet -> these markdown notes -> C chip models. Soft glue in `board.c` is system wiring, not a redefinition of the IC.
