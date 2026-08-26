# retr01_sim performance guardrails

How the sim stays fast without giving up pin-mode validation. Full gap analysis lives in project notes; this doc tracks **what is landed** in code.

## Control points (keep these)

| Knob | Role |
|------|------|
| **`sim_fast` / `R01S_FAST=1`** | Primary throughput switch: word MAP catchup, thin settle/beam, Host Play, AVR tick skip |
| **`R01S_SETTLE_PASSES` vs `_FAST`** | Combinatorial depth per wire pass (PIN: 2, FAST: 1) |
| **`R01S_BEAM_DOTS_PER_STEP` vs `_FAST`** | DOT/beam burst per PHI2 step (PIN: 32, FAST: 4) |
| **Worker catchup** | UI never blocks on full board steps ([`CATCHUP_THREADING.md`](CATCHUP_THREADING.md)) |
| **`R01S_SOFTBOOT=1`** | Opt-in host poke (debug only; not FAST) |

Rule: new cost is either **behind `sim_fast`** (default off) or a **data-structure win** that does not change PIN behaviour.

## Landed (both modes unless noted)

1. **Pin-name hash** — `r01s_entity_pin_hash_build` + lazy lookup in `r01s_entity_pin_named` (kills hot-path linear `strcmp` scans).
2. **CPU bus from chip model** — `board_cpu_addr` / `board_cpu_read` / write-data sample use `r01s_w65c02s_ab` / `rwb` / `a` instead of re-reading 16+8 pins every wire pass.

## Landed under `R01S_FAST=1` only

| Shortcut | Why PIN keeps full path |
|----------|-------------------------|
| Word MAP+pal catchup (`board_fast_apply_map_stream`) | PIN must run `$FE93`→`$FE12` on the netlist |
| Settle skips `wire_bg_fetch` / `wire_linebuf` | PIN needs combinatorial depth in settle; beam path still runs `wire_bg_fetch` per dot |
| Skip ATmega328P/1284P `tick()` per step | PIN validates AVR/OAM timing |
| Fat beam burst when Play enabled (`R01S_BEAM_DOTS_PER_STEP_PLAY`) | Interactive preview only |

**Host Play** (pad tick, spawn/camera, OAM sprite shortcut, LCD hold bypass) runs in **both** PIN and FAST after MAP catchup — it is a host preview scaffold, not silicon contract.

Toggle **SIM PIN / SIM FAST** in the sidebar at runtime; switching to FAST can re-run word catchup if needed. Ctrl+R re-runs catchup with the selected mode.

## Next (not yet coded)

- Pointer / net-ID netlist (build once at `r01s_board_build`)
- Fuller W65C02S / AVR models behind env flags (`R01S_CPU_FULL_ISA`, etc.)
- Cached PLD sum-of-products
- Master virtual tick for dual clock domains (PHI2 + DOT + 20 MHz AVR)
- Debug net highlight / pin history (probe-time only)

See [`docs/08_simulator.md`](../docs/08_simulator.md) § Optimization playbook.
