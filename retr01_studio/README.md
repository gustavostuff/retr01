# retr01 Studio

Visual authoring tool for retr01 worlds, screens, and cartridge images. This document is the **product spec** for **Studio Phase 1**. Hardware and memory layout: [`docs/02_graphics_worlds_memory.md`](../docs/02_graphics_worlds_memory.md). Short docs mirror: [`docs/04_retr01_studio.md`](../docs/04_retr01_studio.md).

**Stack:** C11 + SDL2, shared `libretr01_studio_core` + thin shell.

Later Studio phases are **not** specified here; they will be defined when work starts.

---

## Phase 1 game profile -- Smooth + Eagle View

| | |
|--|--|
| **Scroll** | Smooth pixel scroll; camera follows player; **no dead zone** |
| **Sprites** | One hardcoded **8x8** player. Fill color comes from **sprite palette row 0, index 1** (phase 1 default: kit bright red) |
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
- **Start:** Prefer screen **(2, 0)** if present, else first present screen.
- **Collision:** Player AABB must stay on **present** screens only.
- **Warp events (Phase 1 hardcoded):** Press **X** -> instant warp to screen **(0, 0)**. Press **Y** -> instant warp to screen **(1, 0)**. These are **event warp** tests for instant screen swap; see [Events](#events-strategy).
- **Not in Phase 1:** Parallax planes, attr editing UI, sprite banks UI, palette **editor**, pixel paint, Generate, dead-zone / hybrid scroll, fade transitions, multi-tile entities, multi-world tabs.

### World and grid

| Rule | Phase 1 value |
|------|----------------|
| Worlds | **One** world only (**world 0**). No add/switch world UI. |
| Grid size | **3x3** default; PNG import resizes to **NxM** (1-8 per axis, max **8x8**) |
| Screen placement | Grid slots exist; **present** only where PNG has opaque content (transparent cells stay absent). Click present cells to select. |
| Initial content | Empty / absent screens until PNG import. |
| Hardware cap (internal) | Data model still allows up to 8 worlds and 8x8 grids later; Phase 1 UI uses **world 0** only. |

### Authoring UI

Fixed **640x360** logical canvas, integer scale (default **2x**).

```text
+------------------------------------------------------------------+
|  [ Worlds -- NxM grid ]  |                                       |
|  (single world,          |                                       |
|   default 3x3;           |         [ Screen ]                    |
|   click to select)       |   (read-only view of active screen)   |
|                          |                                       |
+------------------------------------------------------------------+
```

| Region | Phase 1 |
|--------|---------|
| **Left -- Worlds** | Only panel in the left column. Shows the world grid (default **3x3**; resizes to match PNG atlas); click selects active **present** screen. |
| **Right -- Screen** | Shows the active grid screen (128x120). **No pixel or attr editing** -- display only, fed from PNG import data. |
| **Hidden** | Planes, BG bank viewer, Sprite banks, palette **editor**, Constraints, Generate, VRAM 1x camera preview. |
| **Read-only chrome** | Active **BG/SPR** palette strip for `default_pal_row` (not an editor) |

**Play:** Available (e.g. **Space** / **PLAY**). Behavior must match exported cart logic for scroll, player, and X/Y warps.

**Save / load:** **Ctrl+S** / **Ctrl+O** -> `test_game/test.r01proj` (created on save). JSON **version 4**.

**Export cart:** **Ctrl+E** -> `test_game/test.retr01` (+ `test_prom.bin`, `test_boot.s`, `test_flash.bin` in the same folder).

### PNG import

PNG drop is the **only** way to author screen graphics in Phase 1.

| Rule | Value |
|------|--------|
| Drop target | **Anywhere** on the app window |
| Destination | **World 0**, **BG bank 0** always |
| Unique patterns | <= **256** unique 8x8 tiles from the import |
| Colors | <= **4** colors per PNG |
| Cell size | **128x120** px per screen cell |
| Grid fit | PNG dimensions must be a multiple of 128x120; sets world grid to **NxM** from atlas size (max **8x8**) |
| Transparent pixels | Skipped (cell not forced present) |
| Packing | All patterns deduped into **BG bank 0**; tile/attr planes for touched screens updated from the import |
| Generate | **No** separate Generate action (**Ctrl+G** disabled / removed in Phase 1) |

### Palettes (auto + read-only strip)

Phase 1 has **no palette editor**. A read-only **BG/SPR** swatch strip shows the active row. Global palette data is written automatically into the project and cart per [`docs/02`](../docs/02_graphics_worlds_memory.md):

- **Cart layout:** **8 global BG palette rows** + **8 global sprite palette rows** (**256 B** total).
- **Color 0 (shared backdrop):** Master index **0** (`#000000`) in every palette. Same shared backdrop convention as NES (index 0 is universal backdrop / transparent for sprites).
- **Before PNG import:** Phase 1 init fills BG colors 1-3 from kit columns `(16+p)` / `(32+p)` / `(48+p)`, and sprite colors 1-3 with kit bright red (player uses row **0**, pal **0**, index **1**).
- **After PNG import:** BG rows (and sprite backdrop) are remapped to **nearest kit masters** from the atlas colors (`r01_project_set_bg_pals_from_png`).
- **Active row:** Play / export use the world's `default_pal_row` (Phase 1: **0**).

Preview and cart burn quantize through the kit Color PROM (**R3G3B2**).

### CHR / banks (internal only -- no UI)

| Asset | Phase 1 |
|-------|---------|
| BG banks | **Bank 0** filled by PNG import. Banks 1-3 empty unless pre-seeded in file |
| Sprite banks | Empty in UI. Export plants solid color-1 tile **1** in SPR bank 0 for the player |
| Attr plane | Import stamps bank/pal. **Flip-aware CHR dedupe** may set `FLIP_H` / `FLIP_V` when matching oriented tiles |

### Data flow

```text
Drop PNG -> pack tiles (flip-aware) -> BG bank 0 + screen tile/attr payloads
       -> remap BG pals from PNG masters
       -> save project JSON
Play  -> smooth scroll + player sprite + X/Y warp (SoT: `core/src/play.c`)
Export -> `.retr01` (present screens only, play table `$8100`, marker `R01P`,
          PRG streams pal + start MAP)
Emu   -> same Play rules applied to exported cart MAP (re-export after edits)
```

### Phase 1 -- out of scope

- Pixel / attr paint, sprite placement, meta-sprites
- Planes / parallax
- Palette **editor**, bank viewers, VRAM 1x preview
- Generate (**Ctrl+G**)
- Second world, manual grid resize UI, grids larger than **8x8**
- Dead-zone, instant-scroll, and fade **profiles** (except X/Y **warp** test events)
- Enemy or multi-tile entity authoring

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

- **core** -- pack, JSON, play, cart (Phase 0 plumbing)

---

## Phase 1 controls

| Action | Input |
|--------|--------|
| Select screen | Click present cell in Worlds |
| Import atlas | **Drop PNG** anywhere on window |
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
| `test_game/test.retr01` | Packed cart (`retr01` magic, globals, world 0 CHR/MAP, palettes, PRG) |
| `test_game/test_prom.bin` | 64-byte Color PROM image (**motherboard**, not in cart) |
| `test_game/test_boot.s` | Human-readable ca65 stub / equates |
| `test_game/test_flash.bin` | Cart image padded to **512 KB** |

**ROM vs Studio chrome:** Editor layout and PNG import UI are not burned into the cart.
Phase 1 **Play SoT** is `play.c`. Export packs matching MAP (present screens only) + play
table; the emulator runs that same Play behavior on the cart.

See [`docs/08_simulator.md` -- Cart ROM vs runners](../docs/08_simulator.md#cart-rom-vs-runners-triage) for sim/emu vs cart boundaries.

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`docs/02`](../docs/02_graphics_worlds_memory.md) | Screens, VRAM, palettes, cart layout |
| [`docs/04`](../docs/04_retr01_studio.md) | Short Phase 1 mirror in docs/ |
| [`docs/08`](../docs/08_simulator.md) | Board sim + cart loading |
| [`retr01_emu/README.md`](../retr01_emu/README.md) | Emulator Phase 1 |
