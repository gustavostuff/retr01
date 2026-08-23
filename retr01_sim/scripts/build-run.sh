#!/usr/bin/env bash
# Configure (if needed), build, then run Retr01 Board Simulator.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [[ ! -f build/CMakeCache.txt ]]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build
exec "$ROOT/build/retr01_sim" "$@"
