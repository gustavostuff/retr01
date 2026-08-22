# Studio E2E tests

Visual + functional end-to-end tests that drive the **real** Studio UI (`app_shell` + `ui_*`) under SDL2.

**Framework:** [greatest](https://github.com/silentbicycle/greatest) (vendored header).  
**Harness:** injects keyboard/mouse/wheel events, captures the 640×360 logical canvas, compares to golden BMPs.

## Run (CI / headless)

`ctest` runs the suite **without a visible window** (`SDL_VIDEODRIVER=offscreen` + hidden window). That is intentional for speed and deterministic goldens.

```bash
cd studio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
# or:
SDL_VIDEODRIVER=offscreen ./build/test_e2e
```

## Watch (see the app)

```bash
cd studio
cmake --build build
E2E_WATCH=1 ./build/test_e2e
# slower/faster stepping:
E2E_WATCH=1 E2E_WATCH_MS=250 ./build/test_e2e
```

Opens a real **Retr01 Studio — E2E watch** window and steps through clicks/keys so you can see each action. Still asserts goldens.

## Update goldens

After intentional UI changes:

```bash
UPDATE_GOLDENS=1 SDL_VIDEODRIVER=offscreen ./build/test_e2e
```

Writes `app/tests/goldens/*.bmp`. On failure without update, diffs land in `build/e2e_out/` (`*_got.bmp`, `*_diff.bmp`).

## Coverage

| Test | Checks |
|------|--------|
| `cold_start_layout` | Initial shell layout (visual) |
| `place_and_select_screen` | Ctrl+click world grid |
| `paint_bg_pixels` | PIX paint + color |
| `attr_mode_flags` | Tab ATTR + SOLID/ANIM |
| `layer_sprite_place_oam` | SPR layer + OAM place |
| `generate_bg_bank` | Ctrl+G pack |
| `constraints_and_play` | Scroll constraints, Play/Esc |
| `save_load_roundtrip` | Ctrl+S / Ctrl+O |
| `export_cart_bundle` | Ctrl+E → `.retr01` + prom/boot/flash |
| `planes_toggle` | Parallax P0 |
| `grid_toggle_and_palette` | G + palette region |
