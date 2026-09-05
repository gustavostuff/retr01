# Retr01 Studio

Visual authoring for Retr01 worlds, screens, and `.retr01` cartridge images. Studio is two tools in one app:

1. **Authoring (UI)**. Edit worlds, tiles, palettes, sprites, entities, and instances.
2. **Export + Play**. **Ctrl+E** (or **Play**) writes a packed cart and generated game tree under `output/`. **Play** then opens the **emulator render screen** on that cart so Studio preview matches standalone `./emu` pixel-for-pixel.

Authoring state lives in `output/<stem>.r01proj` (JSON). **`custom_logic.c`** is created on first export and never overwritten. Hardware contract: [`docs/graphics.md`](../../docs/graphics.md).

There is **no** Studio-only host Play path. Preview always goes through export then shared emu core ([`app/emu/`](../emu/README.md)). **Sim is not involved.**

**Stack:** C11 + SDL2 + FreeType (Proggy Tiny), `libretr01_studio_core` + thin shell + shared `retr01_emu` core for Play.

---

## Authoring (UI)

Fixed **640x360** or **1280x720** logical canvas (**Ctrl+Shift+R** toggles). Window present size stays **1280x720** (640x360 is drawn **2x**, nearest). **8px** grid, dark gray chrome. Buttons/labels **16px** tall. Proggy Tiny (`assets/proggy-tiny.ttf`). Screen / Play previews scale with canvas (sharp nearest). Sidebar accordion: per-section expand/collapse (`UI_ACCORDION_ALWAYS_EXPANDED`), **250ms** open/close animation (`UI_ACCORDION_ANIM_MS`).

```text
+--------------------------------------------------------------------------------+
| SIDEBAR (128) | PREVIEW (centered)              | CTRL (128)                  |
| Worlds        | +------------------+            | [ Play ]                    |
|  [1]..[8]     | | screen preview   |            | ( ) BG layer                |
|  world map    | | (same slot for   |            | ( ) SPR layer               |
| Palettes..    | |  edit + play)    |            | ( ) Tile sel                |
| Sprites..     | +------------------+            | ( ) Tile paint              |
+--------------------------------------------------------------------------------+
```

| Control | Behavior |
|---------|----------|
| **Worlds** | **8** world buttons (**1-8**, internal indices **0-7**). World 1 starts with **3x3** blank screens on a **16x16** slot map. Worlds 2-8 start empty until first click. Cart cap: **48 present BG1 screens**/world ([`docs/graphics.md`](../../docs/graphics.md), [`docs/memory.md`](../../docs/memory.md)). Selected world: active strip **11px** + **7px** **BG1**/**BG0** sub-button. Inactive dual-view world tabs fill **18px** (tab+sub stack) so the row height matches |
| **World map (BG1)** | Sparse **16x16** playfield map (col/row **0-15**, packed as nibbles in cart). Present = blue. White fill = default spawn. White outline = selected |
| **World map (BG0)** | Second BG plane. Sparse **16x16** chess like BG1. Up to **8** present screens anywhere (double-click to create). Export packs present BG0 screens (coords normalized to the present bbox origin). Non-Play screen preview and Host Play composite BG0 under BG1 color 0; missing BG1 slots / outside the present-screen bbox show backdrop (not BG0). BG0 scrolls on an axis only when its enclosing present extent is **smaller** than BG1 on that axis ([`docs/graphics.md`](../../docs/graphics.md)) |
| **Double-click** empty slot | Create screen (BG1 or BG0 plane) |
| **Click** any slot | Select grid cell (empty or present, white outline). Present also becomes the edit target |
| **Ctrl+C / Ctrl+V** | Copy / paste entire selected screen (tiles+attrs) on BG0 or BG1. Paste creates the slot if empty |
| **Delete** | Remove selected present screen (BG0 or BG1). Prefer instance Delete when a sprite is selected |
| **Ctrl+click** present | Remove screen |
| **Right-click** map cell | **Set default screen** / **Make default world** (BG1) |
| **Chrome clicks** | Tabs, sub-button, accordion, radios, and similar chrome commit on **mouse release** over the same control (paint/drag tools stay press-and-hold) |
| **BG / Sprite layer** | Radios in **control sidebar** (right column). **BG layer**: tile select/paint + tile context menu. **Sprite layer**: select/drag instances, **H/V** or context **Mirror H/V** (per-instance: remaps part X/Y + toggles `FLIP_H`/`FLIP_V` for OAM), instance context menu (edit entity type / remove). Tile radios dim on sprite layer |
| **Tile Sel / Paint** | Radios in **control sidebar** under layer radios (BG layer only). Paint stamps armed tile+palette. **F+click** flood-fills matching tile+attr |
| **Right-click tile** (BG layer) | Move to tile bank, add tile, edit tile, set palette/anim/solid. **Shift** turns **Edit tile** into **Edit tile (all)** |
| **Right-click instance** (Sprite layer) | Mirror H / Mirror V / Edit entity type / Remove instance |
| **Edit tile** modal | **288x160**, 4x4 palette picker, **128x128** pixel canvas. **F+click** flood-fills CHR color. **Ctrl+V** pastes clipboard PNG (transparent -> index 0, opaque matched by brightness to the selected palette). **(all)** save writes CHR and applies bank/pal/H/V to every world cell (BG1+BG0) that matched the original tile id + attrs |
| **Edit sprite** modal | Same canvas as tile (**F+click** flood-fill). **Ctrl+V** pastes clipboard PNG onto the SPR canvas (same rules, SPR palette) |
| **Set Solid** | Toggles `R01_ATTR_SOLID` (`0x40`) on matching tiles in active world (bank+pal+flips, not tile ID) |
| **Palette strip** | Click BG/SPR strip -> **Global palettes** modal. Row **0-7** sets `default_pal_row` for the active world |
| **Sprites** | List of SPR catalog entries (**1x** icons + bank tile index). Empty: **empty** + **Add**. Create/Edit modal: SPR bank dots + 16x16 tile grid, 4x4 SPR palette, LMB drag parts, RMB paint. Right-click: edit, remove, set palette, change sprite bank. New sprites fill bank **0**, then **1..3** |
| **Metasprites** | Reusable multi-part SPR groups (no origin/hitbox). Empty: **empty** + **Add**. Modal: **Name** field (caret/selection/scroll) + derived id (`w_NN_slug`), SPR bank left, 16x16 compose right, 4x4 SPR palette, LMB/RMB. Sidebar: 16x16 centered preview + clipped names. Hover tooltip shows name + id. Right-click: edit, remove. **Studio-only**: not a separate cart table. Export flattens into entity parts |
| **Entities** | List of entity types (16x16 centered preview + **entity name**, clipped). Empty: **empty** + **Add**. Modal: left **metasprite catalog**. Right **Name** / state name text fields, **State**/**Frame** strips, live frame id, compose, guides, SPR palette. Sidebar hover: name + type id. Right-click: **Edit** / **Mark as player** (or Unmark) / **Remove**. Esc blurs field then closes. Click scrim closes. Cart packs state0/frame0 for all types |
| **Place on screen** | Drag a **Sprites**, **Metasprites**, or **Entities** row onto the screen preview (switches to **Sprite layer**). Sprite drop auto-creates a 1-state/1-frame/1-part entity and places an instance. Metasprite drop auto-creates an entity from the group and places an instance. Entity drop places that type. Instance `world_x/y` is the **user origin** (compose cross). Parts/hitbox draw as `(coord - origin)` relative to that. Optional instance `fh`/`fv` mirrors parts around the origin (JSON `"fh"`/`"fv"`, cart instance flags bit0/bit1). Sprites **clip to 128x120** when partially off-screen. On Sprite layer: click/drag instance to move (white outline). **H/V** mirrors. **Delete** removes. Visible in the editor screen preview (not a separate Studio Play compositor) |

