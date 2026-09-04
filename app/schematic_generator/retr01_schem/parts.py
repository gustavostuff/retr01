"""SKiDL Part factories — physical pin numbers from KiCad / datasheets (pinmap.py)."""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List, Optional, Sequence, TYPE_CHECKING

from .bom import BomEntry
from .pinmap import KICAD_ALIASES, PIN_TEMPLATES

if TYPE_CHECKING:
    from .bom import BoardId

try:
    from skidl import KICAD, Part, Pin, SKIDL, lib_search_paths
except ImportError:  # pragma: no cover
    KICAD = None  # type: ignore
    Part = None  # type: ignore
    Pin = None  # type: ignore
    SKIDL = None  # type: ignore
    lib_search_paths = {}  # type: ignore

# Prefer official KiCad symbol libs when present (Manjaro: /usr/share/kicad/symbols).
KICAD_SYMBOL_DIR = Path("/usr/share/kicad/symbols")


def skidl_available() -> bool:
    return Part is not None


def add_library_paths(extra: Optional[Sequence[str]] = None) -> None:
    if not skidl_available() or KICAD is None:
        return
    paths = [str(KICAD_SYMBOL_DIR), str(Path(__file__).resolve().parent.parent / "library")]
    paths.extend(extra or ())
    kicad_paths = list(lib_search_paths.get(KICAD, []))
    for p in paths:
        if p not in kicad_paths:
            kicad_paths.append(p)
    lib_search_paths[KICAD] = kicad_paths


def _pin_num(n: str):
    """KiCad pad id: integer for DIP / TRS pads, letter for barrel MP."""
    try:
        return int(n)
    except ValueError:
        return n


def _skidl_pins(mpn: str, numbers: Sequence[str]) -> List:
    """Pin num and name match footprint pads; attach KiCad aliases when present."""
    aliases = KICAD_ALIASES.get(mpn, {})
    pins = []
    for n in numbers:
        num = _pin_num(n)
        alias = aliases.get(n)
        if alias and alias != n:
            pins.append(Pin(num=num, name=n, aliases=[alias]))
        else:
            pins.append(Pin(num=num, name=n))
    return pins


def make_part(entry: BomEntry, *, refdes_override: Optional[str] = None):
    if not skidl_available():
        raise RuntimeError("skidl is not installed; pip install -r requirements.txt")

    numbers = PIN_TEMPLATES.get(entry.mpn)
    if numbers is None:
        raise KeyError(f"no pin template for MPN {entry.mpn}")

    return Part(
        tool=SKIDL,
        name=entry.mpn,
        ref=refdes_override or entry.refdes,
        footprint=entry.footprint,
        pins=_skidl_pins(entry.mpn, numbers),
    )


def make_passive(mpn: str, refdes: str, footprint: str):
    if not skidl_available():
        raise RuntimeError("skidl is not installed; pip install -r requirements.txt")
    numbers = PIN_TEMPLATES.get(mpn)
    if numbers is None:
        raise KeyError(f"no pin template for MPN {mpn}")
    return Part(
        tool=SKIDL,
        name=mpn,
        ref=refdes,
        footprint=footprint,
        pins=_skidl_pins(mpn, numbers),
    )


def instantiate_bom(
    include_sim_only: bool = False,
    board: Optional["BoardId"] = None,
) -> Dict[str, object]:
    from .bom import BoardId, entries_for_board

    if board is None:
        board = BoardId.MOBO
    add_library_paths()
    parts: Dict[str, object] = {}
    for entry in entries_for_board(board, include_sim_only=include_sim_only):
        if entry.dip_pins <= 0:
            continue
        parts[entry.refdes] = make_part(entry)
    return parts
