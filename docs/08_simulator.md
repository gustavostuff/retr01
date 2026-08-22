# Retr01 Board Simulator

**Status:** Planning. Code will live under `sim/` (not started). IC behavior references: [`hw/md/`](../hw/md/). Current BOM: [`06`](06_hardware_v1_32ic.md). Island checklist: [`03`](03_hardware_implementation.md).

## Goal

Reliably simulate the **Retr01-A motherboard** as a system of discrete ICs: each chip is a model with **pins**, **package form**, and **datasheet behavior**, wired like the real board. The end state is a whole-system sim that boots a cart, accepts pad input, and shows a **digital playfield** (logical 128x120 / RGBS raster path as needed).

**Accuracy** (cycle-exact PHI2, ns-level AC margins, full AVR peripheral set, etc.) is **defined as we go**. Start with behavior that is correct enough to validate islands and catch bus fights; tighten timing and ISA coverage when tests demand it.

This simulator is **not** Retr01 Studio (authoring). It is the **hardware validation** path before / beside silicon.

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
4. **Tests before polish:** unit -> island -> system. Visual DIP widgets are Phase 2+ UI (see earlier plan); Layer 1 does not require a GUI.
5. **Sources of truth:** `hw/*.pdf` + `hw/md/*.md` for pin/behavior; `docs/02` for `$FExx`; `docs/06` for the board netlist ([`01`](01_architecture_overview.md)).

## Near-term focus

| Priority | Work |
|----------|------|
| 1 | `hw/md/` IC reference docs (batches: CPU/MCU, memory, glue/video) |
| 2 | `sim/` scaffolding + **W65C02S** + **AS6C62256** unit tests (system RAM role) |
| 3 | Link CPU + RAM island (island C) |
| Later | Remaining ICs, video, cart, pads, screen |

## Related docs

| Doc | Role |
|-----|------|
| [`hw/md/README.md`](../hw/md/README.md) | Index of IC markdown references |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Memory map, `$FExx`, cart |
| [`06_hardware_v1_32ic.md`](06_hardware_v1_32ic.md) | Current Retr01-A BOM (**32 IC**) |
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | Island checklist / legacy ~52 notes |
