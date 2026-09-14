# IC behavior

How each Retr01 chip behaves for board bring-up and for a future **netlist / IC simulation**. Board wiring lives in `../hardware.md`. CPU map in `../memory.md`.

Every part doc should answer three questions:

1. **Inputs** - what the pins can sense from the netlist
2. **Process** - what the silicon does inside (combinational, clocked, firmware, memory)
3. **Outputs** - what the pins can drive onto the netlist

Levels for digital sims: **H**, **L**, **Z** (hi-Z), **X** (unknown / fight). Pin dirs: **IN**, **OUT**, **IO**, **PWR**, **NC**.

## Layout (growing)

Docs are per **part**. Later they will also be tagged by where they live:

| Area | Examples |
| --- | --- |
| Main motherboard | CPU, AVRs, SRAM, PLDs, mux/latches, color PROM |
| Cart | Flash, save EEPROM |
| Pads / controllers | ATtiny85 in the TRS pad (outside the 19-IC count) |

## Written so far

| Doc | Part |
| --- | --- |
| [W65C02S.md](W65C02S.md) | Game CPU |
| [AVR128DB28.md](AVR128DB28.md) | MCU-M / MCU-S1 / MCU-S2 (same silicon, three firmwares) |

## Sim authoring hints

Chip models are string-keyed parts with a pin list, then `reset` / `eval` / `tick` callbacks. Wire nets by pin **name** (for example `"PHI2"`, `"A0"`, `"PA2"`). Prefer functional edge behavior first, then tighten AC delays.
