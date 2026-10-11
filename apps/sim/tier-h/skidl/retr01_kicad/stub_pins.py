"""Tie unused SKiDL pins so KiCad netlist import matches every footprint pad."""

from __future__ import annotations

from typing import Callable, Dict

from .power_rails import ensure_nc_net


_OSC_CAN = frozenset({"OSC8M", "OSC_DOT", "OSC_4FSC"})


def stub_unconnected_pins(parts: Dict[str, object], nets_map: dict, pin_connect: Callable) -> None:
    """Every part pin must appear in the netlist or Pcbnew warns on import.

    Unused pads each get their own NC_<ref>_<pin> net. A shared NC net would
    draw a ratsnest between every leftover pad.
    """
    gnd = nets_map.get("GND")

    for part in parts.values():
        mpn = getattr(part, "name", "") or ""
        ref = getattr(part, "ref", "") or ""
        for pin in part.pins:
            if pin.net is not None:
                continue
            if mpn in _OSC_CAN and str(pin.num) == "4" and gnd is not None:
                pin_connect(part, str(getattr(pin, "name", "") or ""), pin.num, gnd)
            else:
                pin_connect(
                    part,
                    str(getattr(pin, "name", "") or ""),
                    pin.num,
                    ensure_nc_net(nets_map, ref, pin.num),
                )
