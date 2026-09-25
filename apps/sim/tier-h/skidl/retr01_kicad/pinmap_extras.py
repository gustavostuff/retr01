"""Tier-H / locked-19 parts not in the base pinmap snapshot."""

from __future__ import annotations

from typing import Dict, List

from . import pinmap


def _nums(n: int) -> List[str]:
    return [str(i) for i in range(1, n + 1)]


def _hc573_aliases() -> Dict[str, str]:
    d = {"1": "OE#", "10": "GND", "11": "LE", "20": "VCC"}
    for i in range(8):
        d[str(2 + i)] = f"D{i}"
        d[str(12 + i)] = f"Q{i}"
    return d


def _hc574_aliases() -> Dict[str, str]:
    d = {"1": "OE#", "10": "GND", "11": "CLK", "20": "VCC"}
    for i in range(8):
        d[str(2 + i)] = f"D{i}"
        d[str(12 + i)] = f"Q{i}"
    return d


def apply_pinmap_extras() -> None:
    t = pinmap.PIN_TEMPLATES
    a = pinmap.KICAD_ALIASES

    t["SN74HC573"] = _nums(20)
    t["SN74HC574"] = _nums(20)
    t["AVR128DB28"] = _nums(28)
    t["ATtiny85"] = _nums(8)
    t["OSC4LEGS"] = _nums(14)
    t["AD724"] = _nums(16)
    for key in ("RCJ-012", "RCJ-014", "AUDIO_OUT", "COMPOSITE_OUT"):
        t[key] = ["1A", "1B", "1C", "2"]
    a["RCJ-012"] = {"2": "SIGNAL", "1A": "GND", "1B": "GND", "1C": "GND"}
    a["RCJ-014"] = dict(a["RCJ-012"])

    a["SN74HC573"] = _hc573_aliases()
    a["SN74HC574"] = _hc574_aliases()
    a["ATtiny85"] = {
        "1": "PB5",
        "2": "PB3",
        "3": "PB4",
        "4": "GND",
        "5": "PB0",
        "6": "PB1",
        "7": "PB2",
        "8": "VCC",
    }
    a["OSC4LEGS"] = {
        "1": "1",
        "7": "7",
        "8": "8",
        "14": "14",
    }
    # KiCad Oscillator:Oscillator_DIP-8 only exposes pads 1, 4, 5, 8 (not 2/3/6/7).
    for key in ("OSC8M", "OSC_DOT", "OSC_4FSC"):
        t[key] = ["1", "4", "5", "8"]
        aliases = {
            "1": "OE#",
            "4": "GND",
            "5": "PHI2" if key == "OSC8M" else "OUT",
            "8": "VDD",
        }
        if key == "OSC_DOT":
            aliases["5"] = "DOT"
        if key == "OSC_4FSC":
            aliases["5"] = "OUT"
        a[key] = aliases

    # W65C02S: sim uses signal names on physical pin numbers (see pinmap CPU_*).
    w65: Dict[str, str] = {}
    for i in range(16):
        if i in pinmap.CPU_A:
            w65[pinmap.CPU_A[i]] = f"A{i}"
    for i in range(8):
        w65[pinmap.CPU_D[i]] = f"D{i}"
    w65[pinmap.CPU_VPB] = "VPB"
    w65[pinmap.CPU_RDY] = "RDY"
    w65[pinmap.CPU_PHI1O] = "PHI1O"
    w65[pinmap.CPU_IRQB] = "IRQB"
    w65[pinmap.CPU_MLB] = "MLB"
    w65[pinmap.CPU_NMIB] = "NMIB"
    w65[pinmap.CPU_SYNC] = "SYNC"
    w65[pinmap.CPU_VDD] = "VDD"
    w65[pinmap.CPU_VSS] = "VSS"
    w65[pinmap.CPU_RWB] = "RWB"
    w65[pinmap.CPU_BE] = "BE"
    w65[pinmap.CPU_PHI2] = "PHI2"
    w65[pinmap.CPU_RESB] = "RESB"
    a["W65C02S"] = w65
