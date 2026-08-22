# Retr01 IC reference (markdown)

Keyboard-friendly notes for each motherboard IC: what it is, package/pins, speed and memory, Retr01 role, pin behavior, and how it talks to the rest of the board. Written for **simulator authors** and island bring-up -- not a substitute for the vendor PDF.

**PDFs:** parent folder [`hw/`](../). **Simulator goals:** [`docs/08_simulator.md`](../../docs/08_simulator.md).

## Doc batches

| Batch | Status | Parts |
|-------|--------|-------|
| **1. CPU / MCU** | Done | [`W65C02S.md`](W65C02S.md), [`ATmega1284P.md`](ATmega1284P.md), [`ATmega328P.md`](ATmega328P.md) |
| **2. Memory** | Done | [`AS6C62256.md`](AS6C62256.md), [`SST39SF040.md`](SST39SF040.md), [`AT28C64B.md`](AT28C64B.md), [`AT28C16.md`](AT28C16.md) |
| **3. Logic / video glue** | Done | [`ATF22V10.md`](ATF22V10.md), [`SN74HC157.md`](SN74HC157.md), [`SN74HC245.md`](SN74HC245.md), [`SN74HC573.md`](SN74HC573.md), [`SN74HC688.md`](SN74HC688.md), [`SN74HC161.md`](SN74HC161.md), [`SN74HC_glue.md`](SN74HC_glue.md) (HC00/04/08/14/32/86) |

## Conventions

- Active-low signals keep the datasheet **B** / **#** suffix (`IRQB`, `CE#`).
- Retr01 clocks: CPU **8.000 MHz** PHI2, dot **5.369318 MHz**, 1284 **20 MHz**, 328P **16 MHz** (v0).
- Accuracy level for sim timing is not frozen; docs list datasheet numbers so models can tighten later.
- v1 BOM ([`06`](../../docs/06_hardware_v1_29ic.md)) may merge 328P audio into 1284 and drop AT28C64B; v0 docs here still describe the **discrete** parts.
