"""Apply manifest connections to SKiDL Part instances."""

from __future__ import annotations

from typing import Dict, List, Set

from .manifest import Connection

try:
    from skidl import Net
except ImportError:  # pragma: no cover
    Net = None  # type: ignore

# Manifest pseudo-refdes for power rails (no BOM entry).
PSEUDO_REFDES: Set[str] = {"GND", "+5V", "VCC", "+5V_ANALOG", "VIN_RAW", "VIN_PROT", "VIN_FUSED"}


def _pin(part, name: str):
    """Resolve by physical pin number string (preferred) or legacy name/alias."""
    try:
        return part[name]
    except Exception:
        if hasattr(part, name):
            return getattr(part, name)
        return part[name]


def _get_or_create_net(nets: Dict[str, object], name: str):
    """Reuse one SKiDL Net per logical name (circuit-wide).

    Plain ``Net("GND")`` creates GND, GND1, GND2… across island passes, which
    fragments the rail and leaves KiCad with no net literally named GND.
    """
    net = nets.get(name)
    if net is not None:
        return net
    net = Net.fetch(name)
    nets[name] = net
    return net


def apply_connections(parts: Dict[str, object], connections: List[Connection]) -> Dict[str, object]:
    if Net is None:
        raise RuntimeError("skidl is not installed")

    nets: Dict[str, object] = {}
    for conn in connections:
        net = _get_or_create_net(nets, conn.net)

        for refdes, pin in ((conn.a_refdes, conn.a_pin), (conn.b_refdes, conn.b_pin)):
            if refdes in PSEUDO_REFDES:
                continue
            if refdes not in parts:
                continue
            try:
                p = _pin(parts[refdes], pin)
                p += net
            except Exception:
                # Pin naming mismatch vs template: skip until Retr01_Lib S-expr lock.
                continue
    return nets


def rail_net(name: str) -> object:
    """Shared power/ground net for decoupling and other board helpers."""
    if Net is None:
        raise RuntimeError("skidl is not installed")
    return Net.fetch(name)