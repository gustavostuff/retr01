# Retr01 KiCad projects

Single home for PCB / project files and the custom footprint library.

**Netlists** still come from [`app/schematic_generator/`](../../app/schematic_generator/) (`python generate.py`). Import into the live projects below. Quilter Circuit Comprehension notes: [`docs/passive_rf_etc.md`](../../docs/passive_rf_etc.md) (power nets + bypass caps).

## Layout

| Path | Role |
|------|------|
| **[`motherboard/`](motherboard/)** | **Live** motherboard project. Open `Retr01_Motherboard.kicad_pro` |
| **[`cartridge/`](cartridge/)** | **Live** cartridge project. Open `Retr01_Cartridge.kicad_pro` |
| **[`Retr01_Lib.pretty/`](Retr01_Lib.pretty/)** | Custom footprints (TRS, RCA). Both projects point here via `fp-lib-table` |
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

## Open in KiCad

1. Motherboard: `motherboard/Retr01_Motherboard.kicad_pro`
2. Cartridge: `cartridge/Retr01_Cartridge.kicad_pro`
3. Confirm footprint table resolves `Retr01_Lib` to `../Retr01_Lib.pretty`

SKiDL / schem generator uses the same footprints via a symlink: `app/schematic_generator/library/Retr01_Lib.pretty` -> this folder.

## Archive policy

Do not edit under `archive/` for new layout work. Copy a snapshot into `motherboard/` only if you intentionally revive an old spin. Quilter zips and candidate PCBs stay in `archive/quilter/`.
