"""Behavioral model of UPLDA SEL decode (matches expect_fit/uplda_decode.pld)."""

from __future__ import annotations

# Port offset -> shared SEL pin name in the .pld
PORT_TO_SEL = {
    0x00: "S00",
    0x06: "S00",
    0x02: "S02",
    0x07: "S02",
    0x03: "S03",
    0x04: "S04",
    0x05: "S05",
    0x08: "S08",
    0x90: "S90",
    0x91: "S91",
    0x92: "S92",
    0x93: "S92",
    0x10: "S10",
    0x11: "S10",
    0x12: "S10",
}

ALL_SEL = ("S00", "S02", "S03", "S04", "S05", "S08", "S90", "S91", "S92", "S10")

# Every FE port the motherboard actually decodes today.
REAL_PORTS = tuple(sorted(PORT_TO_SEL.keys()))


def sel_active(port: int, *, fe: bool = True, be: bool = True, rwb: bool = False) -> dict[str, bool]:
    """Return which SEL outputs assert for a CPU write/read cycle.

    rwb False = write (6502 RWB low). Decode equations qualify /RWB.
    """
    out = {name: False for name in ALL_SEL}
    if not (fe and be and (not rwb)):
        return out
    name = PORT_TO_SEL.get(port & 0xFF)
    if name is not None:
        out[name] = True
    return out


def exclusive_sels(active: dict[str, bool]) -> int:
    return sum(1 for v in active.values() if v)
