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
            if ref in ("?", "SCR1", "5V", "GND"):
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


_KICAD_PINTYPE = {
    "POWER-IN": "power_in",
    "POWER-OUT": "power_out",
    "UNSPECIFIED": "unspecified",
    "INPUT": "input",
    "OUTPUT": "output",
    "BIDIRECTIONAL": "bidirectional",
    "TRISTATE": "tri_state",
    "PASSIVE": "passive",
    "NO-CONNECT": "no_connect",
}


def _kicad_pin_name_and_type(pin) -> tuple[str, str]:
    num = str(getattr(pin, "num", "") or "")
    name = str(getattr(pin, "name", "") or num)
    func = "UNSPECIFIED"
    getter = getattr(pin, "get_pin_info", None)
    if callable(getter):
        _, _, func = getter()
    return name, _KICAD_PINTYPE.get(func, "unspecified")


def annotate_netlist_pinfunctions(text: str, parts: dict) -> str:
    """Write datasheet pin names and KiCad pintypes onto netlist nodes.

    Skidl emits pad numbers only. Without pinfunction, PRE#/CLR# on +5V look
    like extra VCC pads. Package VCC/GND stay power_in. Pull-ups stay signal.
    """
    import re

    info: dict[tuple[str, str], tuple[str, str]] = {}
    for ref, part in parts.items():
        for pin in getattr(part, "pins", []) or []:
            num = str(getattr(pin, "num", ""))
            info[(str(ref), num)] = _kicad_pin_name_and_type(pin)

    def repl(m: re.Match) -> str:
        ref = m.group(1)
        pin = m.group(2)
        hit = info.get((ref, pin))
        if not hit:
            return m.group(0)
        name, ptype = hit
        name = name.replace("\\", "\\\\").replace('"', '\\"')
        return f'(node (ref "{ref}") (pin "{pin}") (pinfunction "{name}") (pintype "{ptype}"))'

    return re.sub(
        r'\(node\s+\(ref\s+"([^"]+)"\)\s+\(pin\s+"([^"]+)"\)(?:\s+\(pinfunction\s+"[^"]*"\))?\s+\(pintype\s+"[^"]*"\)\)',
        repl,
        text,
    )


def _netlist_pad_info(netlist_text: str) -> dict[tuple[str, str], tuple[str, str, str]]:
    """Map (ref, pin) to (net name, pinfunction, pintype)."""
    import re

    info: dict[tuple[str, str], tuple[str, str, str]] = {}
    pos = 0
    net_hdr = re.compile(r'\(net\s*\n\s*\(code\s+\d+\)\s*\n\s*\(name\s+"([^"]+)"\)')
    while True:
        m = net_hdr.search(netlist_text, pos)
        if not m:
            break
        netname = m.group(1)
        start = m.end()
        nxt = net_hdr.search(netlist_text, start)
        chunk = netlist_text[start : nxt.start() if nxt else len(netlist_text)]
        pos = nxt.start() if nxt else len(netlist_text)
        for n in re.finditer(
            r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)(?: \(pinfunction "([^"]+)"\))? \(pintype "([^"]+)"\)\)',
            chunk,
        ):
            info[(n.group(1), n.group(2))] = (netname, n.group(3) or "", n.group(4))
    return info


def _pcb_cu_line(x1: float, y1: float, x2: float, y2: float, w: float = 0.3) -> str:
    return (
        f"\n\t\t(fp_line\n"
        f"\t\t\t(start {x1} {y1})\n"
        f"\t\t\t(end {x2} {y2})\n"
        f"\t\t\t(stroke\n"
        f"\t\t\t\t(width {w})\n"
        f"\t\t\t\t(type solid)\n"
        f"\t\t\t)\n"
        f"\t\t\t(layer \"F.Cu\")\n"
        f"\t\t)"
    )


_CLOCK_STRAPS = {
    "U74": {
        "fp": "Retr01_Lib:DIP-14_W7.62mm_74HC74",
        "groups": "1, 4, 10, 13, 14",
        "lines": (
            (0, 0, 7.62, 0),
            (0, 7.62, 2.2, 7.62),
            (2.2, 7.62, 2.2, 0),
            (7.62, 2.54, 5.4, 2.54),
            (7.62, 10.16, 5.4, 10.16),
            (5.4, 10.16, 5.4, 0),
        ),
    },
    "U04": {
        "fp": "Retr01_Lib:DIP-14_W7.62mm_74HCU04",
        "groups": "7, 11, 13",
        "lines": (
            (0, 15.24, 2.2, 15.24),
            (2.2, 15.24, 2.2, 11.43),
            (2.2, 11.43, 5.4, 11.43),
            (5.4, 11.43, 5.4, 2.54),
            (5.4, 7.62, 7.62, 7.62),
            (5.4, 2.54, 7.62, 2.54),
        ),
    },
}


