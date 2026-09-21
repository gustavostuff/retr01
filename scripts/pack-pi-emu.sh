#!/usr/bin/env bash
# Build a Pi-side tree: emu sources + example_01 cart. Compile on the Pi with ./build.sh.
# Usage: ./scripts/pack-pi-emu.sh [dest]
# Default dest: <repo>/raspberry_pi_test
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-$ROOT/raspberry_pi_test}"
CART="$ROOT/example_01/example_01.retr01"
EMU="$ROOT/apps/emu"
COMMON="$ROOT/apps/common"

die() { echo "error: $*" >&2; exit 1; }

[[ "$DEST" != "$ROOT" ]] || die "refusing to write dest at repo root"
[[ -f "$CART" ]] || die "missing cart: $CART"
[[ -d "$EMU/src" ]] || die "missing emu sources"

copy_file() {
  local src="$1"
  local dst="$2"
  mkdir -p "$(dirname "$dst")"
  cp -a "$src" "$dst"
}

echo "packing emu + cart -> $DEST"
rm -rf "$DEST"
mkdir -p "$DEST/apps/emu/src" "$DEST/apps/emu/include/retr01_emu" "$DEST/apps/common/fw"

for f in cart.c cpu.c io.c video.c play.c machine.c main.c; do
  copy_file "$EMU/src/$f" "$DEST/apps/emu/src/$f"
done
for f in cart.h cpu.h io.h video.h play.h machine.h types.h; do
  copy_file "$EMU/include/retr01_emu/$f" "$DEST/apps/emu/include/retr01_emu/$f"
done

COMMON_C=(
  r01_kit_palette.c
  r01_play_anim.c
  r01_play_anim_cart.c
  r01_play_camera.c
  r01_play_physics.c
  r01_custom_logic_scan.c
  r01_nes_synth.c
  r01_apu_fd.c
  r01_apu_tracker.c
  r01_apu_mix.c
  r01_bgm_fd.c
  r01_bgm_host.c
  r01_pad_keys.c
  r01_pad_host.c
  r01_readme_shot.c
)
COMMON_H=(
  r01_kit_palette.h
  r01_play_anim.h
  r01_play_anim_cart.h
  r01_play_camera.h
  r01_play_physics.h
  r01_custom_logic_scan.h
  r01_nes_synth.h
  r01_apu_fd.h
  r01_apu_tracker.h
  r01_apu_mix.h
  r01_bgm_fd.h
  r01_bgm_host.h
  r01_pad_keys.h
  r01_pad_host.h
  r01_readme_shot.h
  r01_apu_cart.h
  r01_hw_regs.h
)
for f in "${COMMON_C[@]}" "${COMMON_H[@]}"; do
  copy_file "$COMMON/$f" "$DEST/apps/common/$f"
done
copy_file "$COMMON/fw/r01_spi_mailbox.h" "$DEST/apps/common/fw/r01_spi_mailbox.h"
copy_file "$COMMON/fw/r01_apu_window.h" "$DEST/apps/common/fw/r01_apu_window.h"
[[ -f "$COMMON/gamecontrollerdb.txt" ]] || die "missing $COMMON/gamecontrollerdb.txt"
copy_file "$COMMON/gamecontrollerdb.txt" "$DEST/apps/common/gamecontrollerdb.txt"
copy_file "$CART" "$DEST/example_01.retr01"

