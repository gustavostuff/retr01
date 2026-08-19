# Retr01 Studio

Retr01 Studio is the **only software tool in active development** for now.

It is a **C/C++** desktop application for visually authoring Retr01 games: pixel patterns, CHR banks, nametables, screens, worlds, and (later) game behavior — not just graphics.

It produces cartridge-ready output aligned with the hardware docs in this folder: **PRG + CHR + MAP** in a `.retr01` cart image.

Studio is **not** the low-level hardware emulator. A separate emulator project is planned later for cycle-accurate validation.

---

## 1. Product scope

### What Studio does

| Area | Studio responsibility |
|------|------------------------|
| **Authoring** | Worlds, screens, CHR patterns, palettes, attrs, sprites, meta-sprites |
| **Generation** | Turn painted screens into CHR BG/sprite banks and MAP screen payloads |
| **Build** | Assemble project data into cart binaries / full ROM (later phases) |
| **Lightweight play** | Fast in-app preview of current project state (later; not low-level emu) |

### What Studio does not do (for now)

- Simulate CPU cycles, `$FExx` registers, or full hardware timing
- Replace the future low-level emulator
- Edit motherboard schematics

The **Play** button (future) runs a **high-level preview**: scroll rules, screen transitions, and attached constraints interpreted by Studio — enough to feel the game, not enough to validate silicon.

---

## 2. UI shell

### Canvas and scaling

The entire app UI is drawn to a fixed **640×360** logical canvas.

| Setting | Value |
|---------|-------|
| Default window scale | **2×** integer (1280×720) |
| Scaling rule | **Sharp integer scale only** (1×, 2×, 3×, … — no fractional blur) |
| Fullscreen | **Ctrl+F** toggles fullscreen; still integer-scaled to fit the display |

Implementation note: distinguish **UI canvas** (640×360 layout space) from **game screen resolution** (256×240). The Screen cell shows a 256×240 edit viewport inside the larger UI.

### Cell manager (not floating windows)

Studio uses a **window-manager-style cell layout**: fixed regions ("cells") arranged on the 640×360 canvas. Cells are **not** free-floating mini-windows and are **not user-resizable** in v1.

Content **inside** a cell may scroll, zoom, or pan within that cell's viewport. In Phase 1, only the **Screen** cell supports zoom and drag.

```text
+------------------------------------------------------------------+
|  [ Worlds map ]          |                                       |
|  (tabs, grid)            |                                       |
+--------------------------+         [ Screen ]                    |
|  [ BG banks ]            |      (BG / Sprite paint)              |
|  (4 tabs, 16×16 each)    |                                       |
+--------------------------+                                       |
|  [ Sprite banks ]        |                                       |
|  (4 tabs, 8×8/8×16 view) |                                       |
+--------------------------+                                       |
|  [ Palettes ]            |                                       |
|  (palette row tabs)      |                                       |
+------------------------------------------------------------------+
```

Exact pixel sizes for each cell are an implementation detail. The **right column** (Screen) gets most of the width; the **left column** stacks Worlds, BG banks, Sprite banks, and Palettes top to bottom.

---

## 3. Main cells

### 3.1 Worlds map (top-left)

**Purpose:** Define which screens exist in each world and where they sit on the virtual grid.

| Feature | Behavior |
|---------|----------|
| World tabs | Dynamic tabs to add/remove worlds. **Min 1**, **max 8** tabs |
| Grid | Interactive sparse grid per world |
| Resize grid | Add/remove **columns** and **rows** (hardware cap: **16×16** virtual grid) |
| Select screen | **Click** a cell → select that grid position |
| Create/remove screen | **Ctrl+click** toggles a stored screen at that grid position |
| Caps | At most **64 stored screens** per world (per hardware spec) |

Empty grid positions are holes (not stored in MAP). Selected position drives which screen the Screen cell edits.

### 3.2 BG banks (below Worlds)

**Purpose:** Show the four CHR **BG banks** (256 tiles each, **16×16** tile grid) for the current world.

