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

## Movement modes

Two ways the player moves between screens (not mutually exclusive per game or world):

1. **Instant screen switch** (events, portals, leaving the current screen boundary, and so on).
2. **Seamless pixel scrolling**.

Instant switch is simple. Seamless scroll is the hard path and drives VRAM design.

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

- Move a bit **right** inside the current 2x2 window: no VRAM reload.
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

### BG0 independence

The same windowing applies to BG0. Updating BG0 VRAM slots is independent of BG1 because scroll rates can differ.

## Parallax scroll rate

Example: BG1 screens fill a solid 4x4 grid (16 screens). BG0 is a 2x2 grid. The enclosing box for BG1 is double BG0 on both axes, so BG0 scrolls at half the rate of BG1.

Screen arrangements can be any shape. Compute the enclosing minimum grid for BG1 screens and for BG0 screens, then derive X and Y scroll relationships from those boxes. C/ASM PRG utilities should help authors with that math.
