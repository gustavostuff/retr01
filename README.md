<img src="app/assets/png/logo_v2.png" alt="Logo V2" />

The Retr01 project is a software tool chain + hardware family. Software includes:

- **Retr01 Studio**: the game editor. Draw maps, tiles, and sprites, then hit **Play** to try your game (same picture as the emulator).
- **Retr01 Emu**: the standalone game player. Runs a cartridge image on your computer. Studio Play uses this same engine.
- **Retr01 Sim**: the board simulator. Shows the ICs and wiring of the motherboard so we can bring up the real design before hardware.

Software is still a WIP. This is the overall hardware roadmap:

- **Retr01-A (stage 1 arcade)** / **Retr01-C (stage 1 console)**: One THT board (~**14 x 12 cm**, 4-layer) for both shells. Same 32-IC core, [**36-pin cart**](docs/cart.md), **RGBS + composite**, 5 V barrel. Arcade microswitch headers **and** PCB footprints for **2x Switchcraft 35RAPC** TRS ([`docs/controllers.md`](docs/controllers.md)). Shell + population choose I/O path, not two mobos.
- **Retr01-H (stage 2)**: Handheld. SMD, battery, LCD + driver, contact pads. Same software contract.

## Software tree

Apps and shared code live under [`app/`](app/):

| Path | Role |
|--|--|
| `app/studio/` | Authoring app |
| `app/emu/` | Cart emulator (+ Studio Play core) |
| `app/sim/` | Pin-level board simulator |
| `app/schematic_generator/` | SKiDL netlist from sim wiring (Code-to-Copper) |
| `bin/` | Release binaries from `./build-all` (`studio`, `emu`, `sim`) |

`./build-all` builds Release binaries into `bin/`. `./studio`, `./emu`, and `./sim` only run those binaries. `./unit-tests` runs the test suites.

## At a glance

| Aspect | Description |
|--|--|
| CPU | W65C02S @ **8 MHz** |
| Playfield | **128 x 120** logical (**16 x 15** tiles), board **2x** to **256 x 240** RGBS |
| Art | **8 x 8** tiles, **2 bpp**, **64** master colors on-board Color PROM (**AT27C256R**) |
| Worlds | up to **8** worlds, **48** BG1 screens each on a **16x16** grid + optional **BG0** on a **512 KB** cart (**32 KB** PRG) |
| Scroll | **2 x 2** live nametable window (BG1) + structured second BG (BG0) with show-through |
| Sprites | **64** OAM entries, **16** per scanline |
| VRAM / RAM | **32 KB** interleaved VRAM + **32 KB** system RAM |

Same **32 KB PRG** as classic NES NROM games (Exitebike, Balloon Fight, Ice Climbers) but it buys far more game: **4.5x** more CPU cycles per frame at **8 MHz**, **32 KB** system RAM (not 2 KB). Scroll, sprite line fill and world map streaming are hardware jobs, so PRG stays game logic, not VBlank nametable tricks. See [`docs/selling_points.md`](docs/selling_points.md).

## Screenshots (scaled to 1x)

Peek at Retr01 Studio:

<img src="app/assets/png/studio.png" alt="Studio" />

Emulator + Debug screen:

<img src="app/assets/png/emu.png" alt="Emu" />

<img src="app/assets/png/debug.png" alt="Debug" />

Sim:

<img src="app/assets/png/sim.png" alt="Sim" />

## Where to go next

- [`docs/graphics.md`](docs/graphics.md): VRAM, BG0/BG1, sprites, palettes, graphics ports
- [`docs/memory.md`](docs/memory.md): chips, cart layout, read/write timing
- [`docs/hardware.md`](docs/hardware.md): IC BOM, block diagram, bring-up (chips only)
- [`docs/cart.md`](docs/cart.md): cartridge pinout, form factor, USB-C flasher
- [`docs/controllers.md`](docs/controllers.md): Retr01-A arcade vs Retr01-C pad protocol
- [`docs/lightgun.md`](docs/lightgun.md): CRT light gun accessory (roadmap)
- [`docs/passive_rf_etc.md`](docs/passive_rf_etc.md): passives, ports, stackup / RF
- [`docs/sound.md`](docs/sound.md): APU and bytecode
- [`docs/selling_points.md`](docs/selling_points.md): NES vs Retr01 comparison
- [`hw/`](hw/): part list + [`hw/md/`](hw/md/) chip notes

Built for people who want to *make* 8-bit games, not only play them.