| Feature | Phase 1 | Later |
|---------|---------|-------|
| Tabs | **4 tabs** (banks 0–3) | same |
| Grid | **256 cells** per tab (16×16), read-only display | optional direct tile edit |
| Source | Filled by **Generate bank** from Screen cell | import, duplicate, manual edit |

Each cell represents one **8×8** BG tile index (0–255) in that CHR bank.

### 3.3 Sprite banks (below BG banks)

**Purpose:** Same idea as BG banks, but for **sprite CHR banks**.

| Feature | Behavior |
|---------|----------|
| Tabs | **4 tabs** (sprite banks 0–3) |
| Grid | **256 cells** per tab |
| View toggle | **Right-click menu** on tab or tab content: **8×8** vs **8×16** layout |
| Toggle effect | **Visualization only** — reorders how tiles are drawn in the grid to preview 8×16 meta-tiles; does not change hardware tile size (always 8×8 in CHR) |

**Phase 1:** cell visible but **disabled / empty** (no sprite authoring yet).

### 3.4 Screen (right column, primary workspace)

**Purpose:** Paint one **32×30** nametable screen (256×240 px) at **8×8** tile resolution.

#### Layers

| Layer | Phase 1 | Later |
|-------|---------|-------|
| **BG** | **Yes** — primary paint target | attrs, parallax flags |
| **Sprite** | **No** | paint sprite layer, place meta-sprites |

#### Generate bank (BG)

Per BG layer toolbar:

- **Generate bank** button
- **Radio buttons** for target CHR BG bank **0–3**

**Generate bank** scans the painted BG layer, deduplicates unique 8×8 patterns, packs them into the selected CHR BG bank (up to 256 tiles), and rewrites the screen's **nametable tile indices** to point at the packed tiles. Updates the BG banks cell (read-only grids).

If unique tiles exceed 256, Studio reports an error and does not silently truncate without user confirmation.

#### Edit modes (later split; Phase 1 = pixel only)

| Mode | Input | Phase 1 |
|------|-------|---------|
| **Pixel edit** | 4 paint colors (grayscale preview of 2bpp indices 0–3) | **Yes** |
| **Attr / tile mode** | Assign palette 0–3 per tile via palette strip | **No** (Phase 2+) |

In pixel edit mode, the user paints **pattern bits** only. Palette assignment happens in attr mode (later). While painting pixels, the UI may **preview** a palette (e.g. map gray levels through the active palette row) but does not write attr bytes yet.

#### Viewport (Phase 1 only cell with zoom/pan)

- Zoom in/out within the Screen cell
- Click-drag to pan when zoomed
- Other cells: fixed viewport for now

### 3.5 Palettes (bottom-left)

**Purpose:** Edit **palette rows** for the current world / cart.

Terminology alignment with hardware:

- Each tab = one **palette row** (index **0–7**; up to **8 tabs**)
- Each tab shows **4 BG palettes** + **4 sprite palettes** in that row (the strips)
- User can select one **Palette** in a strip and edit individual color indices (master palette 0–63)

**Phase 1:** **Hidden or read-only stub.** No attr mode → no palette assignment workflow yet. Screen painting uses fixed grayscale indices 0–3. Default master palette may still load for future preview.

### 3.6 Painting colors (global rule)

Internal paint buffer always uses **4 colors** (2bpp indices 0–3). UI shows them as **grayscale** in pixel edit mode.

Palette strip selection is allowed **only in attr mode** (not Phase 1). In attr mode, selecting a palette for a tile updates attr bytes; preview colors update to match real in-game appearance.

---

## 4. Future: Play preview

A **Play** button (not Phase 1) runs an **in-Studio preview** of the current project:

- Uses worlds, screens, banks, and palettes as authored in the UI
- Interprets attached **game constraints** (scroll mode, player layout, etc.)
- Does **not** emulate the 6502, VRAM phases, or `$FExx` map

Purpose: quick feel-test while iterating. ROM correctness and hardware fidelity remain the job of the future emulator and real hardware.

