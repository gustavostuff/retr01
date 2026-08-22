# Retr01 Studio

**Only active software tool** for now. **C/C++** desktop app to author worlds, screens, CHR, palettes, and later behavior. Later phases emit `.retr01` (PRG + CHR + MAP). Not the cycle emulator (planned separately).

| Does | Does not (for now) |
|------|---------------------|
| Author + generate CHR/MAP, later cart build | Cycle / `$FExx` / silicon timing |
| Later: high-level **Play** preview | Replace the future emulator |

## UI shell

Fixed **640x360** canvas. Default **2x** window (1280x720). Integer scale only. **Ctrl+F** fullscreen (still integer). Cells are fixed regions (not floating / not user-resizable). Only **Screen** zooms/pans in Phase 1.

```text
+------------------------------------------------------------------+
|  [ Worlds map ]          |                                       |
|  (tabs, grid)            |                                       |
+--------------------------+             [ Screen ]                |
|  [ BG banks ]            |         (BG/Sprite paint)             |
|  (4 tabs, 16x16 each)    |                                       |
+--------------------------+                                       |
|  [ Sprite banks ]        |                                       |
|  (4 tabs, 8x8/8x16 view) |                                       |
+--------------------------+                                       |
|  [ Palettes ]            |                                       |
|  (palette row tabs)      |                                       |
+------------------------------------------------------------------+
```

Right column = Screen (most width). Left = Worlds, BG banks, Sprite banks, Palettes.

## Cells

| Cell | Behavior |
|------|----------|
| **Worlds** | Tabs labeled **1-8** in the UI = hardware worlds **0-7** (`$FE30`). Sparse grid **1-8** cols/rows (UI). Click = select. **Ctrl+click** = toggle stored screen. Max **32** screens/world. Holes = not in MAP |
| **BG banks** | 4 tabs (0-3), 16x16 read-only tiles. Filled by **Generate bank** |
| **Sprite banks** | Same shape, 8x8/8x16 preview toggle. Phase 1: visible but disabled |
| **Screen** | One **16x15** BG paint (128x120). 4 grayscale colors (2bpp 0-3). **Generate bank** + radio bank **0-3**: dedupe 8x8 -> CHR (error if >256). Attr mode Phase 2+ |
| **Palettes** | Tabs = palette rows **0-7** (4 BG + 4 sprite strips). Phase 1: hidden/stub. Master RGB = Color PROM preview; **quantize to R3G3B2** for PROM burn ([`02`](02_graphics_worlds_memory.md) / [`06`](06_hardware_v1_32ic.md)). Logical kit swatches stay 24-bit in the editor |

Paint always stores indices **0-3**. Palette assignment only in attr mode.

## Phases

### Phase 0 - core library

**Goal:** shared `libretr01_studio_core` before/alongside UI. Overlaps Phase 1.

**In:** screen buffers (240+240), sparse MAP directory model, CHR BG pack (dedupe 8x8), project JSON skeleton, CTest/unit tests with golden fixtures.

**Out:** SDL UI, paint tools, cart binary emit.

**Done when:** pack + encode/decode + save/load round-trip tests are green.

### Phase 1 - implement now

**Goal:** sparse world grid, paint BG, pack CHR BG banks, save/load. No attrs UI, sprites, Play, or full cart build.

**In:** shell + worlds + Screen BG paint + Generate bank + BG bank view + JSON project + unit tests (tile plane, CHR pack, round-trip, grid caps).

**Out:** sprite layer, attr/palette editors, meta-sprites, constraints, Play, `.retr01` build, parallax flags, emulator.

| Data | Phase 1 |
|------|---------|
| Tile plane | 240 B from paint + generate |
| Attr plane | 240 B stub (pal 0, `BANK` = generate radio) |
| CHR BG | up to 4x256 tiles/world |
| CHR spr / pals | empty / hardcoded row-0 indices |

**Flow:** open project -> place screens (Ctrl+click) -> paint -> Generate bank 0-3 -> inspect BG banks -> save.

**Stack:** C/C++, SDL2 (or equiv), custom UI into 640x360, `libretr01_studio_core` (buffers, CHR pack, I/O).

