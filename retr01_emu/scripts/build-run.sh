#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CART="${1:-$ROOT/../retr01_studio/test_game/test.retr01}"
cd "$ROOT"
if [[ ! -d build ]]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build
exec ./build/retr01_emu "$CART"
