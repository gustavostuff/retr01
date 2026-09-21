# World map and scrolling

How games are laid out in worlds and screens, how VRAM holds a sliding window of screens, and how BG0 parallax rate relates to BG1.

## World layout (high level)

Binary cart map is in `memory.md` (`.retr01` world blobs). Conceptually:

- Up to **8 worlds** per game.
- Up to **64 BG1 screens** per world.
- Each screen is **16x15 tiles** (240 tile-index bytes + 240 attribute bytes).
- Screens sit on a sparse virtual grid of up to **16x16 screen slots**.

That supports linear levels up to 16 screens tall or wide, labyrinth worlds, open NxM grids, and other shapes (64 present screens is the cart cap, not a full 16x16 fill).

Worlds may also have **BG0 screens** for depth: **0..16** present screens per world (sparse on the same 16x16 idea).

### Pattern banks (cart-wide)

CHR is cart-global:

- 16 independent BG banks.
- 16 independent Sprite banks.

Each bank is 256 patterns of 8x8 at 2bpp. Nametable and sprite attrs name **bank 0-15** per cell / sprite. See `video-graphics.md`.

Hard cap: **32** entity **types** **cart-wide** (one global catalog). On-screen instance count is soft: limited by the **64** hardware sprites in OAM (and **16**/scanline), not by the type cap. Soft art pressure without tile reuse still tops out around **21** fully maxed unique-tile types (4096 / 192). See `memory.md` and `software-api.md`.

## Movement modes

Two ways the player moves between screens (not mutually exclusive per game or world):

1. **Instant screen switch** (events, portals, leaving the current screen boundary, and so on).
2. **Seamless pixel scrolling**.

Instant switch is simple. Seamless scroll is the hard path and drives VRAM design.

## Player movement vs camera movement

These go together in many games, but they are **not** the same thing.

| Piece | What it is |
| --- | --- |
| **Player movement** | The player entity's position in the world (input, physics, collisions) |
| **Camera movement** | Where the BG1 view looks in the world (scroll registers / VRAM window) |

PRG owns both. The camera may follow the player, ignore the player, or move on a script. Player motion never *is* camera motion by itself.

### Dead zone

World header bytes **30-31** hold the camera dead-zone size (**width**, **height**) in pixels inside the logical **128x120** view.

- **No dead zone** (0x0, or disabled): the camera tracks the player one-for-one. When the player moves, the camera moves with them (within clamp rules).
- **Dead zone set** (default **32x30** when enabled): the camera stays put while the player walks inside a box of that size. As soon as the player entity leaves that box, the camera starts moving so the player stays at the edge of the zone (classic follow-with-slack).

`r01_camera_set_deadzone(ctx, w, h)` packs **w** and **h** into those header bytes as given. Host Play follow does not always use that rectangle pixel-for-pixel.

#### 2 px edge parity

Spawn and `r01_play_camera_snap` place the player on the viewport center (**64**, **60**). Walk is 1 logic pixel per frame. Hold-X run is 2. Camera motion when the player leaves the box is the overflow past that edge, not the full step.

If an edge has the opposite parity from the center, a 2 px step from snap never lands on the edge (example: center **64**, right **79**, run hits **78** then **80**). Overflow is 1 px. BG0 and BG1 follow the camera, so that frame is a 1 px hitch in both layers.

Host Play snaps each live edge inward so left/right share parity with **64** and top/bottom share parity with **60**. Packed size is unchanged. Live size is packed, or one pixel smaller on that axis. The box may sit one pixel off center. Live edges only move inward.

Example: packed **32x70** follows as **48..78** on X (31 px) and **26..94** on Y (69 px).

A packed size whose edges already match that parity is used as-is. On 128x120, odd **31x69** is the 32x70-shaped box with no shrink.

World header bytes **30-31** pack width and height. **0,0** means no dead zone (1:1 track). Defaults and axis-lock helpers are in `software-api.md`.

### Camera drive modes (examples)

Authors can mix these per world or moment:

- **Follow player** (with or without dead zone), on **X**, **Y**, or **both** axes.
- **Axis lock**: fix the camera on one axis (side-scroller: scroll X only) or allow both.
- **Auto / scripted camera**: move the view on a timer or path even if the player stands still (cutscenes, forced marches, rail sections).
- **Player-relative vs independent**: camera follow is separate from **autoscroll** on BG0 and/or BG1 (see below).

