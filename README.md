<img src="img/logo_v3.png" alt="Logo V3" />

The Retr01 project is a software + hardware tool chain family. Software includes:

- **Retr01 Studio**: visual authoring for worlds, screens and game entities. **Play** exports a cart and opens the shared emulator render screen (same pixels as standalone emu).
- **Retr01 Emu**: software-visible 65C02 + `$FExx` I/O + video. Standalone `./emu` and the library Studio Play uses. Loads a cart, runs PRG boot catchup and Host Play for movement, camera, and collision.
- **Retr01 Sim**: pin-level model of the **32-IC** Retr01-A netlist. Interactive SDL board UI with draggable islands, per-chip tests, and the same cart boot path as the emulator.

Software is still a WIP. This is the overall hardware roadmap:

- **Retr01-A (stage 1)**: Arcade motherboard, first build. THT, ~14 x 12 cm PCB, cabinet (micro switch) sticks and buttons, RGBS, S-Video and composite.
- **Retr01-C (stage 2)**: Home console. Same architecture and cart format, smaller board, 2 controller ports (3-cable line).
- **Retr01-H (stage 3)**: Handheld. SMD components, battery, LCD + driver, contact pads. Same software contract.

## At a glance

| Aspect | Description |
|--|--|
| CPU | W65C02S @ **8 MHz** |
| Playfield | **128 x 120** logical (**16 x 15** tiles), board **2x** to **256 x 240** RGBS |
| Art | **8 x 8** tiles, **2 bpp**, **64** master colors on-board Color PROM |
| Worlds | up to **8** worlds, **32** L1 screens each + optional **L0 / BG0** on a **512 KB** cart (**32 KB** PRG) |
| Scroll | **2 x 2** live nametable window (L1) + structured second BG (L0) with show-through |
| Sprites | **64** OAM entries, **16** per scanline |
| VRAM / RAM | **32 KB** interleaved VRAM + **32 KB** system RAM |

Same **32 KB PRG** as classic NES NROM games (Exitebike, Balloon Fight, Ice Climbers) but it buys far more game: **4.5x** more CPU cycles per frame at **8 MHz**, **32 KB** system RAM (not 2 KB). Scroll, sprite line fill and world map streaming are hardware jobs, so PRG stays game logic, not VBlank nametable tricks. See [`docs/selling_points.md`](docs/selling_points.md).

## Where to go next

- [`docs/graphics.md`](docs/graphics.md): VRAM, L0/L1, sprites, palettes, graphics ports
- [`docs/memory.md`](docs/memory.md): chips, cart layout, read/write timing
- [`docs/hardware.md`](docs/hardware.md): 32-IC BOM, PCB block diagram, bring-up
- [`docs/sound.md`](docs/sound.md): APU and bytecode
- [`docs/selling_points.md`](docs/selling_points.md): NES vs Retr01 comparison
- [`hw/`](hw/): datasheet PDFs + [`hw/md/`](hw/md/) chip notes

Built for people who want to *make* 8-bit games, not only play them.
