#!/usr/bin/env bash
# Compile SDK C + game_logic.c to a 32 KB PRG (llvm-mos).
# Usage: ./apps/sdk/r01_c/build-prg.sh [game_logic.c] [out.prg] [data_dir]
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
LOGIC_DIR="$(dirname "$LOGIC")"
INC_LOGIC="$SDK/include"
if [ -d "$LOGIC_DIR/include" ]; then
  INC_LOGIC="$LOGIC_DIR/include"
fi

DATA="${3:-}"
if [ -z "$DATA" ]; then
  if [ -f "$OUTDIR/data/play8100.bin" ]; then
    DATA="$OUTDIR/data"
  elif [ -f "$LOGIC_DIR/data/play8100.bin" ]; then
    DATA="$LOGIC_DIR/data"
  else
    DATA="$SDK/data"
  fi
fi
if [ ! -d "$DATA" ]; then
  echo "error: PRG data dir missing ($DATA)" >&2
  exit 1
fi
DATA="$(cd "$DATA" && pwd)"

if [ ! -x "$CC" ]; then
  echo "error: llvm-mos missing ($CC). Run ./scripts/fetch-llvm-mos.sh" >&2
  exit 1
fi

for f in r01p.bin play8100.bin worlddir.bin solids.bin; do
  if [ ! -f "$DATA/$f" ]; then
    echo "error: missing $DATA/$f" >&2
    exit 1
  fi
done

SOL=$(wc -c < "$DATA/solids.bin")
MAX=$((0x8800 - 0x8700))
if [ "$SOL" -gt "$MAX" ]; then
  echo "error: solids.bin $SOL B hits \$8800 (max $MAX)" >&2
  exit 1
fi

COMMON="$ROOT/apps/common"

mkdir -p "$OUTDIR"

TAB_S="$OUTDIR/r01_tables.s"
{
  printf '.section .r01_r01p,"a",@progbits\n'
  printf '.incbin "%s"\n' "$DATA/r01p.bin"
  printf '.section .r01_play,"a",@progbits\n'
  printf '.incbin "%s"\n' "$DATA/play8100.bin"
  printf '.section .r01_worlddir,"a",@progbits\n'
  printf '.incbin "%s"\n' "$DATA/worlddir.bin"
  printf '.section .r01_solids,"a",@progbits\n'
  printf '.incbin "%s"\n' "$DATA/solids.bin"
  if [ -f "$DATA/wplay.bin" ]; then
    WPLAY=$(wc -c < "$DATA/wplay.bin")
    if [ "$WPLAY" -gt 0 ]; then
      printf '.section .r01_wplay,"a",@progbits\n'
      printf '.incbin "%s"\n' "$DATA/wplay.bin"
    fi
  fi
} > "$TAB_S"

# W65C02S only. NMOS 6502 (-mcpu=mos6502) is not a PRG target.
"$CC" -Oz -g -mcpu=mosw65c02 -mlto-zp=218 \
  -ffunction-sections -fdata-sections \
  -I "$INC_LOGIC" \
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
  "$TAB_S" \
  "$LOGIC"

SZ=$(wc -c < "$OUT")
USED=0
if [ -x "$SIZEBIN" ] && [ -f "$OUT.elf" ]; then
  USED=$("$SIZEBIN" -A "$OUT.elf" | awk '
    /^\.(text|data|rodata|boot|text\.nmi|r01_bootmap|r01_r01p|r01_play|r01_worlddir|r01_solids|r01_wplay)/ { s += $2 }
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
