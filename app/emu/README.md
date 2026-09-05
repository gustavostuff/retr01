# Retr01 Emulator

Software-visible C emulator for Retr01 **Phase 1** carts. Separate from the IC board
simulator ([`app/sim/`](../sim/)). Contract:
[`docs/graphics.md`](../../docs/graphics.md).

Run from repo root: `./emu output/test.retr01` (after `./build-all`).

Studio **Play** uses this same emu core after export (shared library + standalone `./emu`). See [`app/studio/README.md`](../studio/README.md). Later emulator phases are **not** specified here.

## Phase 1 scope (active)

| Layer | What runs today |
|-------|-----------------|
| **Cart** | Load `.retr01` (Studio packs present screens only). CHR, pals, Phase 1 PRG (`R01P`) |
| **Play** | **Emu Host Play SoT**. Move / **dead-zone camera** / player anim / collision / X/Y warps from cart bytes |
| **CPU** | Boots world 0. Default: PRG streams pals + start MAP (`$FE93`->`$FE12`). Gameplay still host Play |
| **Video** | Main FB = **VRAM + scroll** + **OAM** + **BG0** show-through under BG1 color 0 (SCALE 2x). Play host-fills BG1 2x2 seams via `sync_camera` |
| **Host** | SDL. Shared Sim pad map: P1 WASD+G/H, P2 arrows+,/. P1 X/Y edge = warp + fixed SFX. Host softsynth BGM (mix / 4). Not cart `$FE40` audio |

**Sync contract:** Emu Host Play (`src/play.c` + `app/common/`) is the Phase 1 gameplay SoT. Studio no longer keeps a parallel preview. Export packs present screens + play table (`$8100`) + `R01P`. Soft-boot (`R01E_SOFTBOOT=1`) keeps the old host memcpy boot path for triage. Default boot runs cart PRG stream catchup like sim.

**Studio integration:** Studio **Play** / **Space** always exports, then embeds this render path in Studio. Export wait uses a Studio-local spinning boot message. Standalone `./emu` stays for triage. **Sim is not part of this path.**

**Collision:** Host Play reads **cart MAP attrs** (`R01_ATTR_SOLID`). Player hitbox follows the **current anim state** from the cart player anim blob when present. PRG collision stub at `$8500` is packed for future 6502 use, not used by host movement today.

**Camera:** Dead zone W x H from world header bytes 30-31 (packed from `r01_camera_set_deadzone` in `custom_logic.c` on export). Centered rectangle on the 128x120 viewport. Shared `../common/r01_play_camera.c`.

**Runtime:** World **0** only (`R01E_PHASE1_WORLDS=1`).

## Build / run

From the repo root:

```bash
./build-all
./emu output/test.retr01
./unit-tests
```

Developer rebuild of this tree only:

```bash
cd app/emu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/retr01_emu ../../output/test.retr01
```

**Controls:** WASD or arrows = move. **X**/**Y** = warp. Space = pause. R = reset. Esc = quit

**Env:** `R01E_SOFTBOOT=1`, host memcpy VRAM/pals at boot (debug). Default runs cart PRG MAP/pal stream catchup.

**Debug (standalone `./emu`):** separate OS window with **BG1** VRAM 2x2 (256x240, red viewport, sprites via OAM), **BG0** 2x2 cart cache (green viewport), **BG1 opacity mask** (128x120, black=transparent, orange=opaque), world map (blue=present, gold=current), active **BG**/**SPR** palette rows, and **CPU busy** chart (2 samples/s, 20 bars). Cyan = busy cycles in active display, orange = busy in VBlank. Red line = soft max **50k** cycles/frame (`R01E_CPU_BUDGET_CYCLES`).

## Layout

| Path | Role |
|------|------|
| `include/retr01_emu/types.h` | Shared constants |
| `include/retr01_emu/cart.h` | `.retr01` parser |
| `include/retr01_emu/cpu.h` | 65C02 core |
| `include/retr01_emu/io.h` | `$FExx` register file |
| `include/retr01_emu/video.h` | CHR / VRAM / render / softboot opt-in / OAM composite |
| `include/retr01_emu/play.h` | Host Play runtime (Phase 1 SoT) |
| `include/retr01_emu/machine.h` | Bus + frame loop |
| `src/main.c` | Standalone SDL host |
| `tests/` | Cart + boot + play smoke tests |
