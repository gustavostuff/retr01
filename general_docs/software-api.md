# Software API and game modes

C (and later ASM) facing tools for authors. The first ship set stays small.

## What is an entity?

Hardware alone thinks in sprites. Authors think in **entities**.

An entity is a being, object, or graphic in the game. It is composed of:

- Up to **4 states** (idle, running, and so on).
- Each state: up to **4 frames**, plus one hitbox (compose-space AABB) shared by those frames.
- Each frame: up to **6 sprites** (8x8), with positions relative to each other, a frame delay, a draw origin, flips, and so on.

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
| Hardware sprites (OAM) | **64** total, **16** per scanline |
| Global shared entity catalog | **None** |
| Player / inventory patterns | Global other **SPR** banks (4). See `memory.md` |
| Other-screen patterns | Global other CHR (**4** BG + **4** SPR). See `memory.md` |

**Types vs on-screen:** The **16** cap is how many *kinds* of entity a world may define (cart catalog). It is **not** a limit on how many entities may be visible at once. Live instances may fill the view **as long as their current frames' sprites fit in the 64 OAM slots**. Example: sixty-four 1-sprite pickups, or ten 6-sprite characters, both fine. The next spawn that would exceed free OAM fails. Scanline overflow (more than **16** sprites on one line) still drops later entries for that line.

Types belong to one world. Same look in another world means another def (and tiles) in that world's blob. Sprite attr bank bits index **this world's** SPR banks for normal entities. If the wrong world CHR is active, those entities look wrong on purpose. That glitch is the tell.

The marked **player** entity is a normal world catalog type (SoT: world **0**). Its part bank bits **0..3** index the cart **global other SPR** banks, not world SPR. No private player bank. Other screens use the same global other CHR block (BG + SPR).

### Packed definition format (locked)

Little-endian. Offsets are **byte offsets from the start of the block that owns them** (`0` = unused slot).

```text
EntityDef (variable length, max 532 B when fully populated)
+0   u8  flags
+1   u8  state_count          (1..4)
+2   u8  default_state        (0..state_count-1)
+3   u8  reserved0
+4   u16 state_off[4]         // offset from EntityDef base, 0 = absent
     ... State blocks ...

State (at EntityDef + state_off[s])
+0   u8  frame_count          (1..4)
+1   u8  reserved1
+2   u16 frame_off[4]         // offset from this State base, 0 = absent
     ... Frame blocks ...

Frame (at State + frame_off[f])
+0   u8  delay                (display duration in frames, min 1)
+1   u8  sprite_count         (1..6)
+2   u8  hitbox_x             // state AABB minus the state's first drawable-frame origin
+3   u8  hitbox_y
+4   u8  hitbox_w
+5   u8  hitbox_h
+6   Sprite sprites[6]        // only first sprite_count are live
     Sprite = { u8 tile, i8 rel_x, i8 rel_y, u8 attr }  // 4 B each
```

**Lookup state S, frame F** (what hardware helpers / PRG use when advancing an entity):

1. `base` = entity def address (MAP or RAM copy).
2. `soff = u16(base + 4 + 2*S)`. If `soff == 0` or `S >= state_count`, invalid.
3. `state = base + soff`.
4. `foff = u16(state + 2 + 2*F)`. If `foff == 0` or `F >= frame_count`, invalid.
5. `frame = state + foff`. Read `delay`, `sprite_count`, state hitbox (packed vs first drawable-frame origin), then `sprites[0..sprite_count)`.
6. For each sprite, resolve CHR from the **current world's** SPR bank (attr bits 0-1) + tile.

| Piece | Max bytes |
| --- | ---: |
| EntityDef header | 12 |
| One State header | 10 |
| One Frame (6 sprites) | 30 |
| **Fully maxed def** (4x4x6) | **532** |
| 16 maxed defs (one world) | **8512** (~8.3 KB) |
| 7 worlds x 16 maxed defs | **59584** (~58.2 KB) |

**Entity spawn locations** live in **PRG** (data tables and/or code that calls `spawn_entity`), not in the world blob. Cart holds defs in the **per-world** catalog only.

Phase 1 Studio carts also embed a compact **instance table** in PRG (see `memory.md`). That table feeds Host Play / emu until authors switch to full `spawn_entity` tables.

**Catalog on cart:** at world `OFF_TYPES`, a **`u16` directory** (`type_count` entries, offset from catalog base, little-endian) then concatenated **EntityDef** blobs (this locked pack). `OFF_INSTS` points past the catalog (PA start when present). Studio authors **hitbox on the state** and **draw origin on the frame**. Export writes sprite `rel_*` in **that frame's draw-origin** space (authoring origin baked in). Packed frame hitbox is `state.hitbox - first_drawable_frame.origin` (same bytes on every frame of the state, clamped unsigned). Moving a later frame's draw origin does not change collision.

Optional **`PA`** (player anim) hangs off the world blob after the catalog. Host Play reads it for the marked player. Each drawable frame stores authoring-space origin, the **state** hitbox (compose space), then parts (`tile`, `attr`, `dx`, `dy`). Pose uses the current frame origin. Collision uses the current state's hitbox origin-relative to that state's **first drawable frame**, not the current anim frame.

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
| `set_entity_hitbox` | Overrides or sets the AABB on that state (compose space in authoring, packed origin-relative to the state's first drawable frame) |
| `do_entities_collide` | AABB test using each entity's **current state** hitbox (origin-relative via that state's first drawable frame) |

### Runtime sprite / entity pressure (locked)

| Cap | Value | Meaning |
| --- | ---: | --- |
| Catalog | **16** types / world | How many defs may exist in that world's blob |
| OAM | **64** sprites | How many 8x8 sprites may be drawn at once (all entities + any other OAM users share this) |
| Per scanline | **16** sprites | Later OAM entries on that line are not drawn |

