# Retr01 Board Simulator

IC-first board simulator for the Retr01 motherboard (arcade + console share one netlist). Separate from Retr01 Studio (authoring). Pin/behavior: [`hw/md/`](../../hw/md/). BOM and islands: [`docs/hardware.md`](../../docs/hardware.md).

**Goal:** simulate the Retr01 motherboard as discrete ICs (pins, package, datasheet behavior) wired like the real board. End state: boot a cart, accept pad input, show a digital playfield (logical 128x120 inside a 256x240 RGBS field / LCD sink). Accuracy tightens as tests demand it.

## Status

**10 canvas islands (O first / top-left) + wired-only E/I/P sprite glue, 32-IC sim BOM (HC573 lag vs 23-IC silicon), layer-2 smoke.** SDL board UI. No control-strip power/reset/cart UI (cart image comes from the argv `.retr01` path). Cart USB flasher is **not** on the main canvas: unit-tested via `flasher_bench` only.

| Island | Components (canvas) |
|--------|---------------------|
| O Video | `COMPOSITOR` + `AT28C16` (sim, target **AT27C256R**) + `LCD_SINK` + video `SN74HC245` (top-left) |
| A Power+clk | `PWR5V` + `OSC8M` + `SN74HC14` |
| C CPU + decode | `W65C02S`, `AS6C62256`, `ATF22V10` decode, CPU `SN74HC245` |
| D `$FExx` latch | **9x** `SN74HC573` (`$FE02`-`$FE04`, `$FE08`, `$FE10`-`$FE12`, `$FE90`-`$FE92`) |
| G VRAM | 2nd `AS6C62256` + **3x** `SN74HC157` + `ATF22V10` VRAM glue |
| H Beam | `OSC_DOT` + `BEAM_XY` (X PLD) + `ATF22V10` Y compare vs `$FE04` |
| J Cart | cart `SN74HC245` (mobo socket path) |
| K APU | `ATMEGA328P` stub, `$FE40`-`$FE5F`, 8-voice mix (BGM+SFX) + digital PWM + WAVE monitor (no host speaker audio) |
| L MCU+linebuf | `ATMEGA1284P` + linebuf `AS6C62256` + **3x** `SN74HC157` |
| N Cart module | `SST39SF040` + cart `24C64` (detachable module. Argv `.retr01` is copied into flash) |

**Wired on the netlist, not separate canvas frames:** **E** pads (`$FE60`/`$FE61` via 1284), **I** `BG_FETCH`, sprite field fill stats, **P** integration / NMI stats.

Bench-only (wired, not on canvas): `PRG_ROM` fallback when cart does not own `$8000+`.

**Letter note:** Silicon bring-up docs use **N** for the sprite path ([`docs/hardware.md`](../../docs/hardware.md)). On the sim canvas, **N** is the detachable **cart module** island. Sprite milestones still show as **N** in the health strip detail line.

**Cart load:** argv / default **`output/test_2.retr01`** is copied into cart `SST39SF040` after board reset (override: `./sim path/to/cart.retr01`). The **6502 executes cart PRG from flash** (Studio export includes palette + MAP->VRAM boot via `$FE93`->`$FE12`). Startup catchup runs that stream on a **worker thread** (~12k pin-level steps) so the SDL window stays responsive. `board_build` still installs a synthetic image for unit tests that do not call `r01s_board_load_cart`. Host softboot is opt-in only (`R01S_SOFTBOOT=1`). See [`PERFORMANCE.md`](PERFORMANCE.md).

**Cart flasher:** isolated from the main sim. Use `flasher_bench` + `test_cart_flash_flow` / `test_island_flasher`.

**1_FRAME_DEBUG:** `1_FRAME_DEBUG=true ./sim cart.retr01` logs byte-scale IC traffic (PRG/MAP/VRAM/beam/BG0/sprites/play) into a paginated on-screen panel (`[` `]` or PgUp/PgDn). Logging stops once BG (MAP->VRAM) and the first sprite field fill are done. Full dump: `retr01_sim/debug/frame1_trace.log`.

Why the worker exists: [`CATCHUP_THREADING.md`](CATCHUP_THREADING.md).

**Next:** optional machine EEPROM (1284 path). Retire bring-up overlay when game PRG owns MAP.

## Test layers

```text
  Layer 1: Unit (one IC)
       |
       v
  Layer 2: Island (few ICs + wires). See docs/hardware.md + test_island_abcdeghiojklmnp.c
       |
       v
  Layer 3: System (full board + cart + input + screen)
```

