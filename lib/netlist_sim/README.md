# netlist_sim

Drop-in C library for discrete IC / netlist board simulation (retro consoles and similar).

No project branding in the API. Public symbols use `Ns` / `ns_` / `NS_`.

## Layout

| Path | Role |
|------|------|
| `include/netlist_sim/` | Public headers |
| `src/` | Engine (entity, pin, bus, island, chip registry, timing) |
| `chips/` | Breadboard, passives, video/LCD sink |
| `assets/` | `pin.png`, passives sprites |
| `examples/` | String-keyed chip + island sample |
| `tests/` | Abstract engine tests (not per-part datasheet models) |

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/example_nand
```

Needs C11 + SDL2 (for breadboard/passive draw).

## Authoring (string-keyed)

```c
ns_chip_register(&cls);                 /* part name inside cls.part */
NsEntity *u1 = ns_chip_create("SN74HC00", "U1");
NsIsland *is = ns_island_create("nand_demo");
ns_island_add(is, u1);
ns_island_wire(is, "U1", "1Y", "U2", "1A");
```

Do not mint `sn74hc00_create()` style entry points.

## Visual constants

- `NS_PX_PER_MM` = 2
- DIP pin pitch 5 px (JEDEC 0.1")
- Pin tip pivot: `pin.png` col 1, row 0
- IC / island health outline: `ns_outline_rgb()` (green / warn / red)

## Consume from another repo

Add this folder and `add_subdirectory(netlist_sim)`. Link `netlist_sim`. Include `<netlist_sim/chip.h>` etc.
