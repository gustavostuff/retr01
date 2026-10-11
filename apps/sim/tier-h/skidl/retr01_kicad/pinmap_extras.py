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
    a["AD724"] = {
        "1": "STND",
        "2": "AGND",
        "3": "FIN",
        "4": "APOS",
        "5": "ENCD",
        "6": "RIN",
        "7": "GIN",
        "8": "BIN",
        "9": "CRMA",
        "10": "COMP",
        "11": "LUMA",
        "12": "SELECT",
        "13": "DGND",
        "14": "DPOS",
        "15": "VSYNC",
        "16": "HSYNC",
    }
    t["74HCU04"] = _nums(14)
    t["SN74HCU04"] = _nums(14)
    t["74HC74"] = _nums(14)
    t["SN74HC74"] = _nums(14)
    t["XTAL"] = ["1", "2"]
    t["R_1M"] = _nums(2)
    t["BARREL_5V"] = ["1", "2", "3"]
    t["RGBS_HDR"] = _nums(8)
    t["CART_PROG_HDR"] = _nums(4)
    t["ARCADE_P1"] = _nums(10)
    t["ARCADE_P2"] = _nums(10)
    t["MCP130"] = ["1", "2", "3"]
    t["TRS_P1"] = ["S", "T", "R", "TN", "RN"]
    t["TRS_P2"] = ["S", "T", "R", "TN", "RN"]
    t["TRS635_P1"] = ["S", "T", "R", "TN", "RN", "SN"]
    t["TRS635_P2"] = ["S", "T", "R", "TN", "RN", "SN"]
    for key in ("RCJ-012", "RCJ-014", "AUDIO_OUT", "COMPOSITE_OUT"):
        t[key] = ["1A", "1B", "1C", "2"]
    a["RCJ-012"] = {"2": "SIGNAL", "1A": "GND", "1B": "GND", "1C": "GND"}
    a["RCJ-014"] = dict(a["RCJ-012"])
    a["BARREL_5V"] = {"1": "+5V", "2": "GND", "3": "GND"}
    a["RGBS_HDR"] = {
        "1": "R",
        "2": "G",
        "3": "B",
        "4": "CSYNC",
        "5": "HSYNC",
        "6": "VSYNC",
        "7": "GND",
        "8": "GND",
    }
    a["CART_PROG_HDR"] = {
        "1": "PWR",
        "2": "GND",
        "3": "DATA",
        "4": "GND",
    }
    a["MCP130"] = {"1": "RESET#", "2": "VDD", "3": "VSS"}
    a["TRS_P1"] = {"S": "GND", "T": "+5V", "R": "DATA", "TN": "NC", "RN": "NC"}
    a["TRS_P2"] = dict(a["TRS_P1"])
    a["TRS635_P1"] = {"S": "GND", "T": "+5V", "R": "DATA", "TN": "NC", "RN": "NC", "SN": "NC"}
    a["TRS635_P2"] = dict(a["TRS635_P1"])
    a["XTAL"] = {"1": "1", "2": "2"}
    a["74HCU04"] = {
        "1": "1A", "2": "1Y",
        "3": "2A", "4": "2Y",
        "5": "3A", "6": "3Y",
        "7": "GND",
        "8": "4Y", "9": "4A",
        "10": "5Y", "11": "5A",
        "12": "6Y", "13": "6A",
        "14": "VCC",
    }
    a["SN74HCU04"] = dict(a["74HCU04"])
    a["74HC74"] = {
        "1": "1CLR#", "2": "1D", "3": "1CLK", "4": "1PRE#", "5": "1Q", "6": "1/Q",
        "7": "GND",
        "8": "2/Q", "9": "2Q", "10": "2PRE#", "11": "2CLK", "12": "2D", "13": "2CLR#",
        "14": "VCC",
    }
    a["SN74HC74"] = dict(a["74HC74"])

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
