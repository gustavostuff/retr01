"""Motherboard-only Tier H Skidl export (17 counted ICs per docs/general/hardware.md).

Drops cart silicon, pad MCUs, sim-only PLD helpers, and the sim PMIC (PS1).
Remaps cart flash/EEPROM nodes onto J36. Maps PS1 power pins onto J1.
"""

from __future__ import annotations

from typing import Optional, Tuple

from . import pinmap as P

Node = Tuple[str, str, int]  # refdes, pin name, pin num

# Sim / other-board refdes omitted from the mobo preliminary netlist.
SKIP_REFDES = frozenset(
    {
        "?",
        "SCR1",
        "PAD",
        "UPAD1",
        "UPAD2",
        "U4",  # PRG_ROM sim entity, not a mobo IC
        "UPLDA",
        "UPLDB",
        "UPLDI",
        "UPLDN",
        "UPLDP",
    }
)

# DIP/PLD/MCU on the locked 17-IC motherboard (+ support outside count is separate).
MOBO_COUNTED_IC_REFDES = frozenset(
    {
        "U1",
        "U3",
        "U6",
        "U24",
        "U41",
        "UM",
        "US1",
        "US2",
        "U7A",
        "U7B",
        "U7C",
        "U573",
        "U574",
        "UPLDX",
        "UPLDY",
        "UPLDV",
        "U725",
    }
)


def _flash_to_j36() -> dict[str, str]:
    out: dict[str, str] = {}
    for i in range(14):
        out[P.FLASH_A[i]] = P.cart_a(i + 4)
    for i, edge_n in enumerate(range(13, 18)):
        bit = 14 + i
        out[P.FLASH_A[bit]] = P.cart_b(edge_n)
    for i in range(8):
        out[P.FLASH_D[i]] = P.cart_b(i + 4)
    out[P.FLASH_OE] = P.cart_b(12)
    out[P.FLASH_WE] = P.cart_b(18)
    out[P.FLASH_VCC] = P.cart_a(2)
    out[P.FLASH_GND] = P.cart_a(1)
    return out


_FLASH_J36 = _flash_to_j36()
_EE_J36 = {
    P.EE_SDA: P.cart_a(3),
    P.EE_SCL: P.cart_b(3),
    P.EE_VCC: P.cart_a(2),
    P.EE_GND: P.cart_a(1),
    P.EE_A0: P.cart_a(1),
    P.EE_A1: P.cart_a(1),
    P.EE_A2: P.cart_a(1),
    P.EE_WP: P.cart_a(1),
}

_PS1_J1 = {
    "1": "1",  # VIN -> barrel tip
    "3": "1",  # VDD on +5V net with other loads; input still at barrel tip
    "4": "2",  # GND -> sleeve
}


def normalize_export_node(ref: str, pin_name: str, pin_num: int) -> Optional[Node]:
    """Return mobo refdes/pin for Skidl, or None to drop."""
    if ref in SKIP_REFDES:
        return None
    sn = str(pin_num)
    if ref == "U40":
        j36_pin = _FLASH_J36.get(sn)
        if j36_pin is None:
            return None
        return ("J36", pin_name or "", int(j36_pin))
    if ref == "U50":
        j36_pin = _EE_J36.get(sn)
        if j36_pin is None:
            return None
        return ("J36", pin_name or "", int(j36_pin))
    if ref == "PS1":
        j1_pin = _PS1_J1.get(sn)
        if j1_pin is None:
            return None
        return ("J1", pin_name or "", int(j1_pin))
    return (ref, pin_name or "", pin_num)


def is_mobo_ic_refdes(ref: str) -> bool:
    return ref in MOBO_COUNTED_IC_REFDES
