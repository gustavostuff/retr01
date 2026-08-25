# Retr01 Studio

**Phase 1 product SoT:** [`retr01_studio/README.md`](../retr01_studio/README.md). This file is a short mirror for the docs tree. Later Studio phases are **not** documented here until defined.

**Only active authoring tool** for now. **C11 + SDL2** desktop app. Not the board IC simulator ([`08`](08_simulator.md)).

| Does (Phase 1) | Does not (Phase 1) |
|----------------|--------------------|
| PNG atlas import, Play preview, `.retr01` export | Pixel/attr paint, Generate, palette/bank UI |
| One world, sparse present screens | Multi-world tabs, planes, constraints editor |
| Host Play matching Emulator Phase 1 | Cycle / `$FExx` / silicon timing |

## Phase 1 summary

**Goal:** Author **world 0** via **PNG import**, preview with **Play**, export a **`.retr01`** cart whose runtime matches Play.

### Behavior

- **Scroll:** Smooth **pixel** scroll. Camera follows the player -- **no dead zone**.
- **Player:** One **8x8** overlay sprite (kit **bright red**, master index 34). **WASD** / arrows in Play.
- **Start:** Prefer screen **(1, 1)** if present, else first present screen.
- **Warps (hardcoded):** **X** -> screen **(0, 0)**. **Y** -> screen **(1, 0)**. Instant.
- **Collision:** Player stays on **present** screens only.

### Grid and UI

| Rule | Phase 1 |
|------|---------|
| Worlds | **World 0** only |
| Default grid | **3x3** slots; PNG import sets **NxM** (1-8 per axis, max **8x8**) |
| Presence | Cells with transparent PNG regions stay **absent** (`present=0`); only present cells are clickable |
| Canvas | Fixed **640x360**, integer scale (default **2x**) |
| Left | Worlds grid only |
| Right | Screen preview (read-only) |
| Hidden | Planes, bank viewers, Palettes, Constraints, Generate, VRAM 1x |

```text
+------------------------------------------------------------------+
|  [ Worlds -- NxM grid ]  |                                       |
|  (world 0; click select) |         [ Screen ]                    |
|                          |   (read-only view of active screen)   |
+------------------------------------------------------------------+
```

### PNG import

| Rule | Value |
|------|--------|
| Drop target | **Anywhere** on the window |
| Destination | World 0, **BG bank 0** |
| Unique tiles | <= **256** unique 8x8 patterns |
| Colors | <= **4** colors per PNG |
| Cell size | **128x120** px per screen cell |
| Generate | **No** Ctrl+G in Phase 1 |

Palettes are written automatically (no UI): shared backdrop index **0**, kit strip indices for colors 1-3. See studio README.

### Save / Play / export

| Action | Input / path |
|--------|----------------|
| Save / load | **Ctrl+S** / **Ctrl+O** -> `test_game/test.r01proj` (JSON **v3**) |
| Play | **Space** / **PLAY** -- SoT: `core/src/play.c` |
| Export | **Ctrl+E** -> `test_game/test.retr01` (+ `test_prom.bin`, `test_boot.s`, `test_flash.bin`) |

**Cart:** Present screens only in MAP; play table at PRG `$8100`; marker `R01P`. Stub PRG boots and polls pads; Play math runs on the host (Studio and Emulator), not inside 6502 PRG yet.

### Out of scope (Phase 1)

Pixel/attr paint, sprite placement, planes, palette/bank editors, Generate, multi-world UI, dead-zone / fade profiles (except X/Y warp tests), multi-tile entities.

## Hardware map

Studio follows [`02`](02_graphics_worlds_memory.md) for data meaning. Board BOM: [`06`](06_hardware_v1_32ic.md).

| Studio | Spec (`02`) |
|--------|-------------|
| World | Sparse grid, up to 8x8 / 32 screens (Phase 1 UI: world 0, PNG-sized grid) |
| Screen | 240 tile + 240 attr |
| BG bank 0 | Filled by PNG import |
| Color PROM | 64 indices -> packed **R3G3B2** for burn |
| Play | High-level only; board sim is separate ([`08`](08_simulator.md)) |

## Build

See [`retr01_studio/README.md`](../retr01_studio/README.md). Root helper: `./studio build-run`.
