<img src="img/v_01.png" alt="Logo V1" />

Retr01 is an MCU-assisted 8-bit game system built for arcade and console setups. It is also an emulator and a studio for making games and content.

Hardware is still in development, and the software is being built around that design.

## Inspirations

1. NES: A 64-color global palette with up to 25 colors visible on screen at once.
2. SNES: True parallax with two background planes and pixel-level transparency.
3. [GameTank](https://gametank.zone/): A similar screen resolution and compact feel.

## Graphics

The playfield is 128x120 with chunky pixels, NES-style color limits, and two real background layers. The result is a sharp image with a simple hardware pipeline.

Architecturali speaking, the graphics are based in worlds and screens: up to 7 worlds, 64 screens each (512 _TV screens_ of real state). That, plus the dual BG layer feature, can render visually rich and immersive experiences.

Characters and objects are entities made from states, frames and sprites (a _state_ being things like idle, running, crouching, etc). Also, hardware draws the background layers and sprites so PRG can focus mostly on game logic. More details in [video-graphics.md](docs/general/video-graphics.md).

## Audio

Retr01 uses 8 channels shared between music and effects. Five are reserved for the soundtrack and three for effects so a jump or shot does not mute the song. This was intentionaly designed to be work differently, compared to the NES.

The game program writes notes and a helper chip (and AVR128DB28) mixes them to analog output. See [sound.md](docs/general/sound.md).

## Hardware

The design uses one compact through-hole board for both home-console and arcade cabinet builds. The same PCB can be populated to be a console (by using 3.5mm connectors for game pads) or an arcade board (using male pin headers for microswitch-based sticks and buttons).

A 6502 runs the game logic, while a few helper chips and small glue logic handle video, inputs, saves, and audio mixing. Output is dual sync RGB (RGBS and RGBHV) and composite. The cartridge is a simple memory pack containing program data, tiles, and save data.

## Software pieces

Retr01 Emu runs .retr01 ROM images.

<img src="img/readme/emu.png" alt="Retr01 Emu play window" />

<img src="img/readme/emu-debug.png" alt="Retr01 Emu debug window" />

Retr01 Studio is the authoring tool for worlds, screens, and entities. It includes the emulator for quick playtesting.

<img src="img/readme/studio.png" alt="Retr01 Studio" />

<img src="img/readme/studio-audio.png" alt="Retr01 Studio Audio" />

## Doc map

- [selling-points.md](docs/general/selling-points.md): Product vision, shared PCB design, dual-sync output, entity model, flash support, and other system ideas.
- [hardware.md](docs/general/hardware.md): Main hardware reference for the board, support chips, PLD logic, BOM choices, I/O, and compact PCB layout.
- [ic-comms-risks.md](docs/general/ic-comms-risks.md): Shared-bus and multi-clock communication risks, likely failure points, and anti-patterns to avoid.
- [video-graphics.md](docs/general/video-graphics.md): Resolution, tile layout, sprites, palettes, background layers, and the hardware work behind the graphics pipeline.
- [palette/](docs/general/palette/README.md): The fixed 64-color palette kit and export guidance for GIMP and Aseprite.
- [world-scrolling.md](docs/general/world-scrolling.md): World and screen structure, VRAM buffering, sparse map organization, and scrolling over large areas.
- [cartridge.md](docs/general/cartridge.md): Cartridge hardware, passive memory model, save support, flash workflow, and the basic cart layout.
- [memory.md](docs/general/memory.md): CPU address map, soft $7Fxx windows, MAP port behavior, .retr01 image format, and flash capacity limits.
- [software-api.md](docs/general/software-api.md): Runtime model for entities, data structure sizing, C and ASM API, and expected game modes.
- [sound.md](docs/general/sound.md): Audio architecture, 8-channel software mix, $7F40 register window, 6502 NMI tracker bytecode, and PWM output.
- [open-questions.md](docs/general/open-questions.md): Remaining unknowns and design decisions that still need validation.
- [ic_behavior/](docs/ic_behavior/README.md): Per-chip behavior, role, optional variants, and key caveats.
- [bringup/](docs/bringup/README.md): Hardware bring-up roadmap from early lab validation to a console-style demo with controls and audio.
- [apps/README.md](apps/README.md): Studio authoring app, Emu runtime, and the discrete-IC board simulation app.
