# Retr01 Board Simulator

IC-first board simulator for Retr01-A. Separate from Retr01 Studio (authoring).

See [`docs/08_simulator.md`](../docs/08_simulator.md). Pin/behavior: [`hw/md/`](../hw/md/). BOM: [`docs/06_hardware_v1_32ic.md`](../docs/06_hardware_v1_32ic.md).

## Status

**Islands A–E + G + H + I + O + J + K + L + M + N + P models, wiring, and layer-2 smoke.** SDL board UI. Architecture: [`docs/08_simulator.md`](../docs/08_simulator.md).

| Island | Components |
|--------|------------|
| A Power | `PWR5V` |
| B Clocks + reset | `OSC8M`, `SN74HC14` |
| C CPU + RAM + PRG | `W65C02S`, `AS6C62256`, `PRG_ROM` (breadboard leftover; deselected when cart owns `$8000+`) |
| D `$FExx` latch | `SN74HC573` @ `$FE02` / `$FE03` / `$FE04` (soft decode in board) |
| E Pads | `PADS` stub @ `$FE60`/`$FE61` (pre-1284 merge) |
| G VRAM | 2nd `AS6C62256` + `SN74HC157` mux; soft `$FE10`–`$FE12` + PHI2 interleave |
| H Beam | `OSC_DOT` + `BEAM_XY` (ATF22V10 X/Y stub, 341×262) + `SN74HC688` vs `$FE04` |
| I BG fetch | `BG_FETCH` PLD stub — nametable VA from beam+scroll; PPU-phase VRAM read |
| O Video | `COMPOSITOR` PLD stub + `AT28C16` Color PROM + `LCD_SINK` (128×120 RGBS preview) |
| J Cart | `SST39SF040` — PRG `$8000+` + MAP `$FE90`–`$FE93`; loads `.retr01` / flash bin |
| K APU | `ATMEGA328P` stub — `$FE40`–`$FE5F` regs + digital PWM square |
| L MCU | `ATMEGA1284P` stub — OAM `$FE20`/`$FE21` + 20 MHz tick; `$FE70`–`$FE72` mailbox |
| M Linebuf | 3rd `AS6C62256` + `SN74HC157` — ping-pong 128 px halves |
| N Sprites | `SPRITE_FETCH` stub — OAM→linebuf fill + compositor sprite path (CHR stub) |
| P Integration | `INTEGRATION` stub — beam NMI#→CPU, pads+video+NMI smoke |

**Cart load:** `./sim run -- path/to/project.retr01` (or auto-finds `retr01_studio/project.retr01`). Sim applies a **bring-up PRG overlay** into the cart PRG window so island smoke still runs — that overlay is **not** Studio ROM content ([triage](../docs/08_simulator.md#cart-rom-vs-runners-triage)).

Next: optional **F** machine EEPROM (1284 path).

## Build

```bash
cd retr01_sim
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_sim
```

Or use the helper scripts:

| Script | What it does |
|--------|----------------|
| `scripts/run.sh` | Run the app (build must already exist) |
| `scripts/build-run.sh` | Configure if needed, build, run |
| `scripts/test-unit.sh` | Build + run IC unit tests |

```bash
./scripts/build-run.sh
./scripts/test-unit.sh
```

Needs: CMake, a C compiler, SDL2 (`sdl2` package).

## Run

```bash
./scripts/run.sh
# or: ./build/retr01_sim
```

**Controls:** `Space` pause/resume · `R` reset · `.` single-step (while paused) · **left-drag chip** move within island · **left-drag empty island** move frame · **bottom-right grip** resize island · **Shift+arrows / wheel / right-drag** pan · `Esc` quit.

**Gamepads (island E → `$FE60`/`$FE61`):** bottom-left panels or keyboard:

| | Stick | X | Y | Coin | Start |
|--|-------|---|---|------|-------|
| **P1** | Arrows (8-way) | Z | X | 1 | Enter |
| **P2** | WASD (8-way) | N | M | 2 | Backspace |

Drag the on-screen stick for diagonals (two direction bits). Buttons are momentary.

Live probe (top-right) shows **VDD / PHI2 / RESB**. Pin stubs glow by level (no schematic wires drawn). Status bar shows CPU `PC` / `AB` / phase / cycle count.

## Layout

| Path | Role |
|------|------|
| `include/retr01_sim/` | Public headers (`entity`, `pin`, `bus`, `board`, `island*`, `types`) |
| `src/board.c` | **Board recipe A–E + G + H + I + O + J + K + L + M** — wiring, settle loop, group vtable |
| `src/main.c` | SDL entry: build board + run UI |
| `chips/` | Per-part models (subclass the base entity) |
| `tests/` | Layer-1 unit tests + `test_island_abcdeghiojklmnp` (layer 2) |

## Model

**Entity** — every IC is an `R01sEntity` with pins + vtable (`reset` / `eval` / `tick` / `destroy`).

**Island** — a board region holding entities; optional island vtable for local init/eval.

**Island group** — N islands wired together; group vtable owns cross-island sim step, reset, and status. The full console will eventually be one island group.

**Island builder** — assembles islands + chip placements into a group.

**Board** — `r01s_board_build()` binds the A–E + G recipe (settle passes, memory/`$FExx`/VRAM decode, PHI2 edge). UI mounts the same builder.

**Bus** — undriven pins pull high on read; H+L (or sensing `X`) **aborts** with a stderr bus-fight report (net + drivers + why).
