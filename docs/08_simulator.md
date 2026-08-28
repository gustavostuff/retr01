# retr01 Board Simulator

**Status:** Live under [`retr01_sim/`](../retr01_sim/): 32-IC canvas, island smoke, cart load, IC MAP catchup, Host Play scaffold. IC behavior references: [`hw/md/`](../hw/md/). Current BOM: [`06`](06_hardware_v1_32ic.md). Island checklist: [`03`](03_hardware_implementation.md).

## Goal

Reliably simulate the **retr01-A motherboard** as a system of discrete ICs: each chip is a model with **pins**, **package form**, and **datasheet behavior**, wired like the real board. The end state is a whole-system sim that boots a cart, accepts pad input, and shows a **digital playfield** (logical 128x120 inside a **256x240** RGBS field / LCD sink).

**Accuracy** (cycle-exact PHI2, ns-level AC margins, full AVR peripheral set, etc.) is **defined as we go**. Start with behavior that is correct enough to validate islands and catch bus fights. Tighten timing and ISA coverage when tests demand it.

This simulator is **not** retr01 Studio (authoring). It is the **board IC / netlist validation** path. A separate tighter cycle-level cart check may appear later; do not conflate the two names.

## Three test layers

```text
  Layer 1: Unit (one IC)
       |
       v
  Layer 2: Island (few ICs + wires)
       |
       v
  Layer 3: System (full board + cart + input + screen)
```

### 1. Unit tests (individual ICs)

Each chip model is tested **alone** with a pin harness:

- Drive inputs, clock, reset as needed
- Observe outputs / bus / memory state
- Cover truth tables, address decode, read/write cycles, reset sequences
- Fail hard on undefined multi-drive bus conflicts once the harness knows the net

**Pass:** datasheet-level expectations for that part hold under the chosen accuracy level.

### 2. Island tests (component groups)

Match the bring-up islands in [`03`](03_hardware_implementation.md): wire a **minimum subgraph**, power/clock as required, run smoke checks.

Examples:

| Island | Chips (typical) | Smoke |
|--------|-----------------|-------|
| C | W65C02S + system AS6C62256 + tiny PRG | Fetch, RAM R/W, no bus fight |
| G | CPU + VRAM + interleave mux/PLD | `$FE10`-`$FE12` interleaved R/W |
| N | ATmega1284P + line-buffer SRAM | OAM port, line fill |
| K | ATmega328P APU | Tone / `$FE4x` |

**Pass:** same criteria as the hardware island checklist in `03`.

Layer-2 bring-up for letters **A-E + G + H + I + O + J + K + L + M + N + P** lives in `retr01_sim/tests/test_island_abcdeghiojklmnp.c` (`STA $FE02`, VRAM `$FE12`, `LDA $FE60`, beam HBlank / line advance, **ATF22V10** Y compare vs `$FE04`, BG fetch latches tile `$42` / attr `$07`, Color PROM pixels on 256x240 sink, MAP `$FE93` reads cart magic `'R'`, APU `$FE40`-`$FE42` enables PWM square, OAM `$FE20`/`$FE21` write+readback on 1284, linebuf ping-pong + OAM sprite fill into compositor, VBlank NMI -> CPU).

