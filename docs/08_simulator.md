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
| **Studio editor / Play** | `test_game/test.r01proj` (+ UI) | **No** | Phase 1: PNG import, Worlds/Screen UI, Play scroll/warps. **Host preview only.** Play samples authored data. It never executes PRG or `$FExx`. |
| **Cart image** | `test_game/test.retr01` (+ `test_flash.bin`) | **Yes** (flash) | Packed bytes SoT for PRG/CHR/MAP/pals. Magic `retr01`, `format_ver` **1**. Layout in [`02`](02_graphics_worlds_memory.md). |
| **Color PROM burn** | `test_prom.bin` | **Yes** (motherboard) | **Not inside the cart.** Kit -> R3G3B2. Board AT28C16. |
| **Boot asm listing** | `test_boot.s` | **Human-readable only** | Equates + stub source. The **binary inside `.retr01`** is what runners execute (Studio embeds it). Asm can drift. Treat binary as SoT. |
| **Emulator** | `retr01_emu` | Software-visible CPU/`$FExx` | Loads `.retr01`. Default: PRG catchup streams pals + start MAP. Softboot opt-in (`R01E_SOFTBOOT=1`). Host Play for camera/player. Main FB = **VRAM + scroll** + **OAM composite**. `r01e_video_host_pan` is tests-only. |
| **Board sim** | `retr01_sim` | IC / island netlist | **32-IC BOM** on **9 canvas frames**. Default cart: `retr01_studio/test_game/test.retr01`. **Loaded cart PRG runs as-is** (Phase 1 streams pals + start MAP via `$FE93`->`$FE12`). Bring-up PRG overlay only when **no cart file** (synthetic). Catchup ~12k PIN steps (or FAST word apply). Softboot opt-in (`R01S_SOFTBOOT=1`). **Host Play** after catchup in **both** PIN and FAST. |

### What is actually in `test.retr01` today

Verified against Studio pack (`r01_cart_build`) and `retr01_studio/test_game/test.retr01`. Flash pad is 512 KB.

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
4. **Call out soft helpers explicitly.** Emu always soft-boots + host Plays (main view is not proof of PRG MAP stream). Sim default path runs cart PRG on the netlist (or FAST word apply). `R01S_SOFTBOOT=1` is opt-in host poke only.
5. **Color wrong?** Check `*_prom.bin` / board PROM path separately from cart palette **indices**.

### Sim readiness to load `test.retr01`

**Island J wired.** `./sim run` defaults to `retr01_studio/test_game/test.retr01` (override with a path). Flash owns PRG `$8000+` and MAP `$FE90`-`$FE93` (one `/CE` context at a time). **File load keeps Studio PRG.** Synthetic (no-file) path installs bring-up overlay for island smoke.

**Island K wired.** `$FE40`-`$FE5F` on ATmega328P stub. Bring-up enables a period/vol square. Health watches PWM edges. Not a full AVR core or host audio sink, digital PWM pin only.

**Island L (+ M on same canvas) wired.** ATmega1284P stub: OAM `$FE20`/`$FE21` auto-inc, 20 MHz domain tick counter, soft `$FE70`-`$FE72` mailbox, third `AS6C62256` + `SN74HC157` linebuf ping-pong on the same frame. Pads remain letter **E** (wired via 1284).

**Island N wired.** HBlank OAM scan fills next linebuf half (sprite CHR from flash when cart meta present). Compositor takes sprite pixels.

**Island P wired.** Beam `NMI#` -> CPU `NMIB` on VBlank entry. Integration health wants pads + video + sprites + >=1 NMI pulse and zero bus conflicts. Optional **F** (machine EEPROM) still deferred.

Startup **`r01s_board_catchup_bringup`** runs the MAP/pal stream (~12k PIN steps, or FAST word apply) so the LCD hold lifts. Host softboot is opt-in (`R01S_SOFTBOOT=1`). **Host Play** starts after catchup in PIN and FAST (Studio/emu SoT: move, camera, X->(0,0) / Y->(1,0) warps) until game PRG owns camera/player. Island O still fetches 2bpp CHR from flash.

### Softboot (opt-in only)

`r01s_board_softboot_start_screen` is **not** the default. Set `R01S_SOFTBOOT=1` to poke VRAM/pals and advance `map_addr` without the CPU stream (debug / triage). Default = IC stream ownership (cart PRG or synthetic bring-up overlay).

| Status | Notes |
|--------|--------|
| **Done** | Cart / bring-up PRG streams start MAP+pals via `$FE93`->`$FE12` / `$FE08`/`$FE09` |
| **Done** | Catchup on netlist (~12k steps) or FAST word apply |
| **Temp** | Host Play after catchup in **both** PIN and FAST (retire when game PRG owns Play) |
| **Next** | Game PRG owns camera/player. Retire synthetic bring-up overlay + host Play |

## Modeling principles

1. **IC-first:** one module per part number. Pins named after the datasheet.
2. **Netlist second:** islands and the full board are graphs of pin connections.
3. **retr01-only:** no generic multi-board sandbox. Contracts and clocks match this project.
4. **Tests before polish:** unit -> island -> system. Layer 1 does not require a GUI.
5. **Sources of truth:** `hw/*.pdf` + `hw/md/*.md` for pin/behavior, `docs/02` for `$FExx`, `docs/06` for the board netlist ([`01`](01_architecture_overview.md)).

## Architecture decisions (Phase 1)

Answers to the foundation questions from the simulator roadmap. **Optimize later** (string kill, bus bitmasks, super-components, DOD) only after a pixel is on screen.

