#!/usr/bin/env bash
# RGB-Pi OS4 Ports entry. Binary and cart live under /media/usb1/roms/ports/Retr01_test/.
set -euo pipefail
DEST="/media/usb1/roms/ports/Retr01_test"
BIN="$DEST/retr01_emu"
CART="$DEST/example_01.retr01"
[[ -x "$BIN" ]] || { echo "error: missing $BIN -- run ./build.sh first" >&2; exit 1; }
[[ -f "$CART" ]] || { echo "error: missing $CART" >&2; exit 1; }
export R01E_SCALE="${R01E_SCALE:-1}"
export R01E_FULLSCREEN="${R01E_FULLSCREEN:-1}"
export R01E_NO_DEBUG="${R01E_NO_DEBUG:-1}"
export R01E_AUDIO_SAMPLES="${R01E_AUDIO_SAMPLES:-2048}"
export R01E_AUDIO_RATE="${R01E_AUDIO_RATE:-48000}"
cd "$DEST"
exec "$BIN" "$CART"
