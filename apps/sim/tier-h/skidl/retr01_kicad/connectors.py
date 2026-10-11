"""Motherboard connectors; RCA tips follow sim nets when present."""

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
    "J7",
    "J8",
    "J9",
    "J10",
    "J11",
    "J12",
    "J36",
)

# J11/J12 are 6.35 mm TRS alternatives to J3/J4 (same Tip / Ring / Sleeve nets).
_TRS35_TO_TRS635 = (("J3", "J11"), ("J4", "J12"))

# Nets that exist in Tier H JSON today (pad bus on US2).
PAD_DATA_NET_HINTS = ("NET_752", "PAD_DATA", "PAD_DATA_BUS", "PAD1_DATA")


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
    if pad_net is None:
        pad_net = _find_net_for_part_pin(nets_map, "US2", "16")

    nc = _ensure_net(nets_map, "NC")

    if gnd:
        for ref, pin in (
            ("J3", P.TRS_SLEEVE),
            ("J4", P.TRS_SLEEVE),
            ("J5", "17"),
            ("J5", "19"),
            ("J5", "18"),
            ("J5", "20"),
            ("J7", "2"),
            ("J7", "4"),
            ("J10", "2"),
            ("J10", "4"),
            ("J1", "2"),
            ("J1", "3"),
            ("J36", "1"),   # A1 GND
            ("J36", "2"),   # B1 GND
            ("J36", "35"),  # A18 GND
        ):
            if ref in parts:
                pin_connect(parts[ref], "", pin, gnd)

    if v5:
        for ref, pin in (
            ("J1", "1"),
            ("J7", "1"),
            ("J3", P.TRS_TIP),
            ("J4", P.TRS_TIP),
            ("J36", "3"),  # A2 VCC
            ("J36", "4"),  # B2 VCC
        ):
            if ref in parts:
                pin_connect(parts[ref], "", pin, v5)

    if pad_net and "J3" in parts:
        pin_connect(parts["J3"], "", P.TRS_RING, pad_net)
    if pad_net and "J4" in parts:
        pin_connect(parts["J4"], "", P.TRS_RING, pad_net)

    for ref in ("J3", "J4"):
        if ref in parts:
            for nc_pin in P.TRS_NC:
                pin_connect(parts[ref], "", nc_pin, nc)

    # Populate either 3.5 mm or 6.35 mm: J11 mirrors J3, J12 mirrors J4.
    for small, large in _TRS35_TO_TRS635:
        if large not in parts:
            continue
        for pin_id in (P.TRS_TIP, P.TRS_RING, P.TRS_SLEEVE):
            net = _find_net_for_part_pin(nets_map, small, pin_id)
            if net is not None:
                pin_connect(parts[large], "", pin_id, net)
        for nc_pin in P.TRS635_NC:
            pin_connect(parts[large], "", nc_pin, nc)

    # RCA shells to GND. Tips come from the sim JSON (US2 PWM / U725 COMP).
    if gnd:
        for ref in ("J8", "J9"):
            if ref not in parts:
                continue
            for shell in ("1A", "1B", "1C"):
                pin_connect(parts[ref], "", shell, gnd)

    def _pin_on_net(part, num_or_name: str) -> bool:
        for pin in getattr(part, "pins", []) or []:
            if pin.net is None:
                continue
            if str(getattr(pin, "num", "")) == str(num_or_name):
                return True
            if str(getattr(pin, "name", "") or "") == str(num_or_name):
                return True
        return False

    if "J8" in parts and not _pin_on_net(parts["J8"], "2"):
        pwm = _find_net_by_node(nets_map, "US2", "AUDIO_PWM")
        pin_connect(parts["J8"], "", "2", pwm if pwm is not None else _ensure_net(nets_map, "AUDIO_OUT"))
    if "J9" in parts and not _pin_on_net(parts["J9"], "2"):
        comp = _find_net_by_node(nets_map, "U725", "COMP")
        pin_connect(parts["J9"], "", "2", comp if comp is not None else _ensure_net(nets_map, "COMPOSITE_OUT"))

    # J2 2x4 RGB + CSYNC/HSYNC/VSYNC (docs/general/hardware.md).
    # 1 R  2 G
    # 3 B  4 CSYNC
    # 5 HSYNC  6 VSYNC
    # 7 GND  8 GND
    if "J2" in parts:
        for j2_pin, r_ref in ((1, "R9"), (2, "R10"), (3, "R11")):
            video = _find_net_for_part_pin(nets_map, r_ref, "1")
            if video is not None:
                pin_connect(parts["J2"], "", j2_pin, video)
        csync = _find_net_by_name(nets_map, ("CSYNC", "VIDEO_CSYNC")) or _ensure_net(nets_map, "CSYNC")
        csync.name = "CSYNC"
        pin_connect(parts["J2"], "", 4, csync)
        if "UPLDV" in parts:
            pin_connect(parts["UPLDV"], "", int(P.UPLDV_EQ), csync)
        if "UPLDX" in parts:
            pin_connect(parts["UPLDX"], "CSYNC", "CSYNC", csync)
        hsync = _find_net_by_name(nets_map, ("HSYNC", "VIDEO_HSYNC")) or _ensure_net(nets_map, "HSYNC")
        hsync.name = "HSYNC"
        pin_connect(parts["J2"], "", 5, hsync)
        if "UPLDX" in parts:
            pin_connect(parts["UPLDX"], "HSYNC", "HSYNC", hsync)
        vsync = _find_net_by_name(nets_map, ("VSYNC", "VIDEO_VSYNC")) or _ensure_net(nets_map, "VSYNC")
        vsync.name = "VSYNC"
        pin_connect(parts["J2"], "", 6, vsync)
        if "UPLDY" in parts:
            pin_connect(parts["UPLDY"], "VSYNC", "VSYNC", vsync)
        if gnd is not None:
            pin_connect(parts["J2"], "", 7, gnd)
            pin_connect(parts["J2"], "", 8, gnd)

    # Arcade J5 2x10: even pins P1 (US2 PA), odd pins P2. GND on pins 17-20.

    # J10 2x2 cart program (docs/general/hardware.md).
    # 1 PWR NC   2 GND
    # 3 DATA     4 GND
    if "J10" in parts:
        pwr_nc = _ensure_net(nets_map, "J10_PWR_NC")
        pwr_nc.name = "J10_PWR_NC"
        pin_connect(parts["J10"], "", 1, pwr_nc)
        data = _find_net_by_name(nets_map, ("SPI_MISO", "CART_PROG_DATA")) or _find_net_by_node(
            nets_map, "UM", "SPI_MISO"
        )
        if data is None:
            data = _ensure_net(nets_map, "SPI_MISO")
        pin_connect(parts["J10"], "", 3, data)
