#!/usr/bin/env python3
"""
Build Retr01_Lib THT footprints with minimal silkscreen text.

KiCad stock footprints often draw Reference on F.SilkS *and* a duplicate
${REFERENCE} on F.Fab (assembly only). For netlist-first bring-up we trim
that fab duplicate and put Value on F.SilkS so the board shows refdes +
BOM value (e.g. U3 + AS6C62256), not the footprint filename.

Re-run after KiCad library updates, then copy library via export_netlist.sh.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
KICAD_FP = Path("/usr/share/kicad/footprints")
OUT = REPO / "apps/sim/tier-h/skidl/library/Retr01_Lib.pretty"

# (source library dir, footprint filename)
STOCK: list[tuple[str, str]] = [
    ("Package_DIP.pretty", "DIP-8_W7.62mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-14_W7.62mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-16_W7.62mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-20_W7.62mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-24_W7.62mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-28_W7.62mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-28_W15.24mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-32_W15.24mm.kicad_mod"),
    ("Package_DIP.pretty", "DIP-40_W15.24mm.kicad_mod"),
    ("Capacitor_THT.pretty", "C_Disc_D5.0mm_W2.5mm_P5.00mm.kicad_mod"),
    ("Capacitor_THT.pretty", "CP_Radial_D8.0mm_P3.50mm.kicad_mod"),
    (
        "Resistor_THT.pretty",
        "R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical.kicad_mod",
    ),
    ("Oscillator.pretty", "Oscillator_DIP-8.kicad_mod"),
    ("Connector_BarrelJack.pretty", "BarrelJack_CUI_PJ-063AH_Horizontal.kicad_mod"),
    ("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x04_P2.54mm_Vertical.kicad_mod"),
    ("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x06_P2.54mm_Vertical.kicad_mod"),
    ("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x10_P2.54mm_Vertical.kicad_mod"),
]

FP_TEXT_REF = re.compile(
    r"\t\(fp_text user \"\$\{REFERENCE\}\".*?\n\t\)\n",
    re.DOTALL,
)


def trim_mod(text: str) -> str:
    text = FP_TEXT_REF.sub("", text)
    # Value follows netlist (MPN / 100nF); show on silk, not fab assembly duplicate.
    text = re.sub(
        r'(\(property "Value"[^\n]*\n\t\t\(at [^\n]+\n\t\t\(layer )"F\.Fab"',
        r'\1"F.SilkS"',
        text,
    )
    return text


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    if not KICAD_FP.is_dir():
        print(f"missing KiCad footprints: {KICAD_FP}", file=sys.stderr)
        return 1
    n = 0
    for lib, name in STOCK:
        src = KICAD_FP / lib / name
        if not src.is_file():
            print(f"skip missing {src}", file=sys.stderr)
            continue
        dst = OUT / name
        dst.write_text(trim_mod(src.read_text(encoding="utf-8")), encoding="utf-8")
        n += 1
    for name in ("Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical.kicad_mod",):
        src = OUT / name
        if src.is_file():
            src.write_text(trim_mod(src.read_text(encoding="utf-8")), encoding="utf-8")
            n += 1
    print(f"trimmed {n} footprints -> {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
