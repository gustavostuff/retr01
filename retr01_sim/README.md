# retr01 Board Simulator

IC-first board simulator for retr01-A. Separate from retr01 Studio (authoring). Pin/behavior: [`hw/md/`](../hw/md/). BOM: [`docs/06_hardware_v1_32ic.md`](../docs/06_hardware_v1_32ic.md). Island checklist: [`docs/03_hardware_implementation.md`](../docs/03_hardware_implementation.md).

**Goal:** simulate the retr01-A motherboard as discrete ICs (pins, package, datasheet behavior) wired like the real board. End state: boot a cart, accept pad input, show a digital playfield (logical 128x120 inside a 256x240 RGBS field / LCD sink). Accuracy tightens as tests demand it.

## Status

**9 canvas islands (O first / top-left) + wired-only E/I/N/P - 32-IC BOM, layer-2 smoke.** SDL board UI.

| Island | Components (canvas) |
|--------|---------------------|
| O Video | `COMPOSITOR` + `AT28C16` + `LCD_SINK` + video `SN74HC245` (top-left) |
| A Power+clk | `PWR5V` + `OSC8M` + `SN74HC14` |
| C CPU + decode | `W65C02S`, `AS6C62256`, `ATF22V10` decode, CPU `SN74HC245` |
| D `$FExx` latch | **9x** `SN74HC573` (`$FE02`-`$FE04`, `$FE08`, `$FE10`-`$FE12`, `$FE90`-`$FE92`) |
| E Pads | `$FE60`/`$FE61` via 1284 (sim model, not on canvas) |
| G VRAM | 2nd `AS6C62256` + **3x** `SN74HC157` + `ATF22V10` VRAM glue |
| H Beam | `OSC_DOT` + `BEAM_XY` (X PLD) + `ATF22V10` Y compare vs `$FE04` |
| I BG fetch | `BG_FETCH` PLD - nametable VA from beam+scroll (not on canvas) |
| J Cart | `SST39SF040` + cart `24C64` + cart/OAM `SN74HC245` |
| K APU | `ATMEGA328P` stub - `$FE40`-`$FE5F` regs + digital PWM square |
| L MCU+linebuf | `ATMEGA1284P` + linebuf `AS6C62256` + **3x** `SN74HC157` |
| N Sprites | stats via 1284/OAM (not on canvas) |
| P Integration | NMI / bus-fight stats (not on canvas) |

Bench-only (wired, not on canvas): `PRG_ROM` fallback when cart does not own `$8000+`.

**Cart load:** `scripts/run-sim rom/test.retr01` (cart path required). The **6502 executes cart PRG from flash** (Studio export includes palette + MAP->VRAM boot via `$FE93`->`$FE12`). Startup catchup runs that stream on a **worker thread** (~12k pin-level steps) so the SDL window stays responsive. Synthetic test cart still uses sim bring-up overlay for island smoke. Host softboot is opt-in only (`R01S_SOFTBOOT=1`). See [`PERFORMANCE.md`](PERFORMANCE.md).

Why the worker exists: [`CATCHUP_THREADING.md`](CATCHUP_THREADING.md).

**Next:** optional machine EEPROM (1284 path). Retire bring-up overlay when game PRG owns MAP.

## Test layers

```text
  Layer 1: Unit (one IC)
       |
       v
  Layer 2: Island (few ICs + wires)  -- see docs/03 + test_island_abcdeghiojklmnp.c
       |
       v
  Layer 3: System (full board + cart + input + screen)
```

Layer 1: per-chip harness tests in `tests/`. Layer 2: island smoke in `tests/test_island_abcdeghiojklmnp.c`. Layer 3: full netlist + golden cart `rom/test.retr01`.

## Cart ROM vs runners (triage)

When something looks wrong on screen, do not assume the `.retr01` is bad and do not assume the emu/sim is bad. Studio, cart image, emu, and sim are four different layers.

### Who owns what

| Layer | Artifact | On silicon / runners? | Notes |
|-------|----------|----------------------|--------|
| **Studio editor / Play** | `rom/test.r01proj` (+ UI) | **No** | Host preview only - never executes PRG or `$FExx` |
| **Cart image** | `rom/test.retr01` (+ `test_flash.bin`) | **Yes** (flash) | Packed bytes SoT for PRG/CHR/MAP/pals. Layout in [`docs/02`](../docs/02_graphics_worlds_memory.md) |
| **Color PROM burn** | `test_prom.bin` | **Yes** (motherboard) | Not inside the cart. Kit -> R3G3B2. Board AT28C16 |
| **Boot asm listing** | `test_boot.s` | Human-readable only | Binary inside `.retr01` is what runners execute |
| **Emulator** | `retr01_emu` | Software-visible CPU/`$FExx` | Loads `.retr01`. Default: PRG catchup streams pals + start MAP. Softboot opt-in (`R01E_SOFTBOOT=1`). Host Play for camera/player |
| **Board sim** | `retr01_sim` | IC / island netlist | Loaded cart PRG runs as-is. Bring-up overlay only when **no cart file**. Catchup ~12k pin-level steps. Softboot opt-in (`R01S_SOFTBOOT=1`). Host Play after catchup |

