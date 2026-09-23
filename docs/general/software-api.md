# Software API and game modes

C (and later ASM) facing tools for authors. The first ship set stays small.

:warning: **Ctrl+E** writes `data/` blobs, compiles `game_logic.c` with llvm-mos into the 32 KB PRG, and packs `.retr01`. Studio Play and `./scripts/emu.sh` boot that image. `game_logic.c` is created once and is the author source file.

## What is an entity?

Hardware alone thinks in sprites. Authors think in **entities**.

An entity is a being, object, or graphic in the game. It is composed of:

- Up to **4 states** (idle, running, and so on).
- Each state: up to **8 frames**, plus one hitbox (compose-space AABB) shared by those frames.
- Each frame: up to **6 sprites** (8x8), with positions relative to each other, a frame delay, a draw origin, flips, and so on.

That range covers very simple games (**1** state, **1** frame, **1** sprite) and richer ones. Complex bosses can be several entities working together in software (for example left leg, head, right leg, eyes).

### Definitions vs behavior

| Piece | Where it lives | Who writes it |
| --- | --- | --- |
| **Entity definition** (states, frames, sprites, relative positions, delays, hitboxes) | Cart **global entity catalog** | Studio packs into the cart catalog |
| **Entity behavior** (AI, input, physics, state changes, spawn rules) | **PRG** | Author in **C and/or ASM** |
| **Entity spawn locations** (placements) | **PRG** | Author tables / code (`spawn_entity`) |
| **Live instance state** (position, velocity, current state/frame, flags) | System RAM | PRG via the entity API |
| **Player anim (`PA`)** (marked-player draw/collision dump for Host Play) | Cart, one blob after world-0 maps | Studio packs from the marked player type |

### Hard caps (Studio-friendly)

| Scope | Cap |
| --- | --- |
| Entity **types** (global catalog) | **32** |
| Entities **on screen** (live instances) | Soft: limited by **OAM sprite budget**, not by type count |
| Hardware sprites (OAM) | **64** total, **16** per scanline |
| Global shared entity catalog | **Yes** (one pool for the cart) |
| Player / inventory patterns | Global **SPR** banks (16). See `memory.md` |
| Other-screen patterns | Global CHR (**16** BG + **16** SPR). See `memory.md` |

**Types vs on-screen:** The **32** cap is how many *kinds* of entity the cart may define. It is **not** a limit on how many entities may be visible at once. Live instances may fill the view **as long as their current frames' sprites fit in the 64 OAM slots**. Example: sixty-four 1-sprite pickups, or ten 6-sprite characters, both fine. The next spawn that would exceed free OAM fails. Scanline overflow (more than **16** sprites on one line) still drops later entries for that line.

Types are cart-global. The same look in another world is the same def. Sprite attr bank bits **0-3** index **global SPR** banks **0-15**. A wrong bank index shows the wrong tiles.

The marked **player** entity is a normal catalog type. Its part bank bits **0-3** index the same global SPR banks as every other entity. Other screens use the same global CHR block (BG + SPR).

### Instances and player anim (`PA`)

An **instance** is one placed copy of a catalog type. The catalog says what a slime looks like. An instance is the slime sitting at world (80, 40) facing left. Studio placements export as spawn rows in PRG. While the game runs, each live copy also has a RAM record (position, velocity, current state and frame). Pixel patterns and frame lists stay in the catalog. The instance only names a type and a pose.

**`PA`** (player anim) is a Host Play helper blob, magic `'P' 'A'`. It is a flat dump of the **marked player** type's drawable frames (origin, hitbox, sprite parts) so play can animate and collide without walking the full `EntityDef` pack every frame. One blob per cart. Other entities do not get a `PA`.

Byte packs: spawn / live / `PA` sections below. Addresses: `memory.md`.

### Packed definition format (locked)

Little-endian. Offsets are **byte offsets from the start of the block that owns them** (`0` = unused slot).

