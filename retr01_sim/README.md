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

Needs: CMake, C11 compiler, SDL2.

## Layout

| Path | Role |
|------|------|
| `include/retr01_sim/` | Public headers (`entity`, `pin`, `types`) |
| `src/` | Core + SDL UI shell |
| `chips/` | Per-part models (subclass the base entity) |
| `tests/` | Layer-1 unit tests |

## Model

Every IC is an `R01sEntity` with pins + a vtable (`reset` / `eval` / `tick` / `destroy`). Chip-specific state hangs off `impl`. The UI draws DIP-style packages from entity placement + pin list.
