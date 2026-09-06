#!/usr/bin/env python3
"""Generate Retr01 motherboard + cartridge netlists from SKiDL."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

# Point SKiDL / KiCad at official Manjaro symbol libs before importing skidl.
_KICAD_SYM = "/usr/share/kicad/symbols"
os.environ.setdefault("KICAD_SYMBOL_DIR", _KICAD_SYM)
os.environ.setdefault("KICAD10_SYMBOL_DIR", _KICAD_SYM)
for _v in ("KICAD6_SYMBOL_DIR", "KICAD7_SYMBOL_DIR", "KICAD8_SYMBOL_DIR", "KICAD9_SYMBOL_DIR"):
    os.environ.setdefault(_v, _KICAD_SYM)

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from retr01_schem.board import (
    export_manifest_json,
    generate_both_netlists,
    validate_bom_counts,
)
from retr01_schem.bom import BoardId, PassiveProfile, get_passive_profile, set_passive_profile
from retr01_schem.cart_manifest import build_cart_manifest
from retr01_schem.manifest import build_manifest, manifest_gaps


def main() -> int:
    ap = argparse.ArgumentParser(description="Retr01 SKiDL schematic generator")
    ap.add_argument(
        "--out",
        type=Path,
        default=ROOT / "output",
        help="output directory for netlists and manifest JSON",
    )
    ap.add_argument("--manifest-only", action="store_true", help="write wiring manifest JSON only")
    ap.add_argument("--check", action="store_true", help="validate BOM counts + J36 contract and exit")
    ap.add_argument(
        "--full-esd",
        action="store_true",
        help="FULL passive profile: cart/TRS TVS + arcade 47 ohm series (default is BRINGUP)",
    )
    args = ap.parse_args()

    set_passive_profile(PassiveProfile.FULL if args.full_esd else PassiveProfile.BRINGUP)

    errs = validate_bom_counts()
    if errs:
        for e in errs:
            print(f"ERROR: {e}", file=sys.stderr)
        return 1

    if args.check:
        print(
            f"OK: BOM + J36 contract profile={get_passive_profile().value} "
            f"({len(build_manifest())} mobo / {len(build_cart_manifest())} cart connections)"
        )
        return 0

    args.out.mkdir(parents=True, exist_ok=True)
    export_manifest_json(args.out / "retr01_wiring_manifest.json", board=BoardId.MOBO)
    export_manifest_json(args.out / "retr01_cart_wiring_manifest.json", board=BoardId.CART)

    if args.manifest_only:
        print(f"wrote {args.out / 'retr01_wiring_manifest.json'}")
        print(f"wrote {args.out / 'retr01_cart_wiring_manifest.json'}")
        print(f"gaps ({len(manifest_gaps())}):")
        for g in manifest_gaps():
            print(f"  - {g}")
        return 0

    paths = generate_both_netlists(args.out)
    print(f"profile: {get_passive_profile().value}")
    print(f"wrote {paths['mobo']}")
    print(f"wrote {paths['cart']}")
    print(f"wrote {args.out / 'retr01_wiring_manifest.json'}")
    print(f"wrote {args.out / 'retr01_cart_wiring_manifest.json'}")
    print("next: import each netlist into its KiCad project; lock I/O; route with Quilter")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
