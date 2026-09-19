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
- Keep the cartridge passive: no mapper or bank switching. Nametable and map data can stream directly from cart memory into VRAM buffers, which keeps the bus simpler and leaves program space free for [...]

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

- [selling-points.md](general_docs/selling-points.md) — The broader product and design vision: explains the shared-PCB approach, dual-sync output, entity model, flash support, and the extra features that make the platform feel like a full console instead of a bare tech demo.
- [hardware.md](general_docs/hardware.md) — The main hardware reference: covers the board layout, the three AVR support chips, PLD logic, BOM choices, I/O plumbing, and PCB design decisions that keep the system compact and practical.
- [ic-comms-risks.md](general_docs/ic-comms-risks.md) — A design-safety note on shared-bus and multi-clock communication risks, showing where failures are likely and which mitigations or anti-patterns to avoid in the actual implementation.
- [video-graphics.md](general_docs/video-graphics.md) — The graphics system guide: resolution, tile layout, sprites, palettes, background layers, and how much of the rendering work is pushed into hardware rather than the CPU.
- [palette/](general_docs/palette/README.md) — The fixed 64-color palette kit, plus export guidance for GIMP and Aseprite so art stays consistent with the console's color and indexing constraints.
- [world-scrolling.md](general_docs/world-scrolling.md) — Explains world and screen structure, VRAM buffering, sparse map organization, and scroll behavior over large map areas without wasteful dense allocation.
- [cartridge.md](general_docs/cartridge.md) — Describes the cartridge hardware story: passive memory model, save support, flash workflows, and how a simple cart layout keeps the bus uncomplicated.
- [memory.md](general_docs/memory.md) — The memory map reference: CPU address layout, soft `$7Fxx` windows, MAP port behavior, `.retr01` image format, and entity flash capacity limits.
- [software-api.md](general_docs/software-api.md) — Defines the runtime model for entities, the data structure sizes, the C/ASM API, and the game modes the software is expected to support.
- [sound.md](general_docs/sound.md) — Covers the audio architecture: the APU soft window and the MCU-S2 PWM path used to generate the final sound output.
- [open-questions.md](general_docs/open-questions.md) — Tracks the unknowns and open design decisions, with notes on what still needs validation and how to resolve each remaining question.
- [ic_behavior/](ic_behavior/README.md) — A per-chip description for each part in the BOM, including behavior, role, optional variants, and key caveats for every IC in the design.
- [bringup/](bringup/README.md) — The late hardware bring-up roadmap, ordered by tiers A-H from video lab validation through to a full console-style demo with controllers and audio.
- [apps/README.md](apps/README.md) — Documents the app layer of the project, especially the Studio authoring workflow and the Emu runtime used to test content in practice.
