"""Assign meaningful, human-readable net names to preliminary PCB nets.

Replaces auto-generated NET_NNN identifiers with canonical bus, strobe,
and interconnect labels matching Retr01 architecture documentation.
"""

from __future__ import annotations

from typing import Dict, List, Optional, Set

from .power_rails import _rail_markers


# Priority 1: Direct refdes + pin number to descriptive signal name
_PIN_NAME_TABLE: Dict[tuple[str, str], str] = {
    # CPU W65C02S (U1)
    ("U1", "1"): "CPU_VPB",
    ("U1", "2"): "CPU_RDY",
    ("U1", "3"): "CPU_PHI1O",
    ("U1", "4"): "CPU_IRQ#",
    ("U1", "5"): "CPU_MLB",
    ("U1", "6"): "CPU_NMIB",
    ("U1", "7"): "CPU_SYNC",
    ("U1", "9"): "CPU_A0",
    ("U1", "10"): "CPU_A1",
    ("U1", "11"): "CPU_A2",
    ("U1", "12"): "CPU_A3",
    ("U1", "13"): "CPU_A4",
    ("U1", "14"): "CPU_A5",
    ("U1", "15"): "CPU_A6",
    ("U1", "16"): "CPU_A7",
    ("U1", "17"): "CPU_A8",
    ("U1", "18"): "CPU_A9",
    ("U1", "19"): "CPU_A10",
    ("U1", "20"): "CPU_A11",
    ("U1", "22"): "CPU_A12",
    ("U1", "23"): "CPU_A13",
    ("U1", "24"): "CPU_A14",
    ("U1", "25"): "CPU_A15",
    ("U1", "26"): "CPU_D7",
    ("U1", "27"): "CPU_D6",
    ("U1", "28"): "CPU_D5",
    ("U1", "29"): "CPU_D4",
    ("U1", "30"): "CPU_D3",
    ("U1", "31"): "CPU_D2",
    ("U1", "32"): "CPU_D1",
    ("U1", "33"): "CPU_D0",
    ("U1", "34"): "CPU_RW",
    ("U1", "36"): "CPU_BE",
    ("U1", "37"): "CPU_PHI2",
    ("U1", "38"): "CPU_SOB",
    ("U1", "39"): "CPU_PHI2O",
    ("U1", "40"): "CPU_RES#",

    # Cartridge edge connector (J36)
    ("J36", "3"): "CART_SDA",
    ("J36", "4"): "CART_A0",
    ("J36", "5"): "CART_A1",
    ("J36", "6"): "CART_A2",
    ("J36", "7"): "CART_A3",
    ("J36", "8"): "CART_A4",
    ("J36", "9"): "CART_A5",
    ("J36", "10"): "CART_A6",
    ("J36", "11"): "CART_A7",
    ("J36", "12"): "CART_A8",
    ("J36", "13"): "CART_A9",
    ("J36", "14"): "CART_A10",
    ("J36", "15"): "CART_A11",
    ("J36", "16"): "CART_A12",
    ("J36", "17"): "CART_A13",
    ("J36", "21"): "CART_SCL",
    ("J36", "22"): "CART_D0",
    ("J36", "23"): "CART_D1",
    ("J36", "24"): "CART_D2",
    ("J36", "25"): "CART_D3",
    ("J36", "26"): "CART_D4",
    ("J36", "27"): "CART_D5",
    ("J36", "28"): "CART_D6",
    ("J36", "29"): "CART_D7",
    ("J36", "30"): "CART_OE#",
    ("J36", "31"): "CART_A14",
    ("J36", "32"): "CART_A15",
    ("J36", "33"): "CART_A16",
    ("J36", "34"): "CART_A17",
    ("J36", "35"): "CART_A18",
    ("J36", "36"): "CART_WE#",

    # Main System RAM AS6C62256 (U3)
    ("U3", "20"): "RAM_CE#",
    ("U3", "22"): "RAM_OE#",
    ("U3", "27"): "RAM_WE#",

    # Video VRAM AS6C62256 (U6)
    ("U6", "20"): "VRAM_CE#",
    ("U6", "22"): "VRAM_OE#",
    ("U6", "27"): "VRAM_WE#",

    # Line Buffer AS6C62256 (U41)
    ("U41", "20"): "LINEBUF_CE#",
    ("U41", "22"): "LINEBUF_OE#",
    ("U41", "27"): "LINEBUF_WE#",

    # Color PROM AT27C256R (U24)
    ("U24", "20"): "PROM_CE#",
    ("U24", "22"): "PROM_OE#",
    ("U24", "11"): "PROM_O0",
    ("U24", "12"): "PROM_O1",
    ("U24", "13"): "PROM_O2",
    ("U24", "15"): "PROM_O3",
    ("U24", "16"): "PROM_O4",
    ("U24", "17"): "PROM_O5",
    ("U24", "18"): "PROM_O6",
    ("U24", "19"): "PROM_O7",

    # Video DAC Resistors
    ("R1", "1"): "DAC_R2",
    ("R2", "1"): "DAC_R1",
    ("R3", "1"): "DAC_R0",
    ("R4", "1"): "DAC_G2",
    ("R5", "1"): "DAC_G1",
    ("R6", "1"): "DAC_G0",
    ("R7", "1"): "DAC_B1",
    ("R8", "1"): "DAC_B0",
    ("R9", "1"): "VIDEO_RED",
    ("R10", "1"): "VIDEO_GREEN",
    ("R11", "1"): "VIDEO_BLUE",

    # Damping / Series Resistors
    ("R12", "1"): "PHI2_SERIES",
    ("R13", "1"): "DOT_SERIES",
    ("R22", "1"): "CART_OE#",
    ("R23", "1"): "CART_WE#",
    ("R24", "1"): "MCU_SDA",
    ("R25", "1"): "MCU_SCL",
    ("R26", "2"): "PAD_DATA_BUS",
    ("R27", "2"): "SDA_PULLUP",
    ("R28", "2"): "SCL_PULLUP",
    ("R29", "2"): "CPU_RDY_PULLUP",
    ("R30", "2"): "CPU_RES_PULLUP",

    # Latch U573 (Field ALE)
    ("U573", "1"): "ALE_OE#",
    ("U573", "11"): "ALE_LE",
    ("U573", "19"): "LB_A0",
    ("U573", "18"): "LB_A1",
    ("U573", "17"): "LB_A2",
    ("U573", "16"): "LB_A3",
    ("U573", "15"): "LB_A4",
    ("U573", "14"): "LB_A5",
    ("U573", "13"): "LB_A6",
    ("U573", "12"): "LB_A7",

    # Latch U574 (Scroll X)
    ("U574", "1"): "SCROLL_OE#",
    ("U574", "11"): "SCROLL_CLK",

    # Multiplexers (U7A, U7B, U7C)
    ("U7A", "1"): "MUX_VRAM_SEL",
    ("U7A", "15"): "MUX_VRAM_EN#",
    ("U7B", "1"): "MUX_B_SEL",
    ("U7B", "15"): "MUX_B_EN#",
    ("U7C", "1"): "MUX_C_SEL",
    ("U7C", "15"): "MUX_C_EN#",

    # MCU-M (UM) — AVR128DB28 physical SPDIP
    ("UM", "19"): "MCU_M_UPDI",
    ("UM", "2"): "SPI_MOSI",
    ("UM", "3"): "SPI_MISO",
    ("UM", "4"): "SPI_SCK",
    ("UM", "5"): "SS_S1#",
    ("UM", "11"): "SS_S2#",
    ("UM", "8"): "CPU_RDY",
    ("UM", "24"): "MCU_SDA",
    ("UM", "25"): "MCU_SCL",

    # MCU-S1 (US1)
    ("US1", "19"): "MCU_S1_UPDI",

    # MCU-S2 (US2)
    ("US2", "19"): "MCU_S2_UPDI",
    ("US2", "17"): "AUDIO_PWM",
    ("US2", "16"): "PAD_DATA_BUS",

    # Crystal oscillators
    ("U04", "1"): "XTAL_21M_IN",
    ("U04", "2"): "XTAL_21M_OUT",
    ("U04", "5"): "XTAL_10M_IN",
    ("U04", "6"): "XTAL_10M_OUT",
    ("U725", "3"): "FSC_FIN",
    ("U725", "10"): "COMPOSITE_OUT",

    # Connectors
    ("J2", "1"): "VIDEO_RED",
    ("J2", "2"): "VIDEO_GREEN",
    ("J2", "3"): "VIDEO_BLUE",
    ("J2", "4"): "VIDEO_CSYNC",
    ("J3", "R"): "PAD1_DATA",
    ("J4", "R"): "PAD2_DATA",
    ("J5", "1"): "UPDI_PROG_DATA",
    ("J7", "3"): "RESET_SWITCH#",
}

