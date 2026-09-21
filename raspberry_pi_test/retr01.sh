#!/usr/bin/env bash
# RGB-Pi OS4 Ports entry. Keep this file next to retr01_emu, example_01.retr01, and gamecontrollerdb.txt.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/retr01_emu"
CART="$HERE/example_01.retr01"
[[ -x "$BIN" ]] || { echo "error: missing $BIN -- run ./build.sh first" >&2; exit 1; }
[[ -f "$CART" ]] || { echo "error: missing $CART" >&2; exit 1; }
export R01E_SCALE="${R01E_SCALE:-1}"
export R01E_FULLSCREEN="${R01E_FULLSCREEN:-1}"
export R01E_NO_DEBUG="${R01E_NO_DEBUG:-1}"
cd "$HERE"
exec "$BIN" "$CART"
