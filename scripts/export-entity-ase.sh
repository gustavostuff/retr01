#!/usr/bin/env bash
# Export one aseprite_entities/<name> folder to PNG + JSON via the Aseprite CLI.
# Usage: ./scripts/export-entity-ase.sh path/to/player [out_dir]
# Binary: $ASEPRITE if set, else aseprite on PATH.
set -euo pipefail

usage() {
  echo "usage: ./scripts/export-entity-ase.sh path/to/player [out_dir]" >&2
  exit 2
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ] || [ $# -lt 1 ]; then
  usage
fi

SRC="$1"
if [ ! -d "$SRC" ]; then
  echo "error: not a directory: $SRC" >&2
  exit 1
fi

ASE="${ASEPRITE:-aseprite}"
if ! command -v "$ASE" >/dev/null 2>&1 && [ ! -x "$ASE" ]; then
  echo "error: aseprite not found (set ASEPRITE)" >&2
  exit 1
fi

STEM="$(basename "$SRC")"
OUT="${2:-generated/$STEM}"
mkdir -p "$OUT"

shopt -s nullglob
files=("$SRC"/*.ase "$SRC"/*.aseprite)
if [ ${#files[@]} -eq 0 ]; then
  echo "error: no .ase files in $SRC" >&2
  exit 1
fi

for f in "${files[@]}"; do
  base="$(basename "$f")"
  name="${base%.*}"
  dest="$OUT/$name"
  mkdir -p "$dest"
  "$ASE" -b --all-layers --ignore-empty --format json-array --sheet-type horizontal \
    --sheet "$dest/sheet.png" --data "$dest/sheet.json" \
    "$f"
done

echo "exported $STEM -> $OUT"
