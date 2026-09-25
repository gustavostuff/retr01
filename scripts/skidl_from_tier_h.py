#!/usr/bin/env python3
"""
Preliminary KiCad netlist from Tier H JSON (Skidl).

Output is for illustration and early PCB exploration only.
The Retr01 motherboard PCB is not ready for fabrication from this flow.
See docs/bringup/tier-h-skidl-export.md.

Typical invocation:
  apps/sim/tier-h/skidl/export_netlist.sh -q

Or manually:
  ./apps/sim/tier-h/build/export_tier_h_netlist > apps/sim/tier-h/skidl/retr01_tier_h.json
  ./scripts/skidl_from_tier_h.py -q

Requires: pip install skidl, KiCad symbol libraries (see ensure_kicad_symbol_dir).
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
TIER_H_SKIDL_DIR = REPO_ROOT / "apps/sim/tier-h/skidl"
DEFAULT_JSON = TIER_H_SKIDL_DIR / "retr01_tier_h.json"
DEFAULT_NET = TIER_H_SKIDL_DIR / "retr01_prelim.net"


def _die(msg: str) -> None:
    print(msg, file=sys.stderr)
    sys.exit(1)


def ensure_kicad_symbol_dir() -> None:
    """Skidl reads KICAD*_SYMBOL_DIR on first import; call before importing skidl."""
    import os

    for key in (
        "KICAD10_SYMBOL_DIR",
        "KICAD9_SYMBOL_DIR",
        "KICAD8_SYMBOL_DIR",
        "KICAD7_SYMBOL_DIR",
        "KICAD6_SYMBOL_DIR",
        "KICAD_SYMBOL_DIR",
    ):
        if os.environ.get(key):
            return
    default = Path("/usr/share/kicad/symbols")
    if default.is_dir():
        os.environ["KICAD10_SYMBOL_DIR"] = str(default)


def load_json(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    meta = data.get("meta") or {}
    if not meta.get("purpose", "").startswith("preliminary"):
        print("warning: JSON missing preliminary_pcb meta; treat output as non-fab.", file=sys.stderr)
    if meta.get("fabrication_ready"):
        _die("refusing: JSON claims fabrication_ready (expected false)")
    return data


def index_refs(data: dict) -> dict[str, dict]:
    info: dict[str, dict] = {}
    for net_entry in data.get("nets") or []:
        for node in net_entry.get("nodes") or []:
            ref = node.get("ref") or "?"
            if ref in ("?", "SCR1"):
                continue
            rec = info.setdefault(ref, {"max_num": 0, "parts": set()})
            rec["max_num"] = max(rec["max_num"], int(node.get("num") or 0))
            part = node.get("part")
            if part:
                rec["parts"].add(part)
    return info


def connector_count(max_num: int) -> int:
    return max(1, max_num)


def part_for_refdes(ref: str, rec: dict):
    from skidl import Part

    max_num = rec["max_num"]
    parts = rec["parts"]

    if ref == "PS1":
        return Part("Connector_Generic", f"Conn_01x{connector_count(max(max_num, 4)):02d}", ref=ref, footprint=":")

    if ref.startswith("R"):
        return Part("Device", "R", ref=ref, value="?", footprint=":")

    if ref.startswith("C"):
        return Part("Device", "C", ref=ref, value="?", footprint=":")

    if ref.startswith("E"):
        return Part("Device", "C", ref=ref, value="?", footprint=":")

    # Y* are oscillator modules in Tier H (OSC8M / OSC4LEGS), not 2-pin crystals.
    if ref.startswith("Y") or parts & {"OSC8M", "OSC4LEGS"}:
        n = connector_count(max(max_num, 14))
        return Part("Connector_Generic", f"Conn_01x{n:02d}", ref=ref, footprint=":")

    n = connector_count(max_num)
    if ref == "U40":
        n = max(n, 32)
    elif ref == "U50":
        n = max(n, 8)
    elif any("128" in p for p in parts):
        n = max(n, 28)

    sym = f"Conn_01x{n:02d}"
    try:
        return Part("Connector_Generic", sym, ref=ref, footprint=":")
    except Exception:
        return Part("Connector", "Conn_01x40", ref=ref, footprint=":")


def find_pin(part, pin_name: str | None, pin_num: int):
    sn = str(pin_num)
    for pin in part.pins:
        if str(pin.num) == sn:
            return pin
    if pin_name:
        for pin in part.pins:
            if pin.name == pin_name:
                return pin
    return None


def pin_connect(part, pin_name: str, pin_num: int, net) -> None:
    pin = find_pin(part, pin_name or None, pin_num)
    if pin is None:
        return
    pin += net


def configure_skidl_logging(quiet: bool) -> None:
    if not quiet:
        return
    import logging

    from skidl.logger import active_logger, erc_logger, rt_logger

    for logger in (active_logger, rt_logger, erc_logger):
        logger.setLevel(logging.CRITICAL)


def build_skidl(data: dict, *, quiet: bool) -> str:
    from io import StringIO

    import skidl
    from skidl import Net, generate_netlist

    configure_skidl_logging(quiet)
    skidl.reset()

    save_fp = skidl.empty_footprint_handler

    def _noop_fp(part) -> None:
        if not getattr(part, "footprint", ""):
            part.footprint = ":"

    skidl.empty_footprint_handler = _noop_fp

    ref_index = index_refs(data)
    parts: dict[str, object] = {}
    nets_map: dict[str, Net] = {}

    try:
        for net_entry in data.get("nets") or []:
            name = net_entry.get("name") or "NET"
            if name not in nets_map:
                nets_map[name] = Net(name)
            sk_net = nets_map[name]
            for node in net_entry.get("nodes") or []:
                ref = node.get("ref") or "?"
                if ref == "?" or ref == "SCR1":
                    continue
                if ref not in parts:
                    parts[ref] = part_for_refdes(ref, ref_index[ref])
                pin_connect(
                    parts[ref],
                    str(node.get("pin") or ""),
                    int(node.get("num") or 1),
                    sk_net,
                )

        buf = StringIO()
        generate_netlist(file_=buf)
        return buf.getvalue()
    finally:
        skidl.empty_footprint_handler = save_fp


def main() -> None:
    ap = argparse.ArgumentParser(description="Skidl netlist from Tier H JSON (preliminary, not fab-ready)")
    ap.add_argument(
        "json_file",
        nargs="?",
        type=Path,
        default=DEFAULT_JSON,
        help=f"export_tier_h_netlist JSON (default: {DEFAULT_JSON.relative_to(REPO_ROOT)})",
    )
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        default=DEFAULT_NET,
        help=f"KiCad netlist path (default: {DEFAULT_NET.relative_to(REPO_ROOT)})",
    )
    ap.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="suppress Skidl library warnings (footprints, tags, net merges)",
    )
    args = ap.parse_args()

    json_path = args.json_file if args.json_file.is_absolute() else (REPO_ROOT / args.json_file)
    out_path = args.output if args.output.is_absolute() else (REPO_ROOT / args.output)

    if not json_path.is_file():
        _die(f"no such file: {json_path}")

    if importlib.util.find_spec("skidl") is None:
        _die("skidl not installed; run: pip install skidl")

    TIER_H_SKIDL_DIR.mkdir(parents=True, exist_ok=True)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    ensure_kicad_symbol_dir()
    data = load_json(json_path)

    orig_cwd = Path.cwd()
    try:
        os.chdir(TIER_H_SKIDL_DIR)
        netlist_text = build_skidl(data, quiet=args.quiet)
    finally:
        os.chdir(orig_cwd)

    out_path.write_text(netlist_text, encoding="utf-8")
    print(f"wrote {out_path} ({len(netlist_text)} bytes) — PRELIMINARY / NOT FAB-READY", file=sys.stderr)


if __name__ == "__main__":
    main()