Layer 1: per-chip harness tests in `tests/`. Layer 2: island smoke in `tests/test_island_abcdeghiojklmnp.c`. Layer 3: full netlist + golden cart `output/test.retr01`.

## Cart ROM vs runners (triage)

When something looks wrong on screen, do not assume the `.retr01` is bad and do not assume the emu/sim is bad. Studio authoring, cart image, emu, and sim are different layers.

### Who owns what

| Layer | Artifact | On silicon / runners? | Notes |
|-------|----------|----------------------|--------|
| **Studio editor** | `output/test.r01proj` (+ UI) | **No** | Authoring only. Does not execute PRG |
| **Studio Play** | export then shared emu | **Yes** (via cart) | Same path as **Ctrl+E** + emu render. No Studio-only soft preview |
| **Cart image** | `output/test.retr01` (+ `test_flash.bin`) | **Yes** (flash) | Packed bytes SoT for PRG/CHR/MAP/pals. Layout in [`docs/graphics.md`](../../docs/graphics.md) |
| **Color PROM burn** | `test_prom.bin` | **Yes** (motherboard) | Not inside the cart. Kit -> R3G3B2. Target part **AT27C256R** ([`hw/md/AT27C256R.md`](../../hw/md/AT27C256R.md)) |
| **Boot asm listing** | `test_boot.s` | Human-readable only | Binary inside `.retr01` is what runners execute |
| **Emulator** | `retr01_emu` | Software-visible CPU/`$FExx` | Loads `.retr01`. Default: PRG catchup streams pals + start MAP. Softboot opt-in (`R01E_SOFTBOOT=1`). Host Play for camera/player. Used by Studio Play and standalone `./emu` |
| **Board sim** | `retr01_sim` | IC / island netlist | Copies argv/default `.retr01` into cart `SST39SF040`. Catchup ~12k pin-level steps. Flasher is `flasher_bench` unit tests only (not on main canvas). Softboot opt-in (`R01S_SOFTBOOT=1`). Host Play after catchup |

### What is in `test.retr01` today

| In ROM | Meaning |
|--------|---------|
| Header + pointer table | magic `retr01`, `format_ver` **2** only, world count, **36 B** ptr table (24-bit offs + lens) |
| Other screens + credits | Title + interstitial + credits **pages** in other-screens blob (raw 480 B or RLE). PRG owns presentation |
| Global BG/sprite palettes | 8 BG rows + 8 sprite rows (master indices, not RGB) |
| SPR CHR banks | Real tile bytes from Studio export (4 banks x up to 256 tiles) |
| Entity tables | Per-world type records (origin, hitbox, up to 4 parts) + instance list (type, world x/y) |
| 32 KB PRG | Palette + start-screen MAP stream (`$FE93` -> `$FE08`/`$FE09`/`$FE12`). Scroll/player/warps still **host** Play |
| World table + blobs | CHR banks, screen dir, 480 B present-screen payloads |

| **Not** in ROM | Meaning |
|----------------|---------|
| **Metasprite catalog** | Studio JSON only. Export flattens parts into entity type records |
| Host Play motion, camera dead zone, player anim, warps | Emu / sim Host Play + `../common/r01_play_camera.c` (cart-backed). Not a Studio-only preview |
| Host Play BG0 | Emu + sim Host Play: cart BG0 cache, show-through under BG1 color 0, proportional scroll from present extents. Host overlay only (does not drive IC VRAM slots 4-7 or `$FE06`/`$FE07`) |
| Host collision source | Cart flash MAP attrs (`R01_ATTR_SOLID`). PRG collision stub not used by host runners |
| Editor UI state | UI only. Cart boots world **0** |
| Live camera seam streaming (2x2 shift) | Phase 1 PRG loads start screen only |
| Full game loop in 6502 | Still future. Host Play stands in |

### How to tell ROM bug vs runner bug

1. **Hex / dump the cart first.** If the dump is wrong, it is Studio export. If right, blame the runner or soft helpers.
2. **Same `.retr01` on emu and sim.** Both wrong the same way -> ROM/content or shared contract (`02`). Only one fails -> that runner.
3. **Studio Play is cart-backed.** It exports then runs emu. If Studio Play looks wrong, dump the cart. Do not assume a separate Studio compositor.
4. **Call out soft helpers.** `R01E_SOFTBOOT=1` / `R01S_SOFTBOOT=1` are opt-in host poke only.
5. **Color wrong?** Check `*_prom.bin` / board PROM path separately from cart palette indices.

