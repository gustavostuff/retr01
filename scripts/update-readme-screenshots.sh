#!/usr/bin/env bash
# Capture README screenshots for Studio and Emu.
#   Studio (1x present scale, 640x360) -> img/readme/studio.png
#   Emu main window (default scale)    -> img/readme/emu.png
#
# Requires: X11 display, xdotool, ImageMagick import.
# Optional args:
#   --studio-only | --emu-only
#   --project PATH   (default: example_01/example_01.r01proj)
#   --cart PATH      (default: example_01/example_01.retr01)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT/img/readme"
STUDIO_OUT="$OUT_DIR/studio.png"
EMU_OUT="$OUT_DIR/emu.png"
PROJECT="$ROOT/example_01/example_01.r01proj"
CART="$ROOT/example_01/example_01.retr01"
DO_STUDIO=1
DO_EMU=1
SETTLE_MS=1500
WAIT_S=20

die() { echo "error: $*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --studio-only) DO_EMU=0 ;;
    --emu-only) DO_STUDIO=0 ;;
    --project) PROJECT="$2"; shift ;;
    --cart) CART="$2"; shift ;;
    -h|--help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
    *) die "unknown arg: $1" ;;
  esac
  shift
done

[[ -n "${DISPLAY:-}" ]] || die "DISPLAY is unset (need an X11 session)"
have xdotool || die "xdotool not found"
have import || die "ImageMagick import not found"

