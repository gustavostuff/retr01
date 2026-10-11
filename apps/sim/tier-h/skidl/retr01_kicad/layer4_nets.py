"""Layer 4 KiCad net class (quiet analog plus slow digital / I/O).

Assignment follows docs/bring-up-v2/main-pcb-layers.md. Layer 1 keeps clock-rate
buses, digital squares, and +5V. Layer 4 gets analog, reset/ready, handshake,
I2C, pads, arcade, sync, MAP A14-A18, and SPI chip-selects.

The class exists so Pcbnew can color those air wires separately. Track and via
geometry match the Default class.
"""

from __future__ import annotations

import fnmatch
import json
import re
from typing import Iterable, Optional

CLASS_NAME = "Layer4"

# Same geometry as Default in v_0*.kicad_pro. Color is a starting point.
KICAD_CLASS = {
    "bus_width": 12,
    "clearance": 0.2,
    "diff_pair_gap": 0.25,
    "diff_pair_via_gap": 0.25,
    "diff_pair_width": 0.2,
    "line_style": 0,
    "microvia_diameter": 0.3,
    "microvia_drill": 0.1,
    "name": CLASS_NAME,
    "pcb_color": "rgb(220, 143, 50)",
    "priority": 0,
    "schematic_color": "rgb(220, 143, 50)",
    "track_width": 0.2,
    "tuning_profile": "",
    "via_diameter": 0.6,
    "via_drill": 0.3,
    "wire_width": 6,
}

# Never Layer 4, even if a prefix could match.
_NEVER = frozenset(
    {
        "+5V",
        "GND",
        "NC",
        "PHI2",
        "DOT",
        "DOT_INV",
        "CLK_10M",
        "CLK_10M_INV",
        "CLK_21M",
        "CPU_PHI2",
        "CPU_PHI1O",
        "CPU_PHI2O",
        "SPI_MOSI",
        "SPI_MISO",
        "SPI_SCK",
        "ALE_LE",
        "ALE_OE#",
        "U74_PRE_CLR#",
        "U04_SPARE_IN",
        "CPU_RW",
        "CPU_BE",
        "CPU_MLB",
        "CPU_SYNC",
        "CPU_SOB",
        "CPU_VPB",
    }
)

_NEVER_PREFIXES = (
    "CPU_A",
    "CPU_D",
    "CART_D",
    "PROM_O",
    "DAC_",
    "LB_A",
    "RAM_",
    "VRAM_",
    "LINEBUF_",
    "MUX_",
    "SPI_",
    "CLK_",
)

# CART_A0-A13 stay on layer 1. CART_A14-A18 are MAP high bits on layer 4.
_NEVER_CART_A = frozenset(f"CART_A{i}" for i in range(14))

# KiCad netclass_patterns use * and ? wildcards.
KICAD_PATTERNS = (
    "VIDEO*",
    "XTAL*",
    "FSC*",
    "AUDIO*",
    "COMPOSITE*",
    "PAD*",
    "MCU_SDA*",
    "MCU_SCL*",
    "CART_SDA*",
    "CART_SCL*",
    "MCU_M_UPDI*",
    "MCU_S1_UPDI*",
    "MCU_S2_UPDI*",
    "UPDI*",
    "CART_PROG*",
    "CART_ARM*",
    "J10_PWR*",
    "SS_S1*",
    "SS_S2*",
    "CART_A14",
    "CART_A15",
    "CART_A16",
    "CART_A17",
    "CART_A18",
    "CPU_RDY*",
    "CPU_RES*",
    "CPU_IRQ*",
    "CPU_NMIB",
    "RESET*",
    "CSYNC",
    "HSYNC",
    "VSYNC",
    "SDA_PULLUP*",
    "SCL_PULLUP*",
    "SCROLL_CLK*",
    "SIG_J5*",
    "SIG_J6*",
    "SIG_US2*",
    "SIG_UM_*",
    "SIG_U725*",
    "SIG_J3*",
    "SIG_J4*",
    "SIG_J7*",
    "SIG_J8*",
    "SIG_J9*",
    "SIG_J10*",
)

