# Retr01_Lib footprints

Custom THT footprints for the Tier H motherboard KiCad project.

- **Source of truth:** this directory and `../Retr01_Lib.3dshapes/`.
- **KiCad project copy:** `export_netlist.sh` copies both into `apps/sim/tier-h/kicad/main-pcb/v_01/library/` (`fp-lib-table` uses `${KIPRJMOD}/library/Retr01_Lib.pretty`).
- **Stock-derived footprints:** regenerated with `scripts/retr01_trim_silk_footprints.py` (also run from `export_netlist.sh`).
- **Custom footprints** (not re-trimmed): `Jack_3.5mm_CUI_SJ1-3515N_Horizontal`, `CUI_RCJ-014*`, `EDAC_395_*`, `TO-92_Inline`, etc.
- **3D poses:** `scripts/retr01_patch_3d_poses.py` writes offset/scale/rotate into the listed `.kicad_mod` files and into matching `(model ...)` blocks on `v_0*.kicad_pcb`. `export_netlist.sh` runs that script so board-embedded footprints keep the pose (U130 TO-92, RCA jacks, TRS jacks).

Edit footprints here. Commit changes in git. The board file is `kicad/main-pcb/v_01/v_03.kicad_pcb`.
