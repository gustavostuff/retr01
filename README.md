<img src="img/v_01.png" alt="Logo V1" />

The Retr01 project is an MCU-assisted 8-bit system ready for arcade and console setups. It is also an Emulator and a Studio.

NOTE: hardware is still in design phase, software is being built on that design.

## Inspirations

1. NES: Colors are limited to a 64 global source palette and 25 (max) on screen at a time.
2. SNES: True parallax via two BG planes. BG0 behind, and BG1 on top, with pixel level transparency.
3. [GameTank](https://gametank.zone/): Adopting a very similar resolution.

## Graphics

The picture is small and sharp: a **128x120** playfield with chunky pixels, NES-style color limits (**64** colors in the kit, about **25** on screen), and two real background layers so far things can scroll slower than near things. Sprites sit on top. Games mix many tilesets on the same screen without a mapper.

Characters and objects are **entities** (a few poses, each made of small sprites) rather than a pile of loose tiles. Hardware draws the layers and sprites so game code can stay on play. Details: [video-graphics.md](docs/general/video-graphics.md).

## Audio

Music and sound effects share **8** voices. Five stay on the soundtrack (melody, harmony, bass, noise, sampled hits). Three stay on effects, so a jump or shot does not mute the song.

The game program writes notes. A helper chip mixes them to analog out. Details: [sound.md](docs/general/sound.md).

## Hardware

The plan is one compact through-hole board to cover a home console and an arcade cabinet. Same PCB. Populate pad jacks for console pads, arcade buttons, or both.

A **6502** runs the game. A few helper chips and small glue logic own video, pads, saves, and mix. Output is RGB plus composite. The cartridge is a simple memory pack (program, tiles, and save), not a mapper board. Details: [hardware.md](docs/general/hardware.md).

## Software pieces

**Retr01 Emu** runs `.retr01` ROM (cartridge) images:

<img src="img/readme/emu.png" alt="Retr01 Emu play window" />

<img src="img/readme/emu-debug.png" alt="Retr01 Emu debug window" />

**Retr01 Studio** is the authoring app for worlds, screens, and entities. It embeds the emulator in-editor for immediate playtest.

<img src="img/readme/studio.png" alt="Retr01 Studio" />

<img src="img/readme/studio-audio.png" alt="Retr01 Studio Audio" />

## Doc map

- [selling-points.md](docs/general/selling-points.md): The broader product and design vision: the shared-PCB approach, dual-sync output, entity model, flash support, and extra features that make the platform feel like a full console instead of a bare tech demo.
- [hardware.md](docs/general/hardware.md): The main hardware reference: board layout, the three AVR support chips, PLD logic, BOM choices, I/O plumbing, and PCB decisions that keep the system compact and practical.
- [ic-comms-risks.md](docs/general/ic-comms-risks.md): Shared-bus and multi-clock communication risks: where failures are likely, and which mitigations or anti-patterns to avoid.
- [video-graphics.md](docs/general/video-graphics.md): The graphics system: resolution, tile layout, sprites, palettes, background layers, and how much rendering work sits in hardware rather than on the CPU.
- [palette/](docs/general/palette/README.md): The fixed 64-color palette kit, plus export guidance for GIMP and Aseprite so art stays consistent with the console color and indexing constraints.
- [world-scrolling.md](docs/general/world-scrolling.md): World and screen structure, VRAM buffering, sparse map organization, and scroll behavior over large map areas without wasteful dense allocation.
- [cartridge.md](docs/general/cartridge.md): The cartridge hardware story: passive memory model, save support, flash workflows, and a simple cart layout that keeps the bus uncomplicated.
- [memory.md](docs/general/memory.md): The memory map: CPU address layout, soft `$7Fxx` windows, MAP port behavior, `.retr01` image format, and entity flash capacity limits.
- [software-api.md](docs/general/software-api.md): The runtime model for entities, data structure sizes, the C/ASM API, and the game modes the software is expected to support.
- [sound.md](docs/general/sound.md): The audio architecture: 8-channel software mix on MCU-S2, the `$7F40` register window, 6502 NMI tracker bytecode, and PWM out.
- [open-questions.md](docs/general/open-questions.md): Unknowns and open design decisions, with notes on what still needs validation and how each remaining question gets resolved.
- [ic_behavior/](docs/ic_behavior/README.md): A per-chip description for each part in the BOM: behavior, role, optional variants, and key caveats.
- [bringup/](docs/bringup/README.md): The late hardware bring-up roadmap, ordered by tiers A-H from video lab validation through a full console-style demo with controllers and audio.
- [apps/README.md](apps/README.md): The app layer: Studio authoring, Emu runtime, and the discrete-IC board sim.