def patch_pcb_clock_straps(pcb_text: str) -> str:
    """Swap U74/U04 onto strapped DIP footprints and add net-tie copper."""
    import re

    footprint_starts = [m.start() for m in re.finditer(r"\n\t\(footprint\s+", pcb_text)]
    if not footprint_starts:
        return pcb_text
    parts: list[str] = []
    cursor = 0
    for i, start in enumerate(footprint_starts):
        end = footprint_starts[i + 1] if i + 1 < len(footprint_starts) else len(pcb_text)
        chunk = pcb_text[start:end]
        ref_m = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', chunk)
        spec = _CLOCK_STRAPS.get(ref_m.group(1) if ref_m else "")
        if not spec:
            parts.append(pcb_text[cursor:end])
            cursor = end
            continue
        chunk = re.sub(
            r'\(footprint\s+"Retr01_Lib:DIP-14_W7\.62mm[^"]*"',
            f'(footprint "{spec["fp"]}"',
            chunk,
            count=1,
        )
        if "net_tie_pad_groups" not in chunk:
            chunk = chunk.replace(
                "(duplicate_pad_numbers_are_jumpers no)",
                f'(net_tie_pad_groups "{spec["groups"]}")\n\t\t(duplicate_pad_numbers_are_jumpers no)',
                1,
            )
        if "(start 2.2 " not in chunk:
            copper = "".join(_pcb_cu_line(*xy) for xy in spec["lines"])
            pad_at = re.search(r"\n\t\t\(pad\s+", chunk)
            if pad_at:
                chunk = chunk[: pad_at.start()] + copper + chunk[pad_at.start() :]
        parts.append(pcb_text[cursor:start])
        parts.append(chunk)
        cursor = end
    parts.append(pcb_text[cursor:])
    return "".join(parts)


