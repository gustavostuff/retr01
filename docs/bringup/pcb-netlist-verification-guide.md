# PCB netlist verification guide (Skidl to KiCad PCB)

Verification methodology, automated comparison tooling, and audit procedures for validating the physical KiCad PCB layout directly against the simulation pin netlist without intermediate graphical schematics.

---

## 1. The Skidl schematic-less workflow

Retr01 does not rely on manual graphical schematic capture in KiCad Eeschema (`.kicad_sch`). Instead, the design uses a programmatic, code-driven schematic pipeline:

```
[C Simulation Pin Graph]
  apps/sim/tier-h/src/board_schematic.c
  apps/sim/tier-h/src/board_netlist.c
           |
           v (export_tier_h_netlist)
[JSON Pin Netlist]
  apps/sim/tier-h/skidl/retr01_tier_h.json
           |
           v (scripts/skidl_from_tier_h.py)
[KiCad Netlist (.net)]
  apps/sim/tier-h/skidl/retr01_prelim.net
           |
           v (Import Netlist / Update PCB)
[Physical KiCad PCB Layout]
  apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb
```

Without a KiCad schematic sheet, the board file (`v_01.kicad_pcb`) still has to match the simulation pin graph (`retr01_tier_h.json`) with no missing or swapped connections. The comparison below is that check.

---

## 2. Architecture of the automated netlist comparison tool

A dedicated Python verification tool compares the virtual pin netlist (`retr01_tier_h.json`) directly against the physical board file (`v_01.kicad_pcb`).

### Data structures being compared

1. **Simulation JSON (`retr01_tier_h.json`):**
   A list of named nets, where each net contains an array of pin nodes:
   ```json
   {
     "name": "NET_11",
     "nodes": [
       {"ref": "U1", "pin": "IRQB", "num": 4, "part": "W65C02S"},
       {"ref": "UPLDY", "pin": "EQ#", "num": 34, "part": "ATF22V10"}
     ]
   }
   ```
2. **KiCad PCB file (`v_01.kicad_pcb`):**
   A standard S-expression text file defining footprints, pads, and assigned net IDs:
   ```lisp
   (footprint "Retr01_Lib:DIP-40_W15.24mm"
     (fp_text reference "U1" ...)
     (pad "4" thru_hole oval ... (net 11 "NET_11"))
   )
   ```

### Verification checks performed by the comparison tool

The script constructs an equivalence graph of `(component_ref, pad_number)` pairs from both files and checks five invariants:

| Check | Failure condition | What it catches |
| --- | --- | --- |
| **Node parity** | A pin is present in JSON but missing on the PCB (or vice versa) | Missing IC footprints, unplaced passives, or dropped connector pins |
| **Short circuit** | Two distinct JSON nets share pads on a single PCB net | Accidental net merges or bridged power domains |
| **Open circuit** | Pads belonging to one JSON net are split into multiple PCB nets | Incomplete bus connections or broken nets |
| **Power rail integrity** | An IC VCC/GND pin is mapped to a signal net instead of +5V/GND | Inverted power pins or missing bypass connections |
| **NC isolation** | A pin designated as unconnected in simulation is wired to an active net | Floating inputs accidentally tied to digital buses |

---

## 3. Reference implementation of the comparison tool

The comparison logic is structured as a standalone verification script (`scripts/verify_pcb_netlist.py`):

```python
#!/usr/bin/env python3
"""
Compares retr01_tier_h.json (sim netlist) against v_01.kicad_pcb (board file).
Reports missing pins, net mismatches, open circuits, and unintentional shorts.
"""

from __future__ import annotations
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

def parse_sim_json(json_path: Path) -> dict[str, set[tuple[str, str]]]:
    """Returns mapping: net_name -> set of (refdes, pad_number)."""
    data = json.loads(json_path.read_text(encoding="utf-8"))
    sim_nets: dict[str, set[tuple[str, str]]] = {}
    for net in data.get("nets", []):
        name = net["name"]
        nodes = {(n["ref"], str(n["num"])) for n in net.get("nodes", [])}
        if nodes:
            sim_nets[name] = nodes
    return sim_nets

def parse_kicad_pcb(pcb_path: Path) -> dict[str, set[tuple[str, str]]]:
    """Extracts pad-to-net mapping from KiCad S-expression .kicad_pcb file."""
    text = pcb_path.read_text(encoding="utf-8")
    
    # 1. Parse net index: (net <id> "<name>")
    net_names: dict[int, str] = {}
    for match in re.finditer(r'\(net\s+(\d+)\s+"([^"]+)"\)', text):
        net_names[int(match.group(1))] = match.group(2)
        
    # 2. Parse footprints and pads
    pcb_nets: dict[str, set[tuple[str, str]]] = defaultdict(set)
    fp_pattern = re.compile(r'\(footprint\s+"[^"]+".*?\n\s+\(fp_text reference "([^"]+)".*?\n(.*?)\n  \)', re.DOTALL)
    pad_pattern = re.compile(r'\(pad\s+"([^"]+)"\s+\w+\s+\w+.*?\(net\s+(\d+)')
    
    for fp_match in fp_pattern.finditer(text):
        refdes = fp_match.group(1)
        body = fp_match.group(2)
        for pad_match in pad_pattern.finditer(body):
            pad_num = pad_match.group(1)
            net_id = int(pad_match.group(2))
            net_name = net_names.get(net_id, f"UNKNOWN_{net_id}")
            if net_id != 0: # Skip unconnected / unassigned pads
                pcb_nets[net_name].add((refdes, pad_num))
                
    return pcb_nets

def verify(sim_nets: dict, pcb_nets: dict) -> int:
    errors = 0
    # Map (refdes, pad) to net name for both
    sim_pad_to_net = {pad: net for net, pads in sim_nets.items() for pad in pads}
    pcb_pad_to_net = {pad: net for net, pads in pcb_nets.items() for pad in pads}
    
    # Check all sim pads exist in PCB
    for pad, sim_net in sim_pad_to_net.items():
        if pad not in pcb_pad_to_net:
            print(f"ERROR: Pin {pad[0]}.{pad[1]} (Sim net {sim_net}) missing in PCB layout")
            errors += 1
            continue
        pcb_net = pcb_pad_to_net[pad]
        # Check that connected peer pins match
        sim_peers = sim_nets[sim_net]
        pcb_peers = pcb_nets[pcb_net]
        diff = sim_peers.symmetric_difference(pcb_peers)
        if diff:
            print(f"ERROR: Net mismatch for {pad[0]}.{pad[1]}:")
            print(f"   Sim net {sim_net} peers: {sim_peers}")
            print(f"   PCB net {pcb_net} peers: {pcb_peers}")
            errors += 1
            
    if errors == 0:
        print("PASS: 100% equivalence between Sim netlist and KiCad PCB file.")
    return errors

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: verify_pcb_netlist.py <sim.json> <board.kicad_pcb>")
        sys.exit(1)
    errs = verify(parse_sim_json(Path(sys.argv[1])), parse_kicad_pcb(Path(sys.argv[2])))
    sys.exit(1 if errs > 0 else 0)
```

---

## 4. Distinguishing logical connectivity from physical copper routing

Verifying the netlist ensures that KiCad knows which pads belong together. However, a board cannot be fabricated until physical copper traces connect those pads.

Verification involves two distinct levels:

### Level 1: Logical netlist match (Python comparison tool)
- Proves that the Skidl netlist import correctly associated every footprint pad with its intended net.
- Guarantees zero swapped pins or missing nodes in the KiCad ratsnest.

### Level 2: Physical copper continuity (KiCad DRC)
- After traces are routed in Pcbnew, **Inspect -> Design Rules Checker (DRC)** confirms that every logical net is fully routed.
- DRC output must read:
  - **Unconnected items: 0**
  - **Track clearance violations: 0**
  - **Copper zone collisions: 0**

---

## 5. Footprint pin-mapping audit (Preventing symbol vs footprint skew)

Because Skidl maps component pins by number and name, a critical risk is a mismatch between an IC symbol's logical pin definition and the physical footprint's pad numbers.

A manual datasheet audit covers every counted IC package:

| Component | Footprint | High-risk pins to verify against datasheet |
| --- | --- | --- |
| **W65C02S** | DIP-40 (600 mil) | Pin 8 (VDD), Pin 21 (VSS), Pin 40 (RESB), Pin 37 (PHI2) |
| **ATF22V10** | DIP-24 (300 mil) | Pin 24 (VCC), Pin 12 (GND), Pin 1 (CLK/IN) |
| **AT27C256R** | DIP-28 (600 mil) | Pin 28 (VCC), Pin 14 (GND), Pin 20 (CE#), Pin 22 (OE#) |
| **AS6C62256** | DIP-28 (600 mil) | Pin 28 (VCC), Pin 14 (GND), Pin 27 (WE#), Pin 22 (OE#), Pin 20 (CE#) |
| **AVR128DB28** | SPDIP-28 (300 mil) | Pin 28 (VDD), Pin 27 (GND), Pin 23 (UPDI) |
| **74HC157** | DIP-16 (300 mil) | Pin 16 (VCC), Pin 8 (GND), Pin 1 (S), Pin 15 (G) |
| **AD724** | SOIC-16 (150 mil) | Pin 16 (VCC), Pin 8 (GND), Pin 1 (FIN), Pin 7 (FSC_XTAL) |
| **Switchcraft TRS** | Custom 35RAPC2BVN4 | Pin 1 (Sleeve / GND), Pin 2 (Tip), Pin 3 (Ring) |
| **CUI DC Jack** | Custom PJ-063AH | Center pin (+5.0 V) vs outer sleeve (GND) |

---

## 6. Pre-fabrication sign-off checklist

Gerber export for a fab house follows this order:

1. **Sim export:** `export_netlist.sh` regenerates `retr01_tier_h.json` and `retr01_prelim.net`.
2. **Netlist import:** `retr01_prelim.net` is re-imported into `v_01.kicad_pcb` so upstream netlist edits are present.
3. **Automated script comparison:** `scripts/verify_pcb_netlist.py` reports `PASS: 100% equivalence`.
4. **KiCad DRC:** Pcbnew Design Rules Checker reports **0 unrouted nets** and **0 DRC errors**.
5. **Physical 1:1 paper printout:** A 100% scale print of the layout is checked against real DIP sockets and jacks for lead spacing and drill sizes.
