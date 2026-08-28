# retr01 Studio

Visual authoring tool for retr01 worlds, screens, and cartridge images. **Phase 1** (PNG import + Play + export) remains; **Phase 2** adds multi-world chrome, screen create/delete, tile edit/paint, **solid** collision attrs, global palette editing, and **default spawn screen**. Hardware layout: [`docs/02_graphics_worlds_memory.md`](../docs/02_graphics_worlds_memory.md). Short docs mirror: [`docs/04_retr01_studio.md`](../docs/04_retr01_studio.md).

**Stack:** C11 + SDL2 + FreeType (Proggy Tiny), shared `libretr01_studio_core` + thin shell.

---

## Phase 2 -- Worlds + BG tile paint

**Goal:** Author worlds/screens without PNG-only workflows: create/remove screens, edit 8x8 tiles, paint them onto maps.

### UI chrome

Fixed **640x360** logical canvas. Dark gray / darker gray only. All chrome snaps to an **8px** grid. Buttons and labels are **16px** tall; width = content rounded up to a multiple of 8. Font: **Proggy Tiny** (`assets/proggy-tiny.ttf`).

```text
+-------------------------------------------------------------------+
| SIDEBAR (128)          | MAIN                                     |
| [>] Worlds             |              [ Play ]                    |
|  [0][1][2][3]          |         +------------------+  ( ) Sel    |
|  [4][5][6][7]          |         | Screen 256x240   |  ( ) Paint  |
|  +--------------+      |         | 128x120 @2x edit |             |
|  | 128x128 map  |      |         +------------------+             |
|  | 8x8 x 16px   |      |                                          |
|  +--------------+      |                                          |
| [>] Palettes           |                                          |
|  BG  [32px strip]      |                                          |
|  SPR [32px strip]      |                                          |
|  [0][1]...[7] rows     |                                          |
+-------------------------------------------------------------------+
```

| Control | Behavior |
|---------|----------|
| **Worlds** accordion | Collapsible sidebar section: world buttons + map |
| **8 world buttons** | 16x16 each. World 0 starts present with a **3x3** blank screen region on an **8x8** slot map. Other worlds empty until clicked (creates empty world). |
| **World map 128x128** | Cell pitch **16** (15px fill + 1px gap). Present screens **blue**; **white fill** = default spawn screen; **white outline** = selected screen |
| **Double-click** empty slot | Create screen |
| **Ctrl+click** present slot | Remove screen |
| **Click** present slot | Select active screen |
| **Play** | Centered above the screen view (Space / button) |
| **Tile selection / Tile paint** | Radio rows beside the screen (edit mode only). **Paint** uses the armed stamp; **Selection** for marquee + context menu |
| **Palette strip** | Sidebar **Palettes** accordion: **8x8** BG then SPR swatches + row **0–7** buttons. **Click strip** opens **Global palettes** modal |

### Tile edit

| Step | Action |
|------|--------|
| Right-click tile (selection mode) | Context menu: **Move to tile bank**, **Add new tile here**, **Edit tile**, **Set tile palette**, **Set Anim mode**, **Set Solid** |
| **Edit tile** modal | **288x160**. Title, **Palette/color** 4x4 (4 pals x 4 colors, 8px cells), **128x128** pixel canvas (16px per tile pixel), Save / Cancel |
| After Save | Brush armed; left-click in **Tile paint** mode stamps that tile+palette onto the map |
| **Set Solid** | Toggles `R01_ATTR_SOLID` on every tile in the **active world** whose hardware attrs match the clicked cell (**bank + pal + H/V flips**, not tile ID). Re-export cart for emu/sim MAP attrs |
| **Set Anim mode** | Toggles `R01_ATTR_ANIM` on the current tile selection |
| Marquee | Drag on screen in **Tile selection** mode; bank/pal/anim/solid apply to the selection |

Right-click a **world map** cell: **Set default screen** (Play + export spawn), **Make default world**.

PNG drop still imports into the **active** world. Export / cart pack remains **world 0** (Phase 1 cart path).

### Global palettes

Click the **BG/SPR strip** (not Play) to open the **Global palettes** modal: pick BG/SPR plane cells, click kit masters, Save/Cancel. Row **0–7** buttons set `default_pal_row` when the modal is closed.

