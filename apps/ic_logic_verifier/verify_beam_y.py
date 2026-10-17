"""Clock Beam Y through Beam X HSYNC and sample one output at a time."""

from __future__ import annotations

X_MOD = 341
X_HSYNC = 264
Y_MOD = 262
BITS = (
    ("Q0", 23, "q0"),
    ("Q1", 21, "q1"),
    ("VSYNC", 14, "vsync"),
)


def y_count(x_edges: int) -> int:
    """Beam Y advances on the edge that takes Beam X into count 264."""
    if x_edges < X_HSYNC:
        return 0
    rises = 1 + (x_edges - X_HSYNC) // X_MOD
    return rises % Y_MOD


def x_edges(rises: int) -> int:
    if rises <= 0:
        return 0
    return X_HSYNC + (rises - 1) * X_MOD


STOPS = tuple(x_edges(rises) for rises in (0, 1, 2, 3, 243, 245, 246, 262))


def level(x_edges_now: int, bit: str) -> bool:
    count = y_count(x_edges_now)
    if bit == "q0":
        return (count & 1) == 1
    if bit == "q1":
        return (count & 2) == 2
    if bit == "vsync":
        return 243 <= count <= 245
    raise KeyError(bit)


def run_virtual() -> str:
    from pins import ModelPins, walk

    for name, _pin, bit in BITS:
        pins = ModelPins(lambda edges, bit=bit: level(edges, bit))
        reason = walk(pins, STOPS, lambda edges, bit=bit: level(edges, bit), name)
        if reason:
            return reason
    return "IC Beam Y logic works ok"


def run_serial(clock_name: str, watch_name: str, prepare) -> str:
    from pins import check_serial

    reason = check_serial(clock_name, watch_name, STOPS, BITS, level, prepare)
    if reason == "count ok":
        return "IC Beam Y logic works ok"
    return reason


NAME = "Beam Y"
CHIP = "ATF22V10C"
