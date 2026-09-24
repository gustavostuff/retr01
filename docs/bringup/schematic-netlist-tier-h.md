# Tier H schematic netlist (pin graph)

The Tier H sim builds a **schematic-complete pin netlist** (union-find over IC + passive pins) for wire overlay and future KiCad/Skidl export. It does **not** yet model passive analog behavior or AD724 composite encoding.

**Source code:** `apps/sim/tier-h/src/board_schematic.c`, invoked from `r01s_board_netlist_rebuild()`.

**BOM counts:** [`docs/passive_bom.md`](../passive_bom.md).

## Bypass (`C1`–`C21`)

| Cap | IC | VCC pin |
| --- | --- | --- |
| C1 | U1 (W65C02S) | VDD |
| C2 | U3 (sys RAM) | VCC |
| C3 | U6 (VRAM) | VCC |
| C4 | U41 (linebuf SRAM) | VCC |
| C5 | UM (MCU-M) | VCC |
| C6 | US1 (MCU-S1) | VCC |
| C7 | US2 (MCU-S2) | VCC |
| C8 | UPLDX (beam X) | VCC |
| C9 | UPLDY (beam Y) | VCC |
| C10 | UPLDV (compositor) | VCC |
| C11–C13 | U7A–U7C (74HC157) | VCC |
| C14 | U573 (field ALE) | VCC |
| C15 | U574 (scroll X) | VCC |
| C16 | U24 (color PROM) | VCC |
| C17 | *(74HC14, optional)* | +5V rail only in sim |
| C18 | *(AD724)* | +5V rail only in sim |
| C19 | U40 (cart flash) | VDD |
| C20 | U50 (24C64) | VCC |
| C21 | UPAD1 (pad ATtiny) | VCC |

Each bypass: cap `1` → IC VCC, cap `2` → `PS1` GND, `PS1` VDD → IC VCC.

## Bulk and crystals

- **E1:** `+` → `+5V`, `-` → GND.
- **Y1/Y2/Y3** (passive BOM crystals): load **C22–C27**; **Y1**/`Y2` also tie to functional **OSC8M** / **OSC_DOT** chips (same refdes as canned osc sprites — see sim UI).
- **Y3:** load caps only; net **`FSC_XTAL`** on crystal pin until AD724 is modeled.

## Video DAC

Weighted **R1–R8** from **U24** `O7`…`O0` to **SCR1** `RIN`/`GIN`/`BIN` (tier-a pattern). Terminations **R9–R11** to GND. **SCR1** `AGND` → GND.

## Series 33 Ω

| R | Net |
| --- | --- |
| R12 | Y1 `PHI2` ↔ U1 `PHI2` |
| R13 | Y2 `DOT` ↔ UPLDX `DOT` |
| R14–R21 | U1 `D[n]` ↔ U40 `DQ[n]` |
| R22 | `CART_OE#` ↔ U40 `OE#` |
| R23 | `CART_WE#` ↔ U40 `WE#` |
| R24–R25 | UM `SDA`/`SCL` ↔ U50 (I2C) |

## Pull-ups

| R | Net |
| --- | --- |
| R26 | US2 `PAD_DATA` ↔ UPAD1 `DATA` (+5V via R26) |
| R27–R28 | I2C SDA/SCL to +5V (MCU-M side) |
| R29 | CPU `RDY` (+5V, MCU-M `CPU_RDY` tied) |
| R30 | CPU `RESB` to +5V |

## Gaps (not schematic-complete on silicon)

- **AD724** RGB/composite path (only SCR1 DAC inputs + **FSC_XTAL** stub).
- **74HC14** reset/clock conditioning.
- Cart **socket** vs **U40** edge (OE#/WE# named stubs on series resistors).
- Extra PLD helpers (`UPLDA`, `UPLDB`, `UPLDI`, …) share the real BOM refdes where applicable but are not all in the locked-16 bypass table.

When AD724 and optional HC14 land in sim, extend `board_schematic.c` rather than duplicating links in UI code.

**Skidl / KiCad (preliminary only):** [`tier-h-skidl-export.md`](tier-h-skidl-export.md) — not fabrication-ready.