```text
EntityDef (variable length, max 1044 B when fully populated)
+0   u8  flags
+1   u8  state_count          (1..4)
+2   u8  default_state        (0..state_count-1)
+3   u8  reserved0
+4   u16 state_off[4]         // offset from EntityDef base, 0 = absent
     ... State blocks ...

State (at EntityDef + state_off[s])
+0   u8  frame_count          (1..8)
+1   u8  reserved1
+2   u16 frame_off[8]         // offset from this State base, 0 = absent
     ... Frame blocks ...

Frame (at State + frame_off[f])
+0   u8  delay                (display duration in frames, min 1)
+1   u8  sprite_count         (1..6)
+2   i8  hitbox_x             // state AABB minus the state's first drawable-frame origin
+3   i8  hitbox_y
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
6. For each sprite, resolve CHR from the **global** SPR bank (attr bits 0-3) + tile.

| Piece | Max bytes |
| --- | ---: |
| EntityDef header | 12 |
| One State header (`frame_off[8]`) | 18 |
| One Frame (6 sprites) | 30 |
| **Fully maxed def** (4x8x6) | **1044** |
| 32 maxed defs (cart catalog) | **33408** (~32.6 KB) |

**Entity spawn locations** live in **PRG** (data tables and/or code that calls `spawn_entity`). Cart holds defs in the **global** catalog.

Phase 1 Studio carts embed a compact **instance table** in PRG (see `memory.md`). That table feeds Host Play / emu.

**Catalog on cart:** a **`u16` directory** (`type_count` entries, offset from catalog base, little-endian) then concatenated **EntityDef** blobs (this locked pack). Directory at 32 types is **64 B**. Studio authors **hitbox on the state** and **draw origin on the frame**. Export writes sprite `rel_*` in **that frame's draw-origin** space (authoring origin baked in). Packed frame hitbox is `state.hitbox - first_drawable_frame.origin` as **i8** (same bytes on every frame of the state). A centered 8x8 box on origin 4,4 packs as **-4,-4**. Moving a later frame's draw origin does not change collision.

### Spawn instance (PRG, locked)

A placement on the map. **6 B** at `$81C1` (count at `$81C0`). Cap **64**. See `memory.md`.

### Live instance (system RAM, locked)

The running copy of a spawn (or of `spawn_entity`). `EntityId` is an index into this table. Slots are RAM. OAM claims sit beside it.

```text
LiveInstance (12 B)
+0   u8  type_id
+1   u8  flags          // bit0 alive, bit1 flip H, bit2 flip V
+2   u8  state          // 0..3
+3   u8  frame          // 0..7
+4   i16 x
+6   i16 y
+8   i16 vx
+10  i16 vy
```

**64** slots = **768 B**. Fits in system RAM. Not cart flash.

Packed spawn instances (not the marked player) copy into a 16-slot RAM table at boot. Author `game_logic.c` uses `r01_entity_count`, `r01_entity_type`, `r01_entity_get_pos`, `r01_entity_set_pos`, `r01_entity_state`, and `r01_entity_set_state`. Catalog hitbox for type `t` is `r01_ent_hx/hy/hw/hh[t]` (origin-relative). `r01_world_aabb_ok(x, y, w, h)` tests that box against BG1 solids. Draw uses catalog state 0 frame 0; a non-zero live state uses that sprite's tile plus one.

### Player anim blob (`PA`, locked)

Host Play's packed player frames. One blob **per cart** (after world-0 maps). World header flags bit **0** marks it present. Host Play reads it for the marked player.

```text
PA (variable length, max 1031 B at 4 states x 8 frames x 6 parts)
+0   u8  'P'
+1   u8  'A'
+2   u8  state_count     (1..4)
     ... State blocks in order ...

State
+0   u8  drawable_count  (drawable frames only)
     ... Frame blocks ...

Frame (8 B header + 4 B x part_count)
+0   u8  origin_x        // authoring-space draw origin
+1   u8  origin_y
+2   u8  hitbox_x        // state AABB, compose space (same bytes on every frame)
+3   u8  hitbox_y
+4   u8  hitbox_w
+5   u8  hitbox_h
+6   u8  part_count      (1..6)
+7   u8  delay           (min 1)
+8   Part parts[part_count]
     Part = { u8 tile, u8 attr, i8 dx, i8 dy }
