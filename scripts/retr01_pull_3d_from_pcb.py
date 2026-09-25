#!/usr/bin/env python3
"""Copy a footprint's embedded (model ...) from .kicad_pcb into footprint_3d.py.

Use after tuning 3D in KiCad (Footprint Properties → 3D Models), save PCB, then:

  python3 scripts/retr01_pull_3d_from_pcb.py J36 \\
    apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb

Re-run export_netlist.sh (or apply + sync) to propagate everywhere.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FOOTPRINT_3D_PY = REPO / "apps/sim/tier-h/skidl/retr01_kicad/footprint_3d.py"

FP_WITH_REF = re.compile(
    r'\(footprint "Retr01_Lib:([^"]+)"[\s\S]*?'
    r'\(property "Reference" "([^"]+)"',
)


def _parse_vec3(block: str, key: str) -> tuple[float, float, float] | None:
    m = re.search(rf"\({key}[\s\S]*?\(xyz\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\)", block)
    if not m:
        return None
    return float(m.group(1)), float(m.group(2)), float(m.group(3))


def find_footprint_by_ref(pcb_text: str, ref: str) -> tuple[str, str] | None:
    for m in FP_WITH_REF.finditer(pcb_text):
        fp_name, refdes = m.group(1), m.group(2)
        if refdes == ref:
            start = m.start()
            # end at next top-level footprint (same indent as opening)
            rest = pcb_text[start:]
            depth = 0
            for i, c in enumerate(rest):
                if c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                    if depth == 0:
                        return fp_name, rest[: i + 1]
    return None


def extract_model(fp_block: str) -> str | None:
    idx = fp_block.find("(model ")
    if idx < 0:
        return None
    depth = 0
    for i, c in enumerate(fp_block[idx:], idx):
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return fp_block[idx : i + 1]
    return None


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("refdes", help="e.g. J36")
    ap.add_argument("pcb", type=Path)
    ap.add_argument(
        "--write",
        action="store_true",
        help="patch footprint_3d.py (default: print snippet only)",
    )
    args = ap.parse_args()
    if not args.pcb.is_file():
        raise SystemExit(f"missing {args.pcb}")
    text = args.pcb.read_text()
    found = find_footprint_by_ref(text, args.refdes)
    if not found:
        raise SystemExit(f"no Retr01_Lib footprint with Reference {args.refdes!r}")
    fp_name, fp_block = found
    model = extract_model(fp_block)
    if not model:
        raise SystemExit(f"no (model ...) on {args.refdes} ({fp_name})")

    path_m = re.search(r'\(model "([^"]+)"', model)
    off = _parse_vec3(model, "offset")
    scale = _parse_vec3(model, "scale")
    rot = _parse_vec3(model, "rotate")
    if not path_m or not off or not scale or not rot:
        raise SystemExit("could not parse model path/offset/scale/rotate")

    snippet = (
        f'    "{fp_name}": FootprintModel3D(\n'
        f'        path="{path_m.group(1)}",\n'
        f"        offset_mm={off!r},\n"
        f"        scale={scale!r},\n"
        f"        rotate_deg={rot!r},\n"
        f"    ),"
    )
    print(f"# from {args.pcb} {args.refdes} → {fp_name}")
    print(snippet)

    if not args.write:
        print("\n# Re-run with --write to patch footprint_3d.py", file=sys.stderr)
        return

    body = FOOTPRINT_3D_PY.read_text()
    # Replace entry in FOOTPRINT_3D dict by footprint name
    pat = rf'(\s+"{re.escape(fp_name)}": FootprintModel3D\([\s\S]*?\n\s+\),)'
    if not re.search(pat, body):
        raise SystemExit(
            f"{fp_name!r} not in FOOTPRINT_3D — add a stub entry in footprint_3d.py first"
        )
    new_entry = "\n" + snippet + "\n"
    body = re.sub(pat, new_entry, body, count=1)
    FOOTPRINT_3D_PY.write_text(body)
    print(f"wrote {FOOTPRINT_3D_PY}", file=sys.stderr)


if __name__ == "__main__":
    main()
