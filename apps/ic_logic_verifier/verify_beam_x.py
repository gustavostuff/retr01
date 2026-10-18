"""Clock Beam X from a CH340 TXD pin and sample one output at a time."""

from __future__ import annotations

from pathlib import Path

MOD = 341
LOG = Path(__file__).resolve().with_name("verify_beam_x.log")
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


def place(edges: int, name: str) -> str:
    return f"edges {edges} count {edges % MOD} {name}"


def _finish(log, reason: str | None) -> str:
    text = reason or "IC Beam X logic works ok"
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


def run_serial(clock_name: str, watch_name: str, prepare) -> str:
    from pins import BitLog, check_serial

    log = BitLog(LOG, NAME)
    try:
        reason = check_serial(clock_name, watch_name, STOPS, BITS, level, prepare, log, place)
        if reason == "count ok":
            reason = None
        return _finish(log, reason)
    finally:
        log.close()


NAME = "Beam X"
CHIP = "ATF22V10C"