```

Max fill: **3** + **4** x (**1** + **8** x (**8** + **24**)) = **1031 B**. Pose uses the current frame origin. Collision uses the current state's hitbox origin-relative to that state's **first drawable frame**, not the current anim frame.

### Camera helpers (locked intent)

Default dead zone **32x30** pixels inside the 128x120 view. `r01_camera_set_deadzone` in `game_logic.c` sets the live box on the 6502. Play follow snaps live box edges to the same parity as the viewport center so a 2 px hold-X run from a centered snap does not take a 1 px camera hitch. Live size may be 1 px smaller than packed and not pixel-centered. Follow uses the player and dead zone only. Empty BG1 slots and the present-screen bounding box do not stop the camera. Details and the 31x69 exact-match example are in `world-scrolling.md`. **0,0** (or `r01_camera_disable_deadzone`) turns the dead zone off for 1:1 camera track. Axis lock may be **both**, **H only**, or **V only**.

### Starter API (locked signatures)

Types are illustrative C. `EntityId` is a small handle into the live instance table. Returns `0` on success, non-zero on error (OAM full, bad id, and so on).

```c
/* type_id: 0..31 index into the global entity catalog */
int  spawn_entity(u8 type_id, u8 screen_cell, i16 x, i16 y, EntityId *out_id);

int  despawn_entity(EntityId id);

int  move_entity(EntityId id, i16 x, i16 y);           /* absolute draw origin */
int  change_entity_velocity(EntityId id, i16 vx, i16 vy);

