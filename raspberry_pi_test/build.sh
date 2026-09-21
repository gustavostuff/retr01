#!/usr/bin/env bash
# Compile retr01_emu in this pack (Pi or host). Needs cmake, a C compiler, libsdl2-dev.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
JOBS="$(nproc 2>/dev/null || echo 2)"
cmake -S apps/emu -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target retr01_emu -j"$JOBS"
install -m755 build/retr01_emu "$HERE/retr01_emu"
install -m644 apps/common/gamecontrollerdb.txt "$HERE/gamecontrollerdb.txt"
echo "built $HERE/retr01_emu"
echo "run: ./retr01.sh"
