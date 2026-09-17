#!/usr/bin/env bash
# Configure, build, and run unit tests for studio and emu.
# Does not require (or seed) ROM / Studio project fixtures.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STUDIO="$ROOT/apps/studio"
EMU="$ROOT/apps/emu"

build_and_test() {
  local proj="$1"
  shift
  cmake -S "$proj" -B "$proj/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$proj/build" -j"$(nproc)"
  ctest --test-dir "$proj/build" --output-on-failure "$@"
}

echo "== studio =="
build_and_test "$STUDIO"

echo "== emu =="
build_and_test "$EMU"

echo "all unit tests passed"
