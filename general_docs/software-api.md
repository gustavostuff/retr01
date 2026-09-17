# Software API and game modes

C (and later ASM) facing tools for authors. Keep the first ship set small.

## What is an entity?

Hardware alone thinks in sprites. Authors should think in **entities**.

An entity is a being, object, or graphic in the game. It is composed of:

- Up to **4 states** (idle, running, and so on).
- Each state: up to **4 frames**.
- Each frame: up to **6 sprites** (8x8), with positions relative to each other, frame delays, and related metadata (hitbox per state, flips, and so on).

That range covers very simple games (**1** state, **1** frame, **1** sprite) and richer ones. Complex bosses can be several entities working together in software (for example left leg, head, right leg, eyes).

### Definitions vs behavior

| Piece | Where it lives | Who writes it |
| --- | --- | --- |
| **Entity definition** (states, frames, sprites, relative positions, delays, hitboxes) | Cart **per-world entity catalog** | Studio packs into that world blob |
| **Entity behavior** (AI, input, physics, state changes, spawn rules) | **PRG** | Author in **C and/or ASM** |
| **Entity spawn locations** (placements) | **PRG** | Author tables / code (`spawn_entity`) |
| **Live instance state** (position, velocity, current state/frame, flags) | System RAM | PRG via the entity API |

### Hard caps (Studio-friendly)

| Scope | Cap |
| --- | --- |
| Entity **types** per world (catalog) | **16** |
| Entities **on screen** (live instances) | Soft: limited by **OAM sprite budget**, not by type count |
| Hardware sprites (OAM) | **64** total; **16** per scanline |
| Global shared entity catalog | **None** |
| Player item patterns (global bank) | **256** tiles (player inventory icons only). Pack TBD. See `memory.md` |

**Types vs on-screen:** The **16** cap is how many *kinds* of entity a world may define (cart catalog). It is **not** a limit on how many entities may be visible at once. Live instances may fill the view **as long as their current frames' sprites fit in the 64 OAM slots**. Example: sixty-four 1-sprite pickups, or ten 6-sprite characters, both fine. The next spawn that would exceed free OAM fails. Scanline overflow (more than **16** sprites on one line) still drops later entries for that line.

Types belong to one world. Same look in another world means another def (and tiles) in that world's blob. Sprite attr bank bits index **this world's** SPR banks. If the wrong world CHR is active, entities look wrong on purpose. That glitch is the tell.

The **player item bank** is separate: one cart-global **256-tile** pattern bank. Studio moves the marked player entity's SPR patterns there (and restores them on unmark). It is not an entity-type pool and is not shared with world catalogs.

### Packed definition format (locked)

Little-endian. Offsets are **byte offsets from the start of the block that owns them** (`0` = unused slot).

```text
EntityDef (variable length, max 484 B when fully populated)
+0   u8  flags
+1   u8  state_count          (1..4)
+2   u8  default_state        (0..state_count-1)
+3   u8  reserved0
+4   u16 state_off[4]         // offset from EntityDef base, 0 = absent
     ... State blocks ...

State (at EntityDef + state_off[s])
+0   u8  frame_count          (1..4)
+1   u8  reserved1
+2   u8  hitbox_x
+3   u8  hitbox_y
+4   u8  hitbox_w
+5   u8  hitbox_h
+6   u16 frame_off[4]         // offset from this State base, 0 = absent
     ... Frame blocks ...

Frame (at State + frame_off[f])
+0   u8  delay                (display duration in frames, min 1)
+1   u8  sprite_count         (1..6)
+2   Sprite sprites[6]        // only first sprite_count are live
     Sprite = { u8 tile, i8 rel_x, i8 rel_y, u8 attr }  // 4 B each
```

**Lookup state S, frame F** (what hardware helpers / PRG use when advancing an entity):

