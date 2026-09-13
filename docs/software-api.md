# Software API and game modes

C (and later ASM) facing tools for authors. Keep the first ship set small.

## Entities

Hardware alone thinks in sprites. Authors should think in **entities**.

An entity is a being or object made of:

- Up to **4 states** (idle, running, and so on).
- Each state: up to **4 frames**.
- Each frame: up to **4 sprites**.

That covers tiny games (1 state, 1 frame, 1 sprite) and richer ones. Big bosses can be several entities glued in software (legs, head, eyes, and so on).

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

## Game mechanics (initial)

- Camera: instant screen switch and/or smooth scrolling. Both allowed in one game or world.
- Modes: **platformer** and **top-down**.
- Platformer physics stay simple. More power than an NES, still not a physics sandbox.

## Ownership

Entity **logic and instance state** live in system RAM / PRG on the 6502. Drawing goes through OAM (`$7F20`/`$7F21` on MCU-M, SPI to MCU-S1 for the sprite field). See `hardware.md` for the M / S1 / S2 split.
