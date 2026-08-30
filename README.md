<img src="img/logo_v3.png" alt="Logo V3" />

The Retr01 project is a software + hardware tool chain family. Software includes:

- **Retr01 Studio**: visual authoring for worlds, tiles, sprites, and entities. **Play** preview in the editor. **Ctrl+E** export writes a packed `.retr01` cart plus generated `output/C/`, `output/ASM/`, and `output/data/`.
- **Retr01 Emulator**: software-visible 65C02 + `$FExx` I/O + video. Loads a cart, runs PRG boot catchup, and host Play for movement, camera, and collision. Debug pane shows the 2x2 VRAM workbench, world map, palettes, and CPU budget.
- **Retr01 Sim**: pin-level model of the **32-IC** Retr01-A netlist. Interactive SDL board UI with draggable islands, per-chip tests, and the same cart boot path as the emulator.

Software is still a WIP. This is the overall hardware roadmap:

- **Retr01-A (stage 1)**: arcade motherboard, first build. THT, ~14 x 12 cm minimum. Cabinet sticks/buttons, coin/start, RGBS / S-Video / composite.
- **Retr01-C (stage 2)**: home console. Same architecture and cart format, smaller board, 3-wire controllers.
- **Retr01-H (stage 3)**: handheld. SMD, battery, LCD + driver, contact pads. Same software contract.

## At a glance

| | |
|--|--|
| CPU | W65C02S @ **8 MHz** |
| Playfield | **128 x 120** logical (**16 x 15** tiles), board **2x** to **256 x 240** RGBS |
| Art | **8 x 8** tiles, **2 bpp**, **64** master colors on-board Color PROM |
| Worlds | up to **8** worlds, **32** screens each on a **512 KB** cart (**32 KB** PRG) |
| Scroll | **2 x 2** live nametable window crossing screen borders |
| Sprites | **64** OAM entries, **16** per scanline |
| VRAM / RAM | **32 KB** interleaved VRAM + **32 KB** system RAM |

Same **32 KB PRG** as classic NES NROM, but it buys far more game: **~4.5x** cycles per frame at **8 MHz**, **32 KB** system RAM (not 2 KB), and scroll, sprite line fill, and MAP streaming are hardware jobs, so PRG stays game logic, not VBlank nametable tricks. [`why_32kb_prg_is_good_enough.md`](docs/why_32kb_prg_is_good_enough.md)

Full map, cart layout, and `$FExx` ports: [`docs/02_graphics_worlds_memory.md`](docs/02_graphics_worlds_memory.md). Terminology and BOM: [`docs/01_architecture_overview.md`](docs/01_architecture_overview.md).

## Software

Three tools share one cart image (`.retr01`) and one Play contract (`common/` runtime):

- [**Retr01 Studio**](retr01_studio/README.md): author worlds, tiles, sprites, entities. **Play** preview. **Ctrl+E** export writes cart + generated `output/C/`, `output/ASM/`, `output/data/`.
- [**Retr01 Emulator**](retr01_emu/README.md): software-visible 65C02 + `$FExx` + video. Loads a cart, runs PRG boot catchup, host Play for movement/camera.
- [**Board simulator**](retr01_sim/README.md): pin-level model of the **32-IC** netlist. Interactive SDL board UI, island tests, same cart boot path as emu.

Studio **Save** writes `output/<stem>.r01proj`. Export packs **world 0** today (multi-world authoring in UI, single-world cart). User hooks live in `output/C/custom_logic.c` (created once, never overwritten).

## Quick start

```bash
./build-all
./unit-tests
./studio output/test.r01proj    # author + Play
./export-rom                    # or Ctrl+E in Studio
./emu output/test.retr01
./sim output/test.retr01
```

Root wrappers (`studio`, `emu`, `sim`, ...) forward to [`scripts/`](scripts/). Release binaries land in `release/`.

## Documentation

- [`docs/01`](docs/01_architecture_overview.md): architecture, terminology, doc index
- [`docs/02`](docs/02_graphics_worlds_memory.md): software SoT: VRAM, cart, registers
- [`docs/05`](docs/05_hardware_v1_32ic.md): 32-IC BOM and netlist
- [`docs/07`](docs/07_game_modules.md): movement, camera, entities, CPU budgets
- [`hw/`](hw/): datasheet PDFs + [`hw/md/`](hw/md/) chip notes

---

Here's a picture of the emulator, just for fun:

<img src="img/emulator.png" alt="Retr01 Emulator with debug panel" />

Built for people who want to *make* 8-bit games, not only play them.
