#!/usr/bin/env python3
"""Fail if Switchcraft TRS footprint pad 5 is not at 11.8 mm from pad 1 (VN4 CD)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

PAD5 = re.compile(
    r'\(pad "5" thru_hole oval\s*\n\s+\(at\s+([-\d.]+)\s+([-\d.]+)',
    re.MULTILINE,
)
WANT_X = 11.8
TOL = 0.01


def main() -> None:
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "")
    if not path.is_file():
        raise SystemExit(f"missing footprint: {path}")
    text = path.read_text(encoding="utf-8")
    m = PAD5.search(text)
    if not m:
        raise SystemExit(f"{path.name}: no pad 5 oval (at ...) found")
    x, y = float(m.group(1)), float(m.group(2))
    if abs(x - WANT_X) > TOL or abs(y) > TOL:
        raise SystemExit(
            f"{path.name}: pad 5 at ({x}, {y}), expected ({WANT_X}, 0) — library is stale"
        )
    if "Retr01_pin1_to_pin5_mm" not in text:
        print(f"warn: {path.name} missing Retr01_pin1_to_pin5_mm property", file=sys.stderr)
    print(f"OK: {path.name} pad 1→5 span = {x:g} mm")


if __name__ == "__main__":
    main()
