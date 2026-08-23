# Retr01 Studio

Phase 0–5 authoring tool for Retr01 worlds / screens / CHR / palettes / OAM / constraints / Play / cart export.

**Stack:** C11 + SDL2. Shared `libretr01_studio_core` + thin SDL shell — matches [`docs/04_retr01_studio.md`](../docs/04_retr01_studio.md).

## Build

```bash
cd studio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_studio
```

Or use the helper scripts:

| Script | What it does |
|--------|----------------|
| `scripts/run.sh` | Run the app (build must already exist) |
| `scripts/build-run.sh` | Configure if needed, build, run |
| `scripts/test-unit.sh` | Build + run **core** unit tests only |
| `scripts/test-e2e-watch.sh [speed]` | Visual E2E (`E2E_WATCH`); `speed` defaults to `1` (higher = faster) |

```bash
./scripts/build-run.sh
./scripts/test-unit.sh
./scripts/test-e2e-watch.sh 2    # ~2× faster stepping
```

Needs: CMake, a C compiler, SDL2 (`sdl2` package), libpng.

## Tests

```bash
ctest --test-dir build --output-on-failure   # headless (no window)
```

- **core** — unit tests for pack / JSON / play / cart  
- **e2e** — visual + functional UI tests ([app/tests/README.md](app/tests/README.md)); uses [greatest](https://github.com/silentbicycle/greatest) + golden BMPs

Watch the suite drive a real window:

```bash
E2E_WATCH=1 ./build/test_e2e
```

Update UI goldens after intentional layout changes:

```bash
UPDATE_GOLDENS=1 SDL_VIDEODRIVER=offscreen ./build/test_e2e
```

## Controls

| Action | How |
|--------|-----|
| World / planes | Ctrl+click grid or **P0/P1** |
| Import PNG atlas | **Drop** `.png` on the **Worlds** panel only (or **Ctrl+I** → `project.png` / `import.png`). ≤4 colors; size multiple of 128×120; transparent cells skipped. Auto-packs BG banks 0→3 (error toast if >1024 unique tiles) |
| Layer | **L** or **BG** / **SPR** buttons |
| Pixel / attr (BG) | **Tab** or **PIX** / **ATTR** |
| Paint BG | BG layer + PIX: drag (colors **1-4**) |
| BG attrs | ATTR: **B P H V O N** |
| Sprite tile paint | SPR + **TILE**: paint 8x8 CHR (**1-4**) |
| Place OAM | SPR + **PLACE**: click screen |
| Generate | **Ctrl+G** — BG or SPR bank by layer |
| Grid overlay | **G** |
| Constraints / Play | Scroll left to **Play / Constraints**. **Space** / **PLAY**; WASD move. **DZ+WARP** + **FADE-BLK**: **Enter** warps to hub screen **(0,0)** (no-op if already there; Shift unused). Fade lerps BG+SPR to master 0 (10 frames), swaps, fades in |
| Cart I2C save flag | **I2C SAV** in Constraints |
| Export cart | **Ctrl+E** → `project.retr01`, `_prom.bin`, `_boot.s`, `_flash.bin` |
| Save / load | **Ctrl+S** / **Ctrl+O** → `project.json` (v8; RLE on pixels/tiles/attrs hex) |

## Export

**Ctrl+E** writes next to the project stem:

| File | Contents |
|------|----------|
| `*.retr01` | Packed cart (magic `RETR01`, pals, 32 KB stub PRG, worlds/CHR/MAP) |
| `*_prom.bin` | 64-byte Color PROM (R3G3B2) |
| `*_boot.s` | Readable ca65 stub + constraint equates |
| `*_flash.bin` | Same image padded to **512 KB** (SST39SF040) |

## Layout

Fixed **640x360** logical canvas, integer scale. Left column scrolls (Worlds / Planes / BG / Sprite / Palettes / Constraints). Top-right **VRAM 1×** shows the 2×2 camera slots with the 128×120 viewport frame.