PNG drop imports into the **active** world. Cart export packs **world 0** only (ignores `default_world`).

---

## Play

**Play** (button or **Space**) is not an in-editor soft preview. It:

1. Always runs the same **export** path as **Ctrl+E** (pack `.retr01` + regenerate `output/C/`, `output/ASM/`, `output/data/` as needed), even if the project is unsaved.
2. While export runs, shows a Studio-local **boot wait** UI (spinning `Booting console...` style text, same idea as the sim boot spinner, no sim code link).
3. Embeds the **emulator** in Studio via shared emu core. **Play** shows the game framebuffer only (debug pane stays in standalone `./emu`).

Shared emu core with standalone [`emu`](../emu/README.md). Standalone `./emu` remains for triage (may keep its own debug window). Cart export is still **world 0** only. **Sim is out of scope.**

| | |
|--|--|
| **Entry world** | Cart boots **world 0**. Editor **`default_world`** / sidebar selection do not change Phase 1 cart boot until multi-world export lands |
| **Camera** | **Dead-zone** profile (below). `r01_camera_set_deadzone(ctx, W, H)` in **`custom_logic.c`**. Export packs bytes **30-31** of the world header. Emu and Sim Host Play read them. Logic: `../common/r01_play_camera.c` |
| **Scroll** | Smooth pixel scroll. Spawn/warp **snap** centers the view on the player, then clamps origin inside the dead zone |
| **Player** | World **`player_entity`** (Entities context **Mark as player**). **8-dir idle/walk** from cart **player anim blob** (`PA` magic) driven by `custom_logic.c` hooks scanned at export. Stub: **SPR bank 0 tile 1** |
| **Other entities** | **State 0 / frame 0** only in Phase 1 Host Play |
| **Start** | **First placed instance** of the marked player type. If none / unmarked: center of **`default_screen`**. Fallback grid **(2,0)** or first present |
| **Collision** | Current anim-state hitbox vs `R01_ATTR_SOLID` on cart MAP attrs (not PRG collision stub) |
| **Warps** | **X** -> screen (0,0). **Y** -> screen (1,0). Test hooks only |

Gameplay SoT for Phase 1: emu Host Play (`app/emu/src/play.c` + `app/common/`). Studio does not maintain a parallel `core/src/play.c` preview.

### `custom_logic.c` hooks (host export)