### 1. Master clock - how time moves

| Decision | Choice |
|----------|--------|
| Style | **Entity array / island-group step** - `r01s_island_group_step()` runs the board vtable; each step advances OSC PHI2 and (on rising edge) the W65C02S. |
| Current base | **PHI2 half-steps** from `OSC8M` (8 MHz class). No 20 MHz AVR domain yet. |
| When ATmega lands | Plan a **virtual master** at LCM of the crystals (e.g. **40 MHz**), with per-domain counters so 20 MHz and 8 MHz chips tick on their intervals. Until then, stay PHI2-centric. |

Combinatorial parts (`eval` only) do **not** tick. They settle inside the board wire pass.

### 2. Bus resolution - electrical propagation

| Decision | Choice |
|----------|--------|
| Ordering | **Settle loop** - each half-step runs `R01S_SETTLE_PASSES` (currently **2**, or **1** in SIM FAST) of wire + combinatorial `eval` so decode -> CE -> DQ can propagate before the next clock edge. Raise the constant when PLD/glue depth grows. |
| Conflict | `r01s_level_merge` / `r01s_bus_resolve`: H+L -> **hard abort** (`exit(1)`) with a stderr report (net, both drivers/levels, why). Reading a pin already at `X` also aborts. Unit tests may call `r01s_bus_set_fatal_conflicts(0)` to assert on `X` without exiting. |
| Undriven net | **Pull-up to HIGH** - `r01s_bus_read` treats `Z` as `H` (`r01s_level_pulled`), so an idle data bus reads **`$FF`**, matching motherboard pull-ups. |

Wires today are **explicit copy/resolve in the board recipe** (`src/board.c`), not a global netlist object. Pins are per-entity. The board owns how they connect. A pointer-netlist can replace copies later without changing chip models.

### 3. State management - where wires live

| Decision | Choice |
|----------|--------|
| Pin state | On each `R01sPin.level` inside the owning `R01sEntity`. |
| Cross-chip | Board `wire_*` functions drive destination pins from sensed sources (and `r01s_bus_resolve` when two drivers share a net). |
| Not yet | Global netlist object / pin-to-pin pointers. Deferred until island count makes copy wiring painful. |

### 4. Dual clock domains (future)

Documented plan only: when Island N/K (ATmega) is simulated, introduce a master tick at the LCM of 20 MHz and 8 MHz and gate each chip's `tick()` with a divider. Do **not** pretend the domains are synchronous.

### 5. Optimization playbook

1. **Done:** cache pin indices - lazy `pin_hash_idx` in `r01s_entity_pin_named` (see [`PERFORMANCE.md`](../retr01_sim/PERFORMANCE.md))
2. **Done:** CPU address/RWB from chip model in `board.c` wire path (not 16 pin re-reads per settle)
3. Bitmask major buses (`uint16_t address_bus`, etc.) - partial via (2)
4. Merge proven glue into a "super component"
5. Flat entity array for cache locality
6. **Done (toggle):** `R01S_FAST=1` / UI **SIM FAST**. Word MAP catchup + thin settle/beam. Host Play is separate (on after catchup in both modes). Default remains full pin settle ([`CATCHUP_THREADING.md`](../retr01_sim/CATCHUP_THREADING.md), [`PERFORMANCE.md`](../retr01_sim/PERFORMANCE.md))

Pin-level code stays the default for catching PCB bugs early.

## Near-term focus

| Priority | Work |
|----------|------|
| **Done** | **IC MAP stream default** (cart PRG or synthetic bring-up). Softboot opt-in (`R01S_SOFTBOOT=1`). **Host Play** after catchup in PIN and FAST |
| 1 | `hw/md/` IC reference docs (batches: CPU/MCU, memory, glue/video) |
| 2 | Islands **A-E** models + unit tests + layer-2 smoke (**done**) |
| 3 | Island **G** (VRAM interleave) / more `$FExx` latches (**done**, soft `$FE10`-`$FE12`) |
| 4 | Island **H** beam (**done**, `OSC_DOT` + `BEAM_XY` X PLD + `ATF22V10` Y compare / `$FE04`) |
| 5 | Island **I** BG fetch (**done**, `BG_FETCH` nametable VA + PPU-phase VRAM read) |
| 6 | Island **O** video (**done**, compositor + AT28C16 + 256x240 sink. SCALE **1x** default / 2x fills field. CHR from cart flash + `$FE08`/`$FE09` active pals) |
| 7 | Island **J** cart (**done**, SST39SF040 + 24C64 EEPROM, PRG + MAP `$FE90`-`$FE93`, `.retr01` load. Bring-up overlay for synthetic cart only) |
| 8 | **32-IC canvas** (**done**, **9 frames**, O top-left, AUB, LUM, HC245s on C/O/J, letters E/I/N/P wired-only) |
| Next | Wire HC245 into bus paths. Retire Host Play when game PRG owns camera/player |
| Later | Pads->1284 on canvas, optimization passes, JEDEC fuse PLD engine |

## Related docs

| Doc | Role |
|-----|------|
| [`hw/md/README.md`](../hw/md/README.md) | Index of IC markdown references |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Memory map, `$FExx`, cart |
| [`06_hardware_v1_32ic.md`](06_hardware_v1_32ic.md) | Current retr01-A BOM (**32 IC**) |
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | Protoboard letter islands + [sim canvas grouping](03_hardware_implementation.md#sim-canvas-grouping) |
| [`retr01_sim/README.md`](../retr01_sim/README.md) | Sim status / canvas island table |
