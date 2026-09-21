# Retr01 emu (Pi pack)

Standalone **Phase 1** emulator plus `example_01.retr01`. No Studio, no tests, no sim.

Logical framebuffer is **256x240**. `retr01.sh` starts at **1x** and fullscreen (CRT / RGB-Pi OS4 Ports). Home / Guide on a pad opens Reset, Quit, and 1x/2x.

## Build

On the Pi, inside this folder:

```bash
./build.sh
./retr01.sh
```

Needs CMake, a C compiler, pkg-config, and SDL2 development files (`libsdl2-dev` on Debian). `./build.sh` on the Pi produces the ARM binary.

## RGB-Pi OS4 Ports

After `./build.sh`, the runnable Ports payload is:

| File | Role |
|------|------|
| `retr01.sh` | Ports launcher (menu name follows this file) |
| `retr01_emu` | Binary |
| `example_01.retr01` | Cart |
| `gamecontrollerdb.txt` | SDL gamepad DB (next to the binary) |

RGB-Pi OS4 Ports lists `.sh` files from the ports roms dir or USB `ports/`. `retr01.sh` sets `R01E_SCALE=1`, `R01E_FULLSCREEN=1`, and skips the debug window.

Env overrides:

| Var | Value | Effect |
|-----|--------|--------|
| `R01E_SCALE` | `1` | Present 256x240 (default in `retr01.sh`) |
| `R01E_SCALE` | unset / `2` | Present 512x480 |
| `R01E_FULLSCREEN` | `1` | Desktop-fullscreen, no debug window |
| `R01E_NO_DEBUG` | `1` | No debug window |

Gamepad Home / Guide still opens Reset / Quit / 1x-2x.

## Layout

| Path | Role |
|------|------|
| `apps/emu/` | Emulator C sources + CMake |
| `apps/common/` | Shared play / APU / pad host |
| `example_01.retr01` | Packed cart |
| `build.sh` | Release cmake build |
| `retr01.sh` | Run / Ports wrapper |
