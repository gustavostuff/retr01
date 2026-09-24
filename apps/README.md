# Retr01 apps

| App | Path | Build |
| --- | --- | --- |
| Studio | `apps/studio` | `cmake -S apps/studio -B apps/studio/build && cmake --build apps/studio/build -j` |
| Emu | `apps/emu` | `cmake -S apps/emu -B apps/emu/build && cmake --build apps/emu/build -j` |
| Sim (Tier A) | `apps/sim/tier-a` | `cmake -S apps/sim/tier-a -B apps/sim/tier-a/build && cmake --build apps/sim/tier-a/build -j` |
| Sim (Tier B) | `apps/sim/tier-b` | `cmake -S apps/sim/tier-b -B apps/sim/tier-b/build && cmake --build apps/sim/tier-b/build -j` |
| Shared | `apps/common` | Linked by Studio / Emu / Sim |
| Netlist engine | `apps/netlist_sim` | Linked by Sim |

Repo wrappers (after copying binaries to `bin/`): `./scripts/studio.sh`, `./scripts/emu.sh`, `./scripts/sim-tier-a.sh`, and `./scripts/sim-tier-b.sh`. Build with `./scripts/build-all.sh`. Tests: `./scripts/unit-tests.sh`.

## Studio projects

Each save creates a folder under `apps/studio/projects/` (or another chosen parent folder):

```text
MyGame/
  MyGame.r01proj
  MyGame.retr01   (after export)
  game_logic.c
  include/        (generated ids, after export)
  data/
```

**Ctrl+E** packs the cart, `data/` bins, generated `include/` id headers, and llvm-mos PRG beside the project file (or under `output/` if unsaved).

| Shortcut | Action |
| --- | --- |
| Ctrl+S | Save. First time (or unsaved) opens the Save project modal |
| Ctrl+Shift+S | Always open Save project modal (new folder / overwrite name) |
| Ctrl+O | Open project modal (folder with `.r01proj`, or the file itself) |
| Ctrl+E | Export `.retr01` next to the project file |

## Architecture notes

Soft I/O is `$7F00-$7FFF`. Design caps (worlds, CHR, entities) live in `docs/general/memory.md`.
