"""Resolve Tier H JSON (refdes, sim part) -> SKiDL MPN + KiCad footprint."""

from __future__ import annotations

from typing import Iterable, Optional, Tuple

from . import footprints as fp

Res = Tuple[str, str]

REFDES: dict[str, Res] = {
    "U1": ("W65C02S", fp.DIP40),
    "U3": ("AS6C62256", fp.DIP28),
    "U6": ("AS6C62256", fp.DIP28),
    "U24": ("AT27C256R", fp.DIP28),
    "U41": ("AS6C62256", fp.DIP28),
    "U573": ("SN74HC573", fp.DIP20),
    "U574": ("SN74HC574", fp.DIP20),
    "U7A": ("SN74HC157", fp.DIP16),
    "U7B": ("SN74HC157", fp.DIP16),
    "U7C": ("SN74HC157", fp.DIP16),
    "UM": ("AVR128DB28", fp.DIP28N),
    "US1": ("AVR128DB28", fp.DIP28N),
    "US2": ("AVR128DB28", fp.DIP28N),
    "UPLDX": ("ATF22V10", fp.DIP24),
    "UPLDY": ("ATF22V10", fp.DIP24),
    "UPLDV": ("ATF22V10", fp.DIP24),
    "U725": ("AD724", fp.DIP16),
    "J1": ("BARREL_5V", fp.BARREL),
    "J2": ("RGBS_HDR", fp.HDR6),
    "J3": ("TRS_P1", fp.TRS),
    "J4": ("TRS_P2", fp.TRS),
    "J5": ("ARCADE_2x10", fp.HDR2x10),
    "J7": ("CAB_PWR_RST", fp.HDR4),
    "J8": ("RCJ-012", fp.RCA_AUDIO),
    "J9": ("RCJ-014", fp.RCA),
    "J36": ("CART_EDGE_36", fp.EDGE36_MOBO),
    "Y1": ("OSC8M", fp.OSC8),
    "Y2": ("OSC_DOT", fp.OSC8),
    "Y3": ("OSC_4FSC", fp.OSC8),
}

PART: dict[str, Res] = {
    "W65C02S": ("W65C02S", fp.DIP40),
    "AS6C62256": ("AS6C62256", fp.DIP28),
    "AT27C256R": ("AT27C256R", fp.DIP28),
    "PRG_ROM": ("AT27C256R", fp.DIP28),
    "SST39SF040": ("SST39SF040", fp.DIP32),
    "24C64": ("24C64", fp.DIP8),
    "SN74HC157": ("SN74HC157", fp.DIP16),
    "SN74HC573": ("SN74HC573", fp.DIP20),
    "SN74HC574": ("SN74HC574", fp.DIP20),
    "ATF22V10": ("ATF22V10", fp.DIP24),
    "AVR128DB28": ("AVR128DB28", fp.DIP28N),
    "ATtiny85": ("ATtiny85", fp.DIP8),
    "PADS": ("ATtiny85", fp.DIP8),
    "PWR5V": ("BARREL_5V", fp.BARREL),
    "OSC8M": ("OSC8M", fp.OSC8),
    "OSC_DOT": ("OSC_DOT", fp.OSC8),
    "OSC4LEGS": ("OSC4LEGS", fp.DIP14),
    "OSC_4FSC": ("OSC_4FSC", fp.OSC8),
    "AD724": ("AD724", fp.DIP16),
    "RCJ-012": ("RCJ-012", fp.RCA_AUDIO),
    "RCJ-014": ("RCJ-014", fp.RCA),
    "CCAP": ("C_100N", fp.C_CER),
    "ECAP": ("C_BULK", fp.C_ELEC),
    "R": ("R_33", fp.R_AX),
    "SPRITE_FETCH": ("ATF22V10", fp.DIP24),
    "INTEGRATION": ("ATF22V10", fp.DIP24),
}

RESISTOR_REFDES: dict[str, str] = {
    "R1": "R_4K",
    "R2": "R_4K",
    "R3": "R_2K",
    "R4": "R_4K",
    "R5": "R_2K",
    "R6": "R_2K",
    "R7": "R_1K",
    "R8": "R_1K",
    "R9": "R_75",
    "R10": "R_75",
    "R11": "R_75",
    **{f"R{i}": "R_33" for i in range(12, 26)},
    "R26": "R_4K7",
    "R27": "R_4K7",
    "R28": "R_4K7",
    "R29": "R_4K7",
    "R30": "R_10K",
}

CAP_REFDES: dict[str, str] = {f"C{i}": "C_100N" for i in range(1, 22)}
CAP_REFDES.update({f"C{i}": "C_22P" for i in range(22, 28)})


def resolve(refdes: str, part_hints: Iterable[str]) -> Optional[Res]:
    if refdes in REFDES:
        return REFDES[refdes]
    if refdes.startswith("R") and refdes in RESISTOR_REFDES:
        mpn = RESISTOR_REFDES[refdes]
        return (mpn, fp.R_AX)
    if refdes.startswith("C") and refdes in CAP_REFDES:
        mpn = CAP_REFDES[refdes]
        foot = fp.C_CER if mpn == "C_100N" else fp.C_CER
        return (mpn, foot)
    if refdes == "E1":
        return ("C_BULK", fp.C_ELEC)
    hints = list(part_hints)
    for hint in hints:
        if hint in PART:
            return PART[hint]
    if refdes.startswith("Y"):
        if "OSC_DOT" in hints:
            return ("OSC_DOT", fp.OSC8)
        if "OSC8M" in hints:
            return ("OSC8M", fp.OSC8)
        if "OSC4LEGS" in hints:
            return ("OSC4LEGS", fp.DIP14)
        return ("OSC_4FSC", fp.OSC8)
    return None
