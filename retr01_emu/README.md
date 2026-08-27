# retr01 Emulator

Software-visible C emulator for retr01-A **Phase 1** carts. Separate from the IC board
simulator ([`retr01_sim/`](../retr01_sim/)). Contract:
[`docs/02_graphics_worlds_memory.md`](../docs/02_graphics_worlds_memory.md).

Root helper: [`../emu`](../emu) (`build` * `run` * `build-run` * `unit`).

Later emulator phases are **not** specified here; they will be defined when work starts.

## Phase 1 scope (active)

| Layer | What runs today |
|-------|-----------------|
| **Cart** | Load `.retr01` (Studio packs present screens only). CHR, pals, Phase 1 PRG (`R01P`) |
| **Play** | **Studio Play SoT** -- same move / camera / collision / X/Y warps as Studio |
| **CPU** | Boots world 0. Gameplay in host Play until full 6502 play |
| **Video** | Soft-boot MAP/CHR into 2x2 workbench (debug atlas). **Main FB** samples cart MAP while Play is on |
| **Host** | SDL; WASD/arrows move; **X**/**Y** warp (same as Studio) |

**Sync contract:** Studio Play (`play.c`) is the source of truth. Export packs only **present**
screens + play table (`$8100`) + `R01P` marker. Emulator applies the same play rules to that
cart MAP so live Play and emu feel match after a fresh Ctrl+E export. Soft-boot is mandatory for
emu Play parity. It is not the same as sim IC MAP stream.

## Build / run

```bash
./emu build-run
./emu unit

# or
cd retr01_emu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/retr01_emu ../retr01_studio/test_game/test.retr01
```

**Controls:** WASD or arrows = move * **X**/**Y** = warp * Space = pause * R = reset * Esc = quit

**Debug window ("Rendering debug"):** VRAM 2x2 workbench (256x240, red = viewport, player = palette red) beside a world map sized to the present-screen bounding box (blue = present, gold = current screen). Active **BG** and **SPR** palette rows (4×4 swatches) along the bottom.

## Layout

| Path | Role |
|------|------|
| `include/retr01_emu/types.h` | Shared constants |
| `include/retr01_emu/cart.h` | `.retr01` parser |
| `include/retr01_emu/cpu.h` | 65C02 core |
| `include/retr01_emu/io.h` | `$FExx` register file |
| `include/retr01_emu/video.h` | CHR / VRAM / render / soft boot (host pan = tests only) |
| `include/retr01_emu/play.h` | Host Play runtime (Phase 1) |
| `include/retr01_emu/machine.h` | Bus + frame loop |
| `src/main.c` | SDL host |
| `tests/` | Cart + boot smoke tests |