Created on first export. Never overwritten. Typical init:

```c
void r01_custom_on_init(R01GameCtx *ctx) {
    r01_player_anim_set_idle_state(ctx, 0);
    r01_player_anim_set_walk_all(ctx, 1);
    r01_entity_state_frame_delay_set(ctx, 0, 10);
    r01_entity_state_frame_delay_set(ctx, 1, 4);
    r01_camera_set_deadzone(ctx, 32, 30);  /* centered rect W x H */
}
```

See generated `output/C/include/r01_*.h` for the full engine API (camera, player anim, projectiles, fades, warps, button events).

---

## Save / load (JSON v7)

**Ctrl+S** / **Ctrl+O** -> `output/test.r01proj` by default. Quit does **not** auto-save (use **Ctrl+S**).

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
| Grid | Sets world to **NxM** from atlas (max **16x16**) |
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

Kit **master indices** only ([`docs/graphics.md`](../../docs/graphics.md)). No per-tile RGB editor. Strip shows active BG/SPR row for the **active world**. Modal edits all **8 BG + 8 SPR** rows project-wide. Preview and cart burn quantize through Color PROM (**R3G3B2**).

---

## Code generation & export

**Ctrl+E** writes under `output/` (relative to launch cwd). **Save** (`Ctrl+S`) only updates the `.r01proj`. Export does not rewrite it unless you save first.

### Cart & sidecars

| Path | Contents |
|------|----------|
| `<stem>.r01proj` | Authoring JSON (save/load) |
| `<stem>.retr01` | Packed cart (**world 0**): BG+SPR CHR, MAP, palettes, PRG, entity tables, other screens (`format_ver` 2) |
| `<stem>_prom.bin` | 64-byte Color PROM image (motherboard, not in cart) |
| `<stem>_flash.bin` | Cart padded to **512 KB** |

PRG marker `R01P` at `$80F0`. Play table at `$8100`. Collision tables in PRG are for future on-cart 6502 use. Editor chrome is not burned into the cart. See [`app/sim/README.md`](../sim/README.md#cart-rom-vs-runners-triage).

### Generated game tree

| Path | Role |
|------|------|
| `C/base_game.c` | Regenerated each export: frozen tables, init/tick/vblank, calls into `custom_logic` |
| `C/r01_runtime.c` | Regenerated host stubs (pad helpers, warp, button events) |
| `C/custom_logic.c` | **User file**. Template on first export. Hooks for game-specific logic (camera dead zone, player anim, events) |
| `C/include/*.h` | `R01GameCtx`, engine API (`r01_input.h`, `r01_player.h`, ...) |
| `ASM/**` | Subdivided 6502 sources (`boot/`, `game/`, `io/`, `player/`, `sprite/`, `collision/`, `tables/`) |
| `data/*` | Palette, CHR, and per-screen MAP bins for `.incbin` |

The packer still builds PRG bytes in `prg_phase1.c` (byte-compatible with pre-codegen export). The on-disk `ASM/` and `data/` trees are the stable layout for a future ca65 build. They are not assembled during export today. **Play** always consumes the packed cart through the shared emu path (not an in-editor compositor).

Compile from `output/C/` with `-Iinclude` (or `#include "include/r01_engine.h"` as generated).

### Limitations (current)

- **NPC / non-player** instances use **state 0 / frame 0** only in Phase 1 Host Play (player uses full anim blob).
- Dead zone is configured in **`custom_logic.c`** only (no Studio UI slider yet).
- **World 0** only in cart export. Worlds 1-7 are session-only in the UI until multi-world save lands. Studio Play therefore previews world 0 only.
- No entity-vs-entity collision, NPC AI, or full on-cart 6502 gameplay loop yet.
- No ca65 / `make` step in the default export path.
- Studio Play uses embedded emu after export. Studio-only `core/src/play.c` remains for unit tests only.

Shared host runtime (not duplicated in export tree): `../common/r01_play_camera.c`, `../common/r01_play_anim*.c`, `../common/r01_custom_logic_scan.c`. Emu Host Play owns cart-backed preview.

---

## Build and run

From the repo root:

```bash
./build-all
./studio output/test.r01proj
./unit-tests
```

Developer rebuild of this tree only:

```bash
cd app/studio
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
| Play / pause | **Space** / **PLAY** (export cart, then open emu render) |
| Move player | **WASD** / arrows |
| Warp test | **X** -> (0,0), **Y** -> (1,0) |
| Save / load | **Ctrl+S** / **Ctrl+O** -> `output/test.r01proj` |
| Export cart | **Ctrl+E** -> `output/test.retr01` (+ `C/`, `ASM/`, `data/`) |
| Toggle canvas | **Ctrl+Shift+R** -> **640x360** / **1280x720** |

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`docs/graphics.md`](../../docs/graphics.md) | Screens, VRAM, palettes, cart layout |
| [`docs/selling_points.md`](../../docs/selling_points.md) | Game modules (movement, camera dead zone, entities) |
| [`app/sim/README.md`](../sim/README.md) | Board sim + cart triage |
| [`app/emu/README.md`](../emu/README.md) | Cart runtime emulator |