### What is in `test.retr01` today

| In ROM | Meaning |
|--------|---------|
| Header + pointer table | magic `retr01`, `format_ver` 1, world count, 24-bit offs + lens |
| Global BG/sprite palettes | 8 BG rows + 8 sprite rows (master indices, not RGB) |
| 32 KB PRG | Palette + start-screen MAP stream (`$FE93` -> `$FE08`/`$FE09`/`$FE12`). Scroll/player/warps still **host** Play |
| World table + blobs | CHR banks, screen dir, 480 B present-screen payloads |

| **Not** in ROM | Meaning |
|----------------|---------|
| Studio Play motion, camera, warps | Host `play.c` / emu Play / sim Host Play |
| Host collision source | Cart flash MAP attrs (`R01_ATTR_SOLID`). PRG collision stub not used by host runners |
| Editor UI state | UI only. Cart boots world **0** |
| Live camera seam streaming (2x2 shift) | Phase 1 PRG loads start screen only |
| Full game loop in 6502 | Still future. Host Play stands in |

### How to tell ROM bug vs runner bug

1. **Hex / dump the cart first** - if the dump is wrong, it is Studio export. If right, blame the runner or soft helpers.
2. **Same `.retr01` on emu and sim** - both wrong the same way -> ROM/content or shared contract (`02`). Only one fails -> that runner.
3. **Never use Studio Play as proof the cart boots** - Play bypasses PRG.
4. **Call out soft helpers.** `R01E_SOFTBOOT=1` / `R01S_SOFTBOOT=1` are opt-in host poke only.
5. **Color wrong?** Check `*_prom.bin` / board PROM path separately from cart palette indices.

## Architecture (summary)

| Topic | Choice |
|-------|--------|
| Time base | PHI2 half-steps from `OSC8M`. Combinatorial parts settle in wire pass |
| Bus | Settle loop (`R01S_SETTLE_PASSES`). H+L -> hard abort. Undriven -> pull-up HIGH (`$FF`) |
| Wiring | Explicit `wire_*` in `board.c`. Global netlist deferred |
| Boot UX | Worker-thread MAP catchup - [`CATCHUP_THREADING.md`](CATCHUP_THREADING.md) |
| Perf | [`PERFORMANCE.md`](PERFORMANCE.md) |

**Model:** every IC is an `R01sEntity` (pins + vtable). Islands hold entities. Island groups wire them. `r01s_board_build()` binds the full netlist onto 9 canvas frames. Undriven pins pull high. H+L aborts with a bus-fight report.

## Build

```bash
cd retr01_sim
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_sim
```

Or from repo root:

```bash
scripts/run-unit-tests
scripts/run-sim rom/test.retr01
```

Needs: CMake, a C compiler, SDL2 (`sdl2` package).

## Run

```bash
scripts/run-sim rom/test.retr01
# or: ./build/retr01_sim /path/to/cart.retr01
```

**Controls:** `Space` pause/resume * `Ctrl+R` reset * `R` rotate selected IC * **SCALE 1X/2X** (left sidebar button or `G`, default **1x**) * `.` single-step (while paused) * **COMPACT / ISLANDS** (HUD) * **left-drag chip** move * **right-click chip** orient H/V * **left-drag empty island** move frame * **bottom-right grip** resize * **Shift+arrows / wheel / middle-drag** pan * `Esc` quit.

**Layout persistence:** island frames + chip positions saved to `retr01_sim/ui_layout.json` (override with `R01S_LAYOUT`).

**Gamepads (island E -> `$FE60`/`$FE61`):** bottom-left panels or keyboard. After boot catchup, **Host Play** uses P1 for move + warps (Studio/emu rules, collision from cart MAP attrs):

| | Stick | X (warp -> screen 0,0) | Y (warp -> screen 1,0) | Coin | Start |
|--|-------|----------------------|----------------------|------|-------|
| **P1** | Arrows or WASD (8-way) | **X** or Z | **Y** | 1 | Enter |
| **P2** | IJKL (8-way) | N | M | 2 | Backspace |

Live probe (top-right) shows **VDD / PHI2 / RESB**. Status bar shows CPU `PC` / `AB` / phase / cycle count.

## Layout

| Path | Role |
|------|------|
| `include/retr01_sim/` | Public headers (`entity`, `pin`, `bus`, `board`, `island*`, `types`) |
| `src/board.c` | Board recipe - 9 canvas islands, wiring, settle loop |
| `src/main.c` | SDL entry |
| `chips/` | Per-part models |
| `tests/` | Layer-1 unit tests + `test_island_abcdeghiojklmnp` (layer 2) |