The SDL **canvas** is **9 frames** (O top-left; A+B, L+M, and former Q HC245s co-located) - see [`03` sim canvas grouping](03_hardware_implementation.md#sim-canvas-grouping) and [`retr01_sim/README.md`](../retr01_sim/README.md).

### 3. System tests (whole board)

Full retr01-A netlist: **current BOM** [`06`](06_hardware_v1_32ic.md) (**32 IC**). Software ports always from [`02`](02_graphics_worlds_memory.md).

- Cart image (`.retr01` / PRG+CHR+MAP)
- Pad bytes (`$FE60`/`$FE61`) from host input
- Video path to a **digital screen** (logical framebuffer or RGBS sample view)
- Optional audio sink (PWM / sample buffer)

**Pass:** cart boots, NMI ~60 Hz class, stable video, controllable pads, no hot / fighting buses in the model.

## Cart ROM vs runners (triage)

When something looks wrong on screen, **do not assume the `.retr01` is bad** and **do not assume the emu/sim is bad**. Studio, cart image, emu, and sim are four different layers. Bugs live in one of them.

### Who owns what

| Layer | Artifact | Runs on silicon / runners? | Notes |
|-------|----------|----------------------------|--------|
| **Studio editor / Play** | `rom/test.r01proj` (+ UI) | **No** | Host preview only — never executes PRG or `$FExx` |
| **Cart image** | `rom/test.retr01` (+ `test_flash.bin`) | **Yes** (flash) | Packed bytes SoT for PRG/CHR/MAP/pals. Magic `retr01`, `format_ver` **1**. Layout in [`02`](02_graphics_worlds_memory.md). |
| **Color PROM burn** | `test_prom.bin` | **Yes** (motherboard) | **Not inside the cart.** Kit -> R3G3B2. Board AT28C16. |
| **Boot asm listing** | `test_boot.s` | **Human-readable only** | Equates + stub source. The **binary inside `.retr01`** is what runners execute (Studio embeds it). Asm can drift. Treat binary as SoT. |
| **Emulator** | `retr01_emu` | Software-visible CPU/`$FExx` | Loads `.retr01`. Default: PRG catchup streams pals + start MAP. Softboot opt-in (`R01E_SOFTBOOT=1`). Host Play for camera/player. Main FB = **VRAM + scroll** + **OAM composite**. `r01e_video_host_pan` is tests-only. |
| **Board sim** | `retr01_sim` | IC / island netlist | **32-IC BOM** on **9 canvas frames**. Golden cart: `rom/test.retr01`. **Loaded cart PRG runs as-is** (Phase 1 streams pals + start MAP via `$FE93`->`$FE12`). Bring-up PRG overlay only when **no cart file** (synthetic). Catchup ~12k pin-level steps. Softboot opt-in (`R01S_SOFTBOOT=1`). **Host Play** after catchup. |

### What is actually in `test.retr01` today

Verified against Studio pack (`r01_cart_build`) and `rom/test.retr01`. Flash pad is 512 KB.

| In ROM | Meaning |
|--------|---------|
| Header + pointer table | magic `retr01`, `format_ver` 1, world count, 24-bit offs + lens ([`02`](02_graphics_worlds_memory.md)) |
| Global BG/sprite palettes | **8** BG rows + **8** sprite rows (**128+128** master indices, not RGB) |
| 32 KB PRG | Phase 1: init (scroll from spawn cam), **palette + start-screen MAP stream** (`$FE93` -> `$FE08`/`$FE09`/`$FE12`), then VBlank wait / pad loop. Play table at `$8100`. Marker `R01P` at `$80F0`. Scroll/player/warps still **host** Play, not 6502 |
| World table + world blobs | CHR banks, screen dir, **480 B** present-screen payloads |

| **Not** in ROM (sugar / other chips / host) | Meaning |
|---------------------------------------------|---------|
| Studio Play motion, camera, warps | Host `play.c` / emu Play / sim Host Play |
| Editor UI state | UI only. Cart boots world **0** |
| Live OAM game sprites from authoring | Stub does not drive a full sprite game. Sim Host Play may write player OAM |
| Live camera **seam** streaming (2x2 shift) | Phase 1 PRG loads **start screen** only. Emu soft-boot fills a workbench for debug. Full seam PRG is future |
| Kit RGB / Play preview colors | Logical kit. Burn path is `*_prom.bin` on the board |
| Full game loop in 6502 | Still future. Host Play stands in |

### How to tell ROM bug vs runner bug

1. **Hex / dump the cart first** - magic, PRG stream bytes, a known screen payload offset, CHR bank. If the dump is wrong, it's Studio export / pack. If the dump is right, blame the runner or soft helpers.
2. **Same `.retr01` on emu and sim** - if both misbehave the same way after a dump-clean cart, prefer ROM/content or shared contract (`02`). If only one runner fails, prefer that runner.
3. **Never use Studio Play as proof the cart boots** - Play bypasses PRG. Use emu (cart image + soft helpers) or sim (flash IC / cart PRG stream) for burnable behavior.
4. **Call out soft helpers explicitly.** Emu always soft-boots + host Plays (main view is not proof of PRG MAP stream). Sim default path runs cart PRG on the netlist. `R01S_SOFTBOOT=1` is opt-in host poke only.
5. **Color wrong?** Check `*_prom.bin` / board PROM path separately from cart palette **indices**.

### Sim readiness

`scripts/run-sim rom/test.retr01` loads the golden cart (path required). Islands **J** (flash PRG/MAP), **K** (APU stub), **L/M** (1284 + linebuf), **N** (sprites), **O** (video), **P** (NMI integration) are wired. Startup catchup runs the MAP/pal stream (~12k pin-level steps on a worker thread); **Host Play** follows until game PRG owns camera/player. Island/canvas details: [`retr01_sim/README.md`](../retr01_sim/README.md).

### Softboot (opt-in only)

Set `R01S_SOFTBOOT=1` to poke VRAM/pals without the CPU stream (debug). Default = cart PRG or synthetic bring-up overlay on the netlist.

## Modeling principles

1. **IC-first:** one module per part number. Pins named after the datasheet.
2. **Netlist second:** islands and the full board are graphs of pin connections.
3. **retr01-only:** no generic multi-board sandbox. Contracts and clocks match this project.
4. **Tests before polish:** unit -> island -> system. Layer 1 does not require a GUI.
5. **Sources of truth:** `hw/*.pdf` + `hw/md/*.md` for pin/behavior, `docs/02` for `$FExx`, `docs/06` for the board netlist ([`01`](01_architecture_overview.md)).

## Architecture (summary)

| Topic | Choice |
|-------|--------|
| Time base | PHI2 half-steps from `OSC8M`; combinatorial parts settle in wire pass, no `tick` |
| Bus | **Settle loop** (`R01S_SETTLE_PASSES`); H+L → hard abort; undriven → pull-up HIGH (`$FF`) |
| Wiring | Explicit `wire_*` in `board.c`; global netlist deferred |
| Dual clock | Future: LCM master tick when ATmega domains land (20 MHz + 8 MHz) |
| Boot UX | Worker-thread MAP catchup — [`CATCHUP_THREADING.md`](../retr01_sim/CATCHUP_THREADING.md) |
| Perf notes | [`PERFORMANCE.md`](../retr01_sim/PERFORMANCE.md) |

Pin-level models stay default for catching PCB bugs early.

## Next

- Retire **Host Play** when game PRG owns camera/player
- `hw/md/` IC reference batches; optional machine EEPROM island **F**
- Later: bus bitmasks, super-components, JEDEC fuse PLD engine

Run/build/controls: [`retr01_sim/README.md`](../retr01_sim/README.md).