_EXACT = frozenset(
    {
        "VIDEO_RED",
        "VIDEO_GREEN",
        "VIDEO_BLUE",
        "VIDEO_CSYNC",
        "VIDEO_HSYNC",
        "VIDEO_VSYNC",
        "CSYNC",
        "HSYNC",
        "VSYNC",
        "XTAL_DOT_IN",
        "XTAL_DOT_OUT",
        "XTAL_CPU_IN",
        "XTAL_CPU_OUT",
        "XTAL_21M_IN",
        "XTAL_21M_OUT",
        "XTAL_10M_IN",
        "XTAL_10M_OUT",
        "FSC_FIN",
        "FSC_XTAL",
        "COMPOSITE_OUT",
        "AUDIO_PWM",
        "AUDIO_OUT",
        "CPU_RDY",
        "CPU_RDY_PULLUP",
        "CPU_RES#",
        "CPU_RES_PULLUP",
        "RESET_SWITCH#",
        "CPU_IRQ#",
        "CPU_NMIB",
        "MCU_SDA",
        "MCU_SCL",
        "SDA_PULLUP",
        "SCL_PULLUP",
        "CART_SDA",
        "CART_SCL",
        "MCU_M_UPDI",
        "MCU_S1_UPDI",
        "MCU_S2_UPDI",
        "CART_PROG_DATA",
        "CART_ARM",
        "J10_PWR_NC",
        "PAD_DATA_BUS",
        "PAD1_DATA",
        "PAD2_DATA",
        "SS_S1#",
        "SS_S2#",
        "CART_A14",
        "CART_A15",
        "CART_A16",
        "CART_A17",
        "CART_A18",
        "SCROLL_CLK",
    }
)

# Handshake / GPIO pins that often export as SIG_* names.
_LAYER4_PINS = frozenset(
    {
        ("UM", "7"),
        ("UM", "9"),
        ("UM", "10"),
        ("UM", "16"),
        ("UM", "17"),
        ("UM", "18"),
        ("UM", "19"),
        ("US1", "18"),
        ("US1", "19"),
        ("US2", "16"),
        ("US2", "17"),
        ("US2", "18"),
        ("US2", "19"),
        ("U04", "1"),
        ("U04", "2"),
        ("U04", "5"),
        ("U04", "6"),
        ("U725", "3"),
        ("U725", "6"),
        ("U725", "7"),
        ("U725", "8"),
        ("U725", "9"),
        ("U725", "10"),
        ("U725", "11"),
        ("U725", "15"),
        ("U725", "16"),
        ("U574", "11"),
        ("J7", "3"),
        ("J8", "1"),
        ("J9", "1"),
    }
    | {(f"J5", str(i)) for i in (1, 2, 3, 4, 5, 6, 8, 10)}
    | {(f"J6", str(i)) for i in (1, 2, 3, 4, 5, 6, 8, 10)}
    | {(f"J2", str(i)) for i in range(1, 7)}
    | {(f"Y{n}", p) for n in ("1", "2", "3") for p in ("1", "2")}
    | {(f"US2", str(p)) for p in (1, 2, 3, 4, 5, 7, 8, 9, 22, 23, 24, 25, 26, 27, 28)}
)


def _never_name(name: str) -> bool:
    if name in _NEVER or name in _NEVER_CART_A:
        return True
    if name.startswith("CART_OE") or name.startswith("CART_WE"):
        return True
    for prefix in _NEVER_PREFIXES:
        if name.startswith(prefix):
            return True
    return False


def is_layer4_name(name: str) -> bool:
    """True when a net name belongs on the Layer4 class."""
    if not name or _never_name(name):
        return False
    if name in _EXACT:
        return True
    for pat in KICAD_PATTERNS:
        if fnmatch.fnmatch(name, pat):
            return True
    return False


