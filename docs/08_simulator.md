# Retr01 Board Simulator

**Status:** Scaffold under [`retr01_sim/`](../retr01_sim/). IC behavior references: [`hw/md/`](../hw/md/). Current BOM: [`06`](06_hardware_v1_32ic.md). Island checklist: [`03`](03_hardware_implementation.md).

## Goal

Reliably simulate the **Retr01-A motherboard** as a system of discrete ICs: each chip is a model with **pins**, **package form**, and **datasheet behavior**, wired like the real board. The end state is a whole-system sim that boots a cart, accepts pad input, and shows a **digital playfield** (logical 128x120 / RGBS raster path as needed).

**Accuracy** (cycle-exact PHI2, ns-level AC margins, full AVR peripheral set, etc.) is **defined as we go**. Start with behavior that is correct enough to validate islands and catch bus fights; tighten timing and ISA coverage when tests demand it.

This simulator is **not** Retr01 Studio (authoring). It is the **board IC / netlist validation** path. A separate tighter cycle-level cart check may appear later; do not conflate the two names.

## Three test layers

```text
  Layer 1 -- Unit (one IC)
       |
       v
  Layer 2 -- Island (few ICs + wires)
       |
       v
  Layer 3 -- System (full board + cart + input + screen)
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

Layer-2 bring-up for **A–E + G + H + I + O + J + K** lives in `retr01_sim/tests/test_island_abcdeghiojk.c` (`STA $FE02`, VRAM `$FE12`, `LDA $FE60`, beam HBlank / line advance, HC688 vs `$FE04`, BG fetch latches tile `$42` / attr `$07`, Color PROM pixels on 128×120 sink, MAP `$FE93` reads cart magic `'R'`, APU `$FE40`–`$FE42` enables PWM square).

### 3. System tests (whole board)

Full Retr01-A netlist: **current BOM** [`06`](06_hardware_v1_32ic.md) (**32 IC**). Optional legacy ~52 netlist only if explicitly testing that path ([`03`](03_hardware_implementation.md)). Software ports always from [`02`](02_graphics_worlds_memory.md).

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
| **Studio editor / Play** | `project.json` (+ UI) | **No** | Paint, OAM place, Play scroll/fade, active world tab — **host preview only**. Play samples authored data; it never executes PRG or `$FExx`. |
| **Cart image** | `project.retr01` (+ optional `project_flash.bin`) | **Yes** (flash) | Packed bytes SoT for PRG/CHR/MAP/pals. Magic `RETR01` layout in [`02`](02_graphics_worlds_memory.md). |
| **Color PROM burn** | `project_prom.bin` | **Yes** (motherboard) | **Not inside the cart.** Kit → R3G3B2; board AT28C16. |
| **Boot asm listing** | `project_boot.s` | **Human-readable only** | Equates + stub source. The **binary stub inside `.retr01`** is what runners execute (Studio embeds it; asm can drift — treat binary as SoT). |
| **Emulator** | `retr01_emu` | Software-visible CPU/`$FExx` | Loads `.retr01`. Today also **soft-boots** world CHR/MAP into VRAM and **host-pans** the atlas — Studio stub PRG does **not** stream MAP. |
| **Board sim** | `retr01_sim` | IC / island netlist | Islands A–E+G+H+I+O+J+**K**. Cart flash loads `.retr01`; **bring-up PRG overlay** replaces cart PRG for island smoke (call it out — not Studio ROM). CHR→video still stubbed. |

### What is actually in `project.retr01` today

Verified against Studio pack (`r01_cart_build`) and the checked-in `retr01_studio/project.retr01` (~138 KB; flash pad is 512 KB):

| In ROM | Meaning |
|--------|---------|
| Header + pointer table | magic, format, world count, I2C-save flag, 24-bit offs |
| Global BG/sprite palettes | 16+16 master indices (not RGB) |
| 32 KB PRG | **Boring stub only**: `SEI`/`CLD`/`TXS`, `STA $FE30` (world **0**), clear scroll, **hang**. Constraint bytes at `$8100+` are data, not a game loop. |
| World table + world blobs | CHR banks, optional world pals, screen/parallax dirs, **480 B** screen payloads |

| **Not** in ROM (sugar / other chips / host) | Meaning |
|---------------------------------------------|---------|
| Studio Play motion, deadzone, fade | Host `play.c` from JSON constraints |
| Editor active world tab | UI only; stub always boots world **0** (`_boot.s` `START_WORLD` can disagree — binary wins) |
| OAM/meta placement as live sprites | Authored into project; cart CHR may hold banks, but stub never writes `$FE20`/`$FE21` |
| Live camera seam streaming | Needs real PRG + `$FE90`–`$FE93`; emu soft-boot / host pan stand in today |
| Kit RGB / Play preview colors | Logical kit; burn path is `*_prom.bin` on the board |
| Full constraints→cc65 game | Still future toolchain; not in this stub |

### How to tell ROM bug vs runner bug

1. **Hex / dump the cart first** — magic, PRG stub bytes, a known screen payload offset, CHR bank. If the dump is wrong, it’s Studio export / pack. If the dump is right, blame the runner or soft helpers.
2. **Same `.retr01` on emu and (later) sim** — if both misbehave the same way after a dump-clean cart, prefer ROM/content or shared contract (`02`). If only one runner fails, prefer that runner.
3. **Never use Studio Play as proof the cart boots** — Play bypasses PRG. Use emu (cart load) or sim (flash IC) for burnable behavior.
4. **Call out soft helpers explicitly** — emu `r01e_ppu_boot_world` / host pan are **runner conveniences**, not silicon and not in the stub. A “blank screen” with stub PRG and **no** soft-boot is expected until PRG streams MAP or the runner soft-loads.
5. **Color wrong?** Check `*_prom.bin` / board PROM path separately from cart palette **indices**.

### Sim readiness to load `project.retr01`

**Island J wired.** Load with `./sim run -- retr01_studio/project.retr01` (or auto-detect). Flash owns PRG `$8000+` and MAP `$FE90`–`$FE93` (one `/CE` context at a time). Sim **overlays bring-up smoke PRG** into the cart PRG window so island checks still pass — dump flash/`off_prg` to see overlay vs Studio stub.

**Island K wired.** `$FE40`–`$FE5F` on ATmega328P stub; bring-up enables a period/vol square; health watches PWM edges. Not a full AVR core or host audio sink — digital PWM pin only.

Still missing for “cart looks like Play”: CHR fetch into Island O, real game PRG (no overlay), MAP streaming of screens (stub hangs). Use emu soft-boot for atlas viewing; use sim for bus/island validation.

## Modeling principles

1. **IC-first:** one module per part number; pins named after the datasheet.
2. **Netlist second:** islands and the full board are graphs of pin connections.
3. **Retr01-only:** no generic multi-board sandbox; contracts and clocks match this project.
4. **Tests before polish:** unit -> island -> system. Layer 1 does not require a GUI.
5. **Sources of truth:** `hw/*.pdf` + `hw/md/*.md` for pin/behavior; `docs/02` for `$FExx`; `docs/06` for the board netlist ([`01`](01_architecture_overview.md)).

## Architecture decisions (Phase 1)

Answers to the foundation questions from the simulator roadmap. **Optimize later** (string kill, bus bitmasks, super-components, DOD) only after a pixel is on screen.

### 1. Master clock — how time moves

| Decision | Choice |
|----------|--------|
| Style | **Entity array / island-group step** — `r01s_island_group_step()` runs the board vtable; each step advances OSC PHI2 and (on rising edge) the W65C02S. |
| Current base | **PHI2 half-steps** from `OSC8M` (8 MHz class). No 20 MHz AVR domain yet. |
| When ATmega lands | Plan a **virtual master** at LCM of the crystals (e.g. **40 MHz**), with per-domain counters so 20 MHz and 8 MHz chips tick on their intervals. Until then, stay PHI2-centric. |

Combinatorial parts (`eval` only) do **not** tick; they settle inside the board wire pass.

### 2. Bus resolution — electrical propagation

| Decision | Choice |
|----------|--------|
| Ordering | **Settle loop** — each half-step runs `R01S_SETTLE_PASSES` (currently **4**) of wire + combinatorial `eval` so decode → CE → DQ can propagate before the next clock edge. Raise the constant when PLD/glue depth grows. |
| Conflict | `r01s_level_merge` / `r01s_bus_resolve`: H+L → **hard abort** (`exit(1)`) with a stderr report (net, both drivers/levels, why). Reading a pin already at `X` also aborts. Unit tests may call `r01s_bus_set_fatal_conflicts(0)` to assert on `X` without exiting. |
| Undriven net | **Pull-up to HIGH** — `r01s_bus_read` treats `Z` as `H` (`r01s_level_pulled`), so an idle data bus reads **`$FF`**, matching motherboard pull-ups. |

Wires today are **explicit copy/resolve in the board recipe** (`src/board.c`), not a global netlist object. Pins are per-entity; the board owns how they connect. A pointer-netlist can replace copies later without changing chip models.

### 3. State management — where wires live

| Decision | Choice |
|----------|--------|
| Pin state | On each `R01sPin.level` inside the owning `R01sEntity`. |
| Cross-chip | Board `wire_*` functions drive destination pins from sensed sources (and `r01s_bus_resolve` when two drivers share a net). |
| Not yet | Global netlist object / pin-to-pin pointers. Deferred until island count makes copy wiring painful. |

### 4. Dual clock domains (future)

Documented plan only: when Island N/K (ATmega) is simulated, introduce a master tick at the LCM of 20 MHz and 8 MHz and gate each chip’s `tick()` with a divider. Do **not** pretend the domains are synchronous.

### 5. Optimization playbook (deferred)

Do **not** apply until Layer 3 shows a pixel:

1. Cache pin indices / kill hot-path `strcmp` / `snprintf`
2. Bitmask major buses (`uint16_t address_bus`, etc.)
3. Merge proven glue into a “super component”
4. Flat entity array for cache locality

Current pin-level code is intentional for catching PCB bugs early.

## Near-term focus

| Priority | Work |
|----------|------|
| 1 | `hw/md/` IC reference docs (batches: CPU/MCU, memory, glue/video) |
| 2 | Islands **A–E** models + unit tests + layer-2 smoke (**done**) |
| 3 | Island **G** (VRAM interleave) / more `$FExx` latches (**done** — soft `$FE10`–`$FE12`) |
| 4 | Island **H** beam (**done** — `OSC_DOT` + `BEAM_XY` PLD stub + `HC688` / `$FE04`) |
| 5 | Island **I** BG fetch (**done** — `BG_FETCH` nametable VA + PPU-phase VRAM read) |
| 6 | Island **O** video (**done** — compositor + AT28C16 + 128×120 sink; CHR still stubbed) |
| 7 | Island **J** cart (**done** — SST39SF040, PRG + MAP `$FE90`–`$FE93`, `.retr01` load + bring-up PRG overlay) |
| Next | CHR from cart into video path; retire bring-up PRG overlay when real PRG streams MAP |
| Later | Remaining ICs, pads→1284; then optimization passes |

## Related docs

| Doc | Role |
|-----|------|
| [`hw/md/README.md`](../hw/md/README.md) | Index of IC markdown references |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Memory map, `$FExx`, cart |
| [`06_hardware_v1_32ic.md`](06_hardware_v1_32ic.md) | Current Retr01-A BOM (**32 IC**) |
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | Island checklist / legacy ~52 notes |
