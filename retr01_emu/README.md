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
| **CPU** | Boots world 0. Default: PRG streams pals + start MAP (`$FE93`->`$FE12`). Gameplay still host Play |
| **Video** | Main FB = **VRAM + scroll** + **OAM composite** (SCALE 2x). Play host-fills 2x2 seams via `sync_camera` |
| **Host** | SDL; WASD/arrows move; **X**/**Y** warp (same as Studio) |

**Sync contract:** Studio Play (`play.c`) is the source of truth for motion. Export packs present screens + play table (`$8100`) + `R01P`. Soft-boot (`R01E_SOFTBOOT=1`) keeps the old host memcpy boot path for triage. Default boot runs cart PRG stream catchup like sim.

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

**Env:** `R01E_SOFTBOOT=1` -- host memcpy VRAM/pals at boot (debug). Default runs cart PRG MAP/pal stream catchup.

**Debug window:** VRAM 2x2 workbench (256x240, red = viewport, player via OAM) beside a world map (blue = present, gold = current screen). Active **BG** and **SPR** palette rows along the bottom of that row. Below: **CPU busy** chart (2 samples/s, 20 bars) -- cyan = busy cycles in active display, orange = busy in VBlank (excludes `$FE01` VBlank-wait spins). Red line = soft max **50k** cycles/frame (`docs/10`).

## Layout

| Path | Role |
|------|------|
| `include/retr01_emu/types.h` | Shared constants |
| `include/retr01_emu/cart.h` | `.retr01` parser |
| `include/retr01_emu/cpu.h` | 65C02 core |
| `include/retr01_emu/io.h` | `$FExx` register file |
| `include/retr01_emu/video.h` | CHR / VRAM / render / softboot opt-in / OAM composite |
| `include/retr01_emu/play.h` | Host Play runtime (Phase 1) |
| `include/retr01_emu/machine.h` | Bus + frame loop |
| `src/main.c` | SDL host |
| `tests/` | Cart + boot smoke tests |