resolve() {
  local p="$1"
  if [[ "$p" != /* ]]; then
    if [[ -e "$ROOT/$p" ]]; then
      p="$ROOT/$p"
    elif [[ -e "$p" ]]; then
      p="$(cd "$(dirname "$p")" && pwd)/$(basename "$p")"
    else
      p="$ROOT/$p"
    fi
  fi
  printf '%s' "$p"
}

STUDIO_BIN=""
EMU_BIN=""
if [[ -x "$ROOT/bin/studio" ]]; then
  STUDIO_BIN="$ROOT/bin/studio"
elif [[ -x "$ROOT/apps/studio/build/retr01_studio" ]]; then
  STUDIO_BIN="$ROOT/apps/studio/build/retr01_studio"
else
  die "studio binary missing (bin/studio or apps/studio/build/retr01_studio)"
fi
if [[ -x "$ROOT/bin/emu" ]]; then
  EMU_BIN="$ROOT/bin/emu"
elif [[ -x "$ROOT/apps/emu/build/retr01_emu" ]]; then
  EMU_BIN="$ROOT/apps/emu/build/retr01_emu"
else
  die "emu binary missing (bin/emu or apps/emu/build/retr01_emu)"
fi

PROJECT="$(resolve "$PROJECT")"
CART="$(resolve "$CART")"
mkdir -p "$OUT_DIR"

# Kill leftover capture processes from a prior failed run.
cleanup_pids=()
close_pid() {
  local pid="${1:-}"
  local title_re="${2:-}"
  local wid
  [[ -n "$pid" ]] || return 0
  # Prefer a clean app quit when possible (Emu: Esc).
  if [[ -n "$title_re" ]]; then
    wid="$(xdotool search --name "^${title_re}$" 2>/dev/null | head -n1 || true)"
    if [[ -n "$wid" ]]; then
      xdotool windowactivate --sync "$wid" key --clearmodifiers Escape 2>/dev/null || true
      sleep 0.2
    fi
    # Close every matching window (Emu also opens Debug).
    while read -r wid; do
      [[ -n "$wid" ]] || continue
      xdotool windowclose "$wid" 2>/dev/null || true
      xdotool windowkill "$wid" 2>/dev/null || true
    done < <(xdotool search --name "^${title_re}$" 2>/dev/null || true)
    if [[ "$title_re" == *"Emulator"* ]]; then
      while read -r wid; do
        [[ -n "$wid" ]] || continue
        xdotool windowclose "$wid" 2>/dev/null || true
        xdotool windowkill "$wid" 2>/dev/null || true
      done < <(xdotool search --name "^Debug$" 2>/dev/null || true)
    fi
  fi
  kill "$pid" 2>/dev/null || true
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.1
  done
  if kill -0 "$pid" 2>/dev/null; then
    kill -9 "$pid" 2>/dev/null || true
  fi
  wait "$pid" 2>/dev/null || true
}

cleanup() {
  local pid
  for pid in "${cleanup_pids[@]:-}"; do
    close_pid "$pid" ""
  done
  cleanup_pids=()
  rm -f "$ROOT/.readme-shot-tmp.png"
}
trap cleanup EXIT

wait_window() {
  local title="$1"
  local deadline=$((SECONDS + WAIT_S))
  local id=""
  while (( SECONDS < deadline )); do
    id="$(xdotool search --onlyvisible --name "^${title}$" 2>/dev/null | head -n1 || true)"
    if [[ -n "$id" ]]; then
      printf '%s' "$id"
      return 0
    fi
    sleep 0.2
  done
  return 1
}

shot_window() {
  local title="$1"
  local out="$2"
  local expect_w="${3:-0}"
  local expect_h="${4:-0}"
  local tmp="$ROOT/.readme-shot-tmp.png"
  local wid="" tries=0
  local X Y WIDTH HEIGHT SCREEN name
  # Re-resolve the window each try: SDL may recreate or remap after first present.
  while (( tries < 30 )); do
    wid="$(xdotool search --onlyvisible --name "^${title}$" 2>/dev/null | head -n1 || true)"
    if [[ -n "$wid" ]]; then
      name="$(xdotool getwindowname "$wid" 2>/dev/null || true)"
      if (( expect_w > 0 && expect_h > 0 )); then
        xdotool windowsize --sync "$wid" "$expect_w" "$expect_h" >/dev/null 2>&1 || true
        sleep 0.15
      fi
      if xdotool windowactivate --sync "$wid" >/dev/null 2>&1; then
        sleep 0.4
        # Shell vars: X Y WIDTH HEIGHT SCREEN
        eval "$(xdotool getwindowgeometry --shell "$wid" 2>/dev/null)" || true
        if [[ -n "${WIDTH:-}" && -n "${HEIGHT:-}" && -n "${X:-}" && -n "${Y:-}" ]]; then
          # Crop from the root window. Direct -window grabs are flaky here after
          # Studio exits (occasional near-fullscreen PNGs).
          if import -window root -crop "${WIDTH}x${HEIGHT}+${X}+${Y}" +repage "$tmp" 2>/dev/null; then
            local iw ih
            iw="$(identify -format '%w' "$tmp" 2>/dev/null || echo 0)"
            ih="$(identify -format '%h' "$tmp" 2>/dev/null || echo 0)"
            if (( iw == WIDTH && ih == HEIGHT )); then
              mv -f "$tmp" "$out"
              echo "wrote $out (${iw}x${ih}) name='${name}'"
              return 0
            fi
            echo "retry: crop ${iw}x${ih} vs ${WIDTH}x${HEIGHT} name='${name}'" >&2
          fi
        fi
      fi
    fi
    tries=$((tries + 1))
    sleep 0.2
  done
  die "could not screenshot window matching /^${title}$/"
}

capture_studio() {
  local wid pid
  [[ -f "$PROJECT" ]] || die "project not found: $PROJECT"
  echo "launching Studio: $PROJECT"
  "$STUDIO_BIN" "$PROJECT" >/tmp/r01-readme-studio.log 2>&1 &
  pid=$!
  cleanup_pids+=("$pid")
  wid="$(wait_window "Retr01 Studio")" || die "Studio window did not appear (see /tmp/r01-readme-studio.log)"
  # Default present scale is 2x; force 1x for README (640x360 logical).
  xdotool windowactivate --sync "$wid" key --clearmodifiers ctrl+1
  sleep "$(awk "BEGIN{print $SETTLE_MS/1000}")"
  # Re-assert size in case the hotkey raced with first layout.
  xdotool windowsize "$wid" 640 360 || true
  sleep 0.4
  shot_window "Retr01 Studio" "$STUDIO_OUT" 640 360
  close_pid "$pid" "Retr01 Studio"
  # Let the WM fully drop Studio before Emu maps (avoids bad grabs).
  sleep 0.5
}

capture_emu() {
  local pid
  [[ -f "$CART" ]] || die "cart not found: $CART"
  echo "launching Emu: $CART"
  "$EMU_BIN" "$CART" >/tmp/r01-readme-emu.log 2>&1 &
  pid=$!
  cleanup_pids+=("$pid")
  # Match main play window only (not the separate Debug window).
  wait_window "Retr01 Emulator \\(Phase 1\\)" >/dev/null \
    || die "Emu window did not appear (see /tmp/r01-readme-emu.log)"
  sleep "$(awk "BEGIN{print $SETTLE_MS/1000}")"
  # Pin geometry so the grab is stable across sessions (matches existing README size).
  shot_window "Retr01 Emulator \\(Phase 1\\)" "$EMU_OUT" 512 480
  close_pid "$pid" "Retr01 Emulator \\(Phase 1\\)"
}

if (( DO_STUDIO )); then
  capture_studio
fi
if (( DO_EMU )); then
  capture_emu
fi

echo "done"
