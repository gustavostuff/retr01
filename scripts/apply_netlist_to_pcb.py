#!/usr/bin/env python3
"""Apply pad nets from a KiCad sexpr netlist onto a .kicad_pcb (placement preserved)."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def parse_netlist(path: Path) -> dict[tuple[str, str], str]:
    text = path.read_text()
    m = re.search(r"\(nets\b", text)
    if not m:
        raise SystemExit(f"no (nets ...) in {path}")
    body = text[m.start() :]
    pad_net: dict[tuple[str, str], str] = {}
    for nm in re.finditer(r'\(net\s+\(code\s+\d+\)\s+\(name\s+"([^"]*)"\)', body):
        name = nm.group(1)
        start = nm.end()
        nxt = re.search(r"\n\s*\(net\s+\(code|\n\s*\)\s*$", body[start:])
        chunk = body[start : start + (nxt.start() if nxt else 8000)]
        for ref, pin in re.findall(
            r'\(node\s+\(ref\s+"?([^"\s)]+)"?\)\s+\(pin\s+"?([^"\s)]+)"?\)',
            chunk,
        ):
            pad_net[(ref, pin)] = name
    return pad_net


def _sexpr_end(text: str, start: int) -> int:
    depth = 0
    for j in range(start, len(text)):
        ch = text[j]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return j + 1
    raise ValueError(f"unclosed sexpr at {start}")


def patch_footprint(fp: str, pad_net: dict[tuple[str, str], str], stats: dict) -> str:
    rm = re.search(r'\(property "Reference" "([^"]+)"', fp)
    if not rm:
        return fp
    ref = rm.group(1)
    out: list[str] = []
    pos = 0
    for pm in re.finditer(r'\(pad "([^"]+)"', fp):
        pad_num = pm.group(1)
        pad_start = pm.start()
        out.append(fp[pos:pad_start])
        pad_end = _sexpr_end(fp, pad_start)
        pad_sexpr = fp[pad_start:pad_end]
        key = (ref, pad_num)
        if key in pad_net:
            new_name = pad_net[key]
            nm = re.search(r'\(net "([^"]*)"\)', pad_sexpr)
            if nm:
                if nm.group(1) != new_name:
                    pad_sexpr = (
                        pad_sexpr[: nm.start()]
                        + f'(net "{new_name}")'
                        + pad_sexpr[nm.end() :]
                    )
                    stats["changed"] += 1
                    stats["refs"].add(ref)
            else:
                pad_sexpr = pad_sexpr[:-1] + f'\n\t\t\t(net "{new_name}")\n\t\t)'
                stats["changed"] += 1
                stats["refs"].add(ref)
        else:
            stats["missing"] += 1
        out.append(pad_sexpr)
        pos = pad_end
    out.append(fp[pos:])
    return "".join(out)


def apply(pcb_path: Path, pad_net: dict[tuple[str, str], str]) -> dict:
    pcb = pcb_path.read_text()
    starts = [m.start() for m in re.finditer(r"\n\t\(footprint\b", pcb)]
    if not starts:
        raise SystemExit(f"no footprints in {pcb_path}")
    stats = {"changed": 0, "missing": 0, "refs": set()}
    out = pcb[: starts[0]]
    for i, s in enumerate(starts):
        e = starts[i + 1] if i + 1 < len(starts) else None
        if e is None:
            block = pcb[s + 1 :]  # "(footprint..."
            end_i = _sexpr_end(block, 0)
            fp = patch_footprint(block[:end_i], pad_net, stats)
            out += "\n\t" + fp + block[end_i:]
        else:
            raw = pcb[s + 1 : e]  # "\t(footprint..."
            fp = patch_footprint(raw[1:], pad_net, stats)
            out += "\n\t" + fp
    pcb_path.write_text(out)
    return stats


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("netlist", type=Path)
    ap.add_argument("pcb", type=Path)
    args = ap.parse_args()
    pad_net = parse_netlist(args.netlist)
    stats = apply(args.pcb, pad_net)
    print(
        f"updated {stats['changed']} pads across {len(stats['refs'])} refs; "
        f"{stats['missing']} pads had no netlist entry",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
