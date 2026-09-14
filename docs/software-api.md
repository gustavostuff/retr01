# Software API and game modes

C (and later ASM) facing tools for authors. Keep the first ship set small.

## What is an entity?

Hardware alone thinks in sprites. Authors should think in **entities**.

An entity is a being, object, or graphic in the game. It is composed of:

- Up to **4 states** (idle, running, and so on).
- Each state: up to **4 frames**.
- Each frame: up to **4 sprites** (8x8), with positions relative to each other, frame delays, and related metadata (hitbox per state, flips, and so on).

That range covers very simple games (**1** state, **1** frame, **1** sprite) and richer ones. Complex bosses can be several entities working together in software (for example left leg, head, right leg, eyes).

### Definitions vs behavior

| Piece | Where it lives | Who writes it |
| --- | --- | --- |
| **Entity definition** (states, frames, sprites, relative positions, delays, hitboxes) | Cart **global entity catalog** | Studio packs into `.retr01` |
| **Entity behavior** (AI, input, physics, state changes, spawn rules) | **PRG** | Author in **C and/or ASM** |
| **Entity spawn locations** (placements) | **PRG** | Author tables / code (`spawn_entity`) |
| **Live instance state** (position, velocity, current state/frame, flags) | System RAM | PRG via the entity API |

### Hard caps (Studio-friendly)

| Scope | Cap |
| --- | --- |
| Entity types (global catalog / cart) | **128** |
| Per-world type list | **None** (any world may use any catalog id) |

Why global: share types across worlds (world 1 places A/B/C, world 2 places C/D/E) without duplicating defs.

### Packed definition format (locked)

Little-endian. Offsets are **byte offsets from the start of the block that owns them** (`0` = unused slot).

```text
EntityDef (variable length, max 356 B when fully populated)
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
+1   u8  sprite_count         (1..4)
+2   Sprite sprites[4]        // only first sprite_count are live
     Sprite = { u8 tile, i8 rel_x, i8 rel_y, u8 attr }  // 4 B each
```

**Lookup state S, frame F** (what hardware helpers / PRG use when advancing an entity):

1. `base` = entity def address (MAP or RAM copy).
2. `soff = u16(base + 4 + 2*S)`. If `soff == 0` or `S >= state_count`, invalid.
3. `state = base + soff`.
4. `foff = u16(state + 6 + 2*F)`. If `foff == 0` or `F >= frame_count`, invalid.
5. `frame = state + foff`. Read `delay`, `sprite_count`, then `sprites[0..sprite_count)`.

| Piece | Max bytes |
| --- | ---: |
| EntityDef header | 12 |
| One State header | 14 |
| One Frame (4 sprites) | 18 |
| **Fully maxed def** (4x4x4) | **356** |
| 128 maxed defs (global catalog) | **45568** (~44.5 KB) |

**Entity spawn locations** live in **PRG** (data tables and/or code that calls `spawn_entity`), not in the cart world blob. Cart holds defs in the global catalog only. Optional `PA` (player anim) may still hang off a world blob as an opaque blob for now.

### Starter API (locked signatures)

Types are illustrative C. `EntityId` is a small handle into the live instance table. Returns `0` on success, non-zero on error (OAM full, bad id, and so on).

```c
/* catalog_id: 0..127 index into the global entity catalog */
int  spawn_entity(u8 catalog_id, u8 screen_cell, i16 x, i16 y, EntityId *out_id);

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
| `spawn_entity` | Allocates a live instance, copies def header refs, sets pose, claims OAM for current frame. Fails if OAM cannot fit |
| `despawn_entity` | Frees instance and OAM slots |
| `move_entity` / `change_entity_velocity` | Updates RAM. Drawing uses origin + sprite rel offsets |
| `set_entity_state` / `set_entity_frame` | Resolves pack offsets (see above) and rebuilds OAM for that frame. Fails if OAM short |
| `advance_entity_anim` | Uses current `Frame.delay` as the tick period |
| `rotate_entity` / `flip_entity` | Transforms the whole metasprite (90deg steps / mirror) |
| `set_entity_hitbox` | Overrides or sets the per-state AABB used by collisions |
| `do_entities_collide` | AABB test using each entity's **current state** hitbox |

### Runtime sprite / entity pressure (locked)

Hardware caps: **64** OAM entries, **16** sprites per scanline. Catalog cap: **128** global types.

| Situation | v1 behavior |
| --- | --- |
| `spawn_entity` needs more OAM than free | Fail (no partial spawn) |
| State/frame change needs more OAM than free | Fail, keep previous frame |
| More than 16 sprites on one scanline | Draw first 16 in OAM order. Drop the rest for that line |

## Game mechanics (initial)

- Camera: instant screen switch and/or smooth scrolling. Both allowed in one game or world.
- Modes: **platformer** and **top-down**.

### Platformer physics (v1, locked)

| Feature | v1 |
| --- | --- |
| Movement | Axis-separated (resolve X then Y, or the reverse, consistently) |
| Solids | BG tiles with attr **bit 6** set |
| Colliders | Entity AABB hitboxes (per state) |
| Gravity / jump | Simple constant gravity + jump impulse (PRG tunes numbers) |
| Slopes | **No** |
| Moving platforms | **No** |
| One-way platforms | **No** |

Top-down mode skips gravity and uses the same solid / AABB rules.

## Ownership

| Piece | Owner |
| --- | --- |
| Entity **definitions** | Cart pack (MAP-readable) |
| Entity **behavior** | PRG on the 6502 (C/ASM) |
| Live instance state | System RAM |
| Drawing | OAM `$7F20`/`$7F21` on MCU-M, SPI to MCU-S1 (**VBlank** field fill) |

See `hardware.md` for the M / S1 / S2 split.
