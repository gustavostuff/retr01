# Retr01 BG tile attributes

**Status:** accepted. Per-tile attrs + per-tile `BANK` are in `02`/`03`/`05`. Studio Phase 2 UI and flip+bank silicon timing still open (A3-A5).

One **attr byte per BG tile**. High nibble layout is fixed below. Tile animation uses a fixed CHR convention: **4 consecutive frames in a row**.

Related: [`02`](02_graphics_worlds_memory.md), [`03`](03_hardware_implementation.md), [`04`](04_retr01_studio.md), [`05`](05_costs_and_open_questions.md) Q16.

---

## Goals

- Per-tile **palette** and **H/V flip** (mirror BG patterns -> fewer unique BG CHR tiles).
- Per-tile **BG bank** select (0-3).
- Per-tile **collision** bit for gameplay.
- Simple **BG animation**: living tiles cycle **4 frames** laid out as four neighbors in the BG bank.
- Ship **C and/or ASM helpers** in the Retr01 **dev kit** as the default path for load / anim / collide / bank stamping.

## Custom code vs dev kit

The **attr byte layout and bit meanings are hardware / format rules**. Studio, MAP payloads, and the BG fetch path assume them. Games **must** honor those bits even if they ignore our libraries.

Developers **may** implement their own animation, collision, movement response, anim rates, living-tile lists, etc. That is expected for advanced engines.

The dev kit still **provides** small C/ASM routines (see below) so most games do not reinvent VBlank anim, solid shadows, or bank-aware loads. Treat kit code as **optional convenience**, not the only legal approach - but **not** as permission to redefine `PAL` / flip / `BANK` / `SOLID` / `ANIM` or the 4-frame CHR strip rule when `ANIM=1`.

| Layer | Who owns it |
|-------|-------------|
| Attr **bitfields** + 4-frame strip when `ANIM=1` | **Platform contract** (fixed) |
| When to animate, how hard to collide, physics feel | **Game** (custom or kit) |
| Reference load / anim_step / solid_at / phys_move_xy | **Dev kit** (optional link) |

---

## Attr byte (per tile)

Screen **16x15** -> **240** tile bytes + **240** attr bytes = **480 B**/screen (fits **512 B** VRAM slot).

| Bits | Name | Meaning |
|------|------|---------|
| 1-0 | `PAL` | BG palette 0-3 |
| 2 | `FLIP_H` | horizontal mirror |
| 3 | `FLIP_V` | vertical mirror |
| 5-4 | `BANK` | BG CHR bank 0-3 (same range as `$FE31`-`$FE36`) |
| 6 | `SOLID` | 0 = empty / pass, 1 = solid |
| 7 | `ANIM` | 0 = static, 1 = living (4-frame cycle) |

Bits 7-4 are fixed as `BANK` / `SOLID` / `ANIM` (see table).

### CHR animation convention (mandatory for `ANIM=1`)

Living tiles use **exactly 4 frames**, stored as **four consecutive tile indices in one BG bank row**:

```text
index:  ... | B | B+1 | B+2 | B+3 | ...
              f0   f1    f2    f3
```

Rules:

- Base index `B` must be **4-aligned** (`B & 3 == 0`).
- All four frames share the same `BANK`, `PAL`, flips, and `SOLID` (attrs are for the cell, not per frame).
- Runtime nametable byte is `B + phase` where `phase` is in `0..3`.
- Studio / packer places strips on a bank row (16 tiles wide): up to **4** strips per row.

Static tiles (`ANIM=0`) use a single index; no alignment rule.

### Why flip matters here

One strip + `FLIP_H` / `FLIP_V` avoids duplicate mirrored water edges, grass tips, etc. Call this out in Studio docs: **mirroring reduces unique BG patterns**.

---

## Hardware implications

| Piece | Hardware? | Extra ICs | Notes |
|-------|-----------|-----------|-------|
| `PAL` | Yes (existing) | 0 | Compositor |
| `FLIP_H` / `FLIP_V` | Yes | 0-1 PLD | Shifter reverse / fine-Y XOR; see contingency 4th ATF22V10 |
| `BANK` | Yes | 0-1 | Mux into CHR high address from attr (critical path: measure on BG island) |
| `SOLID` | No | 0 | Ignored by video |
| `ANIM` | No | 0 | Soft updates nametable tile index |

**Dot clock / CRT / sprite HBlank:** unchanged. Denser attrs (~480 B/screen stream) only affect CPU camera loads.

