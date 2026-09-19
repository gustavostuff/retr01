<img src="img/v_01.png" alt="Logo V1" />

The Retr01 project is an MCU-assisted 8-bit system ready for arcade and console setups. It is also an Emulator and a Studio.

NOTE: hardware is still in design phase, software is being built on that design.

## Inspirations

1. NES (tile and sprite feel, 2bpp, NES-style color limits).
2. SNES (true parallax via two BG planes).
3. [GameTank](https://gametank.zone/) (similar screen resolution target).

## Design goals

- Put most of the video work in hardware so the CPU can focus on gameplay, inputs, physics, and state updates instead of doing low-level tile and sprite work by hand.
- Keep the main PCB compact: roughly 17 ICs total, using a hybrid design instead of a pure FPGA or a pure discrete-logic console.
- Use a middle ground between custom logic and MCU helpers: a main CPU, a few AVR support chips, PLDs, and a small amount of 74xx glue logic.
- Give the CPU a flat 32 KB program space with no banking. At 8 MHz and with 32 KB RAM, the system can do more than a classic NES-style NROM cartridge while staying simple to author.
- Support a predictable entity model with clear caps: up to 16 entity types per world, each with up to 4 states, 4 animation frames, and 6 sprite slots per state.
- Keep world layouts flexible: up to 7 worlds, each with up to 32 screens, arranged on a sparse 16x16 grid so large maps do not need dense, wasteful allocation.
- Keep the cartridge passive: no mapper or bank switching. Nametable and map data can stream directly from cart memory into VRAM buffers, which keeps the bus simpler and leaves program space free for actual game logic.

## Software pieces

**Retr01 Emu** runs `.retr01` ROM (cartridge) images:

<img src="img/readme/emu.png" alt="Retr01 Emu play window" />

**Retr01 Studio** is the authoring app for worlds, screens, and entities. It embeds the emulator in-editor for immediate playtest.

<img src="img/readme/studio.png" alt="Retr01 Studio" />

Maria is a player entity used to test Emu and Studio, this might become a full game later:

<img src="img/readme/maria/idle.gif" alt="Maria idle" />
<img src="img/readme/maria/running.gif" alt="Maria running" />
<img src="img/readme/maria/crouching.png" alt="Maria crouching" />
<img src="img/readme/maria/jumping.png" alt="Maria jumping" />

## Doc map

| Doc | Focus |
| --- | --- |
| [selling-points.md](general_docs/selling-points.md) | Bells and whistles (shared PCB, dual sync, entities, flasher, more) |
| [hardware.md](general_docs/hardware.md) | Main board, 3x AVR, PLDs, BOM, I/O, PCB practices |
| [ic-comms-risks.md](general_docs/ic-comms-risks.md) | Shared-bus / multi-clock risks, mitigations, play-path anti-patterns |
| [video-graphics.md](general_docs/video-graphics.md) | Resolution, tiles, sprites, palettes, BG layers |
| [palette/](general_docs/palette/README.md) | Locked 64-color kit RGB, GIMP/Aseprite exports |
| [world-scrolling.md](general_docs/world-scrolling.md) | Worlds, screens, VRAM buffers, scroll behavior |
| [cartridge.md](general_docs/cartridge.md) | Cart hardware, saves, flashing |
| [memory.md](general_docs/memory.md) | CPU map, soft `$7Fxx`, MAP port, `.retr01` image, entity flash capacity |
| [software-api.md](general_docs/software-api.md) | What an entity is, def size, C/ASM API, game modes |
| [sound.md](general_docs/sound.md) | APU soft window, MCU-S2 PWM |
| [open-questions.md](general_docs/open-questions.md) | TBD items and how to resolve them |
| [ic_behavior/](ic_behavior/README.md) | Per-chip behavior (all 19 BOM ICs + pad / optional) |
| [bringup/](bringup/README.md) | Late-phase hardware bring-up tiers A-H (video lab -> full console) |
| [apps/README.md](apps/README.md) | Studio + Emu apps |
