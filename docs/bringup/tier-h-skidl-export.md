# Tier H Skidl / KiCad netlist (preliminary only)

**Status:** illustrative export pipeline only. Generated netlists must not be used for fabrication, ordering, or design sign-off. The Retr01 motherboard PCB design is not ready. Tier H sim connectivity matches bring-up in places but stays intentionally incomplete versus [`docs/general/hardware.md`](../general/hardware.md).

The export flow supports rough connectivity review, floorplan discussion, and experimental KiCad import. It is not intended for tape-out.

---

## What gets exported

Source of truth after `r01s_board_netlist_rebuild()`:

- Motherboard IC links (`board_netlist.c`)
- Passive / power / DAC / series / pull-up links (`board_schematic.c`)
- Motherboard ICs, passives, and power (sim still models cart flash/EEPROM and pad MCUs; Skidl **remaps** those to **J36** and drops pad silicon)

Known **gaps** (same as [`schematic-netlist-tier-h.md`](schematic-netlist-tier-h.md)):

- **AD724** not modeled (SCR1 video sink stands in for part of the video path)
- **74HC14** optional block not seated
- Cart **edge** vs **U40** module (OE#/WE# stubs)
- **Y1/Y2** refdes shared between BOM crystals and canned osc chips in sim
- Skidl export is **motherboard-only**: 17 counted ICs (includes **U725 / AD724** placeholder), **3× ATF22V10** (UPLDX/Y/V), connectors **J1–J9 + J36**; sim-only PLD helpers (UPLDA/B/I/N/P), cart **U40/U50**, pad **UPAD***, and **U4** PRG_ROM are omitted or remapped

The JSON embeds `"fabrication_ready": false` and `"purpose": "preliminary_pcb_illustrative_only"`.

---

## Step 1: Export JSON (C)

Typical Tier H build:

```bash
cmake -S apps/sim/tier-h -B apps/sim/tier-h/build
cmake --build apps/sim/tier-h/build --target export_tier_h_netlist
./apps/sim/tier-h/build/export_tier_h_netlist > apps/sim/tier-h/skidl/retr01_tier_h.json
```

One-shot (JSON + netlist into `apps/sim/tier-h/skidl/`):

```bash
apps/sim/tier-h/skidl/export_netlist.sh -q
```

API: `ns_pin_netlist_write_json()` in `apps/netlist_sim` (Tier H alias `r01s_pin_netlist_write_json`).

Unit test: `test_export_netlist`.

---

## Step 2: Skidl netlist (Python)

```bash
pip install skidl
./scripts/skidl_from_tier_h.py -q
```

Defaults: JSON `apps/sim/tier-h/skidl/retr01_tier_h.json`, netlist `apps/sim/tier-h/skidl/retr01_prelim.net`. Skidl backup files (`*_sklib.py`, `.erc`, `.log`) land in that same directory.

Add `-q` to hide Skidl footprint/tag warnings (expected for this draft flow).

If Skidl cannot find symbol libraries, set `KICAD10_SYMBOL_DIR` to your KiCad symbols path (on many Linux installs: `/usr/share/kicad/symbols`). The script sets that path before importing Skidl when no KiCad env vars are present.

Script behavior:

- Refuses export if JSON claims `fabrication_ready: true`
- Maps passives to generic `Device` R/C symbols (electrolytics use `C`, not polarized CP)
- Maps oscillators **Y1–Y3** to DIP-8 cans; other parts use **DIP / THT footprints** from `retr01_kicad/` (ported from legacy `schematic_generator`)
- **J3/J4** Switchcraft TRS, **J8/J9** RCA (**RCJ-012** / **RCJ-014**, footprint **`Retr01_Lib:CUI_RCJ-014`** from GameTank `avboard_tht2`), **J5/J6** arcade 1×10, **J2** 1×6 RGB/sync header, **J36** **`Retr01_Lib:EDAC_395_MoboSocket_2x18_2.54x5.08mm`** (2×18 THT + **`${KIPRJMOD}/library/Retr01_Lib.3dshapes/EDAC_395-036-520-201.wrl`**), **J1** barrel — see `retr01_kicad/tier_h_map.py` and `connectors.py`
- **J2** wiring (preliminary): pins 1–3 = PROM DAC guns after **R9–R11**; pin 4 = **CSYNC** with **UPLDV** EQ (pin 14); pins 5–6 = **GND** (RGBS default per [`hardware.md`](../general/hardware.md); RGBHV uses a mode jumper on pin 5 for **VSYNC** later). **U725 / AD724** stays on **`NC`** only (no video nets).
- Custom footprints live in-repo at `apps/sim/tier-h/skidl/library/Retr01_Lib.pretty`. **`export_netlist.sh` copies** that tree into `apps/sim/tier-h/kicad/main-pcb/v_01/library/Retr01_Lib.pretty` (no symlinks). The board **`fp-lib-table`** uses `${KIPRJMOD}/library/Retr01_Lib.pretty`. Re-import the netlist from **`v_01`** after export or KiCad may substitute stock footprints and drop **J3/J4** TRS / the second RCA.
- Skips **SCR1** (sim LCD sink); **PS1** sim PMIC pins remap to **J1**
- Unused footprint pads (arcade headers, **U725**, TRS NC pads, osc NC pins, etc.) tie to net **`NC`** so Pcbnew netlist import does not warn on missing symbol pins

KiCad import of `apps/sim/tier-h/skidl/retr01_prelim.net` is limited to visual experiment. Symbols and footprints require replacement and reconciliation against the schematic source of truth.

### J36 3D model

**J36** uses footprint **`EDAC_395_MoboSocket_2x18_2.54x5.08mm`** in **`Retr01_Lib.pretty`**. Open the board from **`v_01/`** so `${KIPRJMOD}` resolves.

#### TRS (J3/J4) 3D

Footprint **`Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical`** uses **`Switchcraft_35RAPC4BVN4.wrl`** (matte black; STEP alone stays gray in the viewer). **`CUI_RCJ-014`** / **`CUI_RCJ-014_Audio`** use **`CUI_RCJ-014.wrl`** (yellow ring, composite **J9**) and **`CUI_RCJ-014_audio.wrl`** (white ring, audio **J8**). Regenerate from supplier STEP:

```bash
python3 -m venv .venv-wrl && .venv-wrl/bin/pip install cascadio numpy
SH=apps/sim/tier-h/skidl/library/Retr01_Lib.3dshapes
PY=.venv-wrl/bin/python3
$PY scripts/step_colored_to_wrl.py $SH/_step_source/Switchcraft_35RAPC4BVN4.step -o $SH/Switchcraft_35RAPC4BVN4.wrl --default black
$PY scripts/step_colored_to_wrl.py $SH/_step_source/CUI_RCJ-014.step -o $SH/CUI_RCJ-014.wrl --mat mat_1:gray --mat mat_2:yellow
$PY scripts/step_colored_to_wrl.py $SH/_step_source/CUI_RCJ-014.step -o $SH/CUI_RCJ-014_audio.wrl --mat mat_1:gray --mat mat_2:white
```

Supplier **STEP** files live under **`Retr01_Lib.3dshapes/_step_source/`** (not next to WRL) so a stale board footprint cannot keep loading gray STEP. If **J3** still shows **`.step`** in Properties → 3D Models, run:

```bash
python3 scripts/retr01_sync_pcb_3d_models.py apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb
```

(`export_netlist.sh` runs this automatically when `v_01.kicad_pcb` exists.)

3D assets live in **`apps/sim/tier-h/skidl/library/Retr01_Lib.3dshapes/`**; `export_netlist.sh` copies them to **`v_01/library/Retr01_Lib.3dshapes/`**. After updating the library, run **Tools → Update Footprints from Library…** and enable **Replace footprint models** so the board picks up new WRL paths/colors. Netlist re-import (with tstamps) often **does not** refresh embedded `(model …)` paths on existing footprints; if colors stay wrong, select **J3/J4** and check Properties → 3D Models — it must show **`.wrl`**, not **`.step`**. Fix via **Update Footprints from Library** (replace models) or edit the footprint on the board.

#### RCA (J8/J9) 3D

KiCad **10 stock libraries have no 3D model** for **CUI/Same Sky RCJ-01** edge-mount jacks (footprint **`CUI_RCJ-014`** is custom from GameTank). Free official source: [Same Sky RCJ-014 product page](https://www.sameskydevices.com/product/interconnect/connectors/rca-connectors/rcj-014) → **3D Model** / [CAD Model Library](https://www.sameskydevices.com/resources/cad-model-library). Same Sky **STEP** remains in **`Retr01_Lib.3dshapes/_step_source/`** for regeneration; footprints point at colored **WRL** (see TRS section above). Tune offset/rotation in Footprint Editor if needed.

#### Retr01 3D model transforms (Skidl export)

KiCad **netlists do not carry** 3D offset/rotation. Canonical values live in
``apps/sim/tier-h/skidl/retr01_kicad/footprint_3d.py`` (same tree as ``footprints.py``).
``export_netlist.sh`` runs ``scripts/retr01_apply_footprint_3d.py`` to write them into
``Retr01_Lib.pretty``, then ``scripts/retr01_sync_pcb_3d_models.py`` copies full
``(model ...)`` blocks onto ``v_01.kicad_pcb`` when that file exists.

After **netlist import** in Pcbnew, run either ``export_netlist.sh`` again or:

```bash
python3 scripts/retr01_sync_pcb_3d_models.py apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb
```

Add new connectors in ``footprint_3d.py``, re-export, then sync.

#### EDAC J36 hole pattern

**`EDAC_395_MoboSocket_2x18_2.54x5.08mm`** (custom Retr01 footprint, not KiCad’s generic **PinSocket_2x18**) uses **2.54 mm** pitch along the connector (Y) and **5.08 mm** center-to-center between the two rows (X: pads **1/3/5/…** at **0**, **2/4/6/…** at **−5.08**). Re-import **`retr01_prelim.net`** after export so **J36** picks up the new footprint name (embedded **`EDAC_395-036-520-201`** copies on the board will not auto-fix).

#### EDAC 3D colors (gold contacts, black body)

The footprint uses **`EDAC_395-036-520-201.wrl`** (not STEP) in the 3D viewer so materials render correctly. WRL vertices use KiCad’s **0.1 inch per unit** convention (`mm × 10/25.4`) so size matches the supplier **STEP**. Regenerate from the supplier STEP:

```bash
pip install cascadio   # once, in a venv is fine
python3 scripts/edac_395_step_to_wrl.py
```

Contacts (`345-292` / `345-293` groups) are **gold**; housing / lugs are **matte black**. **`EDAC_395-036-520-201.step`** stays in `3dmodels/` for mechanical export if you re-attach it in the footprint.

#### Fix a rotated / misaligned EDAC 3D (KiCad 10)

KiCad has **no drag-to-align** for 3D models — you edit **numbers** while watching the preview.

1. Open **`v_01/v_01.kicad_pcb`** in Pcbnew.
2. Click **J36** → right-click → **Edit Footprint in Library…** (or open **Footprint Editor** → **Retr01_Lib** → **EDAC_395_MoboSocket_2x18_2.54x5.08mm**).
3. **E** → **Footprint Properties** → tab **3D Models**.
4. **Click the model line** in the list (the row with `EDAC_395-036-520-201.step`). If rotation/offset fields are greyed out, click that row again or toggle **Preview** on that row — KiCad only edits the **selected** model.
5. Turn on a live preview: **View → Show 3D Model Preview** (or the 3D preview pane in the footprint editor).
6. **Wrong way round on the board (most common):** change only **Rotation → Z** in **90°** steps: try **0**, **90**, **180**, **270** until the **card slot** runs the same direction as the **two rows of 36 holes** (long axis of the connector matches the long row of pads).
   - **Z** = spin in the plane of the PCB (like turning a knob).
   - **X** / **Y** = tilt the connector up/down or sideways (usually leave at **0** unless the model looks “flopped over”).
7. **Shift without rotating:** adjust **Offset X / Y / Z** (mm). **Z** often needs ~**+20 mm** so the plastic sits on the board with tails through the holes (default in the footprint is a starting guess).
8. **Save** the footprint to **Retr01_Lib** → back in Pcbnew: **Tools → Update Footprints from Library…** (or re-import netlist) → **Alt+3**.

Copy any final **Rotation** / **Offset** values back into **`apps/sim/tier-h/skidl/library/Retr01_Lib.pretty/EDAC_395_MoboSocket_2x18_2.54x5.08mm.kicad_mod`** so the next `export_netlist.sh` does not overwrite your fix (export replaces `v_01/library/` from skidl).

### Silkscreen text (refdes + value)

Stock KiCad DIP footprints draw **Reference on F.SilkS** and a second **${REFERENCE} on F.Fab** (assembly drawing). With **F.Fab** visible in Pcbnew that looks like duplicate **U3** labels; only **F.SilkS** is printed on the physical board from normal gerbers.

For this flow, `scripts/retr01_trim_silk_footprints.py` (run from `export_netlist.sh`) copies THT footprints into **`Retr01_Lib.pretty`**: removes the fab **${REFERENCE}** copy and moves **Value** to **F.SilkS**. Netlist import then shows **refdes + BOM value** (e.g. **U3** / **AS6C62256**, **R1** / **100nF**), not the KiCad footprint filename.

**Fab houses (PCBWay, etc.):** assembly uses the **BOM + centroid** you upload, not silkscreen part numbers. Silk **refdes** helps hand assembly; silk **values** are optional (many production boards omit passive values). Nothing extra is required on silk for PCBWay beyond what you put in the BOM CSV.

---

## Fabrication-ready criteria

Before any fab-ready netlist:

1. AD724 + optional HC14 in sim/schematic links  
2. Resolve refdes / crystal vs osc naming  
3. Curated **refdes to KiCad symbol + footprint** table (locked-19 BOM)  
4. Cart edge and pad harness as designed nets, not stubs  
5. Human schematic review, not sim union-find alone  

Until those criteria are met, Tier H Skidl output remains draft illustration only.

---

## Related

- [`schematic-netlist-tier-h.md`](schematic-netlist-tier-h.md): link tables in sim  
- [`passive_bom.md`](../passive_bom.md): passive counts  
- [`apps/sim/tier-h/README.md`](../../apps/sim/tier-h/README.md): sim overview  
