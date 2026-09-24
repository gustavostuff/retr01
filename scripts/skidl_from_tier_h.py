#!/usr/bin/env python3
"""
Build a preliminary KiCad netlist from Tier H JSON (Skidl).

IMPORTANT: Output is for illustration and early PCB exploration only.
The Retr01 motherboard PCB is NOT ready for fabrication from this flow.
See docs/bringup/tier-h-skidl-export.md.

Usage:
  ./apps/sim/tier-h/build/export_tier_h_netlist > /tmp/retr01.json
  ./scripts/skidl_from_tier_h.py /tmp/retr01.json -o retr01_prelim.net

Requires: pip install skidl
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _die(msg: str) -> None:
    print(msg, file=sys.stderr)
    sys.exit(1)


def load_json(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    meta = data.get("meta") or {}
    if not meta.get("purpose", "").startswith("preliminary"):
        print("warning: JSON missing preliminary_pcb meta; treat output as non-fab.", file=sys.stderr)
    if meta.get("fabrication_ready"):
        _die("refusing: JSON claims fabrication_ready (expected false)")
    return data


def part_for_refdes(ref: str, part_hint: str | None):
    from skidl import Part

    if ref == "PS1":
        return Part("power", "PWR_FLAG", ref=ref, footprint="")
    if ref.startswith("R"):
        return Part("Device", "R", ref=ref, value="?", footprint="")
    if ref.startswith("C"):
        return Part("Device", "C", ref=ref, value="?", footprint="")
    if ref.startswith("E"):
        return Part("Device", "CP", ref=ref, value="?", footprint="")
    if ref.startswith("Y"):
        return Part("Device", "Crystal", ref=ref, value="?", footprint="")

    # ICs / PLDs / sim-only blocks: generic connector placeholder (wrong symbol, right connectivity).
    pin_count = 40
    if part_hint and "128" in part_hint:
        pin_count = 28
    if ref in ("U40", "U50"):
        pin_count = 32 if ref == "U40" else 8
    lib = "Connector_Generic"
    sym = f"Conn_01x{pin_count:02d}"
    try:
        return Part(lib, sym, ref=ref, footprint="")
    except Exception:
        return Part("Connector", "Conn_01x40", ref=ref, footprint="")


def pin_connect(part, pin_name: str, pin_num: int, net):
    try:
        if pin_name and pin_name[0].isalpha():
            part[pin_name] += net
            return
    except Exception:
        pass
    try:
        part[pin_num] += net
    except Exception:
        part[str(pin_num)] += net


def build_skidl(data: dict) -> str:
    from io import StringIO

    import skidl
    from skidl import Net, generate_netlist

    skidl.reset()
    parts: dict[str, object] = {}
    nets_map: dict[str, Net] = {}

    for net_entry in data.get("nets") or []:
        name = net_entry.get("name") or "NET"
        if name not in nets_map:
            nets_map[name] = Net(name)
        sk_net = nets_map[name]
        for node in net_entry.get("nodes") or []:
            ref = node.get("ref") or "?"
            if ref == "?" or ref == "SCR1":
                # LCD sink is sim-only; skip until AD724 schematic part exists.
                continue
            part_hint = node.get("part")
            if ref not in parts:
                parts[ref] = part_for_refdes(ref, part_hint)
            pin_connect(parts[ref], node.get("pin") or "1", int(node.get("num") or 1), sk_net)

    buf = StringIO()
    generate_netlist(file_=buf)
    return buf.getvalue()


def main() -> None:
    ap = argparse.ArgumentParser(description="Skidl netlist from Tier H JSON (illustrative only)")
    ap.add_argument("json_file", type=Path, help="export_tier_h_netlist JSON")
    ap.add_argument("-o", "--output", type=Path, default=Path("retr01_prelim.net"), help="KiCad netlist path")
    args = ap.parse_args()

    if not args.json_file.is_file():
        _die(f"no such file: {args.json_file}")

    try:
        import skidl  # noqa: F401
    except ImportError:
        _die("skidl not installed; run: pip install skidl")

    data = load_json(args.json_file)
    netlist_text = build_skidl(data)
    args.output.write_text(netlist_text, encoding="utf-8")
    print(f"wrote {args.output} ({len(netlist_text)} bytes) — PRELIMINARY / NOT FAB-READY", file=sys.stderr)


if __name__ == "__main__":
    main()
