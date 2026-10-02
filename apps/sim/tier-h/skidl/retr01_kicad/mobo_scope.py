"""Motherboard-only Tier H Skidl export (19 counted ICs per docs/general/hardware.md).

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
        "OSC8M",
        "OSC_DOT",
        "OSC_4FSC",
        "UPLDA",
        "UPLDB",
        "UPLDI",
        "UPLDN",
        "UPLDP",
    }
)

# DIP/PLD/MCU on the locked 19-IC motherboard (+ support outside count is separate).
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
        "U04",
        "U74",
    }
)


def _flash_to_j36() -> dict[str, str]:
    """Map cart-flash silicon pads onto J36.

    A0-A13 / D / OE# / WE# / power only. Do **not** remap flash A14-A18: those
    cart pins are MAP-driven (JSON already has J36 B13-B17). The sim still ties
    flash A14/A15 to CPU_A14/A15, so remapping them would short CPU onto MAP.
    """
    out: dict[str, str] = {}
    for i in range(14):
        out[P.FLASH_A[i]] = P.cart_a(i + 4)
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
}

_PS1_J1 = {
    "1": "1",  # VIN -> barrel center (+5V)
    "3": "1",  # VDD on +5V net with other loads; input still at barrel center
    "4": "2",  # GND -> sleeve
}


_SKIP_PIN_NAMES = frozenset({"CLK", "RUN"})


def normalize_export_node(ref: str, pin_name: str, pin_num: int) -> Optional[Node]:
    """Return mobo refdes/pin for Skidl, or None to drop."""
    if ref in SKIP_REFDES:
        return None
    # Do not clamp by pin count here: U1/J36/UM are 36–40 pin parts. Bad nodes
    # are dropped via SKIP_REFDES and per-part remap tables below.
    if pin_name in _SKIP_PIN_NAMES or pin_num < 1:
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