---

## Phase 1 game profile -- Smooth + Eagle View

| | |
|--|--|
| **Scroll** | Smooth pixel scroll; camera follows player; **no dead zone** |
| **Sprites** | One hardcoded **8x8** player. Fill color comes from **sprite palette row 0, index 1** (phase 1 default: kit bright red) |
| **Collision** | Player **8x8** AABB blocked by tiles with **`R01_ATTR_SOLID`** (`0x40`) on present screens |
| **Start** | Center of **`default_screen`** (world map context menu). New worlds default to **(2, 0)** if present, else first present screen |
| **Events** | **Warp** on pad **X** / **Y** (Phase 1 test hooks; see [Events](#events-strategy)) |

---

## Phase 0 -- plumbing

**Goal:** Shared library and I/O with **no author-facing Studio features**.

**In:**

- Project / world / screen data structures aligned with [`docs/02`](../docs/02_graphics_worlds_memory.md)
- JSON load/save skeleton
- CHR pack helpers (dedupe 8x8 tiles)
- Cart image pack (`.retr01` header, pointers, CHR/MAP/palette blobs, stub PRG slot)
- Unit tests (pack, round-trip, cart layout)

**Out:** SDL UI, PNG import, Play, palette editors, pixel paint, Generate.

Phase 0 is infrastructure only; nothing in the shell beyond what is needed to boot and run tests.

---

## Phase 1 -- Smooth + Eagle View

**Goal:** Author a **single world** on a **3x3 default** grid (PNG import can resize to **NxM**, max **8x8**) via **PNG import**, preview with **Play**, and export a **`.retr01`** cart whose runtime behavior matches Play.

### Game behavior (Smooth + Eagle View)

- **Scroll:** Smooth **pixel** scroll across present screens. When the player moves, the camera moves -- **no dead zone**, no free-box inset.
- **Player:** Exactly **one** sprite entity, an **8x8** tile, hardcoded (not placed or edited in the UI). Fill color is read from **sprite palette row 0, color index 1** (phase 1 init sets that slot to kit bright red). Movement: **WASD** / d-pad arrows in Play.
- **Start:** Player spawns at the **center** of the world's **`default_screen`** (`default_screen` in JSON; set via world map context menu). If unset, falls back to **(2, 0)** when present, else the first present screen. **`begin_play`** switches the active screen to the default before Play starts.
- **Collision:** Movement uses corner **AABB** vs **present** screens and **solid** BG attrs (`R01_ATTR_SOLID`). Same rules in Play, emu, and sim host Play (`core/src/collision.c`).
- **Warp events (Phase 1 hardcoded):** Press **X** -> instant warp to screen **(0, 0)**. Press **Y** -> instant warp to screen **(1, 0)**. These are **event warp** tests for instant screen swap; see [Events](#events-strategy).
- **Not in Phase 1:** Parallax planes, full attr inspector UI, sprite banks UI, Generate, dead-zone / hybrid scroll, fade transitions, multi-tile entities, multi-world cart export.

### World and grid

| Rule | Phase 1 value |
|------|----------------|
| Worlds | **One** world in Phase 1 UI; **Phase 2** adds 8-world sidebar (cart export still **world 0**) |
| Grid size | **3x3** default; PNG import resizes to **NxM** (1-8 per axis, max **8x8**) |
| Screen placement | Grid slots exist; **present** only where PNG has opaque content (transparent cells stay absent). Click present cells to select. |
| Initial content | Empty / absent screens until PNG import. |
| Hardware cap (internal) | Data model still allows up to 8 worlds and 8x8 grids later; Phase 1 UI uses **world 0** only. |

### Authoring UI

Fixed **640x360** logical canvas, integer scale (default **2x**). Current chrome is **Phase 2** (Phase 1 was a read-only screen + world grid only):

```text
+------------------------------------------------------------------+
| SIDEBAR (128)          | MAIN                                     |
| [>] Worlds             |              [ Play ]                    |
|  [0][1][2][3]          |         +------------------+  ( ) Sel    |
|  [4][5][6][7]          |         | Screen 256x240   |  ( ) Paint  |
|  +--------------+      |         | 128x120 @2x edit |             |
|  | 128x128 map  |      |         +------------------+             |
|  | 8x8 x 16px   |      |                                          |
|  +--------------+      |                                          |
| [>] Palettes           |                                          |
|  BG  [32px strip]      |                                          |
|  SPR [32px strip]      |                                          |
|  [0][1]...[7] rows     |                                          |
+------------------------------------------------------------------+
```

| Region | Phase 1 (original) | Current (Phase 2) |
|--------|--------------------|-------------------|
| **Left -- Worlds** | World 0 grid only; click to select | 8 world buttons + **8×8** map accordion; create/delete screens |
| **Left -- Palettes** | — | Accordion: BG/SPR strips + row buttons; click opens global editor |
| **Right -- Screen** | Read-only PNG view | **Tile selection** / **Tile paint** + tile modal |
| **Hidden** | Planes, banks, Generate, VRAM preview | Same |

**Play:** Available (e.g. **Space** / **PLAY**). Behavior must match exported cart logic for scroll, player, and X/Y warps.

**Save / load:** **Ctrl+S** / **Ctrl+O** -> `test_game/test.r01proj` (created on save). JSON **version 4** (`default_screen`, `default_pal_row`, tile/attr planes).

**Export cart:** **Ctrl+E** -> `test_game/test.retr01` (+ `test_prom.bin`, `test_boot.s`, `test_flash.bin` in the same folder).

### PNG import

Bulk atlas import; Phase 2 **tile edit/paint** can also author tiles cell-by-cell.

| Rule | Value |
|------|--------|
| Drop target | **Anywhere** on the app window |
| Destination | **Active** world, **BG bank 0** |
| Unique patterns | <= **256** unique 8x8 tiles from the import |
| Colors | <= **4** colors per PNG |
| Cell size | **128x120** px per screen cell |
| Grid fit | PNG dimensions must be a multiple of 128x120; sets world grid to **NxM** from atlas size (max **8x8**) |
| Transparent pixels | Skipped (cell not forced present) |
| Packing | All patterns deduped into **BG bank 0**; tile/attr planes for touched screens updated from the import |
| Generate | **No** separate Generate action (**Ctrl+G** disabled / removed in Phase 1) |

### Palettes (strip + global editor)

Phase 1 has **no per-tile RGB editor** — colors are **kit master indices** ([`docs/02`](../docs/02_graphics_worlds_memory.md)).

- **Strip:** Active **BG/SPR** row (`default_pal_row`). Row buttons switch the default row.
- **Modal:** Click the strip to edit all **8 BG + 8 SPR** rows (4 pals × 4 colors). Save writes project JSON; Cancel restores snapshot.
- **Before PNG import:** Phase 1 init fills BG colors 1-3 from kit columns `(16+p)` / `(32+p)` / `(48+p)`, and sprite colors 1-3 with kit bright red (player uses row **0**, pal **0**, index **1**).
- **After PNG import:** BG rows (and sprite backdrop) are remapped to **nearest kit masters** from the atlas colors (`r01_project_set_bg_pals_from_png`).

Preview and cart burn quantize through the kit Color PROM (**R3G3B2**).

### CHR / banks (internal only -- no UI)

| Asset | Phase 1 |
|-------|---------|
| BG banks | **Bank 0** filled by PNG import. Banks 1-3 empty unless pre-seeded in file |
| Sprite banks | Empty in UI. Export plants solid color-1 tile **1** in SPR bank 0 for the player |
| Attr plane | Import stamps bank/pal. **Flip-aware CHR dedupe** may set `FLIP_H` / `FLIP_V`. **Solid** / **Anim** via tile context menu (`0x40` / `0x80`) |

### Data flow

```text
Drop PNG -> pack tiles (flip-aware) -> BG bank 0 + screen tile/attr payloads
       -> remap BG pals from PNG masters
       -> save project JSON
Play  -> smooth scroll + player sprite + solid collision + X/Y warp (SoT: core/src/play.c)
Export -> `.retr01` (present screens, play table `$8100`, marker `R01P` v2,
          PRG streams pal + start MAP, `play_pos_ok` @ $8500 + solid tables @ $8700)
Emu/Sim -> host Play uses cart MAP attrs for solid; re-export after solid edits
```

### Phase 1 -- out of scope

- Sprite placement, meta-sprites, multi-tile entity authoring
- Planes / parallax
- Generate (**Ctrl+G**)
- Multi-world **cart** export (UI can author other worlds; pack still world 0)
- Dead-zone, instant-scroll, and fade **profiles** (except X/Y **warp** test events)
- Full 6502 gameplay loop (PRG boot + collision stub; movement still host Play in emu/sim)

---

## Events strategy

Events are **triggers -> actions** attached to inputs or (later) game conditions. Carts and Play share the same high-level event list; Phase 1 only implements a minimal subset.

### Concepts

| Piece | Meaning |
|-------|---------|
| **Trigger** | What fires the event (pad button, collision, timer, ...) |
| **Action** | What happens (warp to screen, fade transition, spawn, ...) |
| **Warp** | Jump player (and camera) to a grid **(col, row)**; may be instant or preceded by fade (later) |

Phase 1 does **not** show an Events authoring UI.

### Phase 1 hardcoded events

| Trigger | Action |
|---------|--------|
| Pad **X** pressed | **Warp** to screen **(0, 0)** -- instant |
| Pad **Y** pressed | **Warp** to screen **(1, 0)** -- instant |

These exist to validate **instant screen swap** and the event pipeline. They are **not** the long-term design for X/Y.

---

## Build and run

```bash
cd retr01_studio   # or ./studio from repo root
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_studio
```

Or from repo root:

```bash
./studio/scripts/build-run.sh
./studio/scripts/test-unit.sh
```

**Needs:** CMake, C compiler, SDL2, libpng.

### Tests

```bash
ctest --test-dir build --output-on-failure
```

- **core** — pack, JSON, play + collision, cart + PRG phase 1 (six suites: project, play, chr, json, cart, palette)

---

## Controls

| Action | Input |
|--------|--------|
| Select screen | Click present cell in Worlds |
| Set default screen / world | Right-click world map cell |
| Import atlas | **Drop PNG** anywhere on window |
| Tile selection / paint | Radio rows beside screen view |
| Tile context menu | Right-click tile (selection mode) |
| Edit global palettes | Click BG/SPR strip |
| Play / pause preview | **Space** / **PLAY** |
| Move player | **WASD** / arrows |
| Warp test | **X** -> screen (0,0), **Y** -> screen (1,0) |
| Save / load project | **Ctrl+S** / **Ctrl+O** -> `test_game/test.r01proj` |
| Export cart | **Ctrl+E** -> `test_game/test.retr01` |

---

## Export artifacts

**Ctrl+E** writes under `test_game/` (repo root relative to launch cwd):

| File | Contents |
|------|----------|
| `test_game/test.retr01` | Packed cart (`retr01` magic, globals, world 0 CHR/MAP, palettes, PRG + collision stub) |
| `test_game/test_prom.bin` | 64-byte Color PROM image (**motherboard**, not in cart) |
| `test_game/test_boot.s` | Human-readable ca65 stub / equates (play table, `play_pos_ok`, solid shadow layout) |
| `test_game/test_flash.bin` | Cart image padded to **512 KB** |

**ROM vs Studio chrome:** Editor layout, tile paint, and palette modal are not burned into the cart.
Phase 1 **Play SoT** is `play.c` + `collision.c`. Export packs MAP attrs (including **solid**), play
table + spawn from **`default_screen`**, and optional 6502 **`play_pos_ok`** for future PRG-owned movement.
Emulator and sim apply the same collision rules against cart MAP; **re-export after solid edits**.

See [`docs/08_simulator.md` -- Cart ROM vs runners](../docs/08_simulator.md#cart-rom-vs-runners-triage) for sim/emu vs cart boundaries.

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`docs/02`](../docs/02_graphics_worlds_memory.md) | Screens, VRAM, palettes, cart layout |
| [`docs/04`](../docs/04_retr01_studio.md) | Short mirror in docs/ (may lag this README) |
| [`docs/08`](../docs/08_simulator.md) | Board sim + cart loading |
| [`retr01_emu/README.md`](../retr01_emu/README.md) | Emulator Phase 1 |
