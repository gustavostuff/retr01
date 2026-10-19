"""SKiDL Part factories (from retr01 schematic_generator)."""

from __future__ import annotations

from pathlib import Path
from typing import List, Optional, Sequence

from .pinmap import KICAD_ALIASES, PIN_TEMPLATES
from .pinmap_extras import apply_pinmap_extras

apply_pinmap_extras()

try:
    from skidl import KICAD, Part, Pin, SKIDL, lib_search_paths
    from skidl.pin import pin_types
except ImportError:  # pragma: no cover
    KICAD = None  # type: ignore
    Part = None  # type: ignore
    Pin = None  # type: ignore
    SKIDL = None  # type: ignore
    pin_types = None  # type: ignore
    lib_search_paths = {}  # type: ignore

KICAD_SYMBOL_DIR = Path("/usr/share/kicad/symbols")
SKIDL_LIB_DIR = Path(__file__).resolve().parent.parent / "library"

PASSIVE_VALUES = {
    "C_100N": "100nF",
    "C_22P": "22pF",
    "C_10U": "10uF",
    "C_BULK": "220uF",
    "R_0": "0R",
    "R_33": "33R",
    "R_47": "47R",
    "R_75": "75R",
    "R_1K": "1k",
    "R_2K": "2k",
    "R_4K": "4k",
    "R_4K7": "4.7k",
    "R_10K": "10k",
    "R_20K": "20k",
    "R_1M": "1M",
}


def skidl_available() -> bool:
    return Part is not None


def add_library_paths() -> None:
    if not skidl_available() or KICAD is None:
        return
    paths = [str(KICAD_SYMBOL_DIR), str(SKIDL_LIB_DIR)]
    kicad_paths = list(lib_search_paths.get(KICAD, []))
    for p in paths:
        if p not in kicad_paths:
            kicad_paths.append(p)
    lib_search_paths[KICAD] = kicad_paths


def _pin_num(n: str):
    try:
        return int(n)
    except ValueError:
        return n


_PWRIN_NAMES = {
    "VCC",
    "VDD",
    "VDDIO2",
    "AVDD",
    "APOS",
    "DPOS",
    "+5V",
    "5V",
    "GND",
    "GND2",
    "VSS",
    "AGND",
    "DGND",
}


def _pin_func(name: str):
    if pin_types is None:
        return None
    n = (name or "").upper()
    if n in _PWRIN_NAMES:
        return pin_types.PWRIN
    return pin_types.UNSPEC


def _skidl_pins(mpn: str, numbers: Sequence[str]) -> List:
    aliases = KICAD_ALIASES.get(mpn, {})
    pins = []
    for n in numbers:
        num = _pin_num(n)
        name = aliases.get(n) or n
        kwargs = {"num": num, "name": name}
        func = _pin_func(name)
        if func is not None:
            kwargs["func"] = func
        pins.append(Pin(**kwargs))
    return pins


def make_skidl_part(mpn: str, refdes: str, footprint: str):
    if not skidl_available():
        raise RuntimeError("skidl is not installed")
    numbers = PIN_TEMPLATES.get(mpn)
    if numbers is None:
        raise KeyError(f"no pin template for MPN {mpn!r} ({refdes})")
    part = Part(
        tool=SKIDL,
        name=mpn,
        ref=refdes,
        footprint=footprint,
        pins=_skidl_pins(mpn, numbers),
    )
    try:
        part.value = PASSIVE_VALUES.get(mpn) or mpn
    except Exception:
        pass
    return part
