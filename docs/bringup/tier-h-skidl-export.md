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
- Skidl export is **motherboard-only**: 17 counted ICs (includes **U725 / AD724** placeholder), **3x ATF22V10** (UPLDX/Y/V), connectors **J1-J9 + J36**. Sim-only PLD helpers (UPLDA/B/I/N/P), cart **U40/U50**, pad **UPAD***, and **U4** PRG_ROM are omitted or remapped

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

API: `ns_pin_netlist_write_json()` in `tools/discrete_ic` (Tier H alias `r01s_pin_netlist_write_json`).

Unit test: `test_export_netlist`.

---

## Step 2: Skidl netlist (Python)

```bash
pip install skidl
./scripts/skidl_from_tier_h.py -q
```

Defaults: JSON `apps/sim/tier-h/skidl/retr01_tier_h.json`, netlist `apps/sim/tier-h/skidl/retr01_prelim.net`. Skidl backup files (`*_sklib.py`, `.erc`, `.log`) land in that same directory.

Add `-q` to hide Skidl footprint/tag warnings (expected for this draft flow).

If Skidl cannot find symbol libraries, set `KICAD10_SYMBOL_DIR` to the host KiCad symbols path (on many Linux installs: `/usr/share/kicad/symbols`). The script sets that path before importing Skidl when no KiCad env vars are present.

Script behavior:

- Refuses export if JSON claims `fabrication_ready: true`
- Maps passives to generic `Device` R/C symbols (electrolytics use `C`, not polarized CP)
- Maps oscillators **Y1-Y3** to DIP-8 cans. Other motherboard ICs use **DIP / THT footprints** from `retr01_kicad/` except **U725 / AD724** (**SOIC-16** `Retr01_Lib:SOIC-16_3.9x9.9mm_P1.27mm`)
- **J3/J4** Switchcraft TRS, **J8/J9** RCA (**RCJ-012** / **RCJ-014**, footprint **`Retr01_Lib:CUI_RCJ-014`** from GameTank `avboard_tht2`), **J5/J6** arcade 1x10, **J2** 1x6 RGB/sync header, **J36** **`Retr01_Lib:EDAC_395_MoboSocket_2x18_2.54x5.08mm`** (2x18 THT + **`${KIPRJMOD}/library/Retr01_Lib.3dshapes/EDAC_395-036-520-201.wrl`**), **J1** barrel (see `retr01_kicad/tier_h_map.py` and `connectors.py`)
- **J2** wiring (preliminary): pins 1-3 = PROM DAC guns after **R9-R11**. Pin 4 = **CSYNC** with **UPLDV** EQ (pin 14). Pins 5-6 = **GND** (RGBS default per [`hardware.md`](../general/hardware.md). RGBHV uses a mode jumper on pin 5 for **VSYNC** later). **U725 / AD724** stays on **`NC`** only (no video nets).
- Custom footprints live in-repo at `apps/sim/tier-h/skidl/library/Retr01_Lib.pretty`. **`export_netlist.sh` copies** that tree into `apps/sim/tier-h/kicad/main-pcb/v_01/library/Retr01_Lib.pretty` (no symlinks). The board **`fp-lib-table`** uses `${KIPRJMOD}/library/Retr01_Lib.pretty`. Re-import the netlist from **`v_01`** after export or KiCad may substitute stock footprints and drop **J3/J4** TRS / the second RCA.
- Skips **SCR1** (sim LCD sink); **PS1** sim PMIC pins remap to **J1**
- Unused footprint pads (arcade headers, **U725**, TRS NC pads, osc NC pins, etc.) tie to net **`NC`** so Pcbnew netlist import does not warn on missing symbol pins

KiCad import of `apps/sim/tier-h/skidl/retr01_prelim.net` is limited to visual experiment. Symbols and footprints require replacement and reconciliation against the schematic source of truth.

### KiCad custom library and board

- **Footprints + WRL:** `apps/sim/tier-h/skidl/library/Retr01_Lib.pretty` and `Retr01_Lib.3dshapes/` (3D offset/rotation live in each `.kicad_mod` `(model ...)` block).
- **Board:** `apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb` and `v_01.kicad_pro`. Open the project from **`v_01/`** so `${KIPRJMOD}` resolves.
- **Export copy:** `export_netlist.sh` copies the library tree into `v_01/library/` for KiCad (`fp-lib-table` -> `${KIPRJMOD}/library/Retr01_Lib.pretty`).

Notable custom footprints: **J36** `EDAC_395_MoboSocket_2x18_2.54x5.08mm` (5.08 mm row spacing), **J3/J4** Switchcraft TRS jack, **J8/J9** `CUI_RCJ-014` / `CUI_RCJ-014_Audio`, **U725** narrow **SOIC-16** (trimmed silk from KiCad `Package_SO`). Supplier STEP sources for WRL regeneration are under `Retr01_Lib.3dshapes/_step_source/`; colored WRLs use `scripts/step_colored_to_wrl.py` when needed.

### Silkscreen text (refdes + value)

Stock KiCad DIP footprints draw **Reference on F.SilkS** and a second **${REFERENCE} on F.Fab** (assembly drawing). With **F.Fab** visible in Pcbnew that looks like duplicate **U3** labels; only **F.SilkS** is printed on the physical board from normal gerbers.

For this flow, `scripts/retr01_trim_silk_footprints.py` (run from `export_netlist.sh`) copies THT footprints into **`Retr01_Lib.pretty`**: removes the fab **${REFERENCE}** copy and moves **Value** to **F.SilkS**. Netlist import then shows **refdes + BOM value** (e.g. **U3** / **AS6C62256**, **R1** / **100nF**), not the KiCad footprint filename.

**Fab houses (PCBWay, etc.):** assembly uses the **BOM + centroid** from the fab upload bundle, not silkscreen part numbers. Silk **refdes** helps hand assembly; silk **values** are optional (many production boards omit passive values). Silk does not need to duplicate BOM fields for PCBWay-style flows.

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

- [`docs/misc/kicad-schematic-tier-h.md`](../misc/kicad-schematic-tier-h.md): manual KiCad schematic capture (symbols, passives, wiring)  
- [`schematic-netlist-tier-h.md`](schematic-netlist-tier-h.md): link tables in sim  
- [`passive_bom.md`](../passive_bom.md): passive counts  
- [`apps/sim/tier-h/README.md`](../../apps/sim/tier-h/README.md): sim overview  
