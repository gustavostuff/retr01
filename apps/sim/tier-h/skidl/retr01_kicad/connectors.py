"""Motherboard connectors. J2 is the Kycon KMDGX-9S-AS mini-DIN."""

from __future__ import annotations

from typing import Callable, Dict, Iterable, Optional

from . import pinmap as P
from .power_rails import ensure_nc_net

# (refdes, mpn, footprint key in tier_h_map via resolve)
CONNECTOR_REFDES = (
    "J1",
    "J2",
    "J3",
    "J4",
    "J5",
    "J6",
    "J7",
    "J10",
    "J11",
    "J36",
)

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

    if gnd:
        for ref, pin in (
            ("J3", P.TRS_SLEEVE),
            ("J4", P.TRS_SLEEVE),
            ("J5", "7"),
            ("J5", "9"),
            ("J6", "1"),
            ("J6", "3"),
            ("J7", "2"),
            ("J7", "4"),
            ("J10", "2"),
            ("J10", "4"),
            ("J11", "3"),
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
            ("J11", "1"),
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
                pin_connect(parts[ref], "", nc_pin, ensure_nc_net(nets_map, ref, nc_pin))

    # J2 Kycon KMDGX-9S-AS. Pin numbers are the recommended PCB layout
    # with the connector facing up (top view of the bottom-view drawing).
    # 1 Blue   2 +5V   3 Green
    # 4 Composite   5 CSYNC   6 Audio
    # 7 Red   8 Audio   9 Audio
    # S shield GND
    # HSYNC and VSYNC stay on the beam PLDs. They have no pin on this socket.
    if "J2" in parts:
        for j2_pin, r_ref in ((1, "R11"), (3, "R10"), (7, "R9")):
            video = _find_net_for_part_pin(nets_map, r_ref, "1")
            if video is not None:
                pin_connect(parts["J2"], "", j2_pin, video)
        comp = _find_net_by_node(nets_map, "U725", "COMP")
        pin_connect(
            parts["J2"],
            "",
            4,
            comp if comp is not None else _ensure_net(nets_map, "COMPOSITE_OUT"),
        )
        csync = _find_net_by_name(nets_map, ("CSYNC", "VIDEO_CSYNC")) or _ensure_net(nets_map, "CSYNC")
        csync.name = "CSYNC"
        pin_connect(parts["J2"], "", 5, csync)
        if "UPLDV" in parts:
            pin_connect(parts["UPLDV"], "", int(P.UPLDV_EQ), csync)
        if "UPLDX" in parts:
            pin_connect(parts["UPLDX"], "CSYNC", "CSYNC", csync)
        hsync = _find_net_by_name(nets_map, ("HSYNC", "VIDEO_HSYNC")) or _ensure_net(nets_map, "HSYNC")
        hsync.name = "HSYNC"
        if "UPLDX" in parts:
            pin_connect(parts["UPLDX"], "HSYNC", "HSYNC", hsync)
        vsync = _find_net_by_name(nets_map, ("VSYNC", "VIDEO_VSYNC")) or _ensure_net(nets_map, "VSYNC")
        vsync.name = "VSYNC"
        if "UPLDY" in parts:
            pin_connect(parts["UPLDY"], "VSYNC", "VSYNC", vsync)
        audio = _find_net_by_node(nets_map, "US2", "AUDIO_PWM")
        if audio is None:
            audio = _find_net_for_part_pin(nets_map, "US2", "17")
        if audio is None:
            audio = _find_net_by_name(nets_map, ("AUDIO_PWM", "AUDIO_OUT"))
        if audio is None:
            audio = _ensure_net(nets_map, "AUDIO_OUT")
        for audio_pin in (6, 8, 9):
            pin_connect(parts["J2"], "", audio_pin, audio)
        if v5 is not None:
            pin_connect(parts["J2"], "", 2, v5)
        if gnd is not None:
            pin_connect(parts["J2"], "", "S", gnd)

    # Arcade J5 (P1) and J6 (P2) 2x5: even 2-10 bits 0-4.
    # J5 odd 7/9 = GND. J5 odd 1/3/5 = PA7/PA6/PA5 (US2 1/28/27).
    # J6 odd 1/3 = GND. J6 odd 5/7/9 = PD2/PF6/PD3 (US2 8/18/9).

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