1. `base` = entity def address (MAP or RAM copy).
2. `soff = u16(base + 4 + 2*S)`. If `soff == 0` or `S >= state_count`, invalid.
3. `state = base + soff`.
4. `foff = u16(state + 6 + 2*F)`. If `foff == 0` or `F >= frame_count`, invalid.
5. `frame = state + foff`. Read `delay`, `sprite_count`, then `sprites[0..sprite_count)`.
6. For each sprite, resolve CHR from the **current world's** SPR bank (attr bits 0-1) + tile.

| Piece | Max bytes |
| --- | ---: |
| EntityDef header | 12 |
| One State header | 14 |
| One Frame (6 sprites) | 26 |
| **Fully maxed def** (4x4x6) | **484** |
| 16 maxed defs (one world) | **7744** (~7.6 KB) |
| 8 worlds x 16 maxed defs | **61952** (~60.5 KB) |

**Entity spawn locations** live in **PRG** (data tables and/or code that calls `spawn_entity`), not in the world blob. Cart holds defs in the **per-world** catalog only. Optional `PA` (player anim) may still hang off a world blob as an opaque blob for now.

Phase 1 Studio carts also embed a compact **instance table** in PRG (see `memory.md`). That table feeds Host Play / emu until authors switch to full `spawn_entity` tables.

**Catalog on cart:** at world `OFF_TYPES`, a **`u16` directory** (`type_count` entries, offset from catalog base, little-endian) then concatenated **EntityDef** blobs (this locked pack). `OFF_INSTS` points past the catalog (PA start when present). Studio packs hitbox and sprite `rel_*` in **draw-origin** space (authoring origin baked in at export).

### Camera helpers (locked intent)

Default dead zone **32x30** pixels inside the 128x120 view when world header bytes 30-31 are non-zero. **0,0** (or `r01_camera_disable_deadzone`) turns the dead zone off for 1:1 camera track. Axis lock may be **both**, **H only**, or **V only**. See `world-scrolling.md`. Studio packs dead-zone size from `r01_camera_set_deadzone` / `r01_camera_disable_deadzone` in author `custom_logic.c` at export time.

### Starter API (locked signatures)

Types are illustrative C. `EntityId` is a small handle into the live instance table. Returns `0` on success, non-zero on error (OAM full, bad id, and so on).

```c
/* type_id: 0..15 index into the *current world's* entity catalog */
int  spawn_entity(u8 type_id, u8 screen_cell, i16 x, i16 y, EntityId *out_id);

int  despawn_entity(EntityId id);

int  move_entity(EntityId id, i16 x, i16 y);           /* absolute draw origin */
int  change_entity_velocity(EntityId id, i16 vx, i16 vy);

int  set_entity_state(EntityId id, u8 state);         /* 0..3, must exist in def */
int  set_entity_frame(EntityId id, u8 frame);         /* 0..3 within current state */
int  advance_entity_anim(EntityId id);                /* step frame using Frame.delay */

int  rotate_entity(EntityId id, u8 turns_cw);         /* 90deg units only, 0..3 */
int  flip_entity(EntityId id, u8 h, u8 v);            /* whole entity as one graphic */

int  set_entity_draw_origin(EntityId id, i16 ox, i16 oy);
int  set_entity_hitbox(EntityId id, u8 state, u8 x, u8 y, u8 w, u8 h);

int  do_entities_collide(EntityId a, EntityId b);     /* 1 = overlap, 0 = no, <0 = err */
```

Behavior:

| Call | Does |
| --- | --- |
| `spawn_entity` | Allocates a live instance from the current world's catalog, sets pose, claims OAM for current frame. Fails if OAM cannot fit or `type_id` is absent |
| `despawn_entity` | Frees instance and OAM slots |
| `move_entity` / `change_entity_velocity` | Updates RAM. Drawing uses origin + sprite rel offsets |
| `set_entity_state` / `set_entity_frame` | Resolves pack offsets (see above) and rebuilds OAM for that frame. Fails if OAM short |
| `advance_entity_anim` | Uses current `Frame.delay` as the tick period |
| `rotate_entity` / `flip_entity` | Transforms the whole metasprite (90deg steps / mirror) |
| `set_entity_hitbox` | Overrides or sets the per-state AABB used by collisions |
| `do_entities_collide` | AABB test using each entity's **current state** hitbox |

