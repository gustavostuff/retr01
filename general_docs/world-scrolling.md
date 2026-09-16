# World map and scrolling

How games are laid out in worlds and screens, how VRAM holds a sliding window of screens, and how BG0 parallax rate relates to BG1.

## World layout (high level)

Binary cart map is in `memory.md` (`.retr01` world blobs). Conceptually:

- Up to **8 worlds** per game.
- Up to **32 BG1 screens** per world.
- Each screen is **16x15 tiles** (240 tile-index bytes + 240 attribute bytes).
- Screens sit on a sparse virtual grid of up to **16x16 screen slots**.

That supports linear levels up to 16 screens tall or wide, labyrinth worlds, open NxM grids, and other shapes (32 present screens is the cart cap, not a full 16x16 fill).

Worlds may also have **BG0 screens** for depth: **0..8** present screens per world (sparse on the same 16x16 idea).

### Pattern banks per world

Each world has:

- 4 independent BG banks.
- 4 sprite banks.

Each bank is 256 patterns of 8x8 at 2bpp.

Hard cap: **16** entity types **per world** (catalog inside that world blob). Sprite bank bits fetch from that world's SPR CHR. Soft art pressure without tile reuse still tops out around **16** fully maxed unique-tile entities per world (1024 / 64), which lines up with the type cap. See `memory.md` and `software-api.md`.

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
- **Dead zone set** (default **32x30** when enabled): the camera stays put while the player walks inside a central box of that size. As soon as the player entity leaves that box, the camera starts moving so the player stays at the edge of the zone (classic follow-with-slack).

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

If the camera window covers a sparse grid **slot with no present screen** (or a neighbor that does not exist), that area is drawn as empty fill using the **current backdrop color**: the shared BG color index **0** of the active palette row (`$7F08`). Default for a plane that is **not** in wrap mode: motion **clamps** at the edges of the present playfield (no wrap to the opposite side) unless PRG implements a portal / instant switch. Either plane may instead **autoscroll** and/or **wrap** under PRG control (see below).

Corner reloads that need three new screens may spill past one frame of DMA. That is allowed. Prefer finishing the stream before unlocking free camera motion again if tear would show.

### BG0 independence

The same 2x2 windowing applies to BG0. Updating BG0 VRAM slots is independent of BG1 because scroll rates can differ.

### Programmatic scroll and wrap (BG0 and BG1)

PRG can drive **BG0 and/or BG1** scroll on their own, not only as a slave of player motion. That includes:

- Autoscrolling a plane while the player stands still (fast clouds, drifting stars, conveyor levels, rail sections).
- Warping / wrapping a plane after **N** screens so a short strip repeats as an infinite band.

Example: a space fly-through with a few star/planet screens that loop once the strip has scrolled past. Same idea for repeating cloud bands. That can be BG0, BG1, or both at different rates.

Wrap means: when scroll on that plane reaches the end of its present strip of screens, PRG (or a helper) loads the **start** of the strip again into the next VRAM slot so the backdrop loops. Autoscroll without wrap stops at the last screen of the strip unless PRG turns wrap on. Exact ports / helper API for "autoscroll + wrap period per plane" stay TBD in `software-api.md`, but the behavior is in scope for v1 authors.

### When to write scroll registers (locked)

Update `$7F02` (scroll X / HC574) and `$7F03` (scroll Y / Beam X) in **NMI / VBlank** only, same as `$7F08`/`$7F09`. Mid-active-display writes can tear the picture or change MAP banks mid-line.

Raster compare `$7F04` may be written outside VBlank only for a **deliberate** split-screen / IRQ effect with documented rules. Do not invent mid-frame scroll by accident.

## Parallax scroll rate

Example: BG1 screens fill a solid 4x4 grid (16 screens). BG0 is a 2x2 grid. The enclosing box for BG1 is double BG0 on both axes, so BG0 scrolls at half the rate of BG1.

Screen arrangements can be any shape. Compute the enclosing minimum grid for BG1 screens and for BG0 screens, then derive X and Y scroll relationships from those boxes. C/ASM PRG utilities should help authors with that math.
