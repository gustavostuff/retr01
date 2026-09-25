# Retr01_Lib footprints

Custom THT footprints for the Tier H motherboard KiCad project.

- **Source of truth:** this directory and `../Retr01_Lib.3dshapes/`.
- **KiCad project copy:** `export_netlist.sh` copies both into `apps/sim/tier-h/kicad/main-pcb/v_01/library/` (`fp-lib-table` uses `${KIPRJMOD}/library/Retr01_Lib.pretty`).
- **Stock-derived footprints:** regenerated with `scripts/retr01_trim_silk_footprints.py` (also run from `export_netlist.sh`).

Edit footprints and embedded `(model …)` blocks here; commit changes in git. The board file is `kicad/main-pcb/v_01/v_01.kicad_pcb`.
