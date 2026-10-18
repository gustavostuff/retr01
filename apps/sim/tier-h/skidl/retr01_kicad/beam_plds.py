"""Beam X / Beam Y nets from the burned ATF22V10 images.

The Tier H JSON still exports the sim stand-in (UPLDY P0-P7 tied to UPLDX Y0-Y7).
mobo_scope drops those nodes. This pass wires the bench pin list.
"""

from __future__ import annotations

from typing import Callable, Dict

from . import pinmap as P


def _ensure_net(nets_map: dict, name: str):
    from skidl import Net

    net = nets_map.get(name)
    if net is None:
        net = Net(name)
        nets_map[name] = net
    net.name = name
    return net


def _net_on(nets_map: dict, ref: str, pin_num: str):
    want = str(pin_num)
    for sk_net in nets_map.values():
        for pin in getattr(sk_net, "pins", []) or []:
            part = getattr(pin, "part", None)
            if part is None or getattr(part, "ref", None) != ref:
                continue
            if str(getattr(pin, "num", "")) == want:
                return sk_net
    return None


def _label(part, labels: Dict[str, str]) -> None:
    for pin in getattr(part, "pins", []) or []:
        name = labels.get(str(getattr(pin, "num", "")))
        if name:
            pin.name = name


def _x_labels() -> Dict[str, str]:
    labels = {
        P.UPLDX_CLK: "CLK",
        P.UPLDX_RES: "RES",
        P.PLD_GND: "GND",
        P.UPLDX_HSYNC: "HSYNC",
        P.PLD_VCC: "VCC",
    }
    for num in P.UPLD_TIED_LOW:
        labels[num] = "IN"
    for bit, num in P.UPLDX_Q.items():
        labels[num] = f"Q{bit}"
    return labels


def _y_labels() -> Dict[str, str]:
    labels = {
        P.UPLDY_CLK: "CLK",
        P.UPLDY_RES: "RES",
        P.PLD_GND: "GND",
        P.UPLDY_VSYNC: "VSYNC",
        P.PLD_VCC: "VCC",
    }
    for num in P.UPLD_TIED_LOW:
        labels[num] = "IN"
    for bit, num in P.UPLDY_Q.items():
        labels[num] = f"Q{bit}"
    return labels


def wire_beam_plds(parts: dict, nets_map: dict, pin_connect: Callable, ensure_part: Callable) -> None:
    """CLK, RES, HSYNC, VSYNC, count outputs, and tied-low inputs."""
    ensure_part("UPLDX")
    ensure_part("UPLDY")
    x = parts["UPLDX"]
    y = parts["UPLDY"]
    _label(x, _x_labels())
    _label(y, _y_labels())

    gnd = _ensure_net(nets_map, "GND")
    for num in P.UPLD_TIED_LOW:
        pin_connect(x, "", num, gnd)
        pin_connect(y, "", num, gnd)

    clk = _net_on(nets_map, "R13", "2") or _net_on(nets_map, "U74", "9")
    if clk is None:
        clk = _ensure_net(nets_map, "BEAM_X_CLK")
    pin_connect(x, "", P.UPLDX_CLK, clk)

    res = _net_on(nets_map, "U1", "40") or _net_on(nets_map, "U130", "1")
    if res is None:
        res = _ensure_net(nets_map, "CPU_RES#")
    pin_connect(x, "", P.UPLDX_RES, res)
    pin_connect(y, "", P.UPLDY_RES, res)

    hsync = _ensure_net(nets_map, "HSYNC")
    pin_connect(x, "", P.UPLDX_HSYNC, hsync)
    pin_connect(y, "", P.UPLDY_CLK, hsync)

    vsync = _ensure_net(nets_map, "VSYNC")
    pin_connect(y, "", P.UPLDY_VSYNC, vsync)

    for bit, num in P.UPLDX_Q.items():
        pin_connect(x, "", num, _ensure_net(nets_map, f"BEAM_X_Q{bit}"))
    for bit, num in P.UPLDY_Q.items():
        pin_connect(y, "", num, _ensure_net(nets_map, f"BEAM_Y_Q{bit}"))