**Simplest bank rule (locked):** CHR bank for a tile **always** comes from attr `BANK`. Screens are not tied to a bank in hardware. On screen load, fill those bits from MAP payload (and/or stamp a MAP default into every attr). `$FE31`-`$FE36` are optional bulk helpers only. **No mid-frame bank switching** (no line-IRQ bank flips): each tile already has its bank.

---

## Software model

### At screen / slot load

1. Stream **240** tiles + **240** attrs into the VRAM slot.
2. Build **collision shadow** in system RAM from `SOLID` (16x15 bits or bytes).
3. Build **anim list**: for each cell with `ANIM=1`, record `{vram_tile_addr, base_index}` where `base_index = tile & ~3`.
4. Cap list length (recommend **32** or **64** living cells per loaded camera workbench).

### Each NMI (or every 2nd frame)

1. `phase = (frame_counter >> rate_shift) & 3`
2. For each anim entry: write `base_index + phase` to that nametable address via `$FE10`-`$FE12`.
3. Do not rewrite attrs every frame.
4. Use **tear-safe** commits (VBlank / beam-Y gate). Do not change a living tile's index while the beam is inside that cell's 8 rows - see [`02`](02_graphics_worlds_memory.md) (*Live VRAM updates and tear avoidance*).

### Collision

Physics reads **only** the RAM shadow. Breakable blocks update VRAM tile/attr **and** the shadow bit together.

### Banks

- **Rendering:** hardware uses attr `BANK` per tile.
- **Loading:** C/ASM helper sets `BANK` bits when unpacking MAP -> VRAM; optional `retr01_bg_set_slot_bank()` fills a slot's attrs when the whole room shares one bank.

---

## Sprite vs solid tiles (performance deep dive)

Worry: N sprites each tested against solid BG, then movement corrected, every frame. On paper that sounds like "N x many tiles." In practice it is **cheap on Retr01** if you never scan the whole screen and you keep a **RAM solid shadow**.

### Budget you actually have

| | Approx |
|--|--|
| CPU | **8.000 MHz** W65C02S |
| Frame | ~**60 Hz** -> ~**133000** cycles/frame |
| vs NES (~1.79 MHz) | ~**4.5x** cycles/frame |
| Work window | **Whole frame** (game logic in system RAM). VRAM port only when touching nametables |

Collision hot path should touch **system RAM only**, never `$FE12`.

### What you are not doing

- Not testing each sprite against all **240** tiles.
- Not reading attrs from VRAM mid-physics.
- Not (for v0 platformers) pixel-perfect BG bitmasks.

You only test tiles that **overlap that sprite's hitbox** after movement - typically **1..6** tiles for an 8x8 or 8x16 body.

### Cost model (order of magnitude)

Per sprite, per axis (move X then resolve, then Y then resolve - classic):

| Step | Rough 65C02 cycles |
|------|---------------------|
| Update position by velocity (8.8 or pixel) | 20-40 |
| Hitbox -> tile range (tx0..tx1, ty0..ty1) | 40-80 |
| Loop 2x2 or 2x3 solid probes (`LDA` shadow) | 80-150 |
| If hit, snap position / clear velocity along axis | 30-60 |
| **Total / sprite / axis** | ~**200-350** |
| **Both axes** | ~**400-700** |

| Active bodies | Cycles / frame | Fraction of 133k |
|---------------|----------------|------------------|
| 8 | ~5k | ~4% |
| 16 | ~10k | ~8% |
| 32 | ~20k | ~15% |
| 64 (all OAM) | ~40k | ~30% |

So: **16 moving actors** is easy. **64** is possible but wasteful - most games should flag **COLLIDE** on a subset (player, enemies, haulables), not empty OAM slots.

Metasprites: collide **one AABB** (or 2 sensors) per logical entity, not every hardware OAM piece.

### Recommended algorithm (SDK default)

**1. Solid shadow (build once per slot load)**

- Packed bits: 16x15 = **30 bytes**/screen, or 32 bytes aligned.
- Camera workbench: maintain solid for slots 0-3 (or blit into one **17x16** viewport map when scroll changes by a tile).
- Query: `solid(tx,ty)` = bit test, ~15-25 cycles in ASM.

**2. Separate axis resolution**

```text
x += vx
resolve_x(sprite)    ; probe tiles overlapping BB; if solid, snap X out, vx=0
y += vy
resolve_y(sprite)    ; same for Y
```

