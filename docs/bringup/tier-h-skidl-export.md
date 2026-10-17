# Tier H Skidl / KiCad netlist (preliminary only)

**Status:** illustrative export pipeline only. Generated netlists must not be used for fabrication, ordering, or design sign-off. The Retr01 motherboard PCB design is not ready. Tier H sim connectivity matches bring-up in places but stays intentionally incomplete versus [`docs/general/hardware.md`](../general/hardware.md).

The export flow supports rough connectivity review, floorplan discussion, and experimental KiCad import. It is not intended for tape-out.

---

## What gets exported

Source of truth after `r01s_board_netlist_rebuild()` (see [`schematic-netlist-tier-h.md`](schematic-netlist-tier-h.md)):

- Motherboard IC links (`board_netlist.c`)
- Passive / power / DAC / series / pull-up links (`board_schematic.c`)

Skidl (`scripts/skidl_from_tier_h.py`) is a **consumer**: JSON in, KiCad `.net` out. Correct pin numbers, power rails, and bus membership belong in the **sim netlist** and chip pin maps; Python only applies motherboard scope (J36/J1 remap, J3/J4 sim TIP/RING/SLEEVE 1/2/3 onto Switchcraft pads 4/2/1), footprint pin templates, NC stubs, and net naming. If JSON and PCB disagree, fix sim/export JSON before patching KiCad by hand.
- Motherboard ICs, passives, and power (sim still models cart flash/EEPROM and pad MCUs, while Skidl **remaps** those to **J36** and drops pad silicon). The cart orderable is the TSOP **SST39SF040-55-4C-TU**. The sim pin diagram is still the PDIP numbering

Known **gaps** (same as [`schematic-netlist-tier-h.md`](schematic-netlist-tier-h.md)):

