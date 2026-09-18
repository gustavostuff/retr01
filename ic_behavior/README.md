# IC behavior

How each Retr01 chip behaves for board bring-up and for a future **netlist / IC simulation**. Board wiring lives in `../general_docs/hardware.md`. CPU map in `../general_docs/memory.md`.

Every part doc answers three questions:

1. **Inputs** - what the pins can sense from the netlist
2. **Process** - what the silicon does inside (combinational, clocked, firmware, memory)
3. **Outputs** - what the pins can drive onto the netlist

Levels for digital sims: **H**, **L**, **Z** (hi-Z), **X** (unknown / fight). Pin dirs: **IN**, **OUT**, **IO**, **PWR**, **NC**.

## Layout

Docs are per **part**, tagged by where they live:

| Area | Examples |
| --- | --- |
| Main motherboard | CPU, AVRs, SRAM, PLDs, mux/latches, color PROM, AD724 |
| Cart | Flash, save EEPROM |
| Pads / controllers | ATtiny85 in the TRS pad (outside the 19-IC count) |
| Optional | 74HC14 Schmitt glue (outside the 19 if populated) |

## Part index

### Counted motherboard (17)

| Doc | Part | Qty | Role |
| --- | --- | --- | --- |
| [W65C02S.md](W65C02S.md) | W65C02S | 1 | Game CPU |
| [AVR128DB28.md](AVR128DB28.md) | AVR128DB28 | 3 | MCU-M / MCU-S1 / MCU-S2 |
| [AS6C62256.md](AS6C62256.md) | AS6C62256 | 3 | Sys RAM, VRAM, field |
| [ATF22V10.md](ATF22V10.md) | ATF22V10 | 3 | Beam X, Beam Y, Compositor |
| [74HC157.md](74HC157.md) | 74HC157 | 3 | VRAM A[11:0] mux |
| [74HC573.md](74HC573.md) | 74HC573 | 1 | Field A[7:0] ALE latch |
| [74HC574.md](74HC574.md) | 74HC574 | 1 | BG1 scroll X `$7F02` |
| [AT27C256R.md](AT27C256R.md) | AT27C256R | 1 | Color PROM (R3G3B2). Kit RGB: [`../general_docs/palette/`](../general_docs/palette/README.md) |
| [AD724.md](AD724.md) | AD724 | 1 | RGB to NTSC/PAL composite |

### Counted cart (2)

| Doc | Part | Qty | Role |
| --- | --- | --- | --- |
| [SST39SF040.md](SST39SF040.md) | SST39SF040 | 1 | 512 KB cart flash |
| [24C64.md](24C64.md) | 24C64 | 1 | Cart save EEPROM (I2C) |

### Outside the 19

| Doc | Part | Role |
| --- | --- | --- |
| [ATtiny85.md](ATtiny85.md) | ATtiny85 | TRS pad MCU |
| [74HC14.md](74HC14.md) | 74HC14 | Optional Schmitt clock/reset cleanup |

## Sim authoring hints

Chip models are string-keyed parts with a pin list, then `reset` / `eval` / `tick` callbacks. Wire nets by pin **name** (for example `"PHI2"`, `"A0"`, `"PA2"`). Prefer functional edge behavior first, then tighten AC delays.
