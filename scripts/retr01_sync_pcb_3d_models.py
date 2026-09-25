#!/usr/bin/env python3
"""Push ``footprint_3d.py`` transforms into embedded PCB footprints.

Run after netlist import (export_netlist.sh runs this when v_01.kicad_pcb exists).

Usage:
  python3 scripts/retr01_sync_pcb_3d_models.py \\
    apps/sim/tier-h/kicad/main-pcb/v_01/v_01.kicad_pcb
"""

from __future__ import annotations

import argparse
import importlib.util
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FOOTPRINT_3D_PY = REPO / "apps/sim/tier-h/skidl/retr01_kicad/footprint_3d.py"

FP_LINE = re.compile(r'^\t+\(footprint "Retr01_Lib:([^"]+)"')


def _load_footprint_3d_module():
    spec = importlib.util.spec_from_file_location("retr01_footprint_3d", FOOTPRINT_3D_PY)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {FOOTPRINT_3D_PY}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


def _model_span(lines: list[str], start: int) -> tuple[int, int] | None:
    """Inclusive line range [start, end) for a (model ...) block."""
    depth = 0
    for i in range(start, len(lines)):
        depth += lines[i].count("(") - lines[i].count(")")
        if depth <= 0 and i >= start:
            return start, i + 1
    return None


def sync_pcb_text(text: str, f3d, *, force: bool = False) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    changed = 0

    while i < len(lines):
        m = FP_LINE.match(lines[i].rstrip("\n"))
        if not m:
            out.append(lines[i])
            i += 1
            continue

        fp_name = m.group(1)
        spec = f3d.model_for_footprint_name(fp_name)
        fp_start = i
        depth = lines[i].count("(") - lines[i].count(")")
        i += 1
        fp_body: list[str] = [lines[fp_start]]

        while i < len(lines) and depth > 0:
            line = lines[i]
            if spec is not None and line.lstrip().startswith("(model "):
                span = _model_span(lines, i)
                if span is None:
                    fp_body.append(line)
                    depth += line.count("(") - line.count(")")
                    i += 1
                    continue
                ms, me = span
                want = f3d.model_sexpr(spec, pcb_embed=True) + "\n"
                have = "".join(lines[ms:me])
                if force or have.strip() != want.strip():
                    fp_body.append(want)
                    changed += 1
                else:
                    fp_body.extend(lines[ms:me])
                for k in range(ms, me):
                    depth += lines[k].count("(") - lines[k].count(")")
                i = me
                continue

            fp_body.append(line)
            depth += line.count("(") - line.count(")")
            i += 1

        out.extend(fp_body)

    new_text = "".join(out)
    return new_text, changed


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pcb", type=Path, help="path to .kicad_pcb")
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="report changes without writing",
    )
    ap.add_argument(
        "--force",
        action="store_true",
        help="rewrite model blocks even if they look identical",
    )
    args = ap.parse_args()
    if not args.pcb.is_file():
        raise SystemExit(f"missing PCB: {args.pcb}")
    if not FOOTPRINT_3D_PY.is_file():
        raise SystemExit(f"missing {FOOTPRINT_3D_PY}")

    f3d = _load_footprint_3d_module()
    text = args.pcb.read_text()
    new_text, n = sync_pcb_text(text, f3d, force=args.force)

    if n == 0:
        print("no changes needed (no matching Retr01 footprints with 3D overrides, or already synced)")
        if "Retr01_Lib:EDAC" not in text and "J36" not in text:
            print(
                "hint: this .kicad_pcb has no J36/EDAC footprint embedded — "
                "import the netlist first, then run this again",
                file=sys.stderr,
            )
        return

    if args.dry_run:
        print(f"would update {n} model block(s) in {args.pcb}")
        return

    args.pcb.write_text(new_text)
    print(f"updated {n} model block(s) in {args.pcb} — reload the board in KiCad (reopen file)")


if __name__ == "__main__":
    main()
