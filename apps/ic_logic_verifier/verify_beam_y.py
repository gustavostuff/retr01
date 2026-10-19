"""Clock Beam Y through Beam X HSYNC and sample one output at a time."""

from __future__ import annotations

from pathlib import Path

X_MOD = 341
LOG = Path(__file__).resolve().with_name("verify_beam_y.log")
X_HSYNC = 264
Y_MOD = 262
BITS = (
    ("Q0", 14, "q0"),
    ("Q1", 15, "q1"),
    ("VSYNC", 23, "vsync"),
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


def place(edges: int, name: str) -> str:
    rises = 0 if edges < X_HSYNC else 1 + (edges - X_HSYNC) // X_MOD
    return f"rises {rises} count {y_count(edges)} {name}"


def _finish(log, reason: str | None) -> str:
    text = reason or "IC Beam Y logic works ok"
    log.note(text)
    return text


def run_virtual() -> str:
    from pins import BitLog, ModelPins, record_bits

    log = BitLog(LOG, NAME)
    try:
        reason = record_bits(
            log,
            STOPS,
            BITS,
            level,
            place,
            lambda bit: ModelPins(lambda edges, bit=bit: level(edges, bit)),
        )
        return _finish(log, reason)
    finally:
        log.close()


def run_serial(clock_name: str, watch_name: str, prepare, hz: float = 10) -> str:
    from pins import BitLog, check_serial

    log = BitLog(LOG, NAME)
    try:
        reason = check_serial(clock_name, watch_name, STOPS, BITS, level, prepare, log, place, hz)
        if reason == "count ok":
            reason = None
        return _finish(log, reason)
    finally:
        log.close()


NAME = "Beam Y"
CHIP = "ATF22V10C"
