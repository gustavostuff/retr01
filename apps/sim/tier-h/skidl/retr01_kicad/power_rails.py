"""Tie package VCC/GND pins to +5V and GND after JSON wiring."""

from __future__ import annotations

from typing import Callable, Dict, Iterable, Optional, Set, Tuple

from . import pinmap as P


def _ensure_net(nets_map: dict, name: str):
    from skidl import Net

    net = nets_map.get(name)
    if net is None:
        net = Net(name)
        nets_map[name] = net
    return net


def _connect(part, pin_num: str, net, pin_connect: Callable) -> None:
    pin_connect(part, "", pin_num, net)


def _mpn(part) -> str:
    return getattr(part, "name", "") or ""


def wire_power_rails(parts: Dict[str, object], nets_map: dict, pin_connect: Callable) -> None:
    v5 = _ensure_net(nets_map, "+5V")
    gnd = _ensure_net(nets_map, "GND")

    for ref, part in parts.items():
        pwr: Optional[Tuple[str, str]] = P.power_pin_nums(_mpn(part))
        if pwr:
            vcc_pin, gnd_pin = pwr
            _connect(part, vcc_pin, v5, pin_connect)
            _connect(part, gnd_pin, gnd, pin_connect)

    for ref in ("UM", "US1", "US2"):
        part = parts.get(ref)
        if part is None:
            continue
        for pin in (str(P.AVR128_VDDIO2), str(P.AVR128_VDD), str(P.AVR128_AVDD)):
            _connect(part, pin, v5, pin_connect)
        for pin in (str(P.AVR128_GND_A), str(P.AVR128_GND_B)):
            _connect(part, pin, gnd, pin_connect)

    ad724 = parts.get("U725")
    if ad724 is not None:
        for pin in ("1", "4", "5", "14", "15"):
            _connect(ad724, pin, v5, pin_connect)
        for pin in ("2", "12", "13"):
            _connect(ad724, pin, gnd, pin_connect)


def _pin_name_upper(pin) -> str:
    return (getattr(pin, "name", "") or "").upper()


def _collect_nets(parts: Dict[str, object]) -> Set[object]:
    nets: Set[object] = set()
    for part in parts.values():
        for pin in getattr(part, "pins", []) or []:
            if pin.net is not None:
                nets.add(pin.net)
    return nets


def _rail_markers(pins: list) -> Tuple[int, int]:
    vdd_markers = 0
    gnd_markers = 0
    for pin in pins:
        pname = _pin_name_upper(pin)
        part = getattr(pin, "part", None)
        ref = getattr(part, "ref", "")
        num = str(getattr(pin, "num", ""))
        mpn = _mpn(part) if part is not None else ""
        pwr = P.power_pin_nums(mpn) if mpn else None
        if pwr and num == pwr[0]:
            vdd_markers += 2
        if pwr and num == pwr[1]:
            gnd_markers += 2
        if mpn == "AVR128DB28" and num in (str(P.AVR128_GND_B),):
            gnd_markers += 2
        if mpn == "AVR128DB28" and num in (
            str(P.AVR128_VDDIO2),
            str(P.AVR128_VDD),
            str(P.AVR128_AVDD),
        ):
            vdd_markers += 2
        if pname in ("VDD", "VCC", "VDDIO2", "AVDD") or (ref == "J1" and num == "1"):
            vdd_markers += 1
        if pname in ("VSS", "GND", "GND2", "AGND", "DGND") or (ref == "J1" and num in ("2", "3")):
            gnd_markers += 1
        if ref.startswith("C") and num == "1":
            vdd_markers += 1
        if ref.startswith("C") and num == "2":
            gnd_markers += 1
    return vdd_markers, gnd_markers


def merge_rail_nets(parts: Dict[str, object], nets_map: dict) -> None:
    """Merge stray rail fragments onto canonical +5V / GND nets."""
    v5 = nets_map.get("+5V")
    gnd = nets_map.get("GND")
    for net in list(_collect_nets(parts)):
        pins = getattr(net, "pins", []) or []
        if not pins:
            continue
        vdd_markers, gnd_markers = _rail_markers(pins)
        if gnd is not None and net is not gnd and gnd_markers >= 3 and gnd_markers >= vdd_markers:
            for pin in list(pins):
                pin += gnd
        elif v5 is not None and net is not v5 and vdd_markers >= 4 and vdd_markers >= gnd_markers:
            for pin in list(pins):
                pin += v5


def restore_rail_net_names(parts: Dict[str, object], nets_map: dict) -> None:
    """Force literal +5V / GND net names after name_all_nets()."""
    merge_rail_nets(parts, nets_map)
    for key in ("+5V", "GND"):
        net = nets_map.get(key)
        if net is not None:
            net.name = key

    for net in _collect_nets(parts):
        pins = getattr(net, "pins", []) or []
        if not pins:
            continue
        vdd_markers, gnd_markers = _rail_markers(pins)
        if vdd_markers >= 4 and vdd_markers >= gnd_markers:
            net.name = "+5V"
        elif gnd_markers >= 3 and gnd_markers >= vdd_markers:
            net.name = "GND"

    for net in _collect_nets(parts):
        nm = getattr(net, "name", "") or ""
        if nm.startswith("+5V") and nm != "+5V":
            net.name = "+5V"
        if nm.startswith("GND") and nm != "GND":
            net.name = "GND"

    # Skidl sometimes leaves the main rail as NET_NNN after merges; pin anchors fix it.
    v5 = nets_map.get("+5V")
    gnd = nets_map.get("GND")
    for net in list(_collect_nets(parts)):
        for pin in list(getattr(net, "pins", []) or []):
            part = getattr(pin, "part", None)
            if part is None:
                continue
            ref = getattr(part, "ref", "")
            num = str(getattr(pin, "num", ""))
            if v5 is not None and net is not v5 and (
                (ref == "U1" and num == "8") or (ref == "J1" and num == "1") or (ref == "J36" and num in ("3", "4"))
            ):
                for p in list(getattr(net, "pins", []) or []):
                    p += v5
                break
            if gnd is not None and net is not gnd and (
                (ref == "U1" and num == "21") or (ref == "J1" and num in ("2", "3")) or (ref == "J36" and num in ("1", "2", "35"))
            ):
                for p in list(getattr(net, "pins", []) or []):
                    p += gnd
                break
    if v5 is not None:
        v5.name = "+5V"
    if gnd is not None:
        gnd.name = "GND"