### Runtime sprite / entity pressure (locked)

| Cap | Value | Meaning |
| --- | ---: | --- |
| Catalog | **16** types / world | How many defs may exist in that world’s blob |
| OAM | **64** sprites | How many 8x8 sprites may be drawn at once (all entities + any other OAM users share this) |
| Per scanline | **16** sprites | Later OAM entries on that line are not drawn |

There is **no** separate “max entities on screen” hard cap. On-screen count is whatever fits the **64** sprite budget for the frames currently claimed. Spawn / pose changes that need more OAM than free fail (no partial claim).

| Situation | v1 behavior |
| --- | --- |
| `spawn_entity` needs more OAM than free | Fail (no partial spawn) |
| State/frame change needs more OAM than free | Fail, keep previous frame |
| More than 16 sprites on one scanline | Draw first 16 in OAM order. Drop the rest for that line |

## Game mechanics (initial)

- **Player movement** and **camera movement** are separate. See `world-scrolling.md` (dead zone, axis lock, follow vs auto).
- Camera: instant screen switch and/or smooth scrolling. Both allowed in one game or world.
- **BG0 layout wrap**: `r01_bg0_set_wrap(ctx, wrap_x, wrap_y)` in author `custom_logic.c`. Studio packs non-zero axes into world header flags byte **7** bits **1**/**2**. Parallax rate stays end-aligned; Host Play / emu only modulo-tiles samples on those axes so empty BG0 regions do not appear. See `world-scrolling.md`.
- **BG0 clip to BG1**: `r01_bg0_set_clip_to_bg1(ctx, enable)` packs into flags byte **7** bit **3**. When enabled, BG0 is hidden outside present BG1 camera slots (backdrop there). Default off: BG0 fills the full viewport under missing/out-of-window BG1. Independent of wrap. See `world-scrolling.md`.
- **BG1** (and manual strip) autoscroll / wrap helpers remain TBD. See `world-scrolling.md`.
- Modes: **platformer** and **top-down**.

### Platformer physics (v1, locked)

| Feature | v1 |
| --- | --- |
| Movement | Axis-separated (resolve X then Y, or the reverse, consistently) |
| Solids | BG tiles with attr **bit 6** set |
| Colliders | Entity AABB hitboxes (per state). Vs BG solids: every overlapping 8x8 tile is tested (not corners only) |
| Gravity / jump | Simple constant gravity + jump impulse (PRG tunes numbers) |
| Slopes | **No** |
| Moving platforms | **No** |
| One-way platforms | **No** |

Top-down mode skips gravity and uses the same solid / AABB rules.

## Ownership

| Piece | Owner |
| --- | --- |
| Entity **definitions** | Cart pack per world (MAP-readable) |
| Entity **behavior** | PRG on the 6502 (C/ASM) |
| Live instance state | System RAM |
| Drawing | OAM `$7F20`/`$7F21` on MCU-M, SPI to MCU-S1 (**early VBlank** / `S1_RDY`, then S1 field fill) |

See `hardware.md` for the M / S1 / S2 split. Shared-bus timing: `ic-comms-risks.md`.

## Timing conventions (locked)

| Work | When |
| --- | --- |
| Scroll `$7F02` / `$7F03`, palette `$7F08` / `$7F09` | NMI / VBlank (or video off) |
| OAM publish to S1 | Early VBlank or wait for `S1_RDY`. Never during HBlank |
| Cart save `$7F22`-`$7F24` | Explicit save only. May span **many VBlanks**. Chunk I2C with **short** `CPU_RDY` pulses, then release so PRG can animate a spinner / saving UI. Do not hold RDY for the whole EEPROM write |
| Machine EE `$7F70`-`$7F72` | Separate from cart saves. Same RDY rule if the cycle cannot close in one PHI2 |

Do not use **STP** in normal play. **WAI** only with a clear NMI/IRQ wake.

**Performance anti-patterns** (RDY-as-default, full OAM every frame, saves in the hot path, and so on): see **Performance: what not to do** in `ic-comms-risks.md`.
