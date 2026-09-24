---
name: retr01
description: >-
  Orient work in the retr01 repo: hardware design, Studio, Emu, Tier A sim,
  netlist sim, the C SDK, and the .retr01 cart format. Use when editing,
  building, debugging, reviewing, or explaining this repository, its apps,
  docs, or example_01.
---

# Retr01

Retr01 is an MCU-assisted 8-bit game system (still in hardware development), plus an emulator and a Studio for making games. Design docs are the source of truth. Code and docs can disagree; when they do, read the doc named below and say which one you followed.

## Refresh before editing

Do this at the start of a task. Do not rely on a memorized file list.

1. `git status --short` and `git log -15 --oneline`
2. Read the doc or README for the subsystem you are about to touch (table below).
3. Open the files you will change. Search for the symbol instead of guessing a path.

## Where to look

| Task | Start here |
| --- | --- |
| Product / graphics / audio intent | `README.md` |
| Address map, `$7Fxx`, `.retr01` layout, flash budget | `docs/general/memory.md` |
| Entities, C/ASM API, game modes | `docs/general/software-api.md` |
| Resolution, tiles, sprites, layers | `docs/general/video-graphics.md` |
| Worlds, screens, scrolling | `docs/general/world-scrolling.md` |
| Board, chips, I/O | `docs/general/hardware.md`, then `docs/ic_behavior/` |
| Audio channels and `$7F40` | `docs/general/sound.md` |
| Cart and save | `docs/general/cartridge.md` |
| Bus and clock risks | `docs/general/ic-comms-risks.md` |
| Bring-up tiers A-H | `docs/bringup/README.md` |
| Unsettled design | `docs/general/open-questions.md` |
| App builds and Studio keys | `apps/README.md` |

## Software layout

| Piece | Path | Role |
| --- | --- | --- |
| Studio | `apps/studio` | Authoring. `app/` is the shell, `core/` is project/export, `ui/` is shared SDL widgets |
| Emu | `apps/emu` | Runs `.retr01` images |
| Tier A sim | `apps/sim/tier-a` | Beam + color lab. Tiers do not share netlist wiring |
| Netlist engine | `apps/netlist_sim` | Discrete-IC sim linked by the tier sims |
| Shared | `apps/common` | Linked by Studio, Emu, and Sim. Firmware headers in `apps/common/fw/` |
| C SDK | `apps/sdk/r01_c` | Game runtime for the 6502. Host tests compile with `R01_HOST_TEST` |
| Sample game | `example_01` | `game_logic.c` plus packed `data/` |

Soft I/O is `$7F00-$7FFF`. Caps (worlds, CHR, entities) live in `docs/general/memory.md`. Do not invent register addresses, sizes, or timing numbers.

## Build and test

Host apps (C11, `-Wall -Wextra -Wpedantic`):

```bash
./scripts/build-all.sh
./scripts/unit-tests.sh
```

One app: `cmake -S <path> -B <path>/build && cmake --build <path>/build -j` with `<path>` one of `apps/studio`, `apps/emu`, `apps/sim/tier-a`.

6502 PRG uses llvm-mos (`scripts/fetch-llvm-mos.sh`, toolchain in `tools/llvm-mos` or `$LLVM_MOS`):

```bash
./apps/sdk/r01_c/build-prg.sh [game_logic.c] [out.prg] [data_dir]
```

Studio export (Ctrl+E) writes the cart, `data/` bins, generated `include/` headers, and the llvm-mos PRG next to the `.r01proj`.

## While changing code

- Match the style of the file you are editing.
- Keep host-only code behind `R01_HOST_TEST`. Cart code must build with llvm-mos.
- Prefer the existing module over a new one. Studio widgets that are not generic stay in `apps/studio/app`, not `apps/studio/ui`.
- After a behavior change, run the closest test target, then `./scripts/unit-tests.sh` when the change crosses apps or `apps/common`.
