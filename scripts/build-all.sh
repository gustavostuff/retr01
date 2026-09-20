#!/usr/bin/env bash
# Build Studio, Emu, and Tier A sim (Release) into bin/.
# Usage: ./scripts/build-all.sh [--clean|-c]
#   --clean  Remove apps/*/build and bin/, then configure and compile from scratch.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/bin"
STUDIO="$ROOT/apps/studio"
EMU="$ROOT/apps/emu"
SIM_A="$ROOT/apps/sim/tier-a"
CLEAN=0

usage() {
  echo "usage: ./scripts/build-all.sh [--clean|-c]" >&2
  exit 2
}

for arg in "$@"; do
  case "$arg" in
    --clean|-c) CLEAN=1 ;;
    -h|--help) usage ;;
    *)
      echo "error: unknown argument: $arg" >&2
      usage
      ;;
  esac
done

build_one() {
  local proj="$1"
  local target="$2"
  local out_name="$3"
  # Folder moves leave CMakeCache pointing at the old source path.
  if [ -f "$proj/build/CMakeCache.txt" ] &&
     ! grep -q "CMAKE_HOME_DIRECTORY:INTERNAL=$proj$" "$proj/build/CMakeCache.txt"; then
    echo "stale CMake cache in $proj/build -- removing"
    rm -rf "$proj/build"
  fi
  cmake -S "$proj" -B "$proj/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$proj/build" --target "$target" -j"$(nproc)"
  install -Dm755 "$proj/build/$target" "$BIN/$out_name"
}

if [ "$CLEAN" -eq 1 ]; then
  echo "cleaning build trees and bin/"
  rm -rf "$STUDIO/build" "$EMU/build" "$SIM_A/build" "$BIN"
fi

mkdir -p "$BIN"

echo "== studio =="
build_one "$STUDIO" retr01_studio studio

echo "== emu =="
build_one "$EMU" retr01_emu emu

echo "== sim tier-a =="
build_one "$SIM_A" retr01_sim_tier_a sim-tier-a

echo "binaries:"
ls -lh "$BIN"/studio "$BIN"/emu "$BIN"/sim-tier-a