def annotate_pcb_pinfunctions(pcb_text: str, netlist_text: str) -> str:
    """Copy netlist nets, pinfunction, and pintype onto matching footprint pads."""
    import re

    info = _netlist_pad_info(netlist_text)
    if not info:
        return pcb_text

    footprint_starts = [m.start() for m in re.finditer(r"\n\t\(footprint\s+", pcb_text)]
    if not footprint_starts:
        return pcb_text
    parts: list[str] = []
    cursor = 0
    for i, start in enumerate(footprint_starts):
        end = footprint_starts[i + 1] if i + 1 < len(footprint_starts) else len(pcb_text)
        chunk = pcb_text[start:end]
        ref_m = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', chunk)
        if not ref_m:
            parts.append(pcb_text[cursor:end])
            cursor = end
            continue
        ref = ref_m.group(1)
        pad_starts = [m.start() for m in re.finditer(r'\n\t\t\(pad\s+"', chunk)]
        if not pad_starts:
            parts.append(pcb_text[cursor:end])
            cursor = end
            continue
        new_chunk = chunk[: pad_starts[0]]
        for j, pstart in enumerate(pad_starts):
            pend = pad_starts[j + 1] if j + 1 < len(pad_starts) else len(chunk)
            block = chunk[pstart:pend]
            pin_m = re.match(r'\n\t\t\(pad\s+"([^"]+)"', block)
            pin = pin_m.group(1) if pin_m else ""
            hit = info.get((ref, pin))
            if hit:
                netname, name, ptype = hit
                netname = netname.replace("\\", "\\\\").replace('"', '\\"')
                name = name.replace("\\", "\\\\").replace('"', '\\"')
                block = re.sub(r'\(net\s+"[^"]+"\)', f'(net "{netname}")', block, count=1)
                if name:
                    replacement = f'(pinfunction "{name}")\n\t\t\t(pintype "{ptype}")'
                    if re.search(r'\(pinfunction\s+"[^"]+"\)', block):
                        block = re.sub(
                            r'\(pinfunction\s+"[^"]+"\)\s*\n\s*\(pintype\s+"[^"]+"\)',
                            replacement,
                            block,
                            count=1,
                        )
                    else:
                        block = re.sub(r'\(pintype\s+"[^"]+"\)', replacement, block, count=1)
            new_chunk += block
        parts.append(pcb_text[cursor:start])
        parts.append(new_chunk)
        cursor = end
    parts.append(pcb_text[cursor:])
    return "".join(parts)


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
    from retr01_kicad.clocks import ensure_clock_parts, wire_clocks
    from retr01_kicad.connectors import ensure_connector_parts, wire_connectors
    from retr01_kicad.layer4_nets import (
        annotate_netlist_layer4_class,
        apply_layer4_netclass,
    )
    from retr01_kicad.mobo_scope import normalize_export_node
    from retr01_kicad.net_names import name_all_nets
    from retr01_kicad.power_rails import restore_rail_net_names, wire_power_rails
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

    part_factory = lambda r: part_for_refdes(r, ref_index.get(r) or {"max_num": 0, "parts": set()})
    ensure_connector_parts(parts, part_factory)
    ensure_clock_parts(parts, part_factory)
    ensure_part("U725")
    ensure_part("U130")
    wire_connectors(parts, nets_map, pin_connect)
    wire_clocks(parts, nets_map, pin_connect)
    wire_power_rails(parts, nets_map, pin_connect)

    if "U130" in parts:
        res_net = nets_map.get("CPU_RES#")
        if res_net is None:
            for sk_net in nets_map.values():
                for p in getattr(sk_net, "pins", []):
                    if getattr(getattr(p, "part", None), "ref", None) == "U1" and str(getattr(p, "num", "")) == "40":
                        res_net = sk_net
                        break
        if res_net is not None:
            pin_connect(parts["U130"], "", "1", res_net)
        pin_connect(parts["U130"], "", "2", nets_map["+5V"])
        pin_connect(parts["U130"], "", "3", nets_map["GND"])

    stub_unconnected_pins(parts, nets_map, pin_connect)
    restore_rail_net_names(parts, nets_map)

    # Name all nets with clean, descriptive architectural labels
    unique_nets = set()
    for part in parts.values():
        for pin in getattr(part, "pins", []):
            if pin.net is not None:
                unique_nets.add(pin.net)
    name_all_nets(unique_nets)
    restore_rail_net_names(parts, nets_map)

    if "J1" in parts:
        for p in getattr(parts["J1"], "pins", []):
            if str(p.num) in ("2", "3") and p.net is not None:
                p.net.name = "GND"
            elif str(p.num) == "1" and p.net is not None:
                p.net.name = "+5V"

    layer4_names = apply_layer4_netclass(unique_nets)

    buf = StringIO()
    generate_netlist(file_=buf)
    # Skidl 2.3 sometimes emits the +5V rail as NET_NNN despite net.name="+5V".
    out = annotate_netlist_pinfunctions(buf.getvalue(), parts)
    out = annotate_netlist_layer4_class(out, layer4_names)
    import re as _re

    out = _re.sub(r'\(name "\+5V\d+"\)', '(name "+5V")', out)
    out = _re.sub(r'\(name "GND\d+"\)', '(name "GND")', out)
    for m in _re.finditer(r'\(name "(NET_\d+)"\)', out):
        name = m.group(1)
        start = m.end()
        nxt = _re.search(r'\(name "', out[start:])
        chunk = out[start : start + (nxt.start() if nxt else 8000)]
        if _re.search(r'\(ref "U1"\) \(pin "8"\)', chunk):
            out = out.replace(f'(name "{name}")', '(name "+5V")', 1)
            break
    return out


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
    print(f"wrote {out_path} ({len(netlist_text)} bytes) - PRELIMINARY / NOT FAB-READY", file=sys.stderr)

    pcb_dir = REPO_ROOT / "apps/sim/tier-h/kicad/main-pcb/v_01"
    if str(TIER_H_SKIDL_DIR) not in sys.path:
        sys.path.insert(0, str(TIER_H_SKIDL_DIR))
    from retr01_kicad.layer4_nets import layer4_net_names_from_netlist, patch_kicad_pro

    layer4_from_net = layer4_net_names_from_netlist(netlist_text)
    for pcb_path in sorted(pcb_dir.glob("v_0*.kicad_pcb")):
        old = pcb_path.read_text(encoding="utf-8")
        new = patch_pcb_clock_straps(old)
        new = annotate_pcb_pinfunctions(new, netlist_text)
        if new != old:
            pcb_path.write_text(new, encoding="utf-8")
            print(f"updated pad names in {pcb_path.relative_to(REPO_ROOT)}", file=sys.stderr)
    for pro_path in sorted(pcb_dir.glob("v_0*.kicad_pro")):
        old = pro_path.read_text(encoding="utf-8")
        new = patch_kicad_pro(old, layer4_from_net)
        if new != old:
            pro_path.write_text(new, encoding="utf-8")
            print(f"updated Layer4 net class in {pro_path.relative_to(REPO_ROOT)}", file=sys.stderr)


if __name__ == "__main__":
    main()
