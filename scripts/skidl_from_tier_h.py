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


def _retr01_kicad():
    if str(TIER_H_SKIDL_DIR) not in sys.path:
        sys.path.insert(0, str(TIER_H_SKIDL_DIR))
    from retr01_kicad import add_library_paths, make_skidl_part, resolve

    return add_library_paths, make_skidl_part, resolve


def part_for_refdes(ref: str, rec: dict):
    add_library_paths, make_skidl_part, resolve = _retr01_kicad()
    add_library_paths()
    spec = resolve(ref, rec.get("parts") or ())
    if spec is None:
        _die(f"no KiCad part mapping for refdes {ref!r} (sim parts: {sorted(rec.get('parts') or ())})")
    mpn, footprint = spec
    return make_skidl_part(mpn, ref, footprint)


def find_pin(part, pin_name: str | None, pin_num: int | str):
    sn = str(pin_num)
    for pin in part.pins:
        if str(pin.num) == sn:
            return pin
    if pin_name:
        for pin in part.pins:
            if pin.name == pin_name:
                return pin
            for alias in getattr(pin, "aliases", []) or []:
                if alias == pin_name:
                    return pin
    return None


def pin_connect(part, pin_name: str, pin_num: int | str, net) -> None:
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

    if str(TIER_H_SKIDL_DIR) not in sys.path:
        sys.path.insert(0, str(TIER_H_SKIDL_DIR))
    from retr01_kicad.connectors import ensure_connector_parts, wire_connectors
    from retr01_kicad.mobo_scope import normalize_export_node
    from retr01_kicad.stub_pins import stub_unconnected_pins

    configure_skidl_logging(quiet)
    skidl.reset()

    ref_index = index_refs(data)
    parts: dict[str, object] = {}
    nets_map: dict[str, Net] = {}

    def ensure_part(ref: str) -> None:
        if ref not in parts:
            rec = ref_index.get(ref) or {"max_num": 0, "parts": set()}
            parts[ref] = part_for_refdes(ref, rec)

    for net_entry in data.get("nets") or []:
        name = net_entry.get("name") or "NET"
        if name not in nets_map:
            nets_map[name] = Net(name)
        sk_net = nets_map[name]
        for node in net_entry.get("nodes") or []:
            ref = node.get("ref") or "?"
            pin_name = str(node.get("pin") or "")
            pin_num = int(node.get("num") or 1)
            mapped = normalize_export_node(ref, pin_name, pin_num)
            if mapped is None:
                continue
            ref, pin_name, pin_num = mapped
            ensure_part(ref)
            pin_connect(parts[ref], pin_name, pin_num, sk_net)

    ensure_connector_parts(parts, lambda r: part_for_refdes(r, ref_index.get(r) or {"max_num": 0, "parts": set()}))
    ensure_part("U725")
    wire_connectors(parts, nets_map, pin_connect)
    stub_unconnected_pins(parts, nets_map, pin_connect)

    buf = StringIO()
    generate_netlist(file_=buf)
    return buf.getvalue()


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
