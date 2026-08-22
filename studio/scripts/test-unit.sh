#!/usr/bin/env bash
# Build (if needed) and run core unit tests only (no e2e).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [[ ! -f build/CMakeCache.txt ]]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build --target test_core
ctest --test-dir build --output-on-failure -R '^core$'
