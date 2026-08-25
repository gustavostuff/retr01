# Retr01 Board Simulator

IC-first board simulator for Retr01-A. Separate from Retr01 Studio (authoring).

See [`docs/08_simulator.md`](../docs/08_simulator.md). Pin/behavior: [`hw/md/`](../hw/md/). BOM: [`docs/06_hardware_v1_32ic.md`](../docs/06_hardware_v1_32ic.md).

## Status

**9 canvas islands (O first / top-left) + wired-only E/I/N/P — 32-IC BOM, layer-2 smoke.** SDL board UI. Architecture: [`docs/08_simulator.md`](../docs/08_simulator.md).

| Island | Components (canvas) |
|--------|---------------------|
| O Video | `COMPOSITOR` + `AT28C16` + `LCD_SINK` + video `SN74HC245` (top-left) |
| A Power+clk | `PWR5V` + `OSC8M` + `SN74HC14` |
| C CPU + decode | `W65C02S`, `AS6C62256`, `ATF22V10` decode, CPU `SN74HC245` |
| D `$FExx` latch | **9×** `SN74HC573` (`$FE02`–`$FE04`, `$FE08`, `$FE10`–`$FE12`, `$FE90`–`$FE92`) |
| E Pads | `$FE60`/`$FE61` via 1284 (sim model; not on canvas) |
| G VRAM | 2nd `AS6C62256` + **3×** `SN74HC157` + `ATF22V10` VRAM glue |
| H Beam | `OSC_DOT` + `BEAM_XY` (X PLD) + `ATF22V10` Y compare vs `$FE04` |
| I BG fetch | `BG_FETCH` PLD — nametable VA from beam+scroll (not on canvas) |
| J Cart | `SST39SF040` + cart `24C64` + cart/OAM `SN74HC245` |
| K APU | `ATMEGA328P` stub — `$FE40`–`$FE5F` regs + digital PWM square |
| L MCU+linebuf | `ATMEGA1284P` + linebuf `AS6C62256` + **3×** `SN74HC157` |
| N Sprites | stats via 1284/OAM (not on canvas) |
| P Integration | NMI / bus-fight stats (not on canvas) |

Bench-only (wired, not on canvas): `PRG_ROM` fallback when cart does not own `$8000+`.

**Cart load:** `./sim run` passes `retr01_studio/test_game/test.retr01` (override with a path; resolved from repo root). Sim applies a **bring-up PRG overlay** for island smoke. For the LCD it **soft-boots** the cart start-screen MAP + pals into VRAM (emu-style host convenience — not Studio game PRG). Still **no** full 2×2 Play camera / player loop.

**Fast path (optional):** Playbook glue inlining — default is full pin-level settle (4 passes) + compositor entity eval per dot. Enable **`R01S_FAST=1`**, **`./sim run -- --fast`**, or press **`F`** for `settle` + `video` + `memory` + `pins`. Slow route unchanged; reserved: `bus` (Pass 2 bitmasks).

Next: optional **F** machine EEPROM (1284 path); retire bring-up overlay when game PRG owns MAP.

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

**Controls:** `Space` pause/resume · `Ctrl+R` reset · `R` rotate selected IC · `G` SCALE 2x/1x · `.` single-step (while paused) · **COMPACT / ISLANDS** (HUD) · **left-drag chip** move · **right-click chip** orient H/V · **left-drag empty island** move frame · **bottom-right grip** resize · **Shift+arrows / wheel / middle-drag** pan · `Esc` quit.

**Layout persistence:** island frames + chip positions (island mode) and compact chip positions are saved to `retr01_sim/ui_layout.json` (override with `R01S_LAYOUT`). Reloaded on next launch.

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
| `src/board.c` | **Board recipe** — 9 canvas islands (O…L), wiring, settle loop, group vtable |
| `src/main.c` | SDL entry: build board + run UI |
| `chips/` | Per-part models (subclass the base entity) |
| `tests/` | Layer-1 unit tests + `test_island_abcdeghiojklmnp` (layer 2) |

## Model

**Entity** — every IC is an `R01sEntity` with pins + vtable (`reset` / `eval` / `tick` / `destroy`).

**Island** — a board region holding entities; optional island vtable for local init/eval.

**Island group** — N islands wired together; group vtable owns cross-island sim step, reset, and status. The full console will eventually be one island group.

**Island builder** — assembles islands + chip placements into a group.

**Board** — `r01s_board_build()` binds the full netlist (settle passes, memory/`$FExx`/VRAM decode, PHI2 edge) onto **9 canvas frames**. UI mounts the same builder.

**Bus** — undriven pins pull high on read; H+L (or sensing `X`) **aborts** with a stderr bus-fight report (net + drivers + why).