---

## 5. Game constraints (dynamics)

These are **project-level properties** that describe how the game behaves — not just how it looks. Studio will expose them in a dedicated constraints panel (later phase). They compile into PRG configuration tables and/or generated code.

Open design space — capture now, implement incrementally:

| ID | Constraint | Questions to resolve |
|----|------------|---------------------|
| **C1** | **Player composition** | Is the player built from sprite layer tiles? How many sprites / OAM entries? Fixed meta-sprite layout? |
| **C2** | **Meta-sprites & animation** | Where are enemy/player meta-sprites defined? Frame sequences, flip bits, bank per character? |
| **C3** | **BG tile animation** | Swap nametable indices at runtime (independent of static ROM screen)? Waterfalls, torches, etc. |
| **C4** | **Scroll mode: pixel** | Does moving the player always use **pixel-level** scrolling? |
| **C5** | **Scroll mode: dead zone** | Camera fixed until player exits a center rectangle, then scroll? |
| **C6** | **Scroll mode: instant** | Screen-at-a-time transitions with no pixel scroll? |
| **C7** | **Scroll mode: hybrid** | Pixel scroll in playfield + instant screen switch or fade on events (doors, warps)? |
| **C8** | **Transition effects** | Fade via palette row interpolation, instant cut, or both — tied to constraint events |

These constraints affect Play preview and generated PRG stubs. They do not change the CHR/MAP export format in Phase 1.

---

## 6. Game logic codegen and ASM optimization

When Studio generates full-featured games, **MAP and CHR data** are unlikely to be the size problem. **PRG game logic** (generated 6502 code from constraints and behaviors) can become spaghetti and bloated if emitted naively as long asm templates.

**Chosen approach:** generate **correct, structured output first**, then optimize in the toolchain — not "perfect asm" at generation time inside the UI.

### Pipeline (Phase 5+)

```text
Studio constraints / behaviors
        |
        v
   Intermediate representation (IR)
   (state tables, jump tables, behavior graphs — not raw asm strings)
        |
        v
   Codegen backend  -->  ca65 / .s  (readable, boring, testable)
        |
        v
   cc65 assemble with -O / -O2
        |
        v
   Optional retr01-opt  (second-pass peephole / size optimizer on asm)
        |
        v
   PRG binary
```

| Stage | Role |
|-------|------|
| **IR** | Studio composes game logic here. Avoids asm spaghetti at the source. |
| **Codegen** | Separate module (`libretr01_codegen` or similar). IR → asm. Correctness over cleverness. |
| **cc65 -O2** | First optimization pass; maintained, good baseline. |
| **retr01-opt** (later) | Optional second pass before final link: dedupe sequences, branch shortening, zero-page promotion, state-machine tail sharing. Can also run on hand-written asm. |

Studio UI and constraint authoring **do not** embed a 6502 optimizer. The codegen backend and optional `retr01-opt` tool do.

---

## 7. Testing

Unit tests are added **alongside development**, not deferred to the end of a phase.

### Principles

- Every **`libretr01_studio_core`** module (RLE, MAP, CHR pack, project I/O, screen buffers) gets tests **when the module lands**.
- Codegen / IR backend gets tests **before** Studio depends on it for builds.
- Prefer **small, deterministic tests** with golden binary blobs (tile plane, CHR bank, RLE round-trip).
- UI cells may use lighter tests early (model/state tests without full SDL render); core format code must be fully covered.

### Suggested layout

```text
retr01_studio/
  core/           # library under test
  tests/
    unit/         # test_rle.c, test_chr_pack.c, test_map.c, ...
    fixtures/     # golden .bin / .json projects
```

Run tests via **CTest** (CMake) or equivalent on every change. Phase 1 is not done until core paths used by Generate bank and save/load have passing unit tests.

### Phase 1 minimum test coverage

