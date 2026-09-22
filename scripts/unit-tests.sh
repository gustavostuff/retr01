#!/usr/bin/env bash
# Configure, build, and run unit tests for studio, emu, sdk C, and tier-a sim.
# Tier-a ctest also runs nested netlist_sim tests. Emu registers test_boot/test_play
# against example_01/example_01.retr01 when that cart is present, and test_sdk_prg
# when llvm-mos is in tools/llvm-mos or $LLVM_MOS.
# Does not require (or seed) extra ROM / Studio project fixtures.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STUDIO="$ROOT/apps/studio"
EMU="$ROOT/apps/emu"
SIM_A="$ROOT/apps/sim/tier-a"

build_and_test() {
  local proj="$1"
  shift
  cmake -S "$proj" -B "$proj/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$proj/build" -j"$(nproc)"
  ctest --test-dir "$proj/build" --output-on-failure "$@"
}

echo "== studio =="
build_and_test "$STUDIO"

echo "== sdk c =="
cmake -S "$ROOT/apps/sdk/r01_c" -B "$ROOT/apps/sdk/r01_c/host-build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/apps/sdk/r01_c/host-build" -j"$(nproc)"
ctest --test-dir "$ROOT/apps/sdk/r01_c/host-build" --output-on-failure

echo "== emu =="
build_and_test "$EMU"

echo "== sim tier-a =="
build_and_test "$SIM_A"

echo "all unit tests passed"
