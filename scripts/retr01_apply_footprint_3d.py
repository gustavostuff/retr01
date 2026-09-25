#!/usr/bin/env python3
"""Write ``retr01_kicad/footprint_3d.py`` transforms into Retr01_Lib.pretty."""

from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FOOTPRINT_3D_PY = REPO / "apps/sim/tier-h/skidl/retr01_kicad/footprint_3d.py"
DEFAULT_LIB = REPO / "apps/sim/tier-h/skidl/library/Retr01_Lib.pretty"


def _load_footprint_3d_module():
    spec = importlib.util.spec_from_file_location("retr01_footprint_3d", FOOTPRINT_3D_PY)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {FOOTPRINT_3D_PY}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


def extract_model_span(text: str) -> tuple[int, int] | None:
    idx = text.find("\t(model ")
    if idx < 0:
        idx = text.find("(model ")
    if idx < 0:
        return None
    depth = 0
    for i in range(idx, len(text)):
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return idx, i + 1
    return None


def apply_to_footprint(path: Path, model_block: str) -> bool:
    text = path.read_text()
    span = extract_model_span(text)
    if span is None:
        close = text.rfind(")\n")
        if close < 0:
            return False
        text = text[:close] + "\n" + model_block + "\n" + text[close:]
        path.write_text(text.rstrip() + "\n")
        return True
    start, end = span
    if text[start:end].strip() == model_block.strip():
        return False
    new_text = text[:start] + model_block + "\n" + text[end:].lstrip("\n")
    path.write_text(new_text.rstrip() + "\n")
    return True


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--library",
        type=Path,
        default=DEFAULT_LIB,
        help="Retr01_Lib.pretty directory",
    )
    args = ap.parse_args()
    lib = args.library.resolve()
    if not lib.is_dir():
        raise SystemExit(f"missing library: {lib}")

    f3d = _load_footprint_3d_module()
    changed = 0
    for fp_name, spec in f3d.FOOTPRINT_3D.items():
        mod = lib / f"{fp_name}.kicad_mod"
        if not mod.is_file():
            print(f"skip missing footprint: {mod.name}", file=sys.stderr)
            continue
        block = f3d.model_sexpr(spec, pcb_embed=False) + "\n"
        if apply_to_footprint(mod, block):
            changed += 1
            print(f"updated 3D model: {fp_name}")
    if not changed:
        print("footprint 3D models already up to date")


if __name__ == "__main__":
    main()
