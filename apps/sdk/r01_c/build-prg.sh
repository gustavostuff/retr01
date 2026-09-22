#!/usr/bin/env bash
# Compile SDK C + game_logic.c to a 32 KB PRG (llvm-mos).
# Usage: ./apps/sdk/r01_c/build-prg.sh [game_logic.c] [out.prg]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SDK="$ROOT/apps/sdk/r01_c"
MOS="${LLVM_MOS:-$ROOT/tools/llvm-mos}"
CC="$MOS/bin/mos-common-clang"
OBJDUMP="$MOS/bin/llvm-objdump"
SIZEBIN="$MOS/bin/llvm-size"
LOGIC="${1:-$SDK/game_logic.c}"
OUT="${2:-$SDK/build/retr01.prg}"
OUTDIR="$(dirname "$OUT")"

if [ ! -x "$CC" ]; then
  echo "error: llvm-mos missing ($CC). Run ./scripts/fetch-llvm-mos.sh" >&2
  exit 1
fi

COMMON="$ROOT/apps/common"

mkdir -p "$OUTDIR"

"$CC" -Os -g -mcpu=mosw65c02 -mlto-zp=218 \
  -ffunction-sections -fdata-sections \
  -I "$SDK/include" \
  -I "$COMMON" \
  -I "$COMMON/fw" \
  -T "$SDK/ld/retr01.ld" \
  -linit-stack -lzero-bss -lexit-loop \
  -Wl,--gc-sections \
  -o "$OUT" \
  "$SDK/asm/boot.s" \
  "$SDK/asm/nmi.s" \
  "$SDK/asm/map_copy.s" \
  "$SDK/src/hw.c" \
  "$SDK/src/boot.c" \
  "$SDK/src/game.c" \
  "$SDK/src/play_tick.c" \
  "$SDK/src/tracker.c" \
  "$SDK/src/map_win.c" \
  "$SDK/src/oam_pa.c" \
  "$SDK/src/main.c" \
  "$COMMON/r01_play_camera.c" \
  "$COMMON/r01_play_physics.c" \
  "$COMMON/r01_play_collision.c" \
  "$COMMON/r01_play_anim.c" \
  "$COMMON/r01_play_anim_cart.c" \
  "$LOGIC"

SZ=$(wc -c < "$OUT")
USED=0
if [ -x "$SIZEBIN" ] && [ -f "$OUT.elf" ]; then
  USED=$("$SIZEBIN" -A "$OUT.elf" | awk '
    /^\.(text|data|rodata|boot|text\.nmi|r01_bootmap)/ { s += $2 }
    END { print s+0 }
  ')
fi
if [ "$SZ" -ne 32768 ]; then
  echo "error: PRG size $SZ ($USED used / 32768)" >&2
  exit 1
fi

if [ -x "$OBJDUMP" ]; then
  "$OBJDUMP" -d -S "$OUT.elf" > "$OUTDIR/listing.txt" || true
fi

echo "wrote $OUT ($USED / 32768 used)"