There is **no** separate "max entities on screen" hard cap. On-screen count is whatever fits the **64** sprite budget for the frames currently claimed. Spawn / pose changes that need more OAM than free fail (no partial claim).

| Situation | v1 behavior |
| --- | --- |
| `spawn_entity` needs more OAM than free | Fail (no partial spawn) |
| State/frame change needs more OAM than free | Fail, keep previous frame |
| More than 16 sprites on one scanline | Draw first 16 in OAM order. Drop the rest for that line |

## Game mechanics (initial)

- **Player movement** and **camera movement** are separate. See `world-scrolling.md` (dead zone, axis lock, follow vs auto).
- Camera: instant screen switch and/or smooth scrolling. Both allowed in one game or world.
- **BG0 layout wrap**: `r01_bg0_set_wrap(ctx, wrap_x, wrap_y)` in author `custom_logic.c`. Studio packs non-zero axes into world header flags byte **7** bits **1**/**2**. Parallax rate stays end-aligned. Host Play / emu only modulo-tiles samples on those axes so empty BG0 regions do not appear. See `world-scrolling.md`.
- **BG0 clip to BG1**: `r01_bg0_set_clip_to_bg1(ctx, enable)` packs into flags byte **7** bit **3**. When enabled, BG0 is hidden outside present BG1 camera slots (backdrop there). Default off: BG0 fills the full viewport under missing/out-of-window BG1. Independent of wrap. See `world-scrolling.md`.
- **BG1** (and manual strip) autoscroll / wrap helpers remain TBD. See `world-scrolling.md`.
- Modes: **platformer** and **top-down**.

### Platformer physics (v1, locked)

| Feature | v1 |
| --- | --- |
| Movement | Axis-separated (resolve X then Y, or the reverse, consistently) |
| Solids | BG tiles with attr **bit 6** set |
| Colliders | Entity AABB hitboxes (per state). Vs BG solids: every overlapping 8x8 tile is tested (not corners only) |
| Gravity / jump | Simple constant gravity + jump impulse (PRG tunes numbers). Gravity units are **1/16** px per frame^2. Release while rising uses 3x gravity (short hop) |
| Meter | Pixels per meter (default **16**). Gravity, jump, walk, and fall cap scale as `n * meter / 16` |
| Slopes | **No** |
| Moving platforms | **No** |
| One-way platforms | **No** |

Top-down mode skips gravity and uses the same solid / AABB rules.

### Platformer (Host Play)

Default mode is **top-down**. Platformer is opt-in from author `custom_logic.c`. Studio packs the choice into the cart. Host Play / emu reads it.

```c
r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER);
r01_platformer_set_gravity(ctx, R01_PLAT_GRAVITY_DEFAULT); /* 4 = 4/16 px/frame^2 at meter 16, clamp 1..255 */
r01_platformer_set_jump(ctx, R01_PLAT_JUMP_DEFAULT);       /* 4 px impulse at meter 16, clamp 1..32 */
r01_platformer_set_meter(ctx, R01_PLAT_METER_DEFAULT);     /* 16 px per meter, clamp 1..64 */
```

| Input | Platformer |
| --- | --- |
| Left / Right | Walk 1 px per frame at meter 16 (scaled), then resolve X |
| Face **Y** | Jump while grounded (edge). Hold for full height. Release while rising cuts the hop (3x gravity). Keyboard P1 is **H** (G is face X) |
| Down | Crouch if `r01_player_anim_set_crouch_state` maps a state in `custom_logic.c`. Grounded only. No walk while crouched |
| Up | Unused in v1 |

Vertical motion is `vel_y` plus gravity, capped at **4** px/frame down at meter 16. Gravity author units are **1/16** px per frame^2 so hang time can be slower than 1 px/frame^2. Default gravity **4** and jump **4** peak in about **16** frames at about **34** px. Walk stays **1** px per frame at meter 16. Y is applied 1 px at a time so a jump cannot skip through an 8x8 solid. Landing (Y+1 blocked) zeros `vel_y` and sets grounded. A ceiling hit zeros `vel_y`. Walk anim uses horizontal delta only. Down selects crouch while grounded. Airborne uses the jump state when mapped.

Player entity states are mapped in author `custom_logic.c`. With no mapping, Host Play draws **state 0 frame 0** and only auto X-flips for left/right facing.

```c
r01_player_anim_set_idle_state(ctx, 0);
r01_player_anim_set_walk_all(ctx, 1);
r01_player_anim_set_crouch_state(ctx, 2);
r01_player_anim_set_jump_state(ctx, 3);
```

Packing: world header flags byte **7** bit **4** = platformer. Gravity, jump, and meter are u8 at PRG `$80F7` / `$80F8` / `$80F9`. **0** (and `R01_PLAT_*_DEFAULT` in `custom_logic.c`) means Host Play uses `apps/common/r01_play_physics.h`. Crouch / idle / walk / jump state indices are `$80FA` / `$80FB` / `$80FC` / `$80FD` (**$FF** = unmapped). A numeric argument is packed as-is. See `memory.md`.

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
| Cart save `$7F22`-`$7F24` | Explicit save only. May span **many VBlanks**. Chunk I2C with **short** `CPU_RDY` pulses, then release so PRG can animate a spinner / saving UI. RDY stays released across the full EEPROM write |
| Machine EE `$7F70`-`$7F72` | Separate from cart saves. Same RDY rule if the cycle cannot close in one PHI2 |

**STP** stays unused in normal play. **WAI** is only for a clear NMI/IRQ wake.

**Performance anti-patterns** (RDY-as-default, full OAM every frame, saves in the hot path, and so on): see **Performance: what not to do** in `ic-comms-risks.md`.
