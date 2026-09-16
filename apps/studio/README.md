# Retr01 Studio

Visual authoring for Retr01 worlds, screens, and `.retr01` cartridge images. Studio is two tools in one app:

1. **Authoring (UI)**. Edit worlds, tiles, palettes, sprites, entities, and instances.
2. **Export + Play**. **Ctrl+E** (or **Play**) writes a packed cart and generated game tree under `output/`. **Play** then opens the **emulator render screen** on that cart so Studio preview matches standalone `./emu` pixel-for-pixel.

Authoring state lives in `output/<stem>.r01proj` (JSON). **`custom_logic.c`** is created on first export and never overwritten. Hardware contract: [`general_docs/video-graphics.md`](../../general_docs/video-graphics.md).

There is **no** Studio-only host Play path. Preview always goes through export then shared emu core ([`app/emu/`](../emu/README.md)). **Sim is not involved.**

**Stack:** C11 + SDL2 + FreeType, `retr01_ui` (Proggy Tiny + widgets) + `libretr01_studio_core` + thin shell + shared `retr01_emu` core for Play.

---

## Authoring (UI)

```text
+---------------- Graphics | Audio ------------------------------------------+
| SIDEBAR (128) | PREVIEW (centered in remaining) | CTRL (128)               |  Graphics
| Tracks list   | BGM horizontal timeline (5 lanes) | Play/Pause/Stop + ch |  Audio
+--------------------------------------------------------------------------------+
```

Fixed **640x360** or **1280x720** logical canvas (**Ctrl+Shift+R** toggles). Present scale **Ctrl+1** / **Ctrl+2** (1x / 2x window). **8px** grid, dark gray chrome. Top **Graphics | Audio** app tabs (`UI_APP_CHROME_H`), flush left. Buttons/labels **16px** tall. Proggy Tiny (bundled in `ui/assets/proggy-tiny.ttf`). PNG chrome stays under `assets/png/` and is injected into `retr01_ui`. Screen / Play previews scale with canvas (sharp nearest). Sidebar accordion: per-section expand/collapse (`UI_ACCORDION_ALWAYS_EXPANDED`), **250ms** open/close animation (`UI_ACCORDION_ANIM_MS`). **Audio** tab: compact left-aligned **BGM | SFX** plane tabs. BGM is a horizontal multi-lane timeline (paint/resize note strips on a quarter-note snap grid via `R01_BGM_NOTE_DIV` / `UI_SOUND_SNAP_DIV`, wheel to change pitch, Ctrl+C/V copy/paste, Play/Pause/Stop playhead). Preview uses a first-party NES-like softsynth (pulse/tri/noise/DPCM stub over SDL2 audio). Not cart-protocol playback. Graphics **Play** remains emu-only (host BGM + P1 X/Y SFX). **Sim is not involved** and does not play host audio.

