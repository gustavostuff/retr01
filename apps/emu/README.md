# Retr01 Emulator

Software-visible C emulator for Retr01 **Phase 1** carts. Contract: [`docs/general/video-graphics.md`](../../docs/general/video-graphics.md).

From repo root: `./scripts/emu.sh path/to/cart.retr01` (after `./scripts/build-all.sh`).

Studio **Play** uses this same emu core after export (shared library + standalone `./scripts/emu.sh`). See [`apps/studio/README.md`](../studio/README.md). Later emulator phases are **not** specified here.

## Phase 1 scope (active)

| Layer | Role |
|-------|-----------------|
| **Cart** | Load `.retr01` (Studio packs present screens only). CHR, pals, Phase 1 PRG (`R01P`) |
| **Play** | **Emu Host Play SoT**. Move / **dead-zone camera** / player anim / collision from cart bytes |
| **CPU** | Boots world 0. Default: PRG streams pals + start MAP (`$7F93` -> `$7F12`). Gameplay stays on Host Play |
| **Video** | Main FB = **VRAM + scroll** + **OAM** + **BG0** show-through under BG1 color 0 (SCALE 2x). Host Play fills BG1 2x2 via `sync_camera` |
| **Host** | SDL. Keyboard or SDL Game Controller (community DB + SDL built-in mappings). First two pads are P1 / P2. Guide / Home opens Reset, Quit, and 1x/2x present scale. Pad map: P1 WASD+G/H, P2 arrows+,/. Platformer jump is face **Y** (P1 **H**, gamepad south). NMI tracker fills `$7F40` from packed PRG BGM (`$80FE` / `$B000`). PC speaker mixes that window. Not MCU-S2 PWM |

**Sync contract:** Emu Host Play (`src/play.c` + `apps/common/`) is the Phase 1 gameplay SoT. Export packs present screens + play table (`$8100`) + `R01P`. Soft-boot (`R01E_SOFTBOOT=1`) uses host memcpy of VRAM and pals at boot (triage). Default boot runs cart PRG stream catchup until the start MAP write reaches **480** bytes (`vram_addr`), then Host Play reloads the camera 2x2 from cart. Collision samples PRG solids. Render samples that VRAM window.

**Studio integration:** Studio **Play** / **Space** always exports, then embeds this render path. Export wait uses a Studio-local spinning boot message. Standalone `./scripts/emu.sh` stays for triage.

**Collision:** Host Play reads **PRG collision tables** (`$8121` directory, 240 bytes per present screen). Player hitbox is the **current anim state's** AABB, origin-relative via that state's first drawable frame in the cart player anim blob when present. Host movement uses that table. Default motion is top-down (axis-separated). World header flags bit **4** selects platformer (gravity + face **Y** jump, short hop on release, Down crouch). Tune bytes at PRG `$80F7`/`$80F8`/`$80F9`/`$80FA`/`$80FB`/`$80FC`/`$80FD` (gravity, jump, meter, crouch / idle / walk / jump states). Unmapped player anim bytes are `$FF` (draw state 0 frame 0 + facing flip only).

**Camera:** Dead zone W x H from world header bytes 30-31 (packed from `r01_camera_set_deadzone` in `custom_logic.c` on export). Centered rectangle on the 128x120 viewport. Shared `../common/r01_play_camera.c`.

**Runtime:** World **0** only (`R01E_PHASE1_WORLDS=1`).

## Build / run

From the repo root:

```bash
./scripts/build-all.sh
./scripts/emu.sh path/to/cart.retr01
./scripts/unit-tests.sh
```

Pi CRT pack (emu + `example_01.retr01` only):

```bash
./scripts/pack-pi-emu.sh
```

Writes `raspberry_pi_test/`. On the Pi: `./build.sh` then `./retr01.sh` (RGB-Pi OS4 Ports launcher, 256x240 fullscreen).

Developer rebuild of this tree only:

```bash
cd apps/emu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/retr01_emu path/to/cart.retr01
```

**Controls:** WASD or arrows = move. Face **Y** (P1 **H**, P2 **.**, gamepad **A**/**Y**) = jump in platformer. Down = crouch in platformer (grounded, no walk). Space = pause. R = reset. **Ctrl+1** / **Ctrl+2** = present scale. Esc = quit. Gamepad Guide / Home = Reset / Quit / **1x**/**2x**. First two SDL Game Controllers are P1 / P2 (community `gamecontrollerdb.txt` plus SDL built-in mappings).

**Env:** `R01E_SOFTBOOT=1` forces host memcpy VRAM/pals at boot (debug). Default runs cart PRG MAP/pal stream catchup to a full start-screen payload. `R01E_SCALE=1` presents 256x240. `R01E_FULLSCREEN=1` uses desktop-fullscreen and skips the debug window. `R01E_NO_DEBUG=1` skips the debug window only. `R01E_AUDIO_SAMPLES` (256..4096) and `R01E_AUDIO_RATE` (22050..48000) size the PC speaker mix. Pi `retr01.sh` uses 2048 samples at 48 kHz.

**Debug (standalone `./scripts/emu.sh`):** separate OS window (~atlas width, shorter than the 2x play window): top row **BG1** VRAM 2x2 + **BG0** 2x2 (red/green viewports), second row **opacity mask** + world map + **BG**/**SPR** pals, bottom **CPU busy** chart (2 samples/s). Cyan = active display, orange = VBlank. Red line = soft max **50k** cycles/frame.

## Layout

| Path | Role |
|------|------|
| `include/retr01_emu/types.h` | Shared constants |
| `include/retr01_emu/cart.h` | `.retr01` parser |
| `include/retr01_emu/cpu.h` | 65C02 core |
| `include/retr01_emu/io.h` | `$7Fxx` register file |
| `include/retr01_emu/video.h` | CHR / VRAM / render / softboot opt-in / OAM composite |
| `include/retr01_emu/play.h` | Host Play runtime (Phase 1 SoT) |
| `include/retr01_emu/machine.h` | Bus + frame loop |
| `src/main.c` | Standalone SDL host |
| `tests/` | Cart + boot + play smoke tests |
