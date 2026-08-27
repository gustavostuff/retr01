# retr01 Studio

**Product SoT:** [`retr01_studio/README.md`](../retr01_studio/README.md). This file is a short mirror for the docs tree.

**Only active authoring tool** for now. **C11 + SDL2 + FreeType (Proggy Tiny)** desktop app. Not the board IC simulator ([`08`](08_simulator.md)).

| Does | Does not (yet) |
|------|----------------|
| PNG atlas import, Play preview, `.retr01` export | Generate, planes, constraints editor |
| Phase 2: 8 world buttons, screen create/delete, tile edit modal, paint brush | Multi-world cart export (still world 0) |
| Host Play matching Emulator Phase 1 | Cycle / `$FExx` / silicon timing |
| Active BG/SPR palette strip (8px cells) | Full palette editor / bank UI |

## Phase 2 UI (current)

- **640x360**, 8px grid chrome, dark / darker gray, Proggy Tiny
- Left: **Worlds** label, 8x16px world buttons, **128x128** map (16px cell pitch)
- Double-click create screen, Ctrl+click remove, click select
- World 0 default: 8x8 slots, **3x3** present blank screens
- Right: screen view @2x, Play above; right-click **Edit tile** (288x160 modal) / **Paint with this**
- Bottom-right: contiguous 8px BG+SPR palette swatches

## Phase 1 behavior (still)

- **Scroll:** Smooth pixel, no dead zone
- **Player:** 8x8 overlay from sprite pal row 0 index 1
- **Start:** Prefer **(2, 0)** if present
- **Warps:** X -> (0,0), Y -> (1,0)
- **Save / load:** Ctrl+S / Ctrl+O -> `test_game/test.r01proj` (JSON v4)
- **Export:** Ctrl+E -> `test_game/test.retr01` (+ prom / boot / flash)

## Hardware map

Studio follows [`02`](02_graphics_worlds_memory.md). Board BOM: [`06`](06_hardware_v1_32ic.md).

## Build

See [`retr01_studio/README.md`](../retr01_studio/README.md). Root helper: `./studio build-run`.
