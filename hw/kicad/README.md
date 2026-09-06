# Retr01 KiCad projects

Single home for PCB / project files and the custom footprint library.

**Netlists** still come from [`app/schematic_generator/`](../../app/schematic_generator/) (`python generate.py`). Import into the live projects below. Quilter Circuit Comprehension notes: [`docs/passive_rf_etc.md`](../../docs/passive_rf_etc.md) (power nets + bypass caps).

## Layout

| Path | Role |
|------|------|
| **[`motherboard/`](motherboard/)** | **Live** motherboard project. Open `Retr01_Motherboard.kicad_pro` |
| **[`cartridge/`](cartridge/)** | **Live** cartridge project. Open `Retr01_Cartridge.kicad_pro` |
| **[`Retr01_Lib.pretty/`](Retr01_Lib.pretty/)** | Custom footprints (TRS, RCA, cart gold fingers). Both projects point here via `fp-lib-table` |
| **[`archive/`](archive/)** | Old boards and Quilter downloads (not the working copy) |

```text
hw/kicad/
+-- Retr01_Lib.pretty/     # authority footprint lib
+-- motherboard/           # live mobo (from former v07)
+-- cartridge/             # live cart
+-- archive/
    +-- motherboard_v01/   # first KiCad project
    +-- snapshots_v02/     # intermediate v02-v07 saves
    +-- quilter/           # Quilter candidate exports
```

## Cartridge gold fingers

Cart netlist `J36` uses `Retr01_Lib:Cart_Edge_2x18_P2.54mm` (pads **1-18** = A / F.Cu, **19-36** = B / B.Cu). Motherboard `J36` stays on the PinSocket stand-in.

**Import / swap in Pcbnew:**

1. Draw `Edge.Cuts` (~**55 x 78 mm**, thickness **1.6 mm**).
2. `File` -> `Import` -> `Netlist` -> `app/schematic_generator/output/retr01_cart.net` (or Update PCB from schematic if you keep a sch).
3. If an old PinSocket `J36` is still on the board: delete it, then Update PCB so the gold-finger footprint appears.
4. Place `J36` with origin on the insert edge. Pads point into the board (+X in the footprint). Silk **A1** marks side A pin 1.
5. Place U40 / U50 / caps and route.

**Quilter (cart):** Circuit Comprehension checklist is in [`docs/passive_rf_etc.md`](../../docs/passive_rf_etc.md#quilter-circuit-comprehension---cartridge-pcb) (pour `+5V` + `GND` only, stubs pour off, `CD1`/`CD2` only). KiCad: `J36` and `Edge.Cuts` locked, GND pours stop above fingers, `FINGER_KEEPOUT` on the tongue. After reopen, **Edit -> Fill All Zones** if you want local pours before upload.

Confirm `fp-lib-table` resolves `Retr01_Lib` (already set for `cartridge/`).

SKiDL / schem generator uses the same footprints via a symlink: `app/schematic_generator/library/Retr01_Lib.pretty` -> this folder.

## Archive policy

Do not edit under `archive/` for new layout work. Copy a snapshot into `motherboard/` only if you intentionally revive an old spin. Quilter zips and candidate PCBs stay in `archive/quilter/`.
