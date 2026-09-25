#!/usr/bin/env bash
# Preliminary Tier H JSON + KiCad netlist (not fab-ready). See docs/bringup/tier-h-skidl-export.md.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$DIR/../../../.." && pwd)"
BUILD="$DIR/../build/export_tier_h_netlist"
JSON="$DIR/retr01_tier_h.json"
QUIET=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -q | --quiet) QUIET=(-q); shift ;;
        -h | --help)
            echo "Usage: $0 [-q]"
            echo "  Writes $JSON and $DIR/retr01_prelim.net"
            exit 0
            ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

if [[ ! -x "$BUILD" ]]; then
    echo "missing $BUILD — run: cmake --build apps/sim/tier-h/build --target export_tier_h_netlist" >&2
    exit 1
fi

mkdir -p "$DIR"
"$BUILD" >"$JSON"
exec python3 "$REPO/scripts/skidl_from_tier_h.py" "${QUIET[@]}"