| Control | Behavior |
|---------|----------|
| **Worlds** | **8** world buttons (**1-8**, internal indices **0-7**). World 1 starts with **3x3** blank screens on a **16x16** slot map. Worlds 2-8 start empty until first click. Cart cap: **32 present BG1 screens**/world ([`general_docs/video-graphics.md`](../../general_docs/video-graphics.md), [`general_docs/memory.md`](../../general_docs/memory.md)). Selected world: active strip **11px** + **7px** **BG1**/**BG0** sub-button. Inactive dual-view world tabs fill **18px** (tab+sub stack) so the row height matches |
| **World map (BG1)** | Sparse **16x16** playfield map (col/row **0-15**, packed as nibbles in cart). Present = blue. White fill = default spawn. White outline = selected |
| **World map (BG0)** | **SNES-like parallax plane.** Sparse **16x16** chess like BG1. Up to **8** present screens (double-click to create). Paint BG1 color **0** as windows into BG0. Host Play composites both layers. Smaller BG0 bbox than BG1 -> slower scroll (true depth). Equal/larger on an axis -> that axis locked. Export packs present BG0 (coords bbox-origin relative). Missing BG1 / outside present bbox -> backdrop, not BG0 ([`general_docs/video-graphics.md`](../../general_docs/video-graphics.md), [`general_docs/software-api.md`](../../general_docs/software-api.md)) |
| **Double-click** empty slot | Create screen (BG1 or BG0 plane) |
| **Click** any slot | Select grid cell (empty or present, white outline). Present also becomes the edit target |
| **Ctrl+C / Ctrl+V** | **Screen preview** (BG / Both): copy / paste the tile selection. Clipboard is separate from the paint brush, so you can click a new target then **Ctrl+V**. Paste top-left is the tile under the cursor, or the selection top-left if the cursor is off-preview. **World map:** copy / paste an entire selected screen (tiles+attrs) on BG0 or BG1. Paste creates the slot if empty |
| **Delete** | Remove selected present screen (BG0 or BG1). Prefer instance Delete when a sprite is selected |
| **Ctrl+click** present | Remove screen |
| **Right-click** map cell | **Set default screen** / **Make default world** (BG1) |
| **Chrome clicks** | Tabs, sub-button, accordion, radios, and similar chrome commit on **mouse release** over the same control (paint/drag tools stay press-and-hold) |
| **Work on / Hide** | Control sidebar (right column). **Work on:** radios **BG layer** / **Sprite layer** / **Both** (default). **Both** prefers entity hits when selecting; empty clicks select tiles. **Hide:** checkboxes hide **BG layer** or **Sprite layer** in the preview and block all interaction with that layer. **BG / Both:** click selects; **Shift+click** expands; **Shift+drag** rubber-band (marching ants). **Ctrl+click/drag** paints with the selection stamp; holding **Ctrl** shows a grid-snapped stamp ghost. Hover ants at 50% opacity; selection ants at 100%. Tile context menu. **Sprite / Both:** select/drag instances, **H/V** or context **Mirror H/V**, instance context menu |
| **Tile paint** | **Ctrl+click/drag** stamps the armed brush (1x1 or full selection rect). **Alt+click** picks stamp. **F+click** flood-fills with the brush's top-left tile+attr |
| **App tabs** | **Graphics** / **Audio** / **Code** (Code screen TBD). Graphics and Audio keep equal half-sidebar widths; Code is sized to its label and sits to the right |
| **Right-click tile** (BG / Both) | Move to tile bank, add tile, edit tile, set palette/anim/solid. **Shift** turns **Edit tile** into **Edit tile (all)** |
| **Right-click instance** (Sprite / Both) | Mirror H / Mirror V / Edit entity type / Remove instance |
| **Edit tile** modal | **288x160**, 4x4 palette picker, **128x128** pixel canvas. Left-click paints; **right-click** picks the pixel color. **F+click** flood-fills CHR color. **Ctrl+Z** / **Ctrl+Y** (or **Ctrl+Shift+Z**) undo/redo pixel strokes while the modal is open. **Ctrl+V** pastes clipboard PNG (transparent -> index 0, opaque matched by HSV Value to the selected palette). **(all)** save writes CHR and applies bank/pal/H/V to every world cell (BG1+BG0) that matched the original tile id + attrs |
| **Edit sprite** modal | Same canvas as tile (**F+click** flood-fill). **Ctrl+V** pastes clipboard PNG onto the SPR canvas (same rules, SPR palette) |
| **Set Solid** | Toggles `R01_ATTR_SOLID` (`0x40`) on matching tiles in active world (bank+pal+flips, not tile ID) |
| **Palette strip** | Click BG/SPR strip -> **Global palettes** modal. Row **0-7** sets `default_pal_row` for the active world |
| **Banks** | BG/SPR CHR bank grids (tabs). Edit tiles from the bank sheet. Soft caps match [`general_docs/video-graphics.md`](../../general_docs/video-graphics.md) |
| **Player bank** | Global 256-tile pattern grid (no tabs). Right-click Add/Edit tile. See [`general_docs/memory.md`](../../general_docs/memory.md) |
| **Entities** | Primary object authoring. Types with up to **4** states x **4** frames x **6** sprites. Modal: full-width **Name** / **State name**; left column (right-aligned) palette, **State**/**Frame** strips, **Add**/**Remove**, **Highlight**, **Brush**; right column frame id + compose canvas with zoom (**Ctrl+wheel**, 1x-4x) and pan (**wheel** / **Shift+wheel**, middle-drag or right-drag; right-click still opens **Add sprite**). **Select | Edit** tool control: Select moves/reorders parts; Edit paints the topmost sprite under the cursor. **Ctrl+V** pastes clipboard PNG into the **selected** sprite CHR (top-left 8x8, same rules as Edit sprite). **Space** toggles light part outlines. **Origin/hitbox** checkbox shows guides (auto-computed from the state sprite bounding-box center). Sidebar hover: name + type id. Right-click list: **Edit** / **Mark as player** / **Remove**. Soft caps and **boss** assemblies (optional BG body + multi-entity attachments): [`general_docs/video-graphics.md`](../../general_docs/video-graphics.md). Cart packs locked EntityDef catalog (general_docs/software-api.md) |
| **Place on screen** | Drag an **Entities** row onto the screen preview (switches to **Sprite layer**) to place that type. Instance `world_x/y` is the **user origin** (compose cross). Parts/hitbox draw as `(coord - origin)` relative to that. Optional instance `fh`/`fv` mirrors parts around the origin (JSON `"fh"`/`"fv"`, cart instance flags bit0/bit1). Sprites **clip to 128x120** when partially off-screen. On Sprite/Both: click/drag instance to move (marching ants). **H/V** mirrors. **Delete** removes |

PNG drop imports into the **active** world. Cart export packs **world 0** only (ignores `default_world`).

---

## Play

**Play** (button or **Space**) is not an in-editor soft preview. It:

1. Always runs the same **export** path as **Ctrl+E** (pack `.retr01` + regenerate `output/C/`, `output/ASM/`, `output/data/` as needed), even if the project is unsaved.
2. While export runs, shows a Studio-local **boot wait** UI (spinning `Booting console...` style text, same idea as the sim boot spinner, no sim code link).
3. Embeds the **emulator** in Studio via shared emu core. **Play** shows the game framebuffer only (debug pane stays in standalone `./emu`).

Shared emu core with standalone [`emu`](../emu/README.md). Standalone `./emu` remains for triage (may keep its own debug window). Cart export is still **world 0** only. **Sim is out of scope.**

| Topic | Detail |
|-------|--------|
| **Entry world** | Cart boots **world 0**. Editor **`default_world`** / sidebar selection do not change Phase 1 cart boot until multi-world export lands |
| **Camera** | **Dead-zone** profile (below). `r01_camera_set_deadzone(ctx, W, H)` in **`custom_logic.c`**. Export packs bytes **30-31** of the world header. Emu and Sim Host Play read them. Logic: `../common/r01_play_camera.c` |
| **Scroll** | Smooth pixel scroll. Spawn/warp **snap** centers the view on the player, then clamps origin inside the dead zone |
| **Player** | World **`player_entity`** (Entities context **Mark as player**). A normal entity (**4** states x **4** frames x **6** sprites) that counts toward the **16** types/world catalog. On-screen instances share the **64** OAM sprite budget (not type-capped). **8-dir idle/walk** from cart **player anim blob** (`PA` magic). Host Play returns to **idle** when the stick is released (same as Studio authoring). Opt out of that snap with `r01_play_anim_set_release_to_idle(ctx, state, 0)` to hold a pose. Stub: **SPR bank 0 tile 1** |
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

**Ctrl+S** / **Ctrl+O** save/reload the current project path (no default fixture). Quit does **not** auto-save.

| Field | Behavior |
|-------|----------|
| `version` | **7** (`R01_JSON_VER`) |
| Palettes | Project-wide: all **8 BG + 8 SPR** rows |
| `other_screens` | Global title + interstitial + credits pages (480 B each. Cart may RLE) |
| World data | **Active world only** on save: grid, screens, `bg_bank0`, `spr_banks`, sprite catalog, `entities`, `player_entity`, `instances`, `default_screen`, `default_pal_row` |
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
| Shortcut | **Ctrl+V** in **Edit tile**, **Create/Edit sprite**, or **Add/Edit entity** (selected part) |
| Source | Clipboard `image/png` (GIMP Copy works). Linux: X11 selection, or `xclip` / `wl-paste` if present |
| Transparent | Alpha < 128 -> palette index **0** |
| Opaque | Match nearest active palette color by HSV **Value** (`max(r,g,b)`). Hue and saturation are ignored |
| SPR opaque | Indices **1..3** only (index **0** is transparent when drawn, so dark pixels are not forced there) |
| BG opaque | Indices **0..3** |
| Size | Top-left **8x8** of the image fills one tile / selected entity sprite |
| Entity modal | Requires a selected part. Writes SPR (or player-bank) CHR for that part. Undoable as a paint stroke |

---

## Palettes

Kit **master indices** only ([`general_docs/video-graphics.md`](../../general_docs/video-graphics.md)). No per-tile RGB editor. Strip shows active BG/SPR row for the **active world**. Modal edits all **8 BG + 8 SPR** rows project-wide. Preview and cart burn quantize through Color PROM (**R3G3B2**).

---

## Code generation & export

**Ctrl+E** writes under `output/` (relative to launch cwd). **Save** (`Ctrl+S`) only updates the `.r01proj`. Export does not rewrite the project file unless it was saved first.

### Cart & sidecars

| Path | Contents |
|------|----------|
| `<stem>.r01proj` | Authoring JSON (save/load) |
| `<stem>.retr01` | Packed cart (**world 0**): BG+SPR CHR, MAP, palettes, PRG, entity tables, other screens (`format_ver` 3) |
| `<stem>_prom.bin` | 64-byte Color PROM image (motherboard, not in cart) |
| `<stem>_flash.bin` | Cart padded to **512 KB** |

PRG marker `R01P` at `$80F0`. Play table at `$8100`. Collision tables in PRG are for future on-cart 6502 use. Editor chrome is not burned into the cart. See [`app/sim/README.md`](../sim/README.md#cart-rom-vs-runners-triage).

### Generated game tree

| Path | Role |
|------|------|
| `C/base_game.c` | Regenerated each export: frozen tables, init/tick/vblank, calls into `custom_logic` |
| `C/r01_runtime.c` | Regenerated host stubs (pad helpers, warp, button events) |
| `C/custom_logic.c` | **User file**. Template on first export. Hooks for game-specific logic (camera dead zone, player anim, events) |
| `C/include/*.h` | `R01GameCtx`, engine API (`r01_input.h`, `r01_player.h`,...) |
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
./studio path/to/project.r01proj
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
| Add / edit entity | Entities accordion -> **Add**, or right-click -> Edit |
| Mark / unmark player | Right-click entity row -> **Mark as player** / **Unmark as player**. Mark copies that entity's SPR patterns into the global **Player bank** and retargets parts; unmark restores them to world SPR banks |
| Place catalog on screen | Drag Sprites / Entities row onto screen preview |
| Play / pause | **Space** / **PLAY** (export cart, then open emu render) |
| Move player | **WASD** / arrows |
| Warp test | **X** -> (0,0), **Y** -> (1,0) |
| Save / load | **Ctrl+S** / **Ctrl+O** current project path |
| Undo / redo | **Ctrl+Z** / **Ctrl+Y** (or **Ctrl+Shift+Z**). Central stack in `ui/undo/` (`undo.c` + `undo_cmds.c`). Map paint / flood / tile paste, **Edit tile** Save (`ui_undo_push_bg_chr_edit` restores bank CHR then refreshes all screen previews), entity compose SPR paint, catalog add/remove, screen create/remove/paste |
| Export cart | **Ctrl+E** beside project (or `output/<name>`) (+ `C/`, `ASM/`, `data/`) |
| Toggle canvas | **Ctrl+Shift+R** -> **640x360** / **1280x720** |
| Fullscreen | **Ctrl+F** desktop fullscreen toggle |
| Present scale | **Ctrl+1** / **Ctrl+2** -> 1x / 2x window |

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`general_docs/video-graphics.md`](../../general_docs/video-graphics.md) | Video, entities, palettes |
| [`general_docs/software-api.md`](../../general_docs/software-api.md) | Flashing, PRG vs HW, author checklist |
| [`general_docs/memory.md`](../../general_docs/memory.md) | Cart image layout |
| [`app/sim/README.md`](../sim/README.md) | Board sim + cart triage |
| [`app/emu/README.md`](../emu/README.md) | Cart runtime emulator |
