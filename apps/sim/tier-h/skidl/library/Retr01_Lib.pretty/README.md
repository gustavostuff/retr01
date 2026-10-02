# Retr01_Lib footprints

Custom THT footprints for the Tier H motherboard KiCad project.

- **Source of truth:** this directory and `../Retr01_Lib.3dshapes/`.
- **KiCad project copy:** `export_netlist.sh` copies both into `apps/sim/tier-h/kicad/main-pcb/v_01/library/` (`fp-lib-table` uses `${KIPRJMOD}/library/Retr01_Lib.pretty`).
- **Stock-derived footprints:** regenerated with `scripts/retr01_trim_silk_footprints.py` (also run from `export_netlist.sh`).
- **Custom footprints** (not re-trimmed): `Jack_3.5mm_CUI_SJ1-3515N_Horizontal`, `CUI_RCJ-014*`, `EDAC_395_*`, etc. Their `(model …)` path/offset/rotate live only in these `.kicad_mod` files — edit here (or Footprint Editor → Save) so `export_netlist.sh` keeps them.

Edit footprints and embedded `(model ...)` blocks here. Commit changes in git. The board file is `kicad/main-pcb/v_01/v_03.kicad_pcb`.
