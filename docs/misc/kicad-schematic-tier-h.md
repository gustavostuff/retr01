# KiCad schematic for Tier H (motherboard)

**Status:** Manual schematic capture for `apps/sim/tier-h/kicad/main-pcb/v_01/`. Preliminary until ERC-clean and matched to [`docs/general/hardware.md`](../general/hardware.md).

**Authority (in order):**

1. [`docs/general/hardware.md`](../general/hardware.md) (BOM, connectors, MCU pin freeze)
2. [`docs/bringup/schematic-netlist-tier-h.md`](../bringup/schematic-netlist-tier-h.md) and `apps/sim/tier-h/src/board_schematic.c` (passive pin links)
3. `apps/sim/tier-h/src/board_netlist.c` (major digital buses in Tier H sim)
4. Tier H sim canvas (visual cross-check, not fab sign-off)

The KiCad tree today has a populated **`v_01.kicad_pcb`** and an empty **`v_01.kicad_sch`**. The intended flow is **schematic-first**: symbols and wires in Eeschema, then **Update PCB from Schematic** (F8). Skidl netlist import ([`tier-h-skidl-export.md`](../bringup/tier-h-skidl-export.md)) remains an optional cross-check, not the schematic source of truth.

---

## Document map

| Doc | Contents |
| --- | --- |
| [`kicad-schematic-tier-h-symbols.md`](kicad-schematic-tier-h-symbols.md) | KiCad project setup, symbol libraries, refdes table, footprints, power symbols, annotation, ERC |
| [`kicad-schematic-tier-h-passives.md`](kicad-schematic-tier-h-passives.md) | Power entry, bypass, bulk, crystals, DAC resistors, 33 ohm series, pull-ups (pin-level) |
| [`kicad-schematic-tier-h-wiring.md`](kicad-schematic-tier-h-wiring.md) | CPU, memories, PLDs, glue, MCUs, field path, video, cart socket **J36**, connectors **J1-J9** |

---

## Suggested capture order

1. Symbols and footprints per symbols doc (all refdes placed, ERC power flags).
2. Global nets: `+5V`, `GND`, and labeled clocks `PHI2`, `DOT`.
3. Passives doc (every **C**, **R**, **E**, **Y** wired before dense buses).
4. Wiring doc block by block (CPU RAM/PRG, then VRAM mux, then video, then MCUs, then cart **J36**, then jacks).
5. **Inspect -> Electrical Rules Checker** on schematic. Fix floating inputs on CMOS parts.
6. **Tools -> Update PCB from Schematic** (F8). Reconcile footprint positions with the existing layout.
7. **Design Rules Checker** on PCB after routing.

---

## Out of scope on the motherboard sheet

| Item | Where it lives |
| --- | --- |
| Cart **SST39SF040** / **24C64** silicon | Cart PCB (nets continue through **J36**) |
| Pad **ATtiny85** | Controller PCB (**J3** / **J4** TRS path only on mobo) |
| PLD JEDEC equations | Pre-programmed ATF22V10 images (document as named nets into PLD pins, not fuse tables in KiCad) |
| Full AD724 analog network | [`hardware.md`](../general/hardware.md) + datasheet (sim uses **SCR1** sink stub for DAC only) |

---

## Related

- [`docs/bringup/tier-h-skidl-export.md`](../bringup/tier-h-skidl-export.md): JSON / Skidl export (optional)
- [`apps/sim/tier-h/README.md`](../../apps/sim/tier-h/README.md): board sim
- [`docs/passive_bom.md`](../passive_bom.md): passive counts and values
