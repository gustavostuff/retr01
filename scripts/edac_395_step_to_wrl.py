#!/usr/bin/env python3
"""Build a KiCad VRML (WRL) from EDAC STEP: gold contacts, black body.

Requires: pip install cascadio numpy (see script header in docs).
KiCad footprint VRML: 1 WRL unit = 0.1 inch (display mm = wrl * 2.54).
Convert STEP/OBJ mm with factor 1/2.54 so size matches the .step model in Alt+3.
"""

from __future__ import annotations

import argparse
import tempfile
from pathlib import Path

# mm → KiCad WRL (0.1 inch per unit): multiply by 10/25.4
MM_TO_KICAD_WRL = 10.0 / 25.4

GOLD_DIFFUSE = (0.83, 0.69, 0.22)
GOLD_SPEC = (0.55, 0.55, 0.45)
BLACK_DIFFUSE = (0.08, 0.08, 0.08)
BLACK_SPEC = (0.15, 0.15, 0.15)


def _is_gold_group(name: str) -> bool:
    n = name.lower()
    return "345-292" in n or "345-293" in n


def _parse_obj_groups(obj_path: Path) -> tuple[list[tuple[float, float, float]], dict[str, list[tuple[int, int, int]]]]:
    verts: list[tuple[float, float, float]] = []
    groups: dict[str, list[tuple[int, int, int]]] = {}
    current = "__default__"
    with obj_path.open() as f:
        for line in f:
            if line.startswith("v "):
                p = line.split()
                verts.append((float(p[1]), float(p[2]), float(p[3])))
            elif line.startswith("g "):
                current = line[2:].strip()
                groups.setdefault(current, [])
            elif line.startswith("f "):
                idx = [int(t.split("/")[0]) - 1 for t in line.split()[1:]]
                groups.setdefault(current, [])
                for j in range(1, len(idx) - 1):
                    groups[current].append((idx[0], idx[j], idx[j + 1]))
    return verts, groups


def _merge_groups(
    groups: dict[str, list[tuple[int, int, int]]],
) -> tuple[list[tuple[int, int, int]], list[tuple[int, int, int]]]:
    gold: list[tuple[int, int, int]] = []
    black: list[tuple[int, int, int]] = []
    for name, tris in groups.items():
        if _is_gold_group(name):
            gold.extend(tris)
        else:
            black.extend(tris)
    return gold, black


def _compact_wrl_shape(
    verts_mm: list[tuple[float, float, float]],
    tris: list[tuple[int, int, int]],
    diffuse: tuple[float, float, float],
    specular: tuple[float, float, float],
    shininess: float,
) -> list[str]:
    if not tris:
        return []
    used: dict[int, int] = {}
    points: list[tuple[float, float, float]] = []
    for a, b, c in tris:
        for i in (a, b, c):
            if i not in used:
                x, y, z = verts_mm[i]
                used[i] = len(points)
                points.append((x * MM_TO_KICAD_WRL, y * MM_TO_KICAD_WRL, z * MM_TO_KICAD_WRL))
    lines = [
        "Shape {",
        "appearance Appearance {",
        "material Material {",
        "ambientIntensity 0.2",
        f"diffuseColor {diffuse[0]:.4f} {diffuse[1]:.4f} {diffuse[2]:.4f}",
        f"specularColor {specular[0]:.4f} {specular[1]:.4f} {specular[2]:.4f}",
        f"shininess {shininess:.2f}",
        "}",
        "}",
        "geometry IndexedFaceSet {",
        "coord Coordinate {",
        "point [",
    ]
    lines.extend(f"{x:.6f} {y:.6f} {z:.6f}," for x, y, z in points)
    lines.append("]")
    lines.append("}")
    lines.append("coordIndex [")
    for a, b, c in tris:
        lines.append(f"{used[a]} {used[b]} {used[c]} -1,")
    lines.append("]")
    lines.append("creaseAngle 0.5")
    lines.append("}")
    lines.append("}")
    return lines


def step_to_wrl(step_path: Path, wrl_path: Path) -> None:
    try:
        import cascadio
    except ImportError as e:
        raise SystemExit("cascadio required: pip install cascadio") from e

    with tempfile.TemporaryDirectory() as td:
        obj_path = Path(td) / "model.obj"
        rc = cascadio.step_to_obj(str(step_path), str(obj_path), use_colors=False)
        if rc != 0:
            raise SystemExit(f"step_to_obj failed with code {rc}")
        verts, groups = _parse_obj_groups(obj_path)
        gold_tris, black_tris = _merge_groups(groups)
        body = _compact_wrl_shape(verts, black_tris, BLACK_DIFFUSE, BLACK_SPEC, 0.12)
        pins = _compact_wrl_shape(verts, gold_tris, GOLD_DIFFUSE, GOLD_SPEC, 0.85)
        out = ["#VRML V2.0 utf8", "", "Group {", "children ["]
        out.extend(body)
        out.extend(pins)
        out.append("]")
        out.append("}")
        wrl_path.write_text("\n".join(out))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "step",
        type=Path,
        nargs="?",
        default=Path("apps/sim/tier-h/skidl/library/3dmodels/EDAC_395-036-520-201.step"),
    )
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("apps/sim/tier-h/skidl/library/3dmodels/EDAC_395-036-520-201.wrl"),
    )
    args = ap.parse_args()
    step = args.step.resolve()
    if not step.is_file():
        raise SystemExit(f"missing STEP: {step}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    step_to_wrl(step, args.output.resolve())
    print(f"wrote {args.output} ({args.output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
