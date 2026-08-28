# Retr01 IC reference (markdown)

Keyboard-friendly notes for each motherboard IC: what it is, package/pins, speed and memory, Retr01 role, pin behavior, and how it talks to the rest of the board. Written for **simulator authors** and island bring-up. Not a substitute for the vendor PDF.

**PDFs:** parent folder [`hw/`](../). **Simulator:** [`retr01_sim/README.md`](../../retr01_sim/README.md).  
**PDIP body sizes (mm + sim px):** [`packages_dip.md`](packages_dip.md).

## Doc batches

| Batch | Status | Parts |
|-------|--------|-------|
| **0. Packages** | Done | [`packages_dip.md`](packages_dip.md) (JEDEC / Microchip PDIP outlines + 4 px/mm canvas scale) |
| **1. CPU / MCU** | Done | [`W65C02S.md`](W65C02S.md), [`ATmega1284P.md`](ATmega1284P.md), [`ATmega328P.md`](ATmega328P.md) |
| **2. Memory** | Done | [`AS6C62256.md`](AS6C62256.md), [`SST39SF040.md`](SST39SF040.md), [`AT28C16.md`](AT28C16.md), [`24C64.md`](24C64.md) (cart I2C save) |
| **3. Logic / video glue** | Done | [`ATF22V10.md`](ATF22V10.md), [`SN74HC157.md`](SN74HC157.md), [`SN74HC245.md`](SN74HC245.md), [`SN74HC573.md`](SN74HC573.md), [`SN74HC_glue.md`](SN74HC_glue.md) (HC00/04/08/14/32/86) |

## Conventions

- Active-low signals keep the datasheet **B** / **#** suffix (`IRQB`, `CE#`).
- Retr01 clocks: CPU **8.000 MHz** PHI2, dot **5.369318 MHz**, 1284 **20 MHz**, 328P **16 MHz**.
- Accuracy level for sim timing is not frozen. Docs list datasheet numbers so models can tighten later.
- Current BOM ([`06`](../../docs/06_hardware_v1_32ic.md)): 328P APU, 3x HC245, cart **24C64** I2C save, 1284 internal machine EEPROM, 1x Color PROM.
- **Authority:** vendor PDF in `hw/` -> these markdown notes -> C chip models. Soft glue in `board.c` is system wiring, not a redefinition of the IC.
- **Package outlines:** use [`packages_dip.md`](packages_dip.md) for body LxW. Do not invent footprints from the sim alone.