## Architecture (summary)

| Topic | Choice |
|-------|--------|
| Time base | PHI2 half-steps from `OSC8M` (`R01S_PHI2_HALF_NS`). Combinatorial settle. `DELAY=typical|max` prints datasheet path budget (pin netlist stays zero-delay so catchup works) |
| Bus | Settle loop (`R01S_SETTLE_PASSES`). H+L -> hard abort. Undriven -> pull-up HIGH (`$FF`) |
| Wiring | Explicit `wire_*` in `board.c`. Global netlist deferred |
| Boot UX | Worker-thread MAP catchup. See [`CATCHUP_THREADING.md`](CATCHUP_THREADING.md) |
| Perf | [`PERFORMANCE.md`](PERFORMANCE.md) |

**Model:** every IC is an `R01sEntity` (pins + vtable). Islands hold entities. Island groups wire them. `r01s_board_build()` binds the full netlist onto **11** canvas frames. Undriven pins pull high. H+L aborts with a bus-fight report.

## Build

From the repo root:

```bash
./build-all
./sim
./unit-tests
```

Developer rebuild of this tree only:

```bash
cd app/sim
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_sim
```

Needs: CMake, a C compiler, SDL2 (`sdl2` package).

## Run

```bash
./sim
./sim output/other.retr01
./sim DELAY=typical   # print typ path budget vs PHI2 half
./sim DELAY=max       # print max (worst-case) path budget
# or: ./sim output/test_2.retr01 DELAY=max
```

`DELAY=typical|max` selects the datasheet corner and prints **path budget** (decode+245+573 vs PHI2 half). The pin netlist stays combinatorial. Deferred HC/PLD outputs miss `STA $FExx` in this settle model. Wall-clock UI FPS != sim ns. See [`PERFORMANCE.md`](PERFORMANCE.md).

**Controls:** `Space` pause/resume * `Ctrl+R` reset * `Ctrl+1` / `Ctrl+2` present scale * `R` rotate selected IC * **SCALE 1X/2X** (left sidebar or `G`. **2X** grows the video island to fit SCR1) * `.` single-step (while paused) * **COMPACT / ISLANDS** (HUD) * **left-drag chip** move * **right-click chip** orient H/V * **left-drag empty island** move frame * **bottom-right grip** resize * **Shift+arrows / wheel / middle-drag** pan * `Esc` quit.

**Layout persistence:** island frames + chip positions saved to `app/sim/ui_layout.json` (override with `R01S_LAYOUT`).

**Gamepads (island E -> `$FE60`/`$FE61`):** bottom-right panels or keyboard. HUD **ARCADE** (default) injects the bitfield directly. **PADS** routes through ATtiny85 poll/reply (`0x55`/`0xAA`) then into the same ports. After boot catchup, **Host Play** uses P1 for move + warps (Studio/emu rules: dead-zone camera, player anim blob, collision from cart MAP attrs):

| | Stick | X | Y | Coin | Start |
|--|-------|---|---|------|-------|
| **P1** | WASD (8-way) | **G** | **H** | 1 | 2 |
| **P2** | Arrows (8-way) | **,** / KP1 | **.** / KP2 | Shift | Enter |

Same keyboard map is used by standalone **emu** and Studio Play.

**Wave monitor (island K):** bottom-left overlay (drag like Islands / pin legend. Position saved in `ui_layout.json`). Lanes **B1-B5** (BGM pulse/pulse/tri/noise/DPCM) and **S6-S8** (SFX) draw mathematical waveforms from voice period/duty/type. Bottom **A** lane plots the mixed analog sample ring (R-2R stand-in). Bring-up smoke still drives B1 via `$FE40`. Sim does **not** play BGM/SFX on the host speaker (timebase). See TODO in [`docs/sound.md`](../../docs/sound.md) for richer WAVE monitor feed later.

Live probe (top-right) shows **VDD / PHI2 / RESB**. Status bar shows CPU `PC` / `AB` / phase / cycle count.

## Layout

| Path | Role |
|------|------|
| `include/retr01_sim/` | Public headers (`entity`, `pin`, `bus`, `board`, `island*`, `types`) |
| `src/board.c` | Board recipe. 10 canvas islands, wiring, settle loop |
| `src/main.c` | SDL entry |
| `chips/` | Per-part models |
| `tests/` | Layer-1 unit tests + `test_island_abcdeghiojklmnp` (layer 2) |
