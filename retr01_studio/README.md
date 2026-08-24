# Retr01 Studio

Visual authoring tool for Retr01 worlds, screens, and cartridge images. This document is the **product spec** for Studio phases. Hardware and memory layout: [`docs/02_graphics_worlds_memory.md`](../docs/02_graphics_worlds_memory.md). Long-form roadmap history: [`docs/04_retr01_studio.md`](../docs/04_retr01_studio.md) (superseded by this file for phase scope).

**Stack:** C11 + SDL2, shared `libretr01_studio_core` + thin shell.

---

## Supported game profiles (overview)

Studio targets discrete **game profiles** — bundles of scroll behavior, transitions, sprite rules, and event wiring that define what a generated cart can do. Only one profile is fully supported in Phase 1; others are documented here for later phases (no implementation yet).

| Profile | Scroll | Transitions | Sprites | Events |
|---------|--------|-------------|---------|--------|
| **Smooth + Eagle View** | Smooth pixel scroll; camera follows player; **no dead zone** | — (continuous scroll only) | One hardcoded **8×8** player (solid black tile) | **Warp** on pad **X** / **Y** (Phase 1 test hooks; see [Events](#events-strategy)) |
| *Instant cut* | Screen snap (non-smooth) | Instant switch on trigger | TBD | Triggers only (no smooth pan between cells) |
| *Fade black* | Screen snap (non-smooth) | Fade to black → instant swap → fade in | TBD | Same trigger model as instant cut |
| *Multi-tile entities* | (any of above) | (any of above) | Player + enemies as **fixed tile arrangements** per entity; enemies static | Expanded event set |

Phase 1 implements **Smooth + Eagle View** only. Later profiles appear in [Future phases](#future-phases-docs-only).

---

## Phase 0 — plumbing

**Goal:** Shared library and I/O with **no author-facing Studio features**.

**In:**

- Project / world / screen data structures aligned with [`docs/02`](../docs/02_graphics_worlds_memory.md)
- JSON load/save skeleton
- CHR pack helpers (dedupe 8×8 tiles)
- Cart image pack (`.retr01` header, pointers, CHR/MAP/palette blobs, stub PRG slot)
- Unit tests (pack, round-trip, cart layout)

**Out:** SDL UI, PNG import, Play, palette editors, pixel paint, Generate.

Phase 0 is infrastructure only; nothing in the shell beyond what is needed to boot and run tests.

---

## Phase 1 — Smooth + Eagle View

**Goal:** Author a **single world** on a **3×3 default** grid (PNG import can resize to **N×M**, max **8×8**) via **PNG import**, preview with **Play**, and export a **`.retr01`** cart whose runtime behavior matches Play.

### Game behavior (Smooth + Eagle View)

- **Scroll:** Smooth **pixel** scroll across present screens. When the player moves, the camera moves — **no dead zone**, no free-box inset.
- **Player:** Exactly **one** sprite entity — an **8×8** tile, **solid black**, hardcoded (not placed or edited in the UI). Movement: **WASD** / d-pad arrows in Play.
- **Start:** Player begins centered on screen **(1, 1)** — grid column 1, row 1 (0-based), the center cell of the default **3×3** grid (still valid after PNG resize if that cell exists).
- **Collision:** Player AABB must stay on **present** screens only (same rule as today’s Play preview).
- **Warp events (Phase 1 hardcoded):** Press **X** → instant warp to screen **(0, 0)**. Press **Y** → instant warp to screen **(1, 0)**. These are **event warp** tests for instant screen swap; see [Events](#events-strategy).
- **Not in Phase 1:** Parallax planes, attr editing, sprite banks UI, palette UI, pixel paint, Generate, dead-zone / hybrid scroll, fade transitions, multi-tile entities, multi-world tabs.

### World and grid

| Rule | Phase 1 value |
|------|----------------|
| Worlds | **One** world only (**world 0**). No add/switch world UI. |
| Grid size | **3×3** default; PNG import resizes to **N×M** (1–8 per axis, max **8×8**) |
| Screen placement | **Fixed** full grid — all cells always present; click to select only. |
| Initial content | Empty screens until PNG import; player starts on **(1, 1)** in Play. |
| Hardware cap (internal) | Data model still allows up to 8 worlds and 8×8 grids later; Phase 1 UI uses **world 0** only — grid starts **3×3**, PNG sets **N×M**. |

### Authoring UI

Fixed **640×360** logical canvas, integer scale (default **2×**).

```text
+------------------------------------------------------------------+
|  [ Worlds — N×M grid ]   |                                       |
|  (single world,          |                                       |
|   default 3×3;           |         [ Screen ]                    |
|   click to select)       |   (read-only view of active screen)   |
|                          |                                       |
+------------------------------------------------------------------+
```

| Region | Phase 1 |
|--------|---------|
| **Left — Worlds** | Only panel in the left column. Shows the world grid (default **3×3**; resizes to match PNG atlas); click selects active screen. |
| **Right — Screen** | Shows the active grid screen (128×120). **No pixel or attr editing** — display only, fed from PNG import data. |
| **Hidden** | Planes, BG bank viewer, Sprite banks, Palettes, Constraints, Generate, VRAM 1× camera preview. |

**Play:** Available (e.g. **Space** / **PLAY**). Behavior must match exported cart logic for scroll, player, and X/Y warps.

**Save / load:** **Ctrl+S** / **Ctrl+O** → project JSON.

**Export cart:** **Ctrl+E** → `project.retr01` (+ companion files as today: `*_prom.bin`, `*_boot.s`, `*_flash.bin` where applicable). Cart **Play-equivalent** behavior: smooth scroll, hardcoded black player, X/Y warps.

### PNG import

PNG drop is the **only** way to author screen graphics in Phase 1.

| Rule | Value |
|------|--------|
| Drop target | **Anywhere** on the app window |
| Destination | **World 0**, **BG bank 0** always |
| Unique patterns | ≤ **256** unique 8×8 tiles from the import (Studio assumes the PNG does not exceed this) |
| Colors | ≤ **4** colors per PNG (same as today) |
| Cell size | **128×120** px per screen cell |
| Grid fit | PNG dimensions must be a multiple of 128×120; sets world grid to **N×M** from atlas size (max **8×8**) |
| Transparent pixels | Skipped (cell not forced present) |
| Packing | All patterns deduped into **BG bank 0**; tile/attr planes for touched screens updated from the import |
| Generate | **No** separate Generate action (**Ctrl+G** disabled / removed in Phase 1) |

### Palettes (internal only — no UI)

Phase 1 does **not** expose a Palettes cell. Global palette data is **written automatically** into the project and cart:

- **Color 0 (shared backdrop):** Master index **0** (`#000000`) in **every** BG and sprite palette row — same **shared BG color** convention as NES (index 0 is universal backdrop / transparent for sprites).
- **Colors 1–3:** For each of the **8** palette rows (4 BG + 4 sprite), indices 1–3 are taken from **vertical strips** of the kit **16×4** master grid ([`docs/02`](../docs/02_graphics_worlds_memory.md)), using **rows 1–3 only** (no dark row-0 swatches):
  - BG palette row *i* (0…3): master indices **(16+*i*)**, **(32+*i*)**, **(48+*i*)** — column *i*, rows 1–3.
  - Sprite palette row *i* (0…3): master indices **(20+*i*)**, **(36+*i*)**, **(52+*i*)** — column *4+i*, rows 1–3.

Preview and cart burn quantize through the kit Color PROM (**R3G3B2**).

### CHR / banks (internal only — no UI)

| Asset | Phase 1 |
|-------|---------|
| BG banks | **Bank 0** filled by PNG import; banks 1–3 empty unless pre-seeded in file |
| Sprite banks | Empty in UI; player tile is **runtime hardcoded**, not from authored CHR |
| Attr plane | Default attrs (bank **0**, palette row **0**, no flips/ANIM) stamped on import |

### Data flow

```text
Drop PNG → pack tiles → BG bank 0 + screen tile/attr payloads
       → apply fixed global palettes
       → save project JSON
Play  → smooth scroll + black 8×8 player + X/Y warp events
Export → .retr01 (PRG stub implements same behavior as Play for Phase 1)
```

### Phase 1 — out of scope

- Pixel / attr paint, sprite placement, meta-sprites
- Planes / parallax
- Palette editor, bank viewers, VRAM 1× preview
- Generate (**Ctrl+G**)
- Second world, manual grid resize UI, grids larger than **8×8**
- Dead-zone, instant-scroll, and fade **profiles** (except X/Y **warp** test events)
- Enemy or multi-tile entity authoring

---

## Events strategy

Events are **triggers → actions** attached to inputs or (later) game conditions. Carts and Play share the same high-level event list; Phase 1 only implements a minimal subset.

### Concepts (all phases)

| Piece | Meaning |
|-------|---------|
| **Trigger** | What fires the event (pad button, collision, timer, …) |
| **Action** | What happens (warp to screen, fade transition, spawn, …) |
| **Warp** | Jump player (and camera) to a grid **(col, row)**; may be instant or preceded by fade (later) |

Future phases add an **Events** authoring surface (lists per world / per screen). Phase 1 does **not** show Events in the UI.

### Phase 1 hardcoded events

| Trigger | Action |
|---------|--------|
| Pad **X** pressed | **Warp** to screen **(0, 0)** — instant |
| Pad **Y** pressed | **Warp** to screen **(1, 0)** — instant |

These exist to validate **instant screen swap** and the event pipeline before smooth scroll is mixed with authored triggers. They are **not** the long-term design for X/Y (later: configurable event list, fade variant, game-specific bindings).

### Later (documentation only)

- **Instant cut** profile: screen changes only via events (no smooth pan between cells).
- **Fade black** profile: warp/screen-change events run **fade out → swap → fade in**.
- Non-warp events: dialogue hooks, room flags, enemy enable — TBD.

---

## Future phases (docs only)

Summaries for planning; **not implemented** until their phase.

### Phase 2 — Instant cut (+ event authoring)

- Game profile: **non-smooth** screen switching; camera snaps with the player.
- Transitions: **instant cut** only.
- Screen changes **only** via **events** (authored or templated), not by walking off-screen.
- UI: basic **Events** list; warp and cut actions.
- X/Y warp behavior becomes **data-driven** instead of hardcoded (Phase 1 hooks remain the default test pair until edited).

### Phase 3 — Fade black

- Adds **fade to black → instant swap → fade from black** transition type on event-driven screen changes.
- Still non-smooth pixel scroll between screens (no continuous pan across seams for this profile).

### Phase 4 — Multi-tile entities

- **Sprite entities** with **multiple 8×8 tiles** in a fixed arrangement (player layout, static enemies).
- Enemies: **no AI** in this phase — placement and draw only.
- Sprite bank UI and entity composer; still no full animation editor.

### Phase 5+ — Eagle View expansion (TBD)

- Additional game profiles, parallax planes UI, attr/palette editors, multi-world tabs, larger grids, full PRG/codegen from events, toolchain polish — scope to be split when Phase 4 lands.

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

- **core** — pack, JSON, play, cart (Phase 0 plumbing)

---

## Phase 1 controls (target)

| Action | Input |
|--------|--------|
| Select screen | Click cell in Worlds |
| Import atlas | **Drop PNG** anywhere on window |
| Play / pause preview | **Space** / **PLAY** |
| Move player | **WASD** / arrows |
| Warp test | **X** → screen (0,0), **Y** → screen (1,0) |
| Save / load project | **Ctrl+S** / **Ctrl+O** |
| Export cart | **Ctrl+E** |

---

## Export artifacts

**Ctrl+E** writes next to the project stem:

| File | Contents |
|------|----------|
| `*.retr01` | Packed cart (`RETR01` magic, globals, world 0 CHR/MAP, palettes, PRG) |
| `*_prom.bin` | 64-byte Color PROM image (**motherboard**, not in cart) |
| `*_boot.s` | Human-readable ca65 stub / equates |
| `*_flash.bin` | Cart image padded to **512 KB** |

**ROM vs Studio chrome:** Editor layout, hidden panels, and PNG import UI are not burned into the cart. Phase 1 PRG implements **Smooth + Eagle View** behavior (scroll, player, X/Y warps) to match **Play**.

See [`docs/08_simulator.md` — Cart ROM vs runners](../docs/08_simulator.md#cart-rom-vs-runners-triage) for sim/emu vs cart boundaries.

---

## Related docs

| Doc | Topic |
|-----|--------|
| [`docs/02`](../docs/02_graphics_worlds_memory.md) | Screens, VRAM, palettes, cart layout |
| [`docs/04`](../docs/04_retr01_studio.md) | Legacy phase checklist (pre-redesign) |
| [`docs/08`](../docs/08_simulator.md) | Board sim + cart loading |
