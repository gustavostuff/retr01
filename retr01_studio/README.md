# retr01 Studio

Visual authoring for retr01 worlds, screens, and `.retr01` cartridge images. **Phase 2** (current): multi-world sidebar, screen create/delete, tile edit/paint, solid collision attrs, global palette editor, default spawn screen. **Phase 1** still applies: PNG import, Play preview, export. Hardware: [`docs/02`](../docs/02_graphics_worlds_memory.md). Docs stub: [`docs/04`](../docs/04_retr01_studio.md).

**Stack:** C11 + SDL2 + FreeType (Proggy Tiny), `libretr01_studio_core` + thin shell.

---

## UI (Phase 2)

Fixed **640×360** logical canvas, **8px** grid, dark gray chrome. Buttons/labels **16px** tall; Proggy Tiny (`assets/proggy-tiny.ttf`).

```text
+-------------------------------------------------------------------+
| SIDEBAR (128)          | MAIN                                     |
| [>] Worlds             |              [ Play ]                    |
|  [0][1][2][3]          |         +------------------+  ( ) Sel    |
|  [4][5][6][7]          |         | Screen 256x240   |  ( ) Paint  |
|  +--------------+      |         | 128x120 @2x edit |             |
|  | 128x128 map  |      |         +------------------+             |
|  +--------------+      |                                          |
| [>] Palettes           |                                          |
|  BG/SPR strips + [0-7] |                                          |
+-------------------------------------------------------------------+
```

| Control | Behavior |
|---------|----------|
| **Worlds** | 8 world buttons; world 0 starts with **3×3** blank screens on an **8×8** slot map |
| **World map** | **16px** cell pitch; present = blue; white fill = default spawn; white outline = selected |
| **Double-click** empty slot | Create screen |
| **Ctrl+click** present | Remove screen |
| **Click** present | Select active screen |
| **Right-click** map cell | Set default screen / default world |
| **Tile Sel / Paint** | Radio beside screen; paint stamps armed tile+palette |
| **Right-click tile** (sel mode) | Move to bank, add tile, edit tile, set palette/anim/solid |
| **Edit tile** modal | **288×160**; 4×4 palette picker, **128×128** pixel canvas |
| **Set Solid** | Toggles `R01_ATTR_SOLID` (`0x40`) on matching tiles in active world (bank+pal+flips, not tile ID) |
| **Palette strip** | Click BG/SPR strip → **Global palettes** modal; row **0–7** sets `default_pal_row` |

PNG drop imports into the **active** world. Cart export remains **world 0** only.

---

## Play (Smooth + Eagle View)

| | |
|--|--|
| **Scroll** | Smooth pixel scroll; camera follows player; no dead zone |
| **Player** | One hardcoded **8×8** sprite; color from sprite pal row 0, index 1 |
| **Collision** | **8×8** AABB vs tiles with `R01_ATTR_SOLID` on present screens |
| **Start** | Center of **`default_screen`** (world map context menu); fallback **(2,0)** or first present |
| **Warps** | **X** → screen (0,0); **Y** → screen (1,0) — Phase 1 test hooks, no Events UI yet |

Play SoT: `core/src/play.c` + `collision.c`. Emu/sim use the same rules; re-export after solid edits.

---

## PNG import

| Rule | Value |
|------|--------|
| Drop target | Anywhere on window → **active** world, **BG bank 0** |
| Cell size | **128×120** px; PNG must be a multiple thereof |
| Grid | Sets world to **N×M** from atlas (max **8×8**) |
| Limits | ≤ **256** unique 8×8 tiles; ≤ **4** colors per PNG |
| Transparent cells | Skipped (screen not forced present) |
| Palettes | BG rows remapped to nearest kit masters after import |

---

## Palettes

Kit **master indices** only ([`docs/02`](../docs/02_graphics_worlds_memory.md)) — no per-tile RGB editor. Strip shows active BG/SPR row; modal edits all **8 BG + 8 SPR** rows (4 pals × 4 colors). Preview and cart burn quantize through Color PROM (**R3G3B2**).

---

## Export

**Ctrl+E** writes under `rom/` (relative to launch cwd):

| File | Contents |
|------|----------|
| `test.retr01` | Packed cart (world 0 CHR/MAP, palettes, PRG + collision stub) |
| `test_prom.bin` | 64-byte Color PROM image (motherboard, not in cart) |
| `test_boot.s` | Human-readable ca65 stub / equates |
| `test_flash.bin` | Cart padded to **512 KB** |

Editor chrome is not burned into the cart. See [`docs/08` — Cart ROM vs runners](../docs/08_simulator.md#cart-rom-vs-runners-triage).

---

## Phase history

| Phase | Scope |
|-------|--------|
| **0** | Core lib: project/world/screen structs, JSON I/O, CHR pack, cart image, unit tests — no author UI |
| **1** | Single-world PNG import, Play, `.retr01` export |
| **2** | Multi-world UI, tile edit/paint, solid/anim attrs, global palettes, default spawn (current) |

**Out of scope (for now):** sprite placement UI, parallax planes, Generate, multi-world cart export, dead-zone/fade scroll profiles, full 6502 gameplay loop.

---

## Build and run

```bash
scripts/run-unit-tests          # from repo root
scripts/run-studio rom/test.r01proj
```

Or manually:

```bash
cd retr01_studio
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_studio
```

**Needs:** CMake, C compiler, SDL2, libpng.

---

## Controls

| Action | Input |
|--------|--------|
| Select screen | Click present cell in Worlds |
| Set default screen / world | Right-click world map cell |
| Import atlas | Drop PNG anywhere |
| Tile sel / paint | Radio rows beside screen |
| Tile context menu | Right-click tile (selection mode) |
| Edit global palettes | Click BG/SPR strip |
| Play / pause | **Space** / **PLAY** |
| Move player | **WASD** / arrows |
| Warp test | **X** → (0,0), **Y** → (1,0) |
| Save / load | **Ctrl+S** / **Ctrl+O** → `rom/test.r01proj` (JSON v4) |
| Export cart | **Ctrl+E** → `rom/test.retr01` |

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`docs/02`](../docs/02_graphics_worlds_memory.md) | Screens, VRAM, palettes, cart layout |
| [`docs/08`](../docs/08_simulator.md) | Board sim + cart loading |
| [`retr01_emu/README.md`](../retr01_emu/README.md) | Emulator Phase 1 |