Instant screen switch still moves the camera in jumps. Seamless scroll moves it in pixels and may reload the 2x2 VRAM window when the view crosses screen boundaries.

## VRAM screen window (seamless scroll)

VRAM holds **four whole screens** for BG1 (and the same idea independently for BG0): a 2x2 window of screens.

Example: a 3x3 world of screens:

```
        world map (3x3 screens)

    +---------+---------+---------+
    |         |         |         |
    |    A    |    B    |    C    |
    |         |         |         |
    +---------+---------+---------+
    |         |         |         |
    |    D    |    E    |    F    |
    |         |         |         |
    +---------+---------+---------+
    |         |         |         |
    |    G    |    H    |    I    |
    |         |         |         |
    +---------+---------+---------+
```

Default start screen is E (center). No scroll yet. VRAM holds a 2x2 window (`#` padding marks screens in VRAM):

```
        world                          VRAM (2x2 buffer)

    +---------+---------+---------+    +---------+---------+
    |         |         |         |    |#########|#########|
    |    A    |    B    |    C    |    |### E ###|### F ###|
    |         |         |         |    |#########|#########|
    +---------+---------+---------+    +---------+---------+
    |         |#########|#########|    |#########|#########|
    |    D    |### E ###|### F ###|    |### H ###|### I ###|
    |         |#########|#########|    |#########|#########|
    +---------+---------+---------+    +---------+---------+
    |         |#########|#########|
    |    G    |### H ###|### I ###|
    |         |#########|#########|
    +---------+---------+---------+
```

From that state:

- Move a little bit to the **right** inside the current 2x2 window: no VRAM reload (the viewport only partially shows F).
- Move a few pixels **up**: stream two new screens from cart (each 240 + 240 bytes). VRAM becomes B-C / E-F:

```
        world                          VRAM after move up

    +---------+---------+---------+    +---------+---------+
    |         |#########|#########|    |#########|#########|
    |    A    |### B ###|### C ###|    |### B ###|### C ###|
    |         |#########|#########|    |#########|#########|
    +---------+---------+---------+    +---------+---------+
    |         |#########|#########|    |#########|#########|
    |    D    |### E ###|### F ###|    |### E ###|### F ###|
    |         |#########|#########|    |#########|#########|
    +---------+---------+---------+    +---------+---------+
    |         |         |         |
    |    G    |    H    |    I    |
    |         |         |         |
    +---------+---------+---------+
```

- Move **left and up** from the initial E-F / H-I window: load three new screens. VRAM becomes A-B / D-E:

```
        world                          VRAM after left+up

    +---------+---------+---------+    +---------+---------+
    |#########|#########|         |    |#########|#########|
    |### A ###|### B ###|    C    |    |### A ###|### B ###|
    |#########|#########|         |    |#########|#########|
    +---------+---------+---------+    +---------+---------+
    |#########|#########|         |    |#########|#########|
    |### D ###|### E ###|    F    |    |### D ###|### E ###|
    |#########|#########|         |    |#########|#########|
    +---------+---------+---------+    +---------+---------+
    |         |         |         |
    |    G    |    H    |    I    |
    |         |         |         |
    +---------+---------+---------+
```

Rule of thumb: stay inside the four-screen buffer without cart traffic, then stream the missing row, column, or corner screens when the camera crosses into a new 2x2.

### Empty or missing screens (locked)

**BG1:** If the camera window covers a sparse grid **slot with no present BG1 screen**, default behavior still draws **BG0** there (same as BG1 color index **0** show-through). Backdrop (shared BG color index **0** of the active palette row at `$7F08`) appears only where BG0 is also off, missing, or transparent.

Optional **`r01_bg0_set_clip_to_bg1(ctx, 1)`** (cart flags byte **7** bit **3** / `0x08`): hide BG0 outside present BG1 slots and use backdrop there instead. Independent of BG0 layout wrap.

**Clamp / wrap:** Default for a plane that is **not** in wrap mode: motion **clamps** at the edges of the present playfield (no wrap to the opposite side) unless PRG implements a portal / instant switch. Either plane may instead **autoscroll** and/or **wrap** under PRG control (see below).

