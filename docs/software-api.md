# Software API and game modes

C (and later ASM) facing tools for authors. Entity work is intended to run on one of the AVRs. Keep the first ship set small.

## Entities

Hardware alone thinks in sprites. Authors should think in **entities**.

An entity is a being or object made of:

- Up to **4 states** (idle, running, and so on).
- Each state: up to **4 frames**.
- Each frame: up to **4 sprites**.

That covers tiny games (1 state, 1 frame, 1 sprite) and richer ones. Big bosses can be several entities glued in software (legs, head, eyes, and so on).

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