cat > "$DEST/apps/emu/CMakeLists.txt" << 'EOF'
cmake_minimum_required(VERSION 3.16)
project(retr01_emu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(R01_PKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(R01_REPO_ROOT "${R01_PKG_ROOT}/.." CACHE PATH "Retr01 pack root")
set(R01_COMMON_DIR "${R01_PKG_ROOT}/common")
set(R01_FW_COMMON_DIR "${R01_COMMON_DIR}/fw")

add_library(r01_play_common
  ${R01_COMMON_DIR}/r01_kit_palette.c
  ${R01_COMMON_DIR}/r01_play_anim.c
  ${R01_COMMON_DIR}/r01_play_anim_cart.c
  ${R01_COMMON_DIR}/r01_play_camera.c
  ${R01_COMMON_DIR}/r01_play_physics.c
  ${R01_COMMON_DIR}/r01_custom_logic_scan.c
  ${R01_COMMON_DIR}/r01_nes_synth.c
  ${R01_COMMON_DIR}/r01_apu_fd.c
  ${R01_COMMON_DIR}/r01_apu_tracker.c
  ${R01_COMMON_DIR}/r01_apu_mix.c
  ${R01_COMMON_DIR}/r01_bgm_fd.c
)
target_include_directories(r01_play_common PUBLIC ${R01_COMMON_DIR} ${R01_FW_COMMON_DIR})
target_link_libraries(r01_play_common PUBLIC m)

add_library(retr01_emu_core
  src/cart.c
  src/cpu.c
  src/io.c
  src/video.c
  src/play.c
  src/machine.c
)
target_include_directories(retr01_emu_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(retr01_emu_core PUBLIC r01_play_common)

add_executable(retr01_emu
  src/main.c
  ${R01_COMMON_DIR}/r01_bgm_host.c
  ${R01_COMMON_DIR}/r01_pad_keys.c
  ${R01_COMMON_DIR}/r01_pad_host.c
  ${R01_COMMON_DIR}/r01_readme_shot.c
)
target_link_libraries(retr01_emu PRIVATE retr01_emu_core PkgConfig::SDL2)
target_compile_definitions(retr01_emu PRIVATE
  "R01_REPO_ROOT=\"${R01_REPO_ROOT}\""
  R01E_NO_DEBUG=1
  R01_README_SHOT=0
)
EOF

cat > "$DEST/build.sh" << 'EOF'
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
EOF

cat > "$DEST/retr01.sh" << 'EOF'
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
EOF

chmod +x "$DEST/build.sh" "$DEST/retr01.sh"

cat > "$DEST/README.md" << 'EOF'
# Retr01 emu (Pi pack)

Standalone **Phase 1** emulator plus `example_01.retr01`. No Studio, no tests, no sim.

Logical playfield is **128x120**. Host FB is hardware **2x** (**256x240**). The Pi binary has **no debug window**. `retr01.sh` starts at **1x** integer (128x120 centered in desktop-fullscreen, CRT / RGB-Pi OS4 Ports). Home / Guide on a pad opens Reset, Quit, 1x/2x, and Mute On/Off, and mutes the speaker while that overlay is open.

## Build

```bash
./build.sh
```

Build depends on CMake, a C compiler, pkg-config, and SDL2 development files (`libsdl2-dev` on Debian). `./build.sh` compiles the ARM binary and installs it with the cart and gamepad DB to `/media/usb1/roms/ports/Retr01_test/`, plus the Ports launcher at `/media/usb1/roms/ports/retr01.sh`.

## RGB-Pi OS4 Ports

After `./build.sh`, the runnable Ports payload is:

| File | Role |
|------|------|
| `/media/usb1/roms/ports/retr01.sh` | Ports launcher |
| `/media/usb1/roms/ports/Retr01_test/retr01_emu` | Binary |
| `/media/usb1/roms/ports/Retr01_test/example_01.retr01` | Cart |
| `/media/usb1/roms/ports/Retr01_test/gamecontrollerdb.txt` | SDL gamepad DB (next to the binary) |

RGB-Pi OS4 Ports lists `.sh` files from USB `ports/`. `retr01.sh` launches that binary and cart at 1x integer (128x120 centered), 48 kHz / 2048-sample audio. The Pi CMake build defines `R01E_NO_DEBUG` so the debug window is never created.

Env overrides:

| Var | Value | Effect |
|-----|--------|--------|
| `R01E_SCALE` | `1` | 128x120 integer, centered (default in `retr01.sh`) |
| `R01E_SCALE` | unset / `2` | 256x240 integer, centered (hardware 2x / console default) |
| `R01E_FULLSCREEN` | `1` | Desktop-fullscreen, no debug window |
| `R01E_NO_DEBUG` | `1` | No debug window (compiled out of the Pi binary) |
| `R01E_AUDIO_SAMPLES` | `2048` | Mix buffer (default in `retr01.sh`, desktop default 256) |
| `R01E_AUDIO_RATE` | `48000` | Mix rate (default in `retr01.sh`, desktop default 44100) |

Gamepad Home / Guide opens Reset / Quit / 1x-2x / Mute On/Off.

## Layout

| Path | Role |
|------|------|
| `apps/emu/` | Emulator C sources + CMake |
| `apps/common/` | Shared play / APU / pad host |
| `example_01.retr01` | Packed cart |
| `build.sh` | Release cmake build |
| `retr01.sh` | Run / Ports wrapper |
EOF

echo "done: $DEST"
echo "on the Pi: cd raspberry_pi_test && ./build.sh"
