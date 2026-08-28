#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=scripts/r01-common.sh
source "$ROOT/scripts/r01-common.sh"

RELEASE="$R01_ROOT/release"
STUDIO="$R01_ROOT/retr01_studio"
EMU="$R01_ROOT/retr01_emu"
SIM="$R01_ROOT/retr01_sim"

r01_configure_release() {
  cmake -S "$1" -B "$1/build" -DCMAKE_BUILD_TYPE=Release
}

r01_release_bin() {
  local proj="$1"
  local name="$2"
  r01_configure_release "$proj"
  cmake --build "$proj/build" --target "$name" -j"$(nproc)"
  install -Dm755 "$proj/build/$name" "$RELEASE/$name"
}

mkdir -p "$RELEASE"

echo "== retr01_studio =="
r01_release_bin "$STUDIO" retr01_studio

echo "== retr01_emu =="
r01_release_bin "$EMU" retr01_emu

echo "== retr01_sim =="
r01_release_bin "$SIM" retr01_sim

echo "release binaries:"
ls -lh "$RELEASE"/retr01_studio "$RELEASE"/retr01_emu "$RELEASE"/retr01_sim
