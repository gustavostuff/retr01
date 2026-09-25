#!/usr/bin/env python3
"""STEP → KiCad VRML with explicit material colors (from cascadio OBJ/MTL).

KiCad WRL uses 0.1 inch per unit; STEP/OBJ mm → multiply by 10/25.4.

Example:
  .venv-wrl/bin/python scripts/step_colored_to_wrl.py \\
    apps/sim/tier-h/skidl/library/Retr01_Lib.3dshapes/Switchcraft_35RAPC4BVN4.step \\
    -o apps/sim/tier-h/skidl/library/Retr01_Lib.3dshapes/Switchcraft_35RAPC4BVN4.wrl \\
    --default black

  .venv-wrl/bin/python scripts/step_colored_to_wrl.py .../CUI_RCJ-014.step \\
    -o .../CUI_RCJ-014.wrl --mat mat_1:gray --mat mat_2:yellow

Requires: pip install cascadio numpy (see docs/bringup/tier-h-skidl-export.md).
"""

from __future__ import annotations

import argparse
import tempfile
from pathlib import Path

MM_TO_KICAD_WRL = 10.0 / 25.4

PALETTE: dict[str, tuple[tuple[float, float, float], tuple[float, float, float], float]] = {
    "black": ((0.08, 0.08, 0.08), (0.15, 0.15, 0.15), 0.12),
    "gray": ((0.74, 0.74, 0.74), (0.45, 0.45, 0.45), 0.35),
    "white": ((0.95, 0.95, 0.95), (0.55, 0.55, 0.55), 0.4),
    "yellow": ((1.0, 1.0, 0.0), (0.5, 0.5, 0.2), 0.45),
    "gold": ((0.83, 0.69, 0.22), (0.55, 0.55, 0.45), 0.85),
}


def _parse_obj_by_material(obj_path: Path) -> tuple[list[tuple[float, float, float]], dict[str, list[tuple[int, int, int]]]]:
    verts: list[tuple[float, float, float]] = []
    by_mtl: dict[str, list[tuple[int, int, int]]] = {}
    current = "__default__"
    with obj_path.open() as f:
        for line in f:
            if line.startswith("v "):
                p = line.split()
                verts.append((float(p[1]), float(p[2]), float(p[3])))
            elif line.startswith("usemtl "):
                current = line.split(maxsplit=1)[1].strip()
                by_mtl.setdefault(current, [])
            elif line.startswith("f "):
                idx = [int(t.split("/")[0]) - 1 for t in line.split()[1:]]
                by_mtl.setdefault(current, [])
                for j in range(1, len(idx) - 1):
                    by_mtl[current].append((idx[0], idx[j], idx[j + 1]))
    return verts, by_mtl


def _wrl_shape(
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


def step_to_wrl(
    step_path: Path,
    wrl_path: Path,
    mat_colors: dict[str, str],
    default_color: str,
) -> None:
    try:
        import cascadio
    except ImportError as e:
        raise SystemExit("cascadio required: pip install cascadio numpy") from e

    default_color = default_color.lower()
    if default_color not in PALETTE:
        raise SystemExit(f"unknown default color {default_color!r}")

    with tempfile.TemporaryDirectory() as td:
        obj_path = Path(td) / "model.obj"
        rc = cascadio.step_to_obj(str(step_path), str(obj_path), use_colors=True)
        if rc != 0:
            raise SystemExit(f"step_to_obj failed with code {rc}")
        verts, by_mtl = _parse_obj_by_material(obj_path)

    out = ["#VRML V2.0 utf8", "", "Group {", "children ["]
    for mtl_name, tris in by_mtl.items():
        if not tris:
            continue
        color_name = mat_colors.get(mtl_name, default_color).lower()
        if color_name not in PALETTE:
            raise SystemExit(f"unknown color {color_name!r} for material {mtl_name!r}")
        diff, spec, shin = PALETTE[color_name]
        out.extend(_wrl_shape(verts, tris, diff, spec, shin))
    out.append("]")
    out.append("}")
    wrl_path.parent.mkdir(parents=True, exist_ok=True)
    wrl_path.write_text("\n".join(out))


def _parse_mat_arg(s: str) -> tuple[str, str]:
    if ":" not in s:
        raise argparse.ArgumentTypeError("expected NAME:COLOR e.g. mat_2:yellow")
    name, color = s.split(":", 1)
    return name.strip(), color.strip()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("step", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument(
        "--mat",
        action="append",
        type=_parse_mat_arg,
        default=[],
        metavar="MTL:COLOR",
        help=f"material color ({', '.join(PALETTE)})",
    )
    ap.add_argument(
        "--default",
        dest="default_color",
        default="gray",
        choices=sorted(PALETTE),
        help="color for unlisted materials",
    )
    args = ap.parse_args()
    step = args.step.resolve()
    if not step.is_file():
        raise SystemExit(f"missing STEP: {step}")
    mat_map = dict(args.mat)
    step_to_wrl(step, args.output.resolve(), mat_map, args.default_color)
    print(f"wrote {args.output} ({args.output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
