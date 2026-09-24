# Tier H Skidl / KiCad netlist (preliminary only)

**Status:** illustrative export pipeline only. Generated netlists must not be used for fabrication, ordering, or design sign-off. The Retr01 motherboard PCB design is not ready. Tier H sim connectivity matches bring-up in places but stays intentionally incomplete versus [`docs/general/hardware.md`](../general/hardware.md).

The export flow supports rough connectivity review, floorplan discussion, and experimental KiCad import. It is not intended for tape-out.

---

## What gets exported

Source of truth after `r01s_board_netlist_rebuild()`:

- Motherboard IC links (`board_netlist.c`)
- Passive / power / DAC / series / pull-up links (`board_schematic.c`)
- Every registered pin on BOM ICs + 61 passives + cart + pad (as mounted in sim)

Known **gaps** (same as [`schematic-netlist-tier-h.md`](schematic-netlist-tier-h.md)):

- **AD724** not modeled (SCR1 video sink stands in for part of the video path)
- **74HC14** optional block not seated
- Cart **edge** vs **U40** module (OE#/WE# stubs)
- **Y1/Y2** refdes shared between BOM crystals and canned osc chips in sim
- Sim-only PLD helpers may appear if their pins are in the graph

The JSON embeds `"fabrication_ready": false` and `"purpose": "preliminary_pcb_illustrative_only"`.

---

## Step 1: Export JSON (C)

Typical Tier H build:

```bash
cmake -S apps/sim/tier-h -B apps/sim/tier-h/build
cmake --build apps/sim/tier-h/build --target export_tier_h_netlist
./apps/sim/tier-h/build/export_tier_h_netlist > retr01_tier_h.json
```

API: `ns_pin_netlist_write_json()` in `apps/netlist_sim` (Tier H alias `r01s_pin_netlist_write_json`).

Unit test: `test_export_netlist`.

---

## Step 2: Skidl netlist (Python)

```bash
pip install skidl   # optional; only for step 2
./scripts/skidl_from_tier_h.py retr01_tier_h.json -o retr01_prelim.net
```

Script behavior:

- Refuses export if JSON claims `fabrication_ready: true`
- Maps passives to generic `Device` R/C/CP/Crystal symbols
- Maps ICs to **generic connector placeholders** (connectivity only, not correct footprints/symbols)
- Skips **SCR1** (sim LCD sink)

KiCad import of `retr01_prelim.net` is limited to visual experiment. Symbols and footprints require replacement and reconciliation against the schematic source of truth.

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