# Protected nets that must never be altered or renamed
_PRESERVED_NETS = frozenset(
    {
        "+5V",
        "GND",
        "NC",
        "AUDIO_OUT",
        "COMPOSITE_OUT",
        "CLK_10M",
        "CLK_10M_INV",
        "CLK_21M",
        "DOT",
        "DOT_INV",
        "FSC_XTAL",
        "FSC_FIN",
        "CSYNC",
        "PHI2",
    }
)


def _suggest_name_for_pins(pins: list) -> Optional[str]:
    """Inspect pins attached to a net and return best human-readable name."""
    priority_order = (
        "U1",
        "J36",
        "UM",
        "US1",
        "US2",
        "U04",
        "U725",
        "U24",
        "U3",
        "U6",
        "U41",
        "R9",
        "R10",
        "R11",
        "U573",
        "U574",
        "J2",
        "J3",
        "J4",
        "J5",
        "J7",
    )

    # Match in priority order
    for pref in priority_order:
        for pin in pins:
            part = getattr(pin, "part", None)
            if part is None:
                continue
            ref = getattr(part, "ref", "")
            if ref == pref:
                num = str(getattr(pin, "num", ""))
                cand = _PIN_NAME_TABLE.get((ref, num))
                if cand:
                    return cand

    # Match any other entry in the lookup table
    for pin in pins:
        part = getattr(pin, "part", None)
        if part is None:
            continue
        ref = getattr(part, "ref", "")
        num = str(getattr(pin, "num", ""))
        cand = _PIN_NAME_TABLE.get((ref, num))
        if cand:
            return cand

    # Point-to-point descriptive interconnect names
    valid_pins = [p for p in pins if getattr(p, "part", None) is not None]
    if len(valid_pins) == 2:
        r1 = getattr(valid_pins[0].part, "ref", "")
        n1 = str(getattr(valid_pins[0], "num", ""))
        r2 = getattr(valid_pins[1].part, "ref", "")
        n2 = str(getattr(valid_pins[1], "num", ""))
        if r1 and r2:
            return f"SIG_{r1}_P{n1}_TO_{r2}_P{n2}"

    if len(valid_pins) == 1:
        r1 = getattr(valid_pins[0].part, "ref", "")
        n1 = str(getattr(valid_pins[0], "num", ""))
        pname = getattr(valid_pins[0], "name", "")
        if pname:
            return f"SIG_{r1}_{pname}"
        return f"SIG_{r1}_P{n1}"

    if len(valid_pins) > 2:
        r1 = getattr(valid_pins[0].part, "ref", "")
        n1 = str(getattr(valid_pins[0], "num", ""))
        return f"SIG_{r1}_P{n1}_BUS"

    return None


def name_all_nets(nets: list) -> None:
    """Iterate over all nets and replace generic NET_NNN names with explicit labels."""
    used_names: Set[str] = set()

    # Collect existing preserved names
    for net in nets:
        name = getattr(net, "name", "") or ""
        if name in _PRESERVED_NETS:
            used_names.add(name)

    for net in nets:
        curr_name = getattr(net, "name", "") or ""
        if curr_name in _PRESERVED_NETS:
            continue

        pins = getattr(net, "pins", []) or []
        vdd_m, gnd_m = _rail_markers(pins)
        if gnd_m >= 3 and gnd_m >= vdd_m:
            net.name = "GND"
            used_names.add("GND")
            continue
        if vdd_m >= 4 and vdd_m >= gnd_m:
            net.name = "+5V"
            used_names.add("+5V")
            continue

        suggested = _suggest_name_for_pins(pins)
        if not suggested:
            suggested = curr_name

        # Ensure uniqueness across the netlist
        candidate = suggested
        counter = 1
        while candidate in used_names:
            candidate = f"{suggested}_{counter}"
            counter += 1

        net.name = candidate
        used_names.add(candidate)
