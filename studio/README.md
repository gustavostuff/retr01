# Retr01 Studio

Phase 0–2 authoring tool for Retr01 worlds / screens / BG CHR / attrs / palettes.

**Stack:** C11 + SDL2 (not Love2D). Shared `libretr01_studio_core` + thin SDL shell — matches [`docs/04_retr01_studio.md`](../docs/04_retr01_studio.md).

## Why C + SDL2 (not Love2D + C)

- Docs already specify C/C++ and CTest for the core.
- One language for pack/JSON and later cart emit / tooling.
- Love2D is fine for prototypes; FFI + dual runtime is extra cost for a long-lived Studio.

## Build

```bash
cd studio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_studio
```

Needs: CMake, a C compiler, SDL2 (`sdl2` package).

## Controls

| Action | How |
|--------|-----|
| World tab | Click 1-8 (UI) = hardware worlds 0-7 |
| Place / remove screen | **Ctrl+click** world grid cell |
| Select screen | Click cell with a screen |
| Parallax plane | **Ctrl+click** **P0**/**P1** to toggle; click to edit (not on grid) |
| Pixel / attr mode | **Tab** or **PIX** / **ATTR** buttons |
| Paint | Pixel mode: click/drag (colors 0-3, keys **1-4**) |
| Edit tile attrs | Attr mode: click tile, then **B** bank, **P** pal, **H**/**V** flip, **O** solid, **N** anim |
| Generate bank radio | Toolbar BANK 0-3 **or** left BG bank tabs (same target via `select_bg_bank`) |
| Generate CHR | **Ctrl+G** — packs into selected bank; view stays on that bank |
| Tile grid overlay | **G** — toggle faint 8x8 grid on Screen |
| Palettes | Scroll left column; tabs **B0-B3** / **S0-S3**; click swatch + master grid or **-**/**=** |
| Save / load | **Ctrl+S** / **Ctrl+O** → `project.json` (format v3) |
| Scroll left column | **Mouse wheel** over left panel |
| Fullscreen | **Ctrl+F** (integer scale) |

Sprite banks remain Phase 3 stubs.

## Layout

Fixed logical canvas **640x360**, integer-scaled window (default 2x). Left column is a scrollable viewport so Worlds / Planes / BG / Sprite / Palette panels keep natural heights.
