# Retr01 Studio

Visual authoring for Retr01 worlds, screens, and `.retr01` cartridge images. **Phase 3E** (current): **Metasprites** accordion + modal (assemble multi-part SPR groups), entity compose from metasprite catalog, JSON v7. **Phase 3D**: cart packs real SPR CHR + entity type/instance tables; emu/sim Play OAM parity with Studio. **Phase 3C**: drag sprite/metasprite/entity onto screen, place instances, Studio Play OAM. **Phase 3B**: Entities accordion + Add/Edit entity modal. **Phase 3A**: Sprites accordion + Create/Edit sprite modal, SPR bank CHR catalog. **Phase 2** still applies: multi-world sidebar, screen create/delete, tile edit/paint, solid collision attrs, global palette editor, default spawn screen. **Phase 1** still applies: PNG import, Play preview, export. Hardware: [`docs/02`](../docs/02_graphics_worlds_memory.md).

**Stack:** C11 + SDL2 + FreeType (Proggy Tiny), `libretr01_studio_core` + thin shell.

---

## UI (Phase 2 + 3A-3E)

Fixed **640x360** logical canvas, **8px** grid, dark gray chrome. Buttons/labels **16px** tall. Proggy Tiny (`assets/proggy-tiny.ttf`).

```text
+-------------------------------------------------------------------+
| SIDEBAR (128)          | MAIN                                     |
| [>] Worlds             |              [ Play ]                    |
|  [1][2][3][4][5][6][7][8] |      +------------------+  ( ) Sel    |
|  +--------------+      |         | Screen 256x240   |  ( ) Paint  |
|  | 128x128 map  |      |         | 128x120 @2x edit |             |
|  +--------------+      |         +------------------+             |
| [>] Palettes           |                                          |
|  BG/SPR strips + [0-7] |                                          |
| [>] Sprites            |                                          |
|  icons + tile index    |                                          |
| [>] Metasprites        |                                          |
|  multi-part icons      |                                          |
| [>] Entities           |                                          |
|  icons + state name    |                                          |
+-------------------------------------------------------------------+
```

| Control | Behavior |
|---------|----------|
| **Worlds** | **8** world buttons (**1-8**, internal indices **0-7**). World 1 starts with **3x3** blank screens on an **8x8** slot map. Worlds 2-8 start empty until first click. Cart cap: **32 present screens**/world ([`docs/02`](../docs/02_graphics_worlds_memory.md)) |
| **World map** | **16px** cell pitch. Present = blue. White fill = default spawn. White outline = selected |
| **Double-click** empty slot | Create screen |
| **Ctrl+click** present | Remove screen |
| **Click** present | Select active screen (edit target) |
| **Right-click** map cell | **Set default screen** / **Make default world** |
| **Tile Sel / Paint** | Radio beside screen. Paint stamps armed tile+palette |
| **Right-click tile** (sel mode) | Move to tile bank, add tile, edit tile, set palette/anim/solid |
| **Edit tile** modal | **288x160**, 4x4 palette picker, **128x128** pixel canvas |
| **Set Solid** | Toggles `R01_ATTR_SOLID` (`0x40`) on matching tiles in active world (bank+pal+flips, not tile ID) |
| **Palette strip** | Click BG/SPR strip -> **Global palettes** modal. Row **0-7** sets `default_pal_row` for the active world |
| **Sprites** | List of SPR catalog entries (**1x** icons + bank tile index). Empty: **empty** + **Add**. Create/Edit modal: SPR bank dots + 16x16 tile grid, 4x4 SPR palette, LMB drag parts, RMB paint. Right-click: edit, remove, set palette, change sprite bank. New sprites fill bank **0**, then **1..3** |
| **Metasprites** | Reusable multi-part SPR groups (no origin/hitbox). Empty: **empty** + **Add**. Modal: same compose flow as entities but assembly-only (SPR bank left, 16x16 compose right, 4x4 SPR palette, LMB/RMB). Right-click: edit, remove. **Studio-only** -- not a separate cart table; export flattens into entity parts |
| **Entities** | List of entity types (composite icon + state-0 name). Empty: **empty** + **Add**. Modal: left **metasprite catalog** (drag onto compose); right state/frame dots, name field, 16x16 compose @8x, origin cross + hitbox (guides checkbox), 4x4 SPR palette. LMB select/drag parts, origin, hitbox; RMB paint selected part. Selected part: **H/V** flips, **1-4** palette, Delete removes. States locked to **0** (Idle) for now. Right-click: edit, remove |
| **Place on screen** | Drag a **Sprites**, **Metasprites**, or **Entities** row onto the screen preview. Sprite drop auto-creates a 1-state/1-frame/1-part entity and places an instance. Metasprite drop auto-creates an entity from the group and places an instance. Entity drop places that type. Instance `world_x/y` is the **user origin** (compose cross); parts/hitbox draw as `(coord - origin)` relative to that. Sprites **clip to 128x120** when partially off-screen. Click instance to select (white outline); **Delete** removes. Visible in edit view and **Play** (OAM slot 0 = player; instances fill 1+) |

PNG drop imports into the **active** world. Cart export packs **world 0** only (ignores `default_world`).

---

## Play (Smooth + Eagle View)

| | |
|--|--|
| **Entry world** | Play calls `begin_play` -> switches to **`default_world`** (map menu -> **Make default world**), not necessarily the sidebar selection |
| **Scroll** | Smooth pixel scroll. Camera follows player. No dead zone |
| **Player** | One hardcoded **8x8** sprite. Color from sprite pal row 0, index 1 |
| **Collision** | **8x8** AABB vs tiles with `R01_ATTR_SOLID` on present screens |
| **Start** | Center of **`default_screen`** in the play world. Fallback grid **(2,0)** or first present |
| **Warps** | **X** -> screen (0,0). **Y** -> screen (1,0). Phase 1 test hooks, no Events UI yet |

