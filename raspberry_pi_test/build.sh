#!/usr/bin/env bash
# Compile retr01_emu and install the Ports payload on USB.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PORTS="/media/usb1/roms/ports"
DEST="$PORTS/Retr01_test"
cd "$HERE"
JOBS="$(nproc 2>/dev/null || echo 2)"
cmake -S apps/emu -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target retr01_emu -j"$JOBS"
mkdir -p "$DEST"
install -m755 build/retr01_emu "$DEST/retr01_emu"
install -m644 "$HERE/example_01.retr01" "$DEST/example_01.retr01"
install -m644 apps/common/gamecontrollerdb.txt "$DEST/gamecontrollerdb.txt"
install -m755 "$HERE/retr01.sh" "$PORTS/retr01.sh"
echo "built $DEST/retr01_emu"
echo "launcher $PORTS/retr01.sh"
