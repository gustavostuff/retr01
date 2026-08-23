# Retr01 Board Simulator

IC-first board simulator for Retr01-A. Separate from Retr01 Studio (authoring).

See [`docs/08_simulator.md`](../docs/08_simulator.md). Pin/behavior: [`hw/md/`](../hw/md/). BOM: [`docs/06_hardware_v1_32ic.md`](../docs/06_hardware_v1_32ic.md).

## Status

**Islands A–C chip models + unit tests.** SDL board UI scaffold. Next: wire island C netlist smoke (CPU+RAM+PRG).

| Island | Components |
|--------|------------|
| A Power | `PWR5V` |
| B Clocks + reset | `OSC8M`, `SN74HC14` |
| C CPU + RAM + PRG | `W65C02S`, `AS6C62256`, `PRG_ROM` |

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

**Controls:** `Space` pause/resume · `R` reset · `.` single-step (while paused) · **left-drag** move chip within its island · `Esc` quit · click a chip to select · **mouse wheel / arrows / right-drag** to pan the 1600×900 board inside the **1280×720** view.

Live probe (top-right) shows **VDD / PHI2 / RESB**. Pin stubs glow by level (no schematic wires drawn). Status bar shows CPU `PC` / `AB` / phase / cycle count.

## Layout

| Path | Role |
|------|------|
| `include/retr01_sim/` | Public headers (`entity`, `pin`, `bus`, `island`, `island_group`, `types`) |
| `src/` | Core sim + SDL UI shell |
| `src/islands/` | Concrete island groups (e.g. bring-up A+B+C) |
| `chips/` | Per-part models (subclass the base entity) |
| `tests/` | Layer-1 unit tests |

## Model

**Entity** — every IC is an `R01sEntity` with pins + vtable (`reset` / `eval` / `tick` / `destroy`).

**Island** — a board region holding entities; optional island vtable for local init/eval.

**Island group** — N islands wired together; group vtable owns cross-island sim step, reset, and status. The full console will eventually be one island group.