def is_layer4_net(net) -> bool:
    name = getattr(net, "name", "") or ""
    if is_layer4_name(name):
        return True
    if _never_name(name):
        return False
    pins = getattr(net, "pins", None) or []
    for pin in pins:
        part = getattr(pin, "part", None)
        if part is None:
            continue
        ref = getattr(part, "ref", "") or ""
        num = str(getattr(pin, "num", "") or "")
        if (ref, num) in _LAYER4_PINS:
            return True
    return False


def layer4_net_names_from_netlist(text: str) -> list[str]:
    names = re.findall(r'\(name "([^"]+)"\)', text)
    return sorted({n for n in names if is_layer4_name(n)})


def layer4_net_names(nets: Iterable) -> list[str]:
    names = []
    for net in nets:
        if is_layer4_net(net):
            n = getattr(net, "name", "") or ""
            if n:
                names.append(n)
    return sorted(set(names))


def apply_layer4_netclass(nets: Iterable) -> list[str]:
    """Assign SKiDL NetClass Layer4. Returns the net names that matched."""
    from skidl import NetClass

    matched = layer4_net_names(nets)
    if not matched:
        return []
    cls = NetClass(
        CLASS_NAME,
        trace_width=0.2,
        clearance=0.2,
        via_dia=0.6,
        via_drill=0.3,
        priority=10,
    )
    want = set(matched)
    for net in nets:
        if (getattr(net, "name", "") or "") in want:
            net.netclasses = cls
    return matched


def annotate_netlist_layer4_class(text: str, names: Iterable[str]) -> str:
    """Force (class "Layer4") on matching nets in a KiCad sexp netlist."""
    want = set(names)
    class_re = re.compile(r'\(name "([^"]+)"\)(\s*)(\(class "[^"]+"\))?')

    def repl(m: re.Match) -> str:
        name = m.group(1)
        cls = CLASS_NAME if name in want else "Default"
        return f'(name "{name}") (class "{cls}")'

    return class_re.sub(repl, text)


def kicad_pro_patterns(extra_names: Optional[Iterable[str]] = None) -> list[dict]:
    seen = set()
    out = []
    for pat in KICAD_PATTERNS:
        if pat not in seen:
            seen.add(pat)
            out.append({"netclass": CLASS_NAME, "pattern": pat})
    if extra_names:
        for name in extra_names:
            if name and name not in seen and is_layer4_name(name):
                seen.add(name)
                out.append({"netclass": CLASS_NAME, "pattern": name})
    return out


def patch_kicad_pro(text: str, extra_names: Optional[Iterable[str]] = None) -> str:
    """Insert or replace the Layer4 class and netclass_patterns in a .kicad_pro."""
    data = json.loads(text)
    ns = data.get("net_settings") or {}
    classes = [c for c in (ns.get("classes") or []) if c.get("name") != CLASS_NAME]
    classes.append(dict(KICAD_CLASS))
    ns["classes"] = classes
    ns["netclass_patterns"] = kicad_pro_patterns(extra_names)
    data["net_settings"] = ns
    start = text.find('"net_settings"')
    if start < 0:
        return json.dumps(data, indent=2) + "\n"
    key = '"net_settings"'
    colon = text.find(":", start)
    obj_start = text.find("{", colon)
    depth = 0
    i = obj_start
    while i < len(text):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                obj_end = i + 1
                break
        i += 1
    else:
        return json.dumps(data, indent=2) + "\n"
    new_obj = json.dumps(data["net_settings"], indent=2)
    new_obj = _indent_block(new_obj, 2)
    return text[:obj_start] + new_obj + text[obj_end:]


def _indent_block(block: str, spaces: int) -> str:
    pad = " " * spaces
    lines = block.splitlines()
    if not lines:
        return block
    out = [lines[0]]
    for line in lines[1:]:
        out.append(pad + line if line else line)
    return "\n".join(out)
