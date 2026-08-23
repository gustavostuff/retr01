#!/usr/bin/env bash
# Build (if needed) and run IC unit tests.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [[ ! -f build/CMakeCache.txt ]]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build
ctest --test-dir build --output-on-failure -R '^test_'
