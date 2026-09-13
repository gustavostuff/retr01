# Software API and game modes

C (and later ASM) facing tools for authors. Keep the first ship set small.

## What is an entity?

Hardware alone thinks in sprites. Authors should think in **entities**.

An entity is a being, object, or graphic in the game. It is composed of:

- Up to **4 states** (idle, running, and so on).
- Each state: up to **4 frames**.
- Each frame: up to **4 sprites** (8x8), with positions relative to each other, frame delays, and related metadata (hitbox per state, flips, and so on).

That range covers very simple games (**1** state, **1** frame, **1** sprite) and richer ones. It is meant to leave room for creativity. Complex bosses can be several entities working together in software (for example left leg, head, right leg, eyes).

### Definitions vs behavior

| Piece | Where it lives | Who writes it |
| --- | --- | --- |
| **Entity definition** (states, frames, sprites, relative positions, delays, hitboxes, and so on) | **World scope** on the cart (inside the world blob) | Studio / data tools pack it into the `.retr01` image |
| **Entity behavior** (AI, input, physics reactions, when to change state, spawn rules, and so on) | **PRG** (the flat 32 KB code region) | The author, in **C and/or ASM** |
| **Live instance state** (position, velocity, current state/frame, flags) | System RAM at runtime | PRG, via the entity API |

Definitions are data. Behavior is code. The C/ASM API should manipulate an entity as a whole, not as loose sprites. Console hardware still draws sprites. MCU-S1 fills the sprite field from OAM. See Ownership below.

### Maxed entity definition size

Worst case for one **type** record (all 4 states, 4 frames, 4 sprites used). Fixed full slots, no sparse compression:

| Piece | Bytes | Notes |
| --- | ---: | --- |
| One sprite | 4 | tile, rel_x, rel_y, attr (bank/pal/flip) |
| One frame | 17 | 1 delay + 4 sprites |
| One state | 72 | hitbox x,y,w,h (4) + 4 frames |
| One entity def | **288** | 4 states, no extra header |
| + small header | **290** | optional flags / default state (2 B) |

Content floor if sprites drop attr to a shared frame byte and you only store raw unique fields: about **224 B** (64 x (tile,x,y) + 16 delays + 4 hitboxes). Prefer budgeting with **288 B** so tools can keep a simple fixed layout.

| Budget | Maxed defs that fit |
| --- | ---: |
| **1 KB** (1024 B) | **3** at 288 B (864 B used, 160 B left) |
| 1 KB at 290 B | **3** (870 B used) |
| 1 KB at 224 B lean floor | **4** (896 B used) |

That is the **definition** catalog only. Spawn **instances** (type id, screen, x, y, and so on) are separate and much smaller.

### How many entity defs fit on a cart

Canonical flash numbers live in `memory.md` (entity catalog capacity). Short version:

- **No hard cart limit** on entity type count.
- Signal: **more than 100** distinct entities on a full cart, with headroom.
- Per-world sprite CHR without tile reuse: about **16** fully maxed unique-tile entities. Reuse tiles to define many more types from the same banks.

### Starter API (illustrative)

Aim for whole-entity ops, not raw sprite poking:

1. `spawn_entity()`
2. `despawn_entity()`
3. `move_entity()`
4. `change_entity_velocity()`
5. `rotate_entity()` (90 degree turns only)
6. `flip_entity()` (whole entity as one graphic)
7. `set_entity_draw_origin()`
8. `set_entity_hitbox()` (likely one hitbox per entity state, not per frame)
9. `do_entities_collide()`

More helpers can wait. Prefer a solid core over a huge day-one API.

### Runtime sprite / entity pressure (locked)

Hardware caps: **64** OAM entries, **16** sprites per scanline.

| Situation | v1 behavior |
| --- | --- |
| `spawn_entity()` would need more OAM slots than free for its **current** frame | Spawn **fails** (API returns an error / no-op). No partial spawn |
| A frame change would need more slots than free | Keep the previous frame drawn, or refuse the state/frame change (API error). Do not corrupt OAM |
| More than 16 sprites on one scanline | Draw the first 16 in OAM order for that line. Drop the rest for that line only |

There is no separate hard “max entities” counter beyond OAM. Tiny 1-sprite entities can pack denser than maxed 4-sprite ones. Authors should budget OAM in PRG.

## Game mechanics (initial)

- Camera: instant screen switch and/or smooth scrolling. Both allowed in one game or world.
- Modes: **platformer** and **top-down**.

### Platformer physics (v1, locked)

| Feature | v1 |
| --- | --- |
| Movement | Axis-separated (resolve X then Y, or the reverse, consistently) |
| Solids | BG tiles with attr **bit 6** set |
| Colliders | Entity AABB hitboxes (per state). `do_entities_collide()` for entity-entity |
| Gravity / jump | Simple constant gravity + jump impulse (PRG tunes the numbers) |
| Slopes | **No** |
| Moving platforms | **No** |
| One-way platforms | **No** (can add later if a demo needs them) |

Top-down mode skips gravity and uses the same solid / AABB rules on the playfield.

## Ownership

| Piece | Owner |
| --- | --- |
| Entity **definitions** | World blob on cart (MAP-readable data) |
| Entity **behavior** | PRG on the 6502, written in C/ASM |
| Live instance state | System RAM |
| Drawing | OAM `$7F20`/`$7F21` on MCU-M, SPI to MCU-S1 for the sprite field |

See `hardware.md` for the M / S1 / S2 split.
