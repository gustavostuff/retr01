# Retr01

The Retr01 is an MCU-assisted 8-bit system, ready for both arcade and console setups.

## Inspirations

1. NES (tile and sprite feel, 2bpp, NES-style color limits).
2. SNES (true parallax via two BG planes).
3. [GameTank](https://gametank.zone/) (similar screen resolution target).

## Design goals

- Hardware does most video work so PRG can stay on game logic.
- Relatively low IC count of ~17 on main PCB.
- Multi-chip middle ground: not an FPGA soft system, not pure TTL. CPU + helper AVRs + PLDs + a little 74xx glue.
- Flat 32 KB PRG with no banking; faster CPU (8 MHz) and larger RAM (32 KB) make it stretch farther than NES NROM.
- Entity system with clear caps: up to 16 types per world, each with up to 4 states × 4 frames × 4 sprites.
- Flexible world layout: up to 8 worlds, each with up to 32 BG1 screens, 0-8 BG0 screens on a sparse 16×16 grid.
- Passive cartridge: no mapper, hardware streaming of MAP/nametable data from cart into VRAM buffers.

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
| [ic_behavior/](ic_behavior/README.md) | Per-chip behavior (all 19 BOM ICs + pad / optional) |