Avoids corner glitches and halves the "what normal do I use?" problem.

**3. Probes, not full rasterization**

For an 8x16 player, often enough:

- **X move:** mid-left and mid-right points (or BB left/right edges at 2 Y samples)
- **Y move:** bottom-left and bottom-right (floor), one head point (ceiling)

That is **2-3 tile lookups** per axis, not every cell under the box. Full BB tile loop is fine too at this CPU speed; sensors are for polish and speed.

**4. Only entities that moved (or are "dynamic")**

Skip `vx==0 && vy==0` unless something else can push them. Skip `ANIM`-only scenery.

**5. Correct / restrict movement**

| Response | Use |
|----------|-----|
| Snap to tile edge | Floor landing, wall slide |
| Clear axis velocity | Stop walking into wall |
| Optional 1px eject loop | Unstick if overlap from spawn (rare; cap iterations at 2) |

Do **not** binary-search the whole move each frame unless speeds are huge (several tiles/frame). At typical 1-2 px/frame, snap-after-move is enough.

### When it gets expensive (and fixes)

| Situation | Fix |
|-----------|-----|
| 64 OAM slots all flagged collidable | Flag only real actors; 8-16 max |
| Huge metasprite = 8 hardware sprites each with own BB | One entity BB |
| Sweeping teleport speeds | Substep move (2 half-steps) still cheap |
| Pixel-perfect vs BG art | Deferred; tile solid is v0 |
| Collision in world space across MAP | Shadow covers **loaded slots** only; on camera shift, rebuild/blit solid with the slot stream |

### Camera / 2x2 workbench

Visible view is **128x120**. When scroll straddles rooms, solid tests use **camera-local** coordinates mapped into the right slot's shadow (or one stitched buffer updated on tile-boundary scroll). Stitch cost: on slot reload you already copy ~30-60 bytes of bits - noise compared to MAP tile stream.

### ASM vs C

| Piece | Lang |
|-------|------|
| `solid_at` / resolve axis inner loop | **ASM** |
| Entity list update, flags | C OK |
| Build shadow from attrs | C or ASM once per load |

Expose to games:

```c
void retr01_phys_move_xy(Retr01Body *b);  /* uses solid_at, separates axes */
int  retr01_solid_at(unsigned char px, unsigned char py); /* pixel -> tile */
```

### Bottom line

With `SOLID` -> RAM bits, **axis-separated** moves, and **~16** active bodies, collision+response is roughly **under 10%** of a frame at 8 MHz - comfortable beside anim steps and game logic. The failure mode is algorithmic (testing everything against everything, or VRAM reads), not the CPU being too slow.

---

## C SDK / ASM library sketch

Not implemented yet. Intended as optional, cart-linkable pieces in the **dev kit** (cc65 C + `.s` stubs). Games may call these or replace them with custom code that still reads/writes the **same attr bits**.

Names illustrative.

### Suggested modules

| Module | Role |
|--------|------|
| `retr01_bg_attr.h` | Bit masks / pack-unpack for the attr byte |
| `retr01_bg_load` | MAP -> VRAM slot + attr fill + bank bits |
| `retr01_bg_collide` | Build/query 16x15 (or camera-sized) solid shadow |
| `retr01_bg_anim` | Living-cell list + NMI step (4-frame phase) |
| `retr01_bg_vram` | Tear-safe nametable/attr poke (VBlank / beam-Y gate) |

### Attr helpers (C)

```c
#define BG_ATTR_PAL_MASK    0x03
#define BG_ATTR_FLIP_H      0x04
#define BG_ATTR_FLIP_V      0x08
#define BG_ATTR_BANK_MASK   0x30
#define BG_ATTR_BANK_SHIFT  4
#define BG_ATTR_SOLID       0x40
#define BG_ATTR_ANIM        0x80

static inline unsigned char bg_attr_pack(
    unsigned char pal, int flip_h, int flip_v,
    unsigned char bank, int solid, int anim)
{
    return (pal & 3)
        | (flip_h ? BG_ATTR_FLIP_H : 0)
        | (flip_v ? BG_ATTR_FLIP_V : 0)
        | ((bank & 3) << BG_ATTR_BANK_SHIFT)
        | (solid ? BG_ATTR_SOLID : 0)
        | (anim ? BG_ATTR_ANIM : 0);
}
```

ASM: same masks as `.equ` / `.define` for hand-written loaders.

