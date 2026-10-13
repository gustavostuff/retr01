"""KiCad net classes for the shared CPU address and data buses.

The electrical bus stays one shared D[7:0] / A[15:0] (no extra transceivers).
These classes only split the Pcbnew ratsnest so address and data can be placed
and routed as two parallel corridors.
"""

from __future__ import annotations

import re
from typing import Iterable, Optional

CLASS_ADDR = "CPU_A"
CLASS_DATA = "CPU_D"

_GEOM = {
    "bus_width": 12,
    "clearance": 0.2,
    "diff_pair_gap": 0.25,
    "diff_pair_via_gap": 0.25,
    "diff_pair_width": 0.2,
    "line_style": 0,
    "microvia_diameter": 0.3,
    "microvia_drill": 0.1,
    "priority": 0,
    "track_width": 0.25,
    "tuning_profile": "",
    "via_diameter": 0.6,
    "via_drill": 0.3,
    "wire_width": 6,
}

KICAD_CLASSES = [
    {
        **_GEOM,
        "name": CLASS_ADDR,
        "pcb_color": "rgb(210, 35, 136)",
        "schematic_color": "rgb(210, 35, 136)",
    },
    {
        **_GEOM,
        "name": CLASS_DATA,
        "pcb_color": "rgb(60, 180, 90)",
        "schematic_color": "rgb(60, 180, 90)",
    },
]

# Digit wildcards so CPU_A_SAMPLE and CART_A14-A18 stay out of these classes.
PATTERNS = [
    {"netclass": CLASS_ADDR, "pattern": "CPU_A?"},
    {"netclass": CLASS_ADDR, "pattern": "CPU_A1?"},
    {"netclass": CLASS_ADDR, "pattern": "CART_A?"},
    {"netclass": CLASS_ADDR, "pattern": "CART_A10"},
    {"netclass": CLASS_ADDR, "pattern": "CART_A11"},
    {"netclass": CLASS_ADDR, "pattern": "CART_A12"},
    {"netclass": CLASS_ADDR, "pattern": "CART_A13"},
    {"netclass": CLASS_DATA, "pattern": "CPU_D?"},
    {"netclass": CLASS_DATA, "pattern": "CART_D?"},
]

_ADDR = re.compile(r"^(CPU_A|CART_A)(\d+)$")
_DATA = re.compile(r"^(CPU_D|CART_D)(\d+)$")


def class_for_name(name: str) -> Optional[str]:
    m = _ADDR.match(name)
    if m:
        n = int(m.group(2))
        if m.group(1) == "CPU_A" and 0 <= n <= 15:
            return CLASS_ADDR
        if m.group(1) == "CART_A" and 0 <= n <= 13:
            return CLASS_ADDR
        return None
    m = _DATA.match(name)
    if m and 0 <= int(m.group(2)) <= 7:
        return CLASS_DATA
    return None


def apply_bus_netclasses(nets: Iterable) -> list[str]:
    """Assign SKiDL NetClass CPU_A / CPU_D. Returns matched net names."""
    from skidl import NetClass

    by_name = {}
    for spec in KICAD_CLASSES:
        by_name[spec["name"]] = NetClass(
            spec["name"],
            trace_width=0.25,
            clearance=0.2,
            via_dia=0.6,
            via_drill=0.3,
            priority=10,
        )
    matched = []
    for net in nets:
        name = getattr(net, "name", "") or ""
        cls = class_for_name(name)
        if cls is None:
            continue
        net.netclasses = by_name[cls]
        matched.append(name)
    return sorted(set(matched))