- [ ] Screen tile plane encode/decode (32×30)
- [ ] CHR BG pack: dedupe identical 8×8 tiles, assign indices
- [ ] Project save/load round-trip (at least one world, sparse grid, one screen, one bank)
- [ ] Grid caps: reject >64 screens per world, >16 grid dimension

---

## 8. Design review

Overall the UI plan is **sound** and matches the hardware model. A few clarifications and risks:

### Aligns well

- **640×360 shell + integer scale** — good for a crisp retro-tool feel; keep game resolution (256×240) inside the Screen cell only.
- **Sparse world grid + Ctrl+click** — matches MAP directory / hole model in `02_graphics_worlds_memory.md`.
- **4 BG bank tabs × 256 tiles** — matches CHR BG bank layout (16×16).
- **Generate bank → pick bank 0–3** — matches per-screen CHR BG bank metadata and bank latches.
- **4-color pixel paint** — matches 2bpp patterns; attrs deferred is correct for Phase 1.
- **Play vs emulator split** — correct division of responsibility.

### Watch out for

| Topic | Note |
|-------|------|
| **Palette tabs vs palette rows** | Each tab should be one **palette row** (4 BG + 4 sprite palettes), not an arbitrary grouping. Name it that way in UI labels to avoid confusion with CHR banks. |
| **Sprite 8×16 toggle** | Display-only reorder is fine; document that CHR is always 8×8 tiles and 8×16 sprites are **two tile rows** in hardware. |
| **Generate bank scope** | Phase 1: BG layer only. Packing is per **selected CHR bank** for the **current world**. |
| **Attr-less export** | Phase 1 screens should export **960-byte tile plane** + **240-byte attr plane stub** (e.g. all palette 0) so MAP format stays valid. |
| **World vs screen selection** | Screen cell edits **one grid position** at a time; switching world tab or grid selection should prompt save/discard if dirty. |
| **64-screen cap** | Grid UI must enforce max stored screens per world. |
| **Constraint numbering** | Many scroll modes can coexist as a **project default + per-area overrides**; avoid making them mutually exclusive enums too early. |

No blocking issues. Phase 1 as scoped below is a good first slice.

---

## 9. Roadmap (summary)

| Phase | Focus |
|-------|--------|
| **0** | Shared C/C++ core: screen buffers, MAP directory, RLE, CHR pack, project file skeleton + **unit tests** |
| **1** | **UI shell + world grid + BG screen paint + BG Generate bank** (§10) + tests for pack/save/load |
| **2** | Attr / tile mode, palette row editor, per-tile palette assignment |
| **3** | Sprite layer, sprite banks, meta-sprites, Generate sprite bank |
| **4** | Game constraints panel, Play preview (high-level) |
| **5** | Full cart build: IR → asm → cc65 → optional **retr01-opt** → PRG + CHR + MAP |

Phases 0 and 1 can overlap: Phase 0 libraries land first; Phase 1 UI consumes them immediately.

---

## 10. Phase 1 — implement now

**Goal:** Author one world's sparse grid, paint BG screens, pack unique tiles into CHR BG banks, and inspect the result — no attrs, no sprites, no play, no ROM build.

### 10.1 In scope

#### UI shell

- [ ] 640×360 logical canvas
- [ ] Default **2×** integer window scale
- [ ] **Ctrl+F** fullscreen toggle with integer scale-to-fit
- [ ] Fixed cell layout (non-resizable cells)

#### Worlds map cell

- [ ] **1–8** world tabs (add/remove)
- [ ] Resizable grid per world (**1–16** cols, **1–16** rows)
- [ ] **Click** → select grid cell
- [ ] **Ctrl+click** → create/remove stored screen at that cell
- [ ] Enforce **64 screens max** per world
- [ ] Show selection state (stored vs hole, selected)

#### Screen cell

- [ ] Edit **one screen** for the selected world grid position (32×30 tiles, 256×240 px)
- [ ] **BG layer only** — 8×8 pixel grid painting
- [ ] **4 grayscale paint colors** (indices 0–3)
- [ ] Zoom and pan **inside Screen cell only**
- [ ] **Generate bank** button + CHR BG bank radio **0–3**
- [ ] On generate: dedupe tiles, pack into selected bank, update nametable indices, refresh BG bank view

