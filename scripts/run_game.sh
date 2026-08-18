#!/usr/bin/env bash
# Rebuild Retr01 tools, optionally export a .r01proj to a cart, then run the emulator.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORIG_PWD="$PWD"
cd "$ROOT"

BUILD_DIR="${RETR01_BUILD_DIR:-$ROOT/build}"
EMU="$BUILD_DIR/retr01_emu/retr01_emu"
EXPORT="$BUILD_DIR/retr01_world_studio/retr01_export"

usage() {
  cat <<EOF
Usage: $(basename "$0") [game.r01proj | game.retr01 | game] [-- extra emu args]

Rebuilds the emulator (and exporter), then runs it on a cart.

  no args              newest *.retr01 under build/, else the one_screen fixture
  path.retr01          use that cart
  path.r01proj         pack + export to build/<stem>.retr01, then emulate
  path                 try path, path.r01proj, then path.retr01

Examples:
  $(basename "$0") untitled.r01proj
  $(basename "$0") test_01
  $(basename "$0") build/test_01.retr01
  $(basename "$0") untitled.r01proj -- --world 0 --col 1 --row 0
EOF
}

list_projects() {
  echo "Projects in repo root:" >&2
  local found=0
  local f
  shopt -s nullglob
  for f in "$ROOT"/*.r01proj; do
    echo "  ${f#$ROOT/}" >&2
    found=1
  done
  for f in "$ROOT"/*; do
    [[ -f "$f" ]] || continue
    [[ "$f" == *.r01proj || "$f" == *.retr01 ]] && continue
    if head -c 80 "$f" 2>/dev/null | grep -q 'format_version'; then
      echo "  ${f#$ROOT/}  (no .r01proj extension)" >&2
      found=1
    fi
  done
  shopt -u nullglob
  if [[ "$found" -eq 0 ]]; then
    echo "  (none — Save in Studio as something.r01proj)" >&2
  fi
  echo "Carts:" >&2
  find "$BUILD_DIR" "$ROOT" -maxdepth 2 -name '*.retr01' -type f 2>/dev/null | sed "s|^$ROOT/|  |" >&2 || true
}

is_project_file() {
  local p="$1"
  [[ -f "$p" ]] || return 1
  [[ "$p" == *.r01proj ]] && return 0
  [[ "$p" == *.retr01 ]] && return 1
  head -c 80 "$p" 2>/dev/null | grep -q 'format_version'
}

resolve_input() {
  local raw="$1"
  local c
  local candidates=(
    "$raw"
    "$ORIG_PWD/$raw"
    "$ROOT/$raw"
    "$raw.r01proj"
    "$ORIG_PWD/$raw.r01proj"
    "$ROOT/$raw.r01proj"
    "$raw.retr01"
    "$ORIG_PWD/$raw.retr01"
    "$ROOT/$raw.retr01"
  )
  for c in "${candidates[@]}"; do
    if [[ -f "$c" ]]; then
      printf '%s\n' "$c"
      return 0
    fi
  done
  return 1
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

INPUT=""
if [[ $# -gt 0 && "$1" != "--" ]]; then
  INPUT="$1"
  shift
fi
if [[ "${1:-}" == "--" ]]; then
  shift
fi

echo "==> cmake configure + build"
cmake -B "$BUILD_DIR" -S "$ROOT" -DRETR01_BUILD_STUDIO=ON >/dev/null
cmake --build "$BUILD_DIR" --target retr01_emu retr01_export

if [[ ! -x "$EMU" ]]; then
  echo "missing emulator: $EMU" >&2
  exit 1
fi

CART=""
if [[ -z "$INPUT" ]]; then
  CART="$(find "$BUILD_DIR" -name '*.retr01' -type f -printf '%T@ %p\n' 2>/dev/null | sort -nr | awk 'NR==1{print $2}')"
  if [[ -z "$CART" ]]; then
    CART="$ROOT/retr01_world_studio/tests/fixtures/one_screen.retr01"
  fi
else
  RESOLVED=""
  if ! RESOLVED="$(resolve_input "$INPUT")"; then
    echo "file not found: $INPUT" >&2
    echo "Save from Studio (File → Save As) as a .r01proj, or pass an existing project/cart." >&2
    list_projects
    exit 1
  fi
  INPUT="$RESOLVED"

  if is_project_file "$INPUT"; then
    if [[ ! -x "$EXPORT" ]]; then
      echo "missing exporter: $EXPORT" >&2
      exit 1
    fi
    stem="$(basename "$INPUT")"
    stem="${stem%.r01proj}"
    CART="$BUILD_DIR/${stem}.retr01"
    echo "==> export $INPUT -> $CART"
    "$EXPORT" "$INPUT" "$CART"
  elif [[ "$INPUT" == *.retr01 ]]; then
    CART="$INPUT"
  else
    echo "not a .retr01 cart or Studio project: $INPUT" >&2
    usage >&2
    exit 1
  fi
fi

if [[ ! -f "$CART" ]]; then
  echo "cart not found: $CART" >&2
  echo "Export from Studio (File → Export) or pass a .r01proj." >&2
  list_projects
  exit 1
fi

echo "==> $EMU --cart $CART $*"
exec "$EMU" --cart "$CART" "$@"
