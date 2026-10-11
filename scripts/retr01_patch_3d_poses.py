#!/usr/bin/env python3
"""
Stamp 3D model offset/scale/rotate onto Retr01_Lib footprints and board instances.

KiCad stores a copy of each footprint on the PCB, so a library-only pose is not
enough. export_netlist.sh runs this after the Skidl netlist step so U130 and the
AV jacks keep their 3D pose on every v_0*.kicad_pcb.
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
LIB = REPO / "apps/sim/tier-h/skidl/library/Retr01_Lib.pretty"
PCB_DIR = REPO / "apps/sim/tier-h/kicad/main-pcb"


@dataclass(frozen=True)
class Pose:
    footprint: str
    model_name: str
    offset: tuple[float, float, float]
    scale: tuple[float, float, float]
    rotate: tuple[float, float, float]


# Numbers match Footprint Editor / 3D Settings (mm and degrees).
POSES: tuple[Pose, ...] = (
    Pose(
        footprint="TO-92_Inline",
        model_name="TO-92_Inline.step",
        offset=(1.25, 0.0, 0.0),
        scale=(1.0, 1.0, 1.0),
        rotate=(0.0, 0.0, 180.0),
    ),
    Pose(
        footprint="CUI_RCJ-014",
        model_name="CUI_RCJ-014.wrl",
        offset=(7.5, 0.0, 6.5),
        scale=(1.0, 1.0, 1.0),
        rotate=(90.0, 180.0, 0.0),
    ),
    Pose(
        footprint="CUI_RCJ-014_Audio",
        model_name="CUI_RCJ-014_audio.wrl",
        offset=(7.5, 0.0, 6.5),
        scale=(1.0, 1.0, 1.0),
        rotate=(90.0, 180.0, 0.0),
    ),
    Pose(
        footprint="Jack_3.5mm_CUI_SJ1-3515N_Horizontal",
        model_name="Jack_3.5mm_CUI_SJ1-3515N_Horizontal.wrl",
        offset=(-2.0, 0.0, 3.0),
        scale=(1.0, 1.0, 1.0),
        rotate=(90.0, 0.0, 90.0),
    ),
)


def _fmt(n: float) -> str:
    if abs(n - round(n)) < 1e-9:
        return str(int(round(n)))
    return f"{n:.6f}".rstrip("0").rstrip(".")


def _xyz(vals: tuple[float, float, float]) -> str:
    return " ".join(_fmt(v) for v in vals)


def _matching_paren(text: str, start: int) -> int:
    if start >= len(text) or text[start] != "(":
        raise ValueError("expected '('")
    depth = 0
    in_str = False
    i = start
    while i < len(text):
        c = text[i]
        if c == '"':
            in_str = not in_str
        elif not in_str:
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    return i + 1
        i += 1
    raise ValueError("unbalanced parentheses in (model ...) block")


def _iter_model_spans(text: str):
    i = 0
    while True:
        j = text.find("(model ", i)
        if j < 0:
            return
        k = _matching_paren(text, j)
        yield j, k, text[j:k]
        i = k


def _set_xyz(block: str, tag: str, vals: tuple[float, float, float]) -> str:
    pat = re.compile(rf"(\({tag}\b[\s\S]*?\(\s*xyz\s+)([^)]+)(\))")
    new, n = pat.subn(rf"\g<1>{_xyz(vals)}\3", block, count=1)
    if n != 1:
        raise SystemExit(f"missing ({tag} (xyz ...)) in 3D model block")
    return new


def apply_pose(block: str, pose: Pose) -> str:
    block = _set_xyz(block, "offset", pose.offset)
    block = _set_xyz(block, "scale", pose.scale)
    block = _set_xyz(block, "rotate", pose.rotate)
    return block


def patch_text(text: str, poses: tuple[Pose, ...]) -> tuple[str, int, int]:
    found = 0
    changed = 0
    out: list[str] = []
    last = 0
    for start, end, block in _iter_model_spans(text):
        out.append(text[last:start])
        patched = block
        for pose in poses:
            if pose.model_name in block:
                found += 1
                patched = apply_pose(block, pose)
                if patched != block:
                    changed += 1
                break
        out.append(patched)
        last = end
    out.append(text[last:])
    return "".join(out), found, changed


def _write_if_changed(path: Path, new: str, old: str) -> bool:
    if new == old:
        return False
    path.write_text(new, encoding="utf-8")
    return True


def patch_library() -> int:
    changed = 0
    for pose in POSES:
        path = LIB / f"{pose.footprint}.kicad_mod"
        if not path.is_file():
            raise SystemExit(f"missing footprint {path}")
        old = path.read_text(encoding="utf-8")
        new, found, _changed = patch_text(old, (pose,))
        if found < 1:
            raise SystemExit(f"no {pose.model_name} model in {path}")
        if _write_if_changed(path, new, old):
            print(f"updated 3D pose in {path.relative_to(REPO)}", file=sys.stderr)
            changed += 1
    return changed


def patch_boards() -> int:
    changed = 0
    boards = [PCB_DIR / "v_04.kicad_pcb"]
    if not boards[0].is_file():
        raise SystemExit(f"missing {boards[0]}")
    for pcb_path in boards:
        old = pcb_path.read_text(encoding="utf-8")
        new, _found, _changed = patch_text(old, POSES)
        if _write_if_changed(pcb_path, new, old):
            print(f"updated 3D pose in {pcb_path.relative_to(REPO)}", file=sys.stderr)
            changed += 1
    return changed


def main() -> int:
    if not LIB.is_dir():
        raise SystemExit(f"missing {LIB}")
    patch_library()
    patch_boards()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