### Phase 2 - attrs and palettes

**Goal:** real per-tile attrs and editable palette rows. MAP payloads match hardware.

**In:**
- Screen **attr mode**: `PAL`, `FLIP_H`/`FLIP_V`, `BANK`, `SOLID`, `ANIM` per tile
- **Palettes** cell live: up to 8 row tabs, 4 BG + 4 sprite strips, edit master indices 0-63
- Preview through Color PROM mirror (not grayscale-only)
- **Generate bank** + `ANIM`: pack 4 frames at `B..B+3` (4-aligned), stamp attrs. Prefer flips over duplicate CHR
- World / screen defaults: `default_bg_bank`, `default_pal_row`, optional parallax flag on dir entries
- Attr plane is real data (no stub zeros except unused bits)

**Out:** sprite authoring, Play, full PRG build.

**Done when:** round-trip keeps attrs + palette banks. Editor previews `ANIM` strips.

### Phase 3 - sprites

**Goal:** sprite CHR and placement, same bank model as BG.

**In:**
- Enable **Sprite banks** cell (4 tabs, 8x8 / 8x16 view toggle)
- Screen **sprite layer**: paint / place tiles, OAM-oriented preview
- **Generate sprite bank** (dedupe into banks 0-3)
- **Meta-sprites**: group tiles, flips, bank, simple frame lists
- Attr bits for sprites: `BANK`/`PAL`/`FLIP_*`/`PRIORITY`/`SIZE` (preview only until Play/PRG)

**Out:** game constraints UI, cart link, emulator.

**Done when:** a screen can show BG + sprites in the editor with packed sprite CHR.

### Phase 4 - constraints and Play

**Goal:** project behavior knobs + fast feel-test without emulating silicon.

**In:**
- Constraints panel (project default + optional per-area overrides):
  - **C1** player composition (OAM/meta-sprite layout)
  - **C2** meta-sprite / enemy anim tables
  - **C3** BG living-tile rates (`ANIM` list)
  - **C4-C7** scroll: pixel / dead zone / instant / hybrid
  - **C8** transitions (cut, palette fade)
- **Play** button: high-level preview (scroll, seams, constraints). Not 6502 / `$FExx` / PHI2

**Out:** shipping `.retr01`, `retr01-opt`, cycle emu.

**Done when:** Play walks a multi-screen world using authored data + chosen scroll mode.

### Phase 5 - cart build

**Goal:** emit a burnable `.retr01` image.

**In:**
- Pointer-table cart layout from `02` (magic `RETR01`, globals, worlds, CHR, MAP, 32 KB PRG)
- IR from constraints/behaviors -> readable ca65 `.s` -> cc65 `-O2`
- Optional **`retr01-opt`** peephole/size pass
- Link PRG + pack CHR/MAP/pals into one flash image
- Export / burn helper for SST39SF040
- Color PROM image emit: quantize kit swatches to packed **R3G3B2** ([`02`](02_graphics_worlds_memory.md))
- Note cart **I2C save** presence in project/cart metadata when saves are used (port/HAL from `02`)

```text
constraints/behaviors -> IR -> ca65 .s -> cc65 -O2 -> optional retr01-opt -> .retr01
```

Generate correct boring asm first. Optimize in toolchain, not inside the UI.

**Out:** separate validation tools -- **board IC simulator** ([`08`](08_simulator.md)) and optional later cycle-level cart check. Not part of Studio Phase 5.

**Done when:** a Phase 4 project builds a `.retr01` that boots on kit/hardware checklist (PRG + one world + screens).

## Hardware map

Studio follows [`02`](02_graphics_worlds_memory.md) for data meaning. Board BOM: [`06`](06_hardware_v1_32ic.md).

| Studio | Spec (`02`) |
|--------|-------------|
| World grid | 8x8 sparse, 32 screens, 8 worlds |
| Screen | 240 tile + 240 attr |
| BG banks 0-3 | 256 tiles each. Live bank = per-tile attr |
| Generate bank | Fills CHR, stamps default `BANK` |
| Color PROM preview | 64 indices -> packed **R3G3B2** for burn |
| Play | High-level only. Board sim / cycle check later load `.retr01` |