Corner reloads that need three new screens may spill past one frame of DMA. That is allowed. Prefer finishing the stream before unlocking free camera motion again if tear would show.

### BG0 independence

The same 2x2 windowing applies to BG0. Updating BG0 VRAM slots is independent of BG1 because scroll rates can differ.

### Programmatic scroll and wrap (BG0 and BG1)

PRG can drive **BG0 and/or BG1** scroll on their own, not only as a slave of player motion. That includes:

- Autoscrolling a plane while the player stands still (fast clouds, drifting stars, conveyor levels, rail sections).
- Warping / wrapping a plane after **N** screens so a short strip repeats as an infinite band.

Example: a space fly-through with a few star/planet screens that loop once the strip has scrolled past. Same idea for repeating cloud bands. That can be BG0, BG1, or both at different rates.

#### BG0 layout wrap (cart flag)

World header byte **7** (flags):

- bit **0**: player anim blob present
- bit **1** (`0x02`): **BG0 wrap X** - tile the present BG0 screen layout horizontally
- bit **2** (`0x04`): **BG0 wrap Y** - tile the present BG0 screen layout vertically
- bit **3** (`0x08`): **BG0 clip to BG1** - hide BG0 outside present BG1 camera slots
- bit **4** (`0x10`): **platformer** - gravity + face Y jump
- bit **5** (`0x20`): **run on X** - hold face X for 2x walk

Author code sets wrap with `r01_bg0_set_wrap(ctx, wrap_x, wrap_y)` and clip with `r01_bg0_set_clip_to_bg1(ctx, enable)` in `custom_logic.c`. Studio packs those calls into the flag bits at cart export (same scan path as camera dead zone). `r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER)` packs bit **4**. `r01_player_set_run_on_x(ctx)` packs bit **5**.

Scroll rate without wrap is end-aligned `(bg0_n - 1) / (bg1_n - 1)` on each axis (see Parallax scroll rate below). With wrap on an axis, the rate is the period ratio `bg0_n / bg1_n` so a repeating strip stays even (8 BG0 screens under 16 BG1 screens is exact 1/2: 1 logic px every 2 frames at walk, 1 px per frame at 2x run). Wrap sampling still modulo-tiles the present BG0 box.

When wrap is on for an axis, sampling maps pixels outside the present BG0 bounding box back into that box with a positive modulo, so the authored BG0 block repeats and empty BG0 regions do not appear. When wrap is off, that axis clips outside the bbox (backdrop).

BG1 strip wrap / autoscroll helpers remain TBD in `software-api.md`. Update scroll registers in NMI / VBlank only (see below).

### When to write scroll registers (locked)

Update `$7F02` (scroll X / HC574) and `$7F03` (scroll Y / Beam X) in **NMI / VBlank** only, same as `$7F08`/`$7F09`. Mid-active-display writes can tear the picture or change MAP banks mid-line.

Raster compare `$7F04` may be written outside VBlank only for a **deliberate** split-screen / IRQ effect with documented rules. Mid-frame scroll is not an accidental side effect.

## Parallax scroll rate

Example: BG1 screens fill a solid 4x4 grid (16 screens). BG0 is a 2x2 grid. Camera travel on each axis is `(screens - 1)` screen widths, so BG0 moves at `(2 - 1) / (4 - 1) = 1/3` the BG1 camera rate. That keeps the start and end of both planes aligned. A naive `bg0_screens / bg1_screens` scale overshoots the BG0 plane and makes BG0 screens look like they slide off the 2x2 grid.

Screen arrangements can be any shape. Compute the enclosing minimum grid for BG1 screens and for BG0 screens, then derive X and Y scroll relationships from those boxes. Without wrap: `(bg0_n - 1) / (bg1_n - 1)` when BG0 is strictly smaller on that axis, otherwise park BG0. Pixel snap is nearest. With wrap: `bg0_n / bg1_n` (period vs period, floor). An 8-col wrapping BG0 under a 16-col BG1 bbox is exact 1/2, so walk is 1 BG0 logic pixel every 2 frames and hold-X run is 1 px per frame. C/ASM PRG utilities help authors with that math.
