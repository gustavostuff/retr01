<img src="img/v_01.png" alt="Logo V1" />

Retr01 is an arcade and console ready, MCU-assisted 8-bit system, plus a host toolchain for editing and playtesting carts.

## Inspirations

1. NES (tile and sprite feel, 2bpp, NES-style color limits).
2. SNES (true parallax via two BG planes).
3. [GameTank](https://gametank.zone/) (similar screen resolution target).

## Design goals

- Hardware does most video work so PRG can stay on game logic.
- Relatively low IC count of ~17 on main PCB.
- Multi-chip middle ground: not an FPGA soft system, not pure TTL. CPU + helper AVRs + PLDs + a little 74xx glue.
- Flat 32 KB PRG with no banking. A faster CPU (8 MHz) and larger RAM (32 KB) make it stretch farther than NES NROM.
- Entity system with clear caps: up to 16 types per world, each with up to 4 states x 4 frames x 4 sprites.
- Flexible world layout: up to 8 worlds, each with up to 32 screens, all on a sparse 16x16 grid.
- Passive cartridge: no mapper, hardware streaming of MAP/nametable data from cart into VRAM buffers.

## Software pieces

**Retr01 Emu** runs `.retr01` carts on the host (play view + a debug window for VRAM atlases, world map, pals, and CPU budget).

<img src="img/readme/emu.png" alt="Retr01 Emu play window" />

<img src="img/readme/emu-debug.png" alt="Retr01 Emu debug window" />

**Retr01 Studio** is the authoring app. It embeds the emulator for in-editor Play, and covers worlds, tiles, entities, audio, and export.

<img src="img/readme/studio.png" alt="Retr01 Studio" />

## Doc map

| Doc | Focus |
| --- | --- |
| [selling-points.md](docs/selling-points.md) | Bells and whistles (shared PCB, dual sync, entities, flasher, more) |
| [hardware.md](docs/hardware.md) | Main board, 3x AVR, PLDs, BOM, I/O, PCB practices |
| [ic-comms-risks.md](docs/ic-comms-risks.md) | Shared-bus / multi-clock risks, mitigations, play-path anti-patterns |
| [video-graphics.md](docs/video-graphics.md) | Resolution, tiles, sprites, palettes, BG layers |
| [world-scrolling.md](docs/world-scrolling.md) | Worlds, screens, VRAM buffers, scroll behavior |
| [cartridge.md](docs/cartridge.md) | Cart hardware, saves, flashing |
| [memory.md](docs/memory.md) | CPU map, soft `$7Fxx`, MAP port, `.retr01` image, entity flash capacity |
| [software-api.md](docs/software-api.md) | What an entity is, def size, C/ASM API, game modes |
| [sound.md](docs/sound.md) | APU soft window, MCU-S2 PWM |
| [open-questions.md](docs/open-questions.md) | TBD items and how to resolve them |
| [ic_behavior/](docs/ic_behavior/README.md) | Per-chip behavior (all 19 BOM ICs + pad / optional) |
| [apps/README.md](apps/README.md) | Studio + Emu apps |
