"""Canonical Retr01_Lib 3D model paths and transforms (applied on skidl export).

KiCad netlists do not include 3D offset/rotation; values here are written into
``Retr01_Lib.pretty`` by ``scripts/retr01_apply_footprint_3d.py`` and pushed onto
the board by ``scripts/retr01_sync_pcb_3d_models.py`` after netlist import.

Footprint name = ``.kicad_mod`` basename without extension (not ``Retr01_Lib:``).
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Tuple

Vec3 = Tuple[float, float, float]


@dataclass(frozen=True)
class FootprintModel3D:
    """One WRL/STEP on a footprint (KiCad ``(model ...)`` block)."""

    path: str
    offset_mm: Vec3 = (0.0, 0.0, 0.0)
    scale: Vec3 = (1.0, 1.0, 1.0)
    rotate_deg: Vec3 = (0.0, 0.0, 0.0)


# Key = footprint name in Retr01_Lib.pretty
_J36_EDAC = FootprintModel3D(
    path="${KIPRJMOD}/library/Retr01_Lib.3dshapes/EDAC_395-036-520-201.wrl",
    offset_mm=(2.15, -55.0, 0.0),
    rotate_deg=(90.0, 180.0, 90.0),
)

# J9 composite (yellow ring WRL) and J8 audio (white ring WRL); same mechanical align.
_RCA_XFORM = ((7.5, 0.0, 6.5), (90.0, 180.0, 0.0))
_RCA_COMPOSITE = FootprintModel3D(
    path="${KIPRJMOD}/library/Retr01_Lib.3dshapes/CUI_RCJ-014.wrl",
    offset_mm=_RCA_XFORM[0],
    rotate_deg=_RCA_XFORM[1],
)
_RCA_AUDIO = FootprintModel3D(
    path="${KIPRJMOD}/library/Retr01_Lib.3dshapes/CUI_RCJ-014_audio.wrl",
    offset_mm=_RCA_XFORM[0],
    rotate_deg=_RCA_XFORM[1],
)

# J3/J4 Switchcraft vertical TRS (same WRL for 2BVN4/4BVN4 mechanical body).
_TRS_JACK = FootprintModel3D(
    path="${KIPRJMOD}/library/Retr01_Lib.3dshapes/Switchcraft_35RAPC4BVN4.wrl",
    offset_mm=(12.75, 1.0, 7.0),
    rotate_deg=(180.0, 0.0, 180.0),
)

# Key = footprint name in Retr01_Lib.pretty (``Retr01_Lib:Name`` on the board).
FOOTPRINT_3D: dict[str, FootprintModel3D] = {
    "EDAC_395_MoboSocket_2x18_2.54x5.08mm": _J36_EDAC,
    "CUI_RCJ-014": _RCA_COMPOSITE,
    "CUI_RCJ-014_Audio": _RCA_AUDIO,
    "Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical": _TRS_JACK,
}

# Older boards / netlists may still embed the previous library name.
FOOTPRINT_3D_ALIASES: dict[str, str] = {
    "EDAC_395-036-520-201": "EDAC_395_MoboSocket_2x18_2.54x5.08mm",
}


def _format_num(x: float) -> str:
    if abs(x - round(x)) < 1e-9:
        return str(int(round(x)))
    return f"{x:g}"


def model_sexpr(m: FootprintModel3D, *, pcb_embed: bool = False) -> str:
    """KiCad ``(model ...)`` sexpr; ``pcb_embed`` adds one tab (board copy)."""
    tab = "\t" * (2 if pcb_embed else 1)
    tab2 = tab + "\t"
    tab3 = tab2 + "\t"
    ox, oy, oz = m.offset_mm
    sx, sy, sz = m.scale
    rx, ry, rz = m.rotate_deg
    return (
        f'{tab}(model "{m.path}"\n'
        f"{tab2}(offset\n"
        f'{tab3}(xyz {_format_num(ox)} {_format_num(oy)} {_format_num(oz)})\n'
        f"{tab2})\n"
        f"{tab2}(scale\n"
        f'{tab3}(xyz {_format_num(sx)} {_format_num(sy)} {_format_num(sz)})\n'
        f"{tab2})\n"
        f"{tab2}(rotate\n"
        f'{tab3}(xyz {_format_num(rx)} {_format_num(ry)} {_format_num(rz)})\n'
        f"{tab2})\n"
        f"{tab})"
    )


def model_for_footprint_name(fp_name: str) -> FootprintModel3D | None:
    canonical = FOOTPRINT_3D_ALIASES.get(fp_name, fp_name)
    return FOOTPRINT_3D.get(canonical)