int  set_entity_state(EntityId id, u8 state);         /* 0..3, must exist in def */
int  set_entity_frame(EntityId id, u8 frame);         /* 0..7 within current state */
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
| `spawn_entity` | Allocates a live instance from the global catalog, sets pose, claims OAM for current frame. Fails if OAM cannot fit or `type_id` is absent |
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
| Catalog | **32** types (global) | How many defs may exist on the cart |
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
- **BG0 layout wrap**: `r01_bg0_set_wrap(ctx, wrap_x, wrap_y)` in author `game_logic.c`. Studio packs non-zero axes into world header flags byte **7** bits **1**/**2**. Host Play / emu modulo-tiles samples on those axes and uses period rate `bg0_n / bg1_n` (not end-aligned `(n-1)/(n-1)`). See `world-scrolling.md`.
- **BG0 clip to BG1**: `r01_bg0_set_clip_to_bg1(ctx, enable)` packs into flags byte **7** bit **3**. When enabled, BG0 is hidden outside present BG1 camera slots (backdrop there). Default off: BG0 fills the full viewport under missing/out-of-window BG1. Independent of wrap. See `world-scrolling.md`.
- **BG1** (and manual strip) autoscroll / wrap helpers remain TBD. See `world-scrolling.md`.
- Modes: **platformer** and **top-down**.

### Platformer physics (v1, locked)

| Feature | v1 |
| --- | --- |
| Movement | Axis-separated (resolve X then Y, or the reverse, consistently) |
| Solids | BG1 cells whose bank index and tile index match a marked pattern. Palette and H/V flip are ignored. A grid slot with no present BG1 screen has no tiles and blocks motion (ledge / world edge). BG0 show-through is decoration. Author code marks patterns with `r01_solid_pattern_add(ctx, bank, tile)` in `game_logic.c`. Studio Set Solid stores the same list as `solid_patterns` in the project JSON. Export packs that JSON list at PRG `$8700`. Boot copies the list into system RAM (`$0200`). See `memory.md` |
| Colliders | Entity AABB hitboxes (per state). Vs BG solids: every overlapping 8x8 tile is tested (not corners only) |
| Gravity / jump | Simple constant gravity + jump impulse (PRG tunes numbers). Gravity units are **1/16** px per frame^2. Release while rising uses 3x gravity (short hop) |
| Meter | Pixels per meter (default **16**). Gravity, jump, walk, and fall cap scale as `n * meter / 16` |
| Slopes | **No** |
| Moving platforms | **No** |
| One-way platforms | **No** |

Top-down mode skips gravity and uses the same solid / AABB rules.

### Platformer (Play)

Default mode is **top-down**. Platformer is opt-in from author `game_logic.c`. Those calls run on the 6502. Play is the emu of the packed PRG.

```c
r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER);
r01_platformer_set_gravity(ctx, R01_PLAT_GRAVITY_DEFAULT); /* 4 = 4/16 px/frame^2 at meter 16, clamp 1..255 */
r01_platformer_set_jump(ctx, R01_PLAT_JUMP_DEFAULT);       /* 4 px impulse at meter 16, clamp 1..32 */
r01_platformer_set_meter(ctx, R01_PLAT_METER_DEFAULT);     /* 16 px per meter, clamp 1..64 */
r01_solid_pattern_add(ctx, 0, 1);                          /* bank 0 tile 1 is solid, pal/flip ignored */
```

Pad bits in `R01_PAD_*` match `$7F60`. `r01_pad_down(ctx, mask)` is true while any of those bits are held. `r01_player_moving_x(ctx)` is Left or Right. Each tick the PRG resets move mul to **1** and live anim delay to **0**, then `r01_game_on_tick` may override. `r01_player_set_move_mul` is **1..8** (walk px per frame at meter 16). `r01_player_anim_set_frame_delay` is **0** (authored delay) or **1..255** live ticks.

```c
void r01_game_on_tick(R01GameCtx *ctx) {
    if (r01_pad_down(ctx, R01_PAD_X) && r01_player_moving_x(ctx)) {
        r01_player_set_move_mul(ctx, 2);
        r01_player_anim_set_frame_delay(ctx, 3);
    }
}
```

Those hooks compile into the cart PRG. Play is the emu of that ROM.

| Input | Platformer |
| --- | --- |
| Left / Right | Walk 1 px per frame at meter 16 (scaled), then resolve X. Author tick may raise that with `r01_player_set_move_mul` |
| Face **X** | Author tick may read `r01_pad_down(ctx, R01_PAD_X)` (keyboard P1 is **G**, gamepad east / west) |
| Face **Y** | Jump while grounded (edge). Hold for full height. Release while rising cuts the hop (3x gravity). Keyboard P1 is **H** (G is face X). Gamepad south / north is Y, east / west is X |
| Down | Crouch if `r01_player_anim_set_crouch_state` maps a state in `game_logic.c`. Grounded only. No walk while crouched |
| Up | Unused in v1 |

Vertical motion is `vel_y` plus gravity, capped at **4** px/frame down at meter 16. Gravity author units are **1/16** px per frame^2 so hang time can be slower than 1 px/frame^2. Default gravity **4** and jump **4** peak in about **16** frames at about **34** px. Walk is **1** px per frame at meter 16. `r01_player_set_move_mul(ctx, 2)` in `r01_game_on_tick` makes that **2** px per frame. `r01_player_anim_set_frame_delay` overrides the current state's frame delay for that tick. Y is applied 1 px at a time so a jump cannot skip through an 8x8 solid. Landing (Y+1 blocked) zeros `vel_y` and sets grounded. A ceiling hit zeros `vel_y`. Walk anim uses horizontal delta only. Down selects crouch while grounded. Airborne uses the jump state when mapped.

Player entity states are mapped in author `game_logic.c`. With no mapping, Play draws **state 0 frame 0** and only X-flips for left/right facing.

```c
r01_player_anim_set_idle_state(ctx, 0);
r01_player_anim_set_walk_all(ctx, 1);
r01_player_anim_set_crouch_state(ctx, 2);
r01_player_anim_set_jump_state(ctx, 3);
```

Packing: world header flags byte **7** bit **4** = platformer. Gravity, jump, and meter are u8 at PRG `$80F7` / `$80F8` / `$80F9`. **0** (and `R01_PLAT_*_DEFAULT` in `game_logic.c`) means Host Play uses `apps/common/r01_play_physics.h`. Crouch / idle / walk / jump state indices are `$80FA` / `$80FB` / `$80FC` / `$80FD` (**$FF** = unmapped). A numeric argument is packed as-is. See `memory.md`.

`r01_solid_pattern_add(ctx, bank, tile)` in author `game_logic.c` is the author API for solid patterns. Studio Set Solid stores `solid_patterns` in the project JSON. Export packs that list at PRG `$8700`. Palette and H/V flip are ignored. The PRG probes BG1 nametable bank+tile against the packed tables.

BGM tracks live in the Studio Audio tab and pack into the cart **BGM** region (bytecode plus Guitar / EGuitar / Piano / Flute ids per channel). `r01_bgm_play(ctx, N)` in author `game_logic.c` selects that 1-based track. Pack sets `$80FE`. **0** means no autoplay. Play and `./scripts/emu.sh` start that packed stream, including the wavetable ids. See `sound.md`.

## Ownership

| Piece | Owner |
| --- | --- |
| Entity **definitions** | Cart pack, global catalog (MAP-readable) |
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

PRG builds with llvm-mos `-mcpu=mosw65c02`. NMI in `nmi.s` saves A, X, Y, and the imaginary register block `$E0-$FF` (`__rc0`..`__rc31`) before `r01_nmi`. The tracker consumes one stream opcode per NMI. `r01_game_on_vblank` stays short.

**Performance anti-patterns** (RDY-as-default, full OAM every frame, saves in the hot path, and so on): see **Performance: what not to do** in `ic-comms-risks.md`.
