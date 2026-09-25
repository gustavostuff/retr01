# Retr01_Lib footprints

Custom and trimmed THT footprints for Tier H / preliminary KiCad export.

- **CUI_RCJ-014** / **CUI_RCJ-014_Audio** — edge-mount RCA (same pads). **J9** (composite)
  uses yellow ring WRL; **J8** (audio) uses white ring WRL. Regenerate WRL from STEP:
  `scripts/step_colored_to_wrl.py` (see `docs/bringup/tier-h-skidl-export.md`).
- **Jack_3.5mm_Switchcraft_…** — matte black WRL from supplier STEP.
- **EDAC_395_MoboSocket_2x18_2.54x5.08mm** — **J36** motherboard socket (395-036-520-201),
  **5.08 mm** row spacing; 3D offset/rotation from ``retr01_kicad/footprint_3d.py``.
- Do not use **PinSocket_2x18_P2.54mm_Vertical** for J36.
- **Cart_Edge_2x18_P2.54mm** — cart PCB gold fingers (not the mobo socket).
- Other `.kicad_mod` files are generated/trimmed by `scripts/retr01_trim_silk_footprints.py`
  unless noted (Switchcraft TRS jack).

Regenerate trimmed stock footprints: `python3 scripts/retr01_trim_silk_footprints.py`
(then `export_netlist.sh` copies this tree into `kicad/main-pcb/v_01/library/`).
