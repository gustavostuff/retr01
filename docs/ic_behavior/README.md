# IC behavior

How each Retr01 chip is expected to behave on the bus, in time, and in firmware. Board rules live in `../hardware.md`.

## Layout (growing)

Docs are per **part**. Later they will also be tagged by where they live:

| Area | Examples |
| --- | --- |
| Main motherboard | CPU, AVRs, SRAM, PLDs, mux/latches, color PROM |
| Cart | Flash, save EEPROM |
| Pads / controllers | ATtiny85 in the TRS pad (outside the 18-IC count) |

## Written so far

| Doc | Part |
| --- | --- |
| [W65C02S.md](W65C02S.md) | Game CPU |
| [AVR128DB28.md](AVR128DB28.md) | MCU-M / MCU-S1 / MCU-S2 |

System map and soft ports: `../memory.md`. Roles on the board: `../hardware.md`.
