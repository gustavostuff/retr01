#!/usr/bin/env bash
# Visual E2E: show the Studio window and step through UI tests.
#
# Usage: scripts/test-e2e-watch.sh [speed]
#   speed — multiplier vs default pacing (default 1). Higher = faster.
#           e.g. 2 ≈ half the delay between steps; 0.5 ≈ twice as slow.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

SPEED="${1:-1}"
if ! awk -v s="$SPEED" 'BEGIN { exit !(s + 0 > 0) }'; then
  echo "speed must be a positive number (got: ${SPEED})" >&2
  exit 1
fi

# Base delay matches e2e harness default (120 ms at speed 1).
BASE_MS=120
WATCH_MS="$(awk -v base="$BASE_MS" -v s="$SPEED" 'BEGIN {
  ms = int(base / s + 0.5)
  if (ms < 1) ms = 1
  print ms
}')"

if [[ ! -f build/CMakeCache.txt ]]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build --target test_e2e

echo "E2E watch @ ${SPEED}x (E2E_WATCH_MS=${WATCH_MS})"
exec env E2E_WATCH=1 E2E_WATCH_MS="$WATCH_MS" "$ROOT/build/test_e2e"
