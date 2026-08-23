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

Layer-2 bring-up for **A–E + G + H + I** lives in `retr01_sim/tests/test_island_abcdeghi.c` (`STA $FE02`, VRAM `$FE12`, `LDA $FE60`, beam HBlank / line advance, HC688 vs `$FE04`, BG fetch latches tile `$42` / attr `$07`).

### 3. System tests (whole board)

Full Retr01-A netlist: **current BOM** [`06`](06_hardware_v1_32ic.md) (**32 IC**). Optional legacy ~52 netlist only if explicitly testing that path ([`03`](03_hardware_implementation.md)). Software ports always from [`02`](02_graphics_worlds_memory.md).

- Cart image (`.retr01` / PRG+CHR+MAP)
- Pad bytes (`$FE60`/`$FE61`) from host input
- Video path to a **digital screen** (logical framebuffer or RGBS sample view)
- Optional audio sink (PWM / sample buffer)

**Pass:** cart boots, NMI ~60 Hz class, stable video, controllable pads, no hot / fighting buses in the model.

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
| Later | Island **O** video/LCD, cart **J**, remaining ICs, pads→1284; then optimization passes |

## Related docs

| Doc | Role |
|-----|------|
| [`hw/md/README.md`](../hw/md/README.md) | Index of IC markdown references |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Memory map, `$FExx`, cart |
| [`06_hardware_v1_32ic.md`](06_hardware_v1_32ic.md) | Current Retr01-A BOM (**32 IC**) |
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | Island checklist / legacy ~52 notes |