#### BG banks cell

- [ ] **4 tabs** (banks 0–3)
- [ ] **16×16** read-only tile grid per tab (256 cells)
- [ ] Reflect tiles written by Generate bank

#### Data / persistence (minimal)

- [ ] In-memory project model: worlds, sparse screens, 4 BG CHR banks per world
- [ ] Save/load project file (format can be minimal JSON or binary — freeze in Phase 1 if needed)
- [ ] Export debug artifacts optional: raw `.bin` tile plane, CHR bank blob (full MAP RLE can wait for Phase 0 hardening)

#### Stub cells (visible, non-functional)

- [ ] Sprite banks cell — empty grids, disabled
- [ ] Palettes cell — hidden or placeholder ("Phase 2")

### 10.2 Out of scope (explicit)

- Sprite layer painting
- Attr / tile mode and palette strip assignment
- Palette row editor tabs
- Meta-sprites and animation authoring
- Game constraints panel
- **Play** preview button
- PRG / full `.retr01` cart build
- Parallax screen flags
- Low-level emulator hookup

### 10.3 Phase 1 data rules

| Data | Phase 1 behavior |
|------|------------------|
| Screen tile plane | **960 bytes** from BG paint + generate (indices into CHR bank) |
| Screen attr plane | **Stub**: all tiles palette **0** (240 packed bytes) |
| Screen MAP flags | CHR BG bank number = bank chosen at generate time; parallax bit = 0 |
| CHR BG banks | Up to **4 × 256** unique 8×8 2bpp tiles per world (from generate) |
| CHR sprite banks | Empty |
| Palette banks | Default row 0 only (hardcoded); not editable in UI |

### 10.4 Phase 1 user flow

1. Create/open project.
2. Add world tab (if needed).
3. Resize grid; **Ctrl+click** to place screens on the map.
4. **Click** a grid cell to select it.
5. Paint BG in Screen cell (grayscale 4 levels).
6. Choose CHR BG bank **0–3**; click **Generate bank**.
7. Inspect packed tiles in BG banks cell tabs.
8. Repeat for other grid cells / worlds.
9. Save project.

### 10.5 Phase 1 success criteria

- Can lay out a sparse world grid consistent with hardware caps.
- Can paint at least one full 32×30 BG screen.
- **Generate bank** produces correct unique tile set and nametable indices for the chosen CHR BG bank.
- BG banks cell displays the packed result accurately.
- Project survives save/load without losing worlds, grid shape, screens, or CHR data.
- **Unit tests** for CHR pack, project round-trip, and grid caps are green (see §7).

### 10.6 Suggested implementation stack

| Piece | Choice |
|-------|--------|
| Language | **C/C++** |
| Window / input | SDL2 (or equivalent) |
| UI rendering | Custom immediate-mode or lightweight retained UI drawn into 640×360 framebuffer |
| Core library | Static `libretr01_studio_core` — screen buffers, CHR pack, project I/O (shared with future build pipeline) |

---

## 11. Relationship to hardware docs

| Studio concept | Hardware doc |
|---------------|--------------|
| World grid | `02_graphics_worlds_memory.md` — sparse 16×16, 64 screens |
| Screen 32×30 | Same — 960 tile bytes + 240 attr bytes |
| CHR BG bank 0–3 | Same — 256 tiles, 16×16 grid, per-world |
| Generate bank | Fills CHR; screen flags record bank for `load_screen` |
| Palette rows | Same — Phase 2+ |
| Play preview | Not hardware-accurate; emulator later |

---

## 12. Future emulator (unchanged)

A **low-level Retr01 emulator** remains planned after Studio is solid. It will simulate cycles, memory decode, and `$FExx` behavior. It will load `.retr01` carts that Studio builds in later phases. Studio's Phase 1 work does not depend on it.