### Collision API (sketch)

```c
/* Packed solid bits: 16x15 -> 30 bytes/slot (or 32 aligned). Camera may stitch slots. */
void retr01_collide_from_attrs(const unsigned char *attrs_240, unsigned char *solid_bits_out);
int  retr01_solid_at_tile(unsigned char tx, unsigned char ty);
int  retr01_solid_at_pixel(unsigned char px, unsigned char py);

typedef struct {
    signed char x, y;       /* or 8.8 fixed in a fuller SDK */
    signed char vx, vy;
    unsigned char w, h;     /* hitbox */
    unsigned char flags;    /* COLLIDE, ON_GROUND, ... */
} Retr01Body;

/* Axis-separated move + snap. ASM hot path recommended. */
void retr01_phys_move_xy(Retr01Body *b);
```

ASM: `solid_at_tile` = index bitfield + `BIT`/`AND`; `phys_move_xy` calls resolve_x/resolve_y with 2-3 probes each.

### Anim API (sketch)

```c
#define RETR01_ANIM_MAX 32

typedef struct {
    unsigned int vram_addr;  /* tile byte address in VRAM */
    unsigned char base;      /* B, 4-aligned */
} Retr01AnimCell;

void retr01_anim_build(const unsigned char *tiles, const unsigned char *attrs,
                       unsigned int vram_tile_base, Retr01AnimCell *out, unsigned char *count);
void retr01_anim_step(Retr01AnimCell *cells, unsigned char count, unsigned char phase);
/* phase = (nmi_frame >> 1) & 3;  commit via tear-safe VRAM poke (VBlank / beam gate) */
```

ASM NMI piece (concept):

```text
; Prefer running anim_step at NMI start (VBlank). If poking mid-frame, gate on beam Y vs cell ty.
; X = cell index, phase in TEMP
; load base, ORA phase, STA via VRAM_DATA with addr preset/restored
```

Keep the inner loop in ASM even if build stays in C.

### Bank API (sketch)

```c
void retr01_bg_attrs_set_bank(unsigned char *attrs_240, unsigned char bank);
void retr01_bg_load_screen(unsigned char slot, const void *map_payload);
/* load_screen: tiles + attrs; BANK bits already correct in payload, or stamped here */
```

Hardware always reads `BANK` from attr. Games rarely write `$FE31`-`$FE36` except as optional bulk stamp tools.

### What the lib should not do

- Full physics engine (only solid queries).
- Arbitrary-length anim strips (v0 = **4 frames** only).
- Mid-frame VRAM spam outside VBlank/NMI budget.
- Nametable/attr pokes that tear a tile under the beam (kit must offer a safe commit path).

### Budget guidance (8 MHz)

| Task | Guidance |
|------|----------|
| `anim_step` 32 cells | Comfortable every NMI |
| `anim_step` 64 cells | OK if phase updates every 2 frames |
| Full 240-tile attr scan every frame | Avoid; build list at load |
| Collision query | RAM only; no `$FE12` |

---

## Studio / MAP export

- Attr mode paints palette, flips, solid, anim, bank.
- **Generate bank** / packer: when `ANIM` set, require four unique frames packed at `B..B+3` and rewrite nametable to `B`.
- Export **240+240** bytes per screen (replace old 64-byte 2x2 attr plane).
- Preview: cycle living tiles at 4 frames in the editor.

---

## Migration note

`02` / `01` / `03` / `05` / `07` now state **240+240** attrs and **per-tile** BG bank. Studio Phase 2 (`04`) still needs attr-mode UI + export aligned with this byte layout.

---

## Decisions locked by this doc

| Item | Choice |
|------|--------|
| High nibble | `BANK` + `SOLID` + `ANIM` |
| Anim frames | **4**, consecutive indices `B..B+3`, `B` 4-aligned |
| Collision | `SOLID` bit + **RAM shadow** |
| Bank | 2-bit **per 8x8 tile** (attr); not per screen / not live slot latch |
| Dev support | Optional **C + ASM** kit for load / collide / anim / bank stamp; custom engines OK if attrs stay valid |

## Still open

| ID | Question |
|----|----------|
| A3 | `RETR01_ANIM_MAX` default 32 vs 64? |
| A4 | Anim rate: fixed global shift, or per-game constant only? |
| A5 | When to freeze Studio Phase 2 attr UI (after BG fetch island proves flip+bank timing) |
