# Retr01 Studio

Visual authoring for Retr01 worlds, screens, and `.retr01` cartridge images. **Phase 5** (current): `output/` tree — save/load defaults, **Ctrl+E** exports generated `C/`, `ASM/`, `data/` plus cart sidecars; `custom_logic.c` created once and preserved. **Phase 4**: named entities/metasprites, **Mark as player**, Play spawn at placed instance, player **hitbox** vs BG solid, cart/emu/sim parity. **Phase 3E**: Metasprites accordion + modal, entity compose from metasprite catalog, JSON v7, cart `format_ver` 2, viewport sprite clipping. **Phase 3D**: cart packs real SPR CHR + entity tables. **Phase 3C–3A / 2 / 1** still apply (place instances, sprites, multi-world sidebar, PNG import, export). Hardware: [`docs/02`](../docs/02_graphics_worlds_memory.md).

**Stack:** C11 + SDL2 + FreeType (Proggy Tiny), `libretr01_studio_core` + thin shell.

---

## UI (Phase 2–4)

Fixed **640x360** logical canvas, **8px** grid, dark gray chrome. Buttons/labels **16px** tall. Proggy Tiny (`assets/proggy-tiny.ttf`).

```text
+-------------------------------------------------------------------+
| SIDEBAR (128)          | MAIN                                     |
| [>] Worlds             |              [ Play ]                    |
|  [1][2][3][4][5][6][7][8] | ( ) BG   +------------------+ ( ) Sel  |
|  +--------------+      | ( ) SPR  | Screen 256x240   | ( ) Paint|
|  | 128x128 map  |      |         | 128x120 @2x edit |           |
|  +--------------+      |         +------------------+           |
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
| **BG / Sprite layer** | Radios **left** of screen. **BG layer**: tile select/paint + tile context menu. **Sprite layer**: select/drag instances, **H/V** or context **Mirror H/V** (per-instance: remaps part X/Y + toggles `FLIP_H`/`FLIP_V` for OAM), instance context menu (edit entity type / remove). Tile radios dim on sprite layer |
| **Tile Sel / Paint** | Radios **right** of screen (BG layer only). Paint stamps armed tile+palette |
| **Right-click tile** (BG layer) | Move to tile bank, add tile, edit tile, set palette/anim/solid |
| **Right-click instance** (Sprite layer) | Mirror H / Mirror V / Edit entity type / Remove instance |
| **Edit tile** modal | **288x160**, 4x4 palette picker, **128x128** pixel canvas. **Ctrl+V** pastes clipboard PNG (transparent -> index 0, opaque matched by brightness to the selected palette) |
| **Edit sprite** modal | Same canvas as tile. **Ctrl+V** pastes clipboard PNG onto the SPR canvas (same rules, SPR palette) |
| **Set Solid** | Toggles `R01_ATTR_SOLID` (`0x40`) on matching tiles in active world (bank+pal+flips, not tile ID) |
| **Palette strip** | Click BG/SPR strip -> **Global palettes** modal. Row **0-7** sets `default_pal_row` for the active world |
| **Sprites** | List of SPR catalog entries (**1x** icons + bank tile index). Empty: **empty** + **Add**. Create/Edit modal: SPR bank dots + 16x16 tile grid, 4x4 SPR palette, LMB drag parts, RMB paint. Right-click: edit, remove, set palette, change sprite bank. New sprites fill bank **0**, then **1..3** |
| **Metasprites** | Reusable multi-part SPR groups (no origin/hitbox). Empty: **empty** + **Add**. Modal: **Name** field (caret/selection/scroll) + derived id (`w_NN_slug`), SPR bank left, 16x16 compose right, 4x4 SPR palette, LMB/RMB. Sidebar: 16x16 centered preview + clipped names; hover tooltip shows name + id. Right-click: edit, remove. **Studio-only**: not a separate cart table. Export flattens into entity parts |
| **Entities** | List of entity types (16x16 centered preview + **entity name**, clipped). Empty: **empty** + **Add**. Modal: left **metasprite catalog**. Right **Name** / state name text fields, **State**/**Frame** strips, live frame id, compose, guides, SPR palette. Sidebar hover: name + type id. Right-click: **Edit** / **Mark as player** (or Unmark) / **Remove**. Esc blurs field then closes; click scrim closes. Cart packs state0/frame0 for all types |
| **Place on screen** | Drag a **Sprites**, **Metasprites**, or **Entities** row onto the screen preview (switches to **Sprite layer**). Sprite drop auto-creates a 1-state/1-frame/1-part entity and places an instance. Metasprite drop auto-creates an entity from the group and places an instance. Entity drop places that type. Instance `world_x/y` is the **user origin** (compose cross). Parts/hitbox draw as `(coord - origin)` relative to that. Optional instance `fh`/`fv` mirrors parts around the origin (JSON `"fh"`/`"fv"`, cart instance flags bit0/bit1). Sprites **clip to 128x120** when partially off-screen. On Sprite layer: click/drag instance to move (white outline). **H/V** mirrors. **Delete** removes. Visible in edit view and **Play** (OAM slot 0 = player. Instances fill 1+) |

PNG drop imports into the **active** world. Cart export packs **world 0** only (ignores `default_world`).

---

## Play

| | |
|--|--|
| **Entry world** | Play calls `begin_play` -> switches to **`default_world`** (map menu -> **Make default world**), not necessarily the sidebar selection |
| **Scroll** | Smooth pixel scroll. Camera follows player origin. No dead zone |
| **Player** | World **`player_entity`** (Entities context **Mark as player**). Draws that type’s **state 0 / frame 0**. If unset/empty, solid stub **SPR bank 0 tile 1**. Placed instances of the player type are skipped in Play OAM |
| **Start** | **First placed instance** of the marked player type (editor origin). If none / unmarked: center of **`default_screen`**. Fallback grid **(2,0)** or first present |
| **Collision** | Marked player’s state-0 **hitbox** (offset from origin) vs `R01_ATTR_SOLID` on present screens. Stub player: **8x8** at the origin |
| **Warps** | **X** -> screen (0,0). **Y** -> screen (1,0). Phase 1 test hooks, no Events UI yet |

Cart packs `player_entity` + hitbox into the world header — **re-export** after marking or solid edits. Play SoT: `core/src/play.c` + `collision.c`. Emu/sim mirror the same rules (separate source copies). Host collision reads **cart MAP attrs**, not the PRG collision stub. OAM X/Y are **viewport-relative signed** coords. Tiles fully outside **128x120** are skipped; partial tiles clip at viewport edges.

---

## Save / load (JSON v7)

**Ctrl+S** / **Ctrl+O** -> `output/test.r01proj` by default.

| Field | Behavior |
|-------|----------|
| `version` | **7** (`R01_JSON_VER`) |
| Palettes | Project-wide: all **8 BG + 8 SPR** rows |
| `other_screens` | Global title + interstitial + credits pages (480 B each. Cart may RLE) |
| World data | **Active world only** on save: grid, screens, `bg_bank0`, `spr_banks`, sprite catalog, **`metasprites`**, `entities`, `player_entity`, `instances`, `default_screen`, `default_pal_row` |
| Load | Always applies saved world data to **world 0**. Restores `default_world` / `active_world` indices. Initializes empty `other_screens` if missing. Legacy `credits` string ignored |
| Worlds 1-7 | Session-only until multi-world JSON lands (world **0** on disk) |
| v6 / older projects | Load OK with missing fields empty. Re-save as v7. No cart-image migration: re-export |

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

## Clipboard PNG paste

| Rule | Value |
|------|--------|
| Shortcut | **Ctrl+V** in **Edit tile** or **Create/Edit sprite** modal |
| Source | Clipboard `image/png` (GIMP Copy works). Linux: X11 selection, or `xclip` / `wl-paste` if present |
| Transparent | Alpha < 128 -> palette index **0** |
| Opaque | Match nearest of the **4** colors in the modal's selected palette by brightness (`r+g+b`) |
| Size | Top-left **8x8** of the image fills the canvas |

---

## Palettes

Kit **master indices** only ([`docs/02`](../docs/02_graphics_worlds_memory.md)). No per-tile RGB editor. Strip shows active BG/SPR row for the **active world**. Modal edits all **8 BG + 8 SPR** rows project-wide. Preview and cart burn quantize through Color PROM (**R3G3B2**).

---

## Export

**Ctrl+E** writes under `output/` (relative to launch cwd):

| Path | Contents |
|------|----------|
| `test.r01proj` | Save target (Ctrl+S only; export does not rewrite unless you save) |
| `test.retr01` | Packed cart (**world 0**): BG+SPR CHR, MAP, palettes, PRG stub, entity tables, other screens (`format_ver` 2) |
| `test_prom.bin` | 64-byte Color PROM image (motherboard, not in cart) |
| `test_flash.bin` | Cart padded to **512 KB** |
| `C/` | Generated `base_game.c`, `r01_runtime.c`, headers; `custom_logic.c` (template, never overwritten) |
| `ASM/` | Subdivided 6502 tree (`boot/`, `game/`, `io/`, `player/`, `sprite/`, `collision/`, `tables/`) |
| `data/` | Palette, CHR, and per-screen MAP bins for `.incbin` |

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
| **3C** | Drag sprite/entity onto screen. Studio Play OAM (origin-relative) |
| **3D** | Cart packs real SPR CHR + entity tables. Emu/sim Play OAM parity |
| **3E** | Metasprites accordion + modal. Entity compose from metasprite catalog. JSON v7. Cart `format_ver` 2 (other screens incl. credits pages + RLE). Viewport sprite clipping |
| **4** | **Mark as player**; Play/cart/emu/sim draw state0/frame0, spawn at first placed instance, collide with authored hitbox. Entity/metasprite **names** + derived ids. Text fields, Esc/scrim modal dismiss, readable sidebar context menus |
| **5** | `rom/` → `output/`; export writes `C/` (`base_game.c`, `custom_logic.c`, headers, `r01_runtime.c`), subdivided `ASM/` + `data/`; cart bytes unchanged (packer still `prg_phase1.c`) |

**Out of scope (for now):** multi-state animation in Play (authoring supports 1-4 states / 1-4 frames; Play/cart still state0/frame0), entity-vs-entity collision, NPC AI, parallax planes / variable-thickness slices authoring, Generate, multi-world cart export, multi-world JSON save, dead-zone/fade scroll profiles, full 6502 gameplay loop.

---

## Build and run

```bash
scripts/run-unit-tests          # from repo root
scripts/run-studio output/test.r01proj
```

Or manually:

```bash
cd retr01_studio
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_studio
```

**Needs:** CMake, C compiler, SDL2, libpng, FreeType 2. Optional: X11 (clipboard PNG), `xclip` / `wl-clipboard`.

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
| Mark / unmark player | Right-click entity row -> **Mark as player** / **Unmark as player** |
| Place catalog on screen | Drag Sprites / Metasprites / Entities row onto screen preview |
| Play / pause | **Space** / **PLAY** |
| Move player | **WASD** / arrows |
| Warp test | **X** -> (0,0), **Y** -> (1,0) |
| Save / load | **Ctrl+S** / **Ctrl+O** -> `output/test.r01proj` |
| Export cart | **Ctrl+E** -> `output/test.retr01` (+ `C/`, `ASM/`, `data/`) |

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`docs/02`](../docs/02_graphics_worlds_memory.md) | Screens, VRAM, palettes, cart layout |
| [`retr01_sim/README.md`](../retr01_sim/README.md) | Board sim + cart triage |
| [`retr01_emu/README.md`](../retr01_emu/README.md) | Emulator Phase 1 |
