"""Motherboard connectors (not in Tier H pin graph); wired where sim nets exist."""

from __future__ import annotations

from typing import Callable, Dict, Iterable, Optional

from . import pinmap as P

# (refdes, mpn, footprint key in tier_h_map via resolve)
CONNECTOR_REFDES = (
    "J1",
    "J2",
    "J3",
    "J4",
    "J5",
    "J6",
    "J7",
    "J8",
    "J9",
    "J36",
)

# Nets that exist in Tier H JSON today (pad bus on US2).
PAD_DATA_NET_HINTS = ("NET_752", "PAD_DATA")


def ensure_connector_parts(
    parts: Dict[str, object],
    part_for_refdes: Callable[[str], object],
) -> None:
    for ref in CONNECTOR_REFDES:
        if ref not in parts:
            parts[ref] = part_for_refdes(ref)


def _find_net_for_part_pin(nets_map: dict, ref: str, pin_num: str | int) -> Optional[object]:
    want = str(pin_num)
    for sk_net in nets_map.values():
        for pin in getattr(sk_net, "pins", []) or []:
            part = getattr(pin, "part", None)
            if part is None or getattr(part, "ref", None) != ref:
                continue
            if str(getattr(pin, "num", "")) == want:
                return sk_net
    return None


def _find_net_by_node(nets_map: dict, ref: str, pin_substr: str) -> Optional[object]:
    for sk_net in nets_map.values():
        for pin in getattr(sk_net, "pins", []) or []:
            part = getattr(pin, "part", None)
            if part is None:
                continue
            if getattr(part, "ref", None) != ref:
                continue
            pname = str(getattr(pin, "name", "") or "")
            if pin_substr in pname or pin_substr == "":
                return sk_net
    return None


def _find_net_by_name(nets_map: dict, names: Iterable[str]) -> Optional[object]:
    for name in names:
        if name in nets_map:
            return nets_map[name]
    return None


def _ensure_net(nets_map: dict, name: str):
    from skidl import Net

    if name not in nets_map:
        nets_map[name] = Net(name)
    return nets_map[name]


def wire_connectors(parts: dict, nets_map: dict, pin_connect: Callable) -> None:
    """Attach J* pins to rails / pad data where the sim netlist has them."""
    gnd = nets_map.get("GND")
    v5 = nets_map.get("+5V")
    pad_net = _find_net_by_name(nets_map, PAD_DATA_NET_HINTS)
    if pad_net is None:
        pad_net = _find_net_by_node(nets_map, "US2", "PAD_DATA")

    if gnd:
        for ref, pin in (
            ("J3", P.TRS_SLEEVE),
            ("J4", P.TRS_SLEEVE),
            ("J5", "9"),
            ("J5", "10"),
            ("J6", "9"),
            ("J6", "10"),
            ("J7", "2"),
            ("J7", "4"),
            ("J1", "2"),
        ):
            if ref in parts:
                pin_connect(parts[ref], "", int(pin), gnd)

    if v5:
        for ref, pin in (
            ("J7", "1"),
            ("J3", P.TRS_TIP),
            ("J4", P.TRS_TIP),
        ):
            if ref in parts:
                pin_connect(parts[ref], "", int(pin), v5)

    if pad_net and "J3" in parts:
        pin_connect(parts["J3"], "", int(P.TRS_RING), pad_net)
    if pad_net and "J4" in parts:
        pin_connect(parts["J4"], "", int(P.TRS_RING), pad_net)

    if gnd:
        for ref in ("J8", "J9"):
            if ref not in parts:
                continue
            for shell in ("1A", "1B", "1C"):
                pin_connect(parts[ref], "", shell, gnd)

    # RCA tip (pad 2); APU / AD724 not in Tier H JSON yet — distinct stub nets.
    if "J8" in parts:
        pin_connect(parts["J8"], "", "2", _ensure_net(nets_map, "AUDIO_OUT"))
    if "J9" in parts:
        pin_connect(parts["J9"], "", "2", _ensure_net(nets_map, "COMPOSITE_OUT"))

    # J2 sync-capable RGB (docs/general/hardware.md): R/G/B + CSYNC/HSYNC pin 4, pin 5 GND or VSYNC.
    if "J2" in parts:
        for j2_pin, r_ref in ((1, "R9"), (2, "R10"), (3, "R11")):
            video = _find_net_for_part_pin(nets_map, r_ref, "1")
            if video is not None:
                pin_connect(parts["J2"], "", j2_pin, video)
        csync = _ensure_net(nets_map, "CSYNC")
        csync.name = "CSYNC"
        pin_connect(parts["J2"], "", 4, csync)
        if "UPLDV" in parts:
            pin_connect(parts["UPLDV"], "", int(P.UPLDV_EQ), csync)
        if gnd is not None:
            # RGBS default: pin 5 at GND (RGBHV mode jumper swaps pin 5 to VSYNC instead).
            pin_connect(parts["J2"], "", 5, gnd)
            pin_connect(parts["J2"], "", 6, gnd)

    # Arcade headers: sim has no GPIO net names yet; GND on pins 9–10 only (see wire above).
