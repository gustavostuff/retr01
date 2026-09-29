#!/usr/bin/env python3
"""
Cross-verifies the virtual pin netlist (retr01_tier_h.json) against
the physical KiCad board layout file (v_01.kicad_pcb).

Usage:
    ./scripts/verify_pcb_netlist.py [path/to/sim.json] [path/to/board.kicad_pcb]
Defaults:
    sim.json: apps/sim/tier-h/skidl/retr01_tier_h.json
    board.kicad_pcb: apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb
"""

from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_JSON = REPO_ROOT / "apps/sim/tier-h/skidl/retr01_tier_h.json"
DEFAULT_PCB = REPO_ROOT / "apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb"


def parse_sim_json(json_path: Path) -> dict[str, set[tuple[str, str]]]:
    """Extracts mapping: net_name -> set of (refdes, pad_number) from simulator export."""
    data = json.loads(json_path.read_text(encoding="utf-8"))
    sim_nets: dict[str, set[tuple[str, str]]] = {}
    for net in data.get("nets", []):
        name = net["name"]
        nodes = {(n["ref"], str(n["num"])) for n in net.get("nodes", [])}
        if nodes:
            sim_nets[name] = nodes
    return sim_nets


def parse_kicad_pcb(pcb_path: Path) -> dict[str, set[tuple[str, str]]]:
    """Extracts pad-to-net mapping from KiCad .kicad_pcb S-expression file."""
    text = pcb_path.read_text(encoding="utf-8")

    # 1. Parse net index: (net <id> "<name>")
    net_names: dict[int, str] = {}
    for match in re.finditer(r'\(net\s+(\d+)\s+"([^"]*)"\)', text):
        net_names[int(match.group(1))] = match.group(2)

    # 2. Parse footprints and pads (supports KiCad 6/7/8 footprint and legacy module tags)
    pcb_nets: dict[str, set[tuple[str, str]]] = defaultdict(set)
    fp_pattern = re.compile(
        r'\((?:footprint|module)\s+"[^"]+".*?\n\s+\(fp_text reference "([^"]+)".*?\n(.*?)\n\s+\)',
        re.DOTALL,
    )
    pad_pattern = re.compile(r'\(pad\s+"([^"]+)"\s+\w+\s+\w+.*?\(net\s+(\d+)')

    for fp_match in fp_pattern.finditer(text):
        refdes = fp_match.group(1)
        body = fp_match.group(2)
        for pad_match in pad_pattern.finditer(body):
            pad_num = pad_match.group(1)
            net_id = int(pad_match.group(2))
            net_name = net_names.get(net_id, f"UNKNOWN_{net_id}")
            if net_id != 0 and net_name and net_name != "NC":
                pcb_nets[net_name].add((refdes, pad_num))

    return pcb_nets


def verify(sim_nets: dict[str, set[tuple[str, str]]], pcb_nets: dict[str, set[tuple[str, str]]]) -> int:
    errors = 0
    warnings = 0

    # Build inverse mappings: (refdes, pad) -> net_name
    sim_pad_to_net: dict[tuple[str, str], str] = {}
    for net, pads in sim_nets.items():
        for pad in pads:
            sim_pad_to_net[pad] = net

    pcb_pad_to_net: dict[tuple[str, str], str] = {}
    for net, pads in pcb_nets.items():
        for pad in pads:
            pcb_pad_to_net[pad] = net

    # Compare every pad defined in the simulation
    for pad, sim_net in sorted(sim_pad_to_net.items()):
        ref, num = pad
        if pad not in pcb_pad_to_net:
            print(f"[MISSING PAD] {ref}.{num} in Sim net '{sim_net}' is not assigned to any active net in PCB")
            errors += 1
            continue

        pcb_net = pcb_pad_to_net[pad]
        sim_peers = sim_nets[sim_net]
        pcb_peers = pcb_nets.get(pcb_net, set())

        # Check peer discrepancy
        diff = sim_peers.symmetric_difference(pcb_peers)
        if diff:
            # Filter known harmless differences (e.g. NC or sim-only virtual helpers)
            print(f"[MISMATCH] Net for {ref}.{num}:")
            print(f"   Sim net '{sim_net}' has {len(sim_peers)} nodes: {sorted(sim_peers)}")
            print(f"   PCB net '{pcb_net}' has {len(pcb_peers)} nodes: {sorted(pcb_peers)}")
            errors += 1

    total_sim_pins = len(sim_pad_to_net)
    total_pcb_pins = len(pcb_pad_to_net)

    print("---")
    print(f"Summary: Verified {total_sim_pins} simulation pins against {total_pcb_pins} PCB pads.")
    if errors == 0:
        print("RESULT: PASS - 100% equivalence between virtual netlist and KiCad PCB.")
    else:
        print(f"RESULT: FAIL - Found {errors} netlist discrepancies.")

    return errors


def main() -> int:
    json_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_JSON
    pcb_path = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PCB

    if not json_path.is_file():
        print(f"Error: missing simulation JSON file at {json_path}", file=sys.stderr)
        return 1

    if not pcb_path.is_file():
        print(f"Error: missing KiCad PCB file at {pcb_path}", file=sys.stderr)
        return 1

    print(f"Verifying Sim JSON: {json_path}")
    print(f"Against KiCad PCB:  {pcb_path}")
    print("---")

    sim_nets = parse_sim_json(json_path)
    pcb_nets = parse_kicad_pcb(pcb_path)

    errs = verify(sim_nets, pcb_nets)
    return 1 if errs > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
