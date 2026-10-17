"""Clock Beam X from a CH340 TXD pin and sample one output at a time."""

from __future__ import annotations

MOD = 341
STOPS = (0, 1, 2, 3, 4, 264, 288, 289, 341)
BITS = (
    ("Q0", 21, "q0"),
    ("Q1", 23, "q1"),
    ("HSYNC", 14, "hsync"),
)


def level(edges: int, bit: str) -> bool:
    count = edges % MOD
    if bit == "q0":
        return (count & 1) == 1
    if bit == "q1":
        return (count & 2) == 2
    if bit == "hsync":
        return 264 <= count <= 288
    raise KeyError(bit)


def run_virtual() -> str:
    from pins import ModelPins, walk

    for name, _pin, bit in BITS:
        pins = ModelPins(lambda edges, bit=bit: level(edges, bit))
        reason = walk(pins, STOPS, lambda edges, bit=bit: level(edges, bit), name)
        if reason:
            return reason
    return "IC Beam X logic works ok"


def run_serial(clock_name: str, watch_name: str, prepare) -> str:
    from pins import check_serial

    reason = check_serial(clock_name, watch_name, STOPS, BITS, level, prepare)
    if reason == "count ok":
        return "IC Beam X logic works ok"
    return reason


NAME = "Beam X"
CHIP = "ATF22V10C"
