# Retr01

The Retr01 is an MCU-assisted 8-bit system. ready for both arcade and console setups.

## Inspirations

1. NES (tile and sprite feel, 2bpp, NES-style color limits).
2. SNES (true parallax via two BG planes).
3. [GameTank](https://gametank.zone/) (similar screen resolution target).

## Design goals

- Hardware does most video work so PRG can stay on game logic.
- Hardware scroll and MAP/nametable streaming from the cart.
- Start small on software features. Grow the API later without locking the silicon early where we can avoid it.

## Doc map

| Doc | Focus |
| --- | --- |
| [selling-points.md](selling-points.md) | Bells and whistles (shared PCB, dual sync, entities, flasher, more) |
| [hardware.md](hardware.md) | Main board, 3x AVR, PLDs, BOM, I/O, PCB practices |
| [video-graphics.md](video-graphics.md) | Resolution, tiles, sprites, palettes, BG layers |
| [world-scrolling.md](world-scrolling.md) | Worlds, screens, VRAM buffers, scroll behavior |
| [cartridge.md](cartridge.md) | Cart hardware, saves, flashing |
| [memory.md](memory.md) | CPU map, MAP port, `.retr01` image, entity flash capacity |
| [software-api.md](software-api.md) | What an entity is, def size, C/ASM API, game modes |
| [open-questions.md](open-questions.md) | TBD items and how to resolve them |
| [ic_behavior/](ic_behavior/README.md) | Per-chip behavior (CPU, AVRs, more as added) |