Play SoT: `core/src/play.c` + `collision.c`. Emu/sim mirror the same rules (separate source copies). Re-export after solid edits. Host collision reads **cart MAP attrs**, not the PRG collision stub. OAM X/Y are **viewport-relative signed** coords; tiles fully outside **128x120** are skipped; partial tiles clip at viewport edges (Studio, emu, sim).

---

## Save / load (JSON v7)

**Ctrl+S** / **Ctrl+O** -> `rom/test.r01proj` by default.

| Field | Behavior |
|-------|----------|
| `version` | **7** (`R01_JSON_VER`) |
| Palettes | Project-wide: all **8 BG + 8 SPR** rows |
| `other_screens` | Global title + interstitial + credits pages (480 B each; cart may RLE) |
| World data | **Active world only** on save: grid, screens, `bg_bank0`, `spr_banks`, sprite catalog, **`metasprites`**, `entities`, `instances`, `default_screen`, `default_pal_row` |
| Load | Always applies saved world data to **world 0**. Restores `default_world` / `active_world` indices. Initializes empty `other_screens` if missing. Legacy `credits` string ignored |
| Worlds 1-7 | Session-only until multi-world JSON lands (world **0** on disk) |
| v6 / older projects | Load OK with missing fields empty; re-save as v7. No cart-image migration -- re-export |

---

## PNG import

| Rule | Value |
|------|--------|
| Drop target | Anywhere on window -> **active** world, **BG bank 0** |
| Cell size | **128x120** px. PNG must be a multiple thereof |
| Grid | Sets world to **NxM** from atlas (max **8x8**) |
| Limits | <= **256** unique 8x8 tiles. <= **4** colors per PNG |
| Transparent cells | Skipped (screen not forced present) |
| Palettes | BG rows remapped to nearest kit masters after import |

---

## Palettes

Kit **master indices** only ([`docs/02`](../docs/02_graphics_worlds_memory.md)). No per-tile RGB editor. Strip shows active BG/SPR row for the **active world**. Modal edits all **8 BG + 8 SPR** rows project-wide. Preview and cart burn quantize through Color PROM (**R3G3B2**).

---

## Export

**Ctrl+E** writes under `rom/` (relative to launch cwd):

| File | Contents |
|------|----------|
| `test.retr01` | Packed cart (**world 0**): BG+SPR CHR, MAP, palettes, PRG stub, entity type/instance tables, **other screens** (title/inter/credits pages) (`format_ver` 2) |
| `test_prom.bin` | 64-byte Color PROM image (motherboard, not in cart) |
| `test_boot.s` | Human-readable ca65 stub / equates |
| `test_flash.bin` | Cart padded to **512 KB** |

PRG marker `R01P` at `$80F0`. Play table at `$8100`. Collision tables in PRG are for future 6502 use. Editor chrome is not burned into the cart. See [`retr01_sim/README.md`](../retr01_sim/README.md#cart-rom-vs-runners-triage).

---

## Phase history

| Phase | Scope |
|-------|--------|
| **0** | Core lib: project/world/screen structs, JSON I/O, CHR pack, cart image, unit tests. No author UI |
| **1** | Single-world PNG import, Play, `.retr01` export |
| **2** | Multi-world UI, tile edit/paint, solid/anim attrs, global palettes, default spawn (partial multi-world persistence) |
| **3A** | Sprites accordion, Create/Edit sprite modal (SPR pals), SPR bank CHR + catalog, JSON v5 |
| **3B** | Entities accordion + Add/Edit entity modal (compose / hitbox / origin) |
| **3C** | Drag sprite/entity onto screen; Studio Play OAM (origin-relative) |
| **3D** | Cart packs real SPR CHR + entity tables; emu/sim Play OAM parity |
| **3E** | Metasprites accordion + modal; entity compose from metasprite catalog; JSON v7; cart `format_ver` 2 (other screens incl. credits pages + RLE); viewport sprite clipping (current) |

**Out of scope (for now):** entity movement/collision, multi-state animation in Play, parallax planes, Generate, multi-world cart export, multi-world JSON save, dead-zone/fade scroll profiles, full 6502 gameplay loop.

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

**Needs:** CMake, C compiler, SDL2, libpng, FreeType 2.

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
| Add / edit sprite | Sprites accordion -> **Add**, or right-click -> Edit |
| Sprite context menu | Right-click sprite row (edit / remove / palette / bank) |
| Add / edit metasprite | Metasprites accordion -> **Add**, or right-click -> Edit |
| Metasprite context menu | Right-click metasprite row (edit / remove) |
| Add / edit entity | Entities accordion -> **Add**, or right-click -> Edit |
| Entity context menu | Right-click entity row (edit / remove) |
| Place catalog on screen | Drag Sprites / Metasprites / Entities row onto screen preview |
| Play / pause | **Space** / **PLAY** |
| Move player | **WASD** / arrows |
| Warp test | **X** -> (0,0), **Y** -> (1,0) |
| Save / load | **Ctrl+S** / **Ctrl+O** -> `rom/test.r01proj` |
| Export cart | **Ctrl+E** -> `rom/test.retr01` |

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`docs/02`](../docs/02_graphics_worlds_memory.md) | Screens, VRAM, palettes, cart layout |
| [`retr01_sim/README.md`](../retr01_sim/README.md) | Board sim + cart triage |
| [`retr01_emu/README.md`](../retr01_emu/README.md) | Emulator Phase 1 |