- **CXA2075M** analog encode (logic stub only; **CVOUT** is a video-present flag)
- **MCP130** supervisor not modeled in sim
- Cart **edge** vs **U40** module (OE#/WE# stubs)
- Skidl export is **motherboard-only**: 19 counted ICs (including **U04** 74HCU04, **U74** 74HC74, and **U2075 / CXA2075M** placeholder), **3x ATF22V10** (UPLDX/Y/V), connectors **J1-J10 + J36**. Sim-only PLD helpers (UPLDA/B/I/N/P), cart **U40/U50**, pad **UPAD***, and **U4** PRG_ROM are omitted or remapped

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

`-q` hides Skidl footprint/tag warnings (expected for this draft flow).

If Skidl cannot find symbol libraries, set `KICAD10_SYMBOL_DIR` to the host KiCad symbols path (on many Linux installs: `/usr/share/kicad/symbols`). The script sets that path before importing Skidl when no KiCad env vars are present.

Script behavior:

- Refuses export if JSON claims `fabrication_ready: true`
- Maps passives to generic `Device` R/C symbols (electrolytics use `C`, not polarized CP)
- Maps quartz crystals **Y1-Y3** to HC-49/US vertical packages and instantiates the discrete clock network with **U04** (74HCU04) and **U74** (74HC74). Motherboard ICs use **DIP / THT footprints** from `retr01_kicad/`. **U2075 / CXA2075M** is `Retr01_Lib:SOP-24_7.5x15.4mm_P1.27mm`. **J11** is `Retr01_Lib:PinHeader_2x02_P2.54mm_Vertical`
- **J3/J4** player TRS. One footprint holds the Switchcraft **35RAPC2BVN4** 3.5 mm outline and the **CLIFF S4** 6.35 mm outline (typical **S4/BMB/PC-C**, netlist footprint `Retr01_Lib:Jack_TRS_3.5mm_and_6.35mm`), **J5**/**J6** arcade 2x5 (P1 right of US2, P2 left), **J2** Kycon **KMDGX-9S-AS** (`Retr01_Lib:Kycon_KMDGX-9S-AS`), **J10** cart program 2x2 (`Retr01_Lib:PinHeader_2x02_P2.54mm_Vertical`), **J36** **`Retr01_Lib:EDAC_395_MoboSocket_2x18_2.54x5.08mm`** (2x18 THT + **`${KIPRJMOD}/library/Retr01_Lib.3dshapes/EDAC_395-036-520-201.wrl`**), **J1** GCT **DCJ200-10-A** horizontal barrel jack (see `retr01_kicad/tier_h_map.py` and `connectors.py`)
- **J2** wiring: pin numbers are the Kycon recommended layout with the connector facing up. Pin 1 Blue (R11), pin 3 Green (R10), pin 7 Red (R9). Pin 5 **CSYNC** (also UPLDX and UPLDV). Pin 4 **U2075** CVOUT. Pins 6, 8, and 9 are MCU-S2 **AUDIO_PWM**. Pin 2 is **+5V**. Pad **S** is **GND** (three shield holes). HSYNC and VSYNC stay on the beam PLDs and are not on J2.
- Custom footprints live in-repo at `apps/sim/tier-h/skidl/library/Retr01_Lib.pretty`. **`export_netlist.sh` copies** that tree into `apps/sim/tier-h/kicad/main-pcb/library/Retr01_Lib.pretty` (no symlinks). Open `main_pcb.kicad_pro` from `main-pcb/` so `${KIPRJMOD}` resolves. Re-import the netlist into **`v_02.kicad_pcb`** after export or KiCad may substitute stock footprints and drop **J3/J4** TRS. Replace-existing updates matching refdes only. Footprints in the netlist and not on the board attach to the cursor after the import dialog confirms. Escape discards that drag and those parts never land on copper.
- Skips **SCR1** (sim LCD sink) and the UI **5V** / **GND** rail symbols. **PS1** sim PMIC pins remap to **J1**
- Unused footprint pads (arcade headers, **U2075**, TRS switch contacts, crystal load pins, etc.) each get a unique **`NC_<ref>_<pin>`** net so Pcbnew netlist import does not warn on missing symbol pins and those pads do not share a ratsnest
- Netlist nodes carry datasheet pin names (`pinfunction`) and KiCad pintypes. Package **VCC** / **GND** pads are `power_in`. **U74** PRE# / CLR# use net `U74_PRE_CLR#` (strapped to VCC in `DIP-14_W7.62mm_74HC74`). **U04** gates 5 and 6 are the Y3 Pierce and the buffer into U2075 SCIN (`FSC_XTAL_IN`, `FSC_XTAL_OUT`, `FSC_CLK`).
- Analog and slow I/O nets use KiCad net class **Quiet** so those air wires can take a different color from Default. CPU address bits use **CPU_A** and CPU data bits use **CPU_D** (two layer-1 corridors on the same electrical bus). Assignment follows [`docs/bring-up-v2/main-pcb-layers.md`](../bring-up-v2/main-pcb-layers.md). Patterns live in `retr01_kicad/layer4_nets.py`, `retr01_kicad/bus_nets.py`, and in `main_pcb.kicad_pro`.

KiCad import of `apps/sim/tier-h/skidl/retr01_prelim.net` is limited to visual experiment. Symbols and footprints require replacement and reconciliation against the schematic source of truth.

### KiCad custom library and board

- **Footprints + WRL:** `apps/sim/tier-h/skidl/library/Retr01_Lib.pretty` and `Retr01_Lib.3dshapes/`. 3D offset/scale/rotate for U130 (`TO-92_Inline`: offset X 1.25 mm, rotate Z 180 deg) and the TRS jacks live in `scripts/retr01_patch_3d_poses.py`. `export_netlist.sh` stamps the poses into the `.kicad_mod` files and into matching `(model ...)` blocks on each `v_0*.kicad_pcb`.
- **Project:** `apps/sim/tier-h/kicad/main-pcb/main_pcb.kicad_pro`. Board revisions are the `v_*.kicad_pcb` files in that folder. Open the project from **`main-pcb/`** so `${KIPRJMOD}` resolves.
- **Export copy:** `export_netlist.sh` copies the library tree into `main-pcb/library/` for KiCad (`fp-lib-table` -> `${KIPRJMOD}/library/Retr01_Lib.pretty`).

Notable custom footprints: **J36** `EDAC_395_MoboSocket_2x18_2.54x5.08mm` (5.08 mm row spacing), **J1** GCT DCJ200-10-A barrel jack, **J3/J4** `Jack_TRS_3.5mm_and_6.35mm` (in-repo `Retr01_Lib`, 3D `Switchcraft_35RAPC4BVN4.wrl` and Neutrik `NMJ6HFD2-AU`), **J2** `Kycon_KMDGX-9S-AS`, **U130** `TO-92_Inline` (MCP130, 3D pose from `retr01_patch_3d_poses.py`), **U2075** `SOP-24_7.5x15.4mm_P1.27mm`. **J11** is the NTSC/PAL 2x2. Supplier STEP sources for WRL regeneration are under `Retr01_Lib.3dshapes/_step_source/`. Colored WRLs use `scripts/step_colored_to_wrl.py` when needed.

### Silkscreen text (refdes + value)

Stock KiCad DIP footprints draw **Reference on F.SilkS** and a second **${REFERENCE} on F.Fab** (assembly drawing). With **F.Fab** visible in Pcbnew that looks like duplicate **U3** labels, but only **F.SilkS** is printed on the physical board from normal gerbers.

For this flow, `scripts/retr01_trim_silk_footprints.py` (run from `export_netlist.sh`) copies THT footprints into **`Retr01_Lib.pretty`**: removes the fab **${REFERENCE}** copy and moves **Value** to **F.SilkS**. Netlist import then shows **refdes + BOM value** (e.g. **U3** / **AS6C62256**, **R1** / **100nF**), not the KiCad footprint filename.

**Fab houses (PCBWay, etc.):** assembly uses the **BOM + centroid** from the fab upload bundle, not silkscreen part numbers. Silk **refdes** helps hand assembly, while silk **values** are optional (many production boards omit passive values). Silk does not need to duplicate BOM fields for PCBWay-style flows.

---

## Fabrication-ready criteria

A fab-ready netlist waits on:

1. CXA2075M pinout + MCP130 in sim/schematic links  
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
