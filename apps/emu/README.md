# Retr01 Emulator

Software-visible C emulator for Retr01 **Phase 1** carts. Contract: [`docs/general/video-graphics.md`](../../docs/general/video-graphics.md).

From repo root: `./scripts/emu.sh path/to/cart.retr01` (after `./scripts/build-all.sh`).

Studio **Play** uses this same emu core after export (shared library + standalone `./scripts/emu.sh`). See [`apps/studio/README.md`](../studio/README.md). Later emulator phases are **not** specified here.

## Phase 1 scope (active)

| Layer | Role |
|-------|-----------------|
| **Cart** | Load `.retr01` (Studio packs present screens only). CHR, pals, llvm-mos PRG (`R01P`) |
| **Play** | Camera / collision follow the C PRG (`$02E0` sys block). Same image as Studio Play |
| **CPU** | Boots world 0. PRG streams pals + live 2x2 MAP (`$7F93` -> `$7F12`), then the C play loop |
| **Video** | Main FB = **VRAM + scroll** + **OAM** + **BG0** show-through under BG1 color 0 (SCALE 2x) |
| **Host** | SDL. Keyboard or SDL Game Controller (community DB + SDL built-in mappings). First two pads are P1 / P2. Guide / Home opens Reset, Quit, 1x/2x present scale, and Mute On/Off. Pad map: P1 WASD+G/H, P2 arrows+,/. Platformer jump is face **Y** (P1 **H**, gamepad south). `r01_game_on_tick` may raise walk speed and anim frame delay from live pad bits (`R01_PAD_X` is P1 **G**). C NMI tracker fills `$7F40` from the cart BGM region (`r01_bgm_play` boot track). PC speaker mixes that window. Not MCU-S2 PWM |

**Sync contract:** The packed C PRG is gameplay. llvm-mos links play tables (`$8100`), solids (`$8700`), and `R01P`. Packer patches the 16 B boot MAP at `$80E0`. Soft-boot (`R01E_SOFTBOOT=1`) uses host memcpy of VRAM and pals at boot (triage). Default boot runs cart PRG stream catchup until the start MAP write reaches **480** bytes (`vram_addr`). Collision samples the packed solid-pattern tables. Render samples that VRAM window.

**Studio integration:** Studio **Play** / **Space** always exports, then embeds this render path. Export wait uses a Studio-local spinning boot message. Standalone `./scripts/emu.sh` stays for triage.

**Collision:** The C PRG tests each BG1 cell against the live MAP nametable plus the pattern list at PRG `$8700`. Palette and H/V flip do not matter. Boot copies that list into system RAM `$0200`. Default motion is top-down (axis-separated). `r01_game_set_mode` in `game_logic.c` selects platformer (gravity + face **Y** jump, short hop on release, Down crouch). `r01_game_on_tick` may set walk mul and live anim frame delay from the pad.

**Camera:** Dead zone W x H from `r01_camera_set_deadzone` in `game_logic.c` (runs on the 6502). Shared `../common/r01_play_camera.c`.

**Runtime:** Packed worlds 1-6 play from the same MAP + `$8700` rules as world 0 (`r01_world_enter`).

## Build / run

From the repo root:

```bash
./scripts/build-all.sh
./scripts/emu.sh path/to/cart.retr01
./scripts/unit-tests.sh
```

Developer rebuild of this tree only:

```bash
cd apps/emu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/retr01_emu path/to/cart.retr01
```

**Controls:** WASD or arrows = move. Face **Y** (P1 **H**, P2 **.**, gamepad **A**/**Y**) = jump in platformer. Face **X** (P1 **G**, P2 **,**, gamepad **B**/**X**) hold = 2x walk when packed. Down = crouch in platformer (grounded, no walk). Space = pause. R = reset. **Ctrl+1** / **Ctrl+2** = present scale. **Ctrl+F** = desktop-fullscreen (integer scale of the 1x/2x dest, centered). Esc = quit. Gamepad Guide / Home = Reset / Quit / **1x**/**2x** / **Mute On/Off**. First two SDL Game Controllers are P1 / P2 (community `gamecontrollerdb.txt` plus SDL built-in mappings).

**Env:** `R01E_SOFTBOOT=1` forces host memcpy VRAM/pals at boot (debug). Default runs cart PRG MAP/pal stream catchup to a full start-screen payload. `R01E_SCALE=1` presents 128x120. `R01E_SCALE=2` presents 256x240 (hardware 2x, console default). `R01E_FULLSCREEN=1` uses desktop-fullscreen at that dest size, centered, and skips the debug window. **Ctrl+F** uses the largest integer multiple of that dest that fits, centered. `R01E_NO_DEBUG=1` skips the debug window only. `R01E_AUDIO_SAMPLES` (256..4096) and `R01E_AUDIO_RATE` (22050..48000) size the PC speaker mix. Home / Guide mutes the speaker while the overlay is open. Mute On keeps the speaker off after the overlay closes.

**Debug (standalone `./scripts/emu.sh`):** separate OS window (~atlas width, shorter than the 2x play window): top row **BG1** VRAM 2x2 + **BG0** 2x2 (red/green viewports), second row **opacity mask** + world map + **BG**/**SPR** pals, bottom **CPU busy** chart (last **20** frames). Cyan = active display, orange = leftover VBlank work. Red line = one CRT frame (~**133k** cycles at 8 MHz). Wait on `$7F01` is idle and not counted.

## Layout

| Path | Role |
|------|------|
| `include/retr01_emu/types.h` | Shared constants |
| `include/retr01_emu/cart.h` | `.retr01` parser |
| `include/retr01_emu/cpu.h` | 65C02 core |
| `include/retr01_emu/io.h` | `$7Fxx` register file |
| `include/retr01_emu/video.h` | CHR / VRAM / render / softboot opt-in / OAM composite |
| `include/retr01_emu/play.h` | Follow packed PRG sys block `$02E0` |
| `include/retr01_emu/machine.h` | Bus + frame loop |
| `src/main.c` | Standalone SDL host |
| `tests/` | Cart + boot + play smoke tests |
