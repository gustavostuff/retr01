# Retr01 Sim performance guardrails

How the sim stays fast without giving up pin-mode validation. Full gap analysis lives in project notes. This doc tracks **what is landed** in code.

## Control points (keep these)

| Knob | Role |
|------|------|
| **`R01S_SETTLE_PASSES`** | Combinatorial depth per wire pass (currently **2**) |
| **`R01S_BEAM_DOTS_PER_STEP`** | DOT/beam burst per PHI2 step (currently **32**) |
| **`R01S_BEAM_DOTS_PER_STEP_PLAY`** | Fatter beam burst while Host Play is enabled (preview only) |
| **Worker catchup** | UI never blocks on full board steps ([`CATCHUP_THREADING.md`](CATCHUP_THREADING.md)) |
| **`R01S_SOFTBOOT=1`** | Opt-in host poke (debug only) |

Rule: new cost should be a **data-structure win** that does not change pin behaviour, or gated behind an explicit debug env flag.

## Landed (pin netlist)

1. **Pin-name hash** - `r01s_entity_pin_hash_build` + lazy lookup in `r01s_entity_pin_named` (kills hot-path linear `strcmp` scans).
2. **CPU bus from chip model** - `board_cpu_addr` / `board_cpu_read` / write-data sample use `r01s_w65c02s_ab` / `rwb` / `a` instead of re-reading 16+8 pins every wire pass.

**Host Play** (pad tick, spawn/camera, OAM sprite shortcut, LCD hold bypass) runs after MAP catchup - it is a host preview scaffold, not silicon contract.

## Next (not yet coded)

- Pointer / net-ID netlist (build once at `r01s_board_build`)
- Fuller W65C02S / AVR models behind env flags (`R01S_CPU_FULL_ISA`, etc.)
- Cached PLD sum-of-products
- Master virtual tick for dual clock domains (PHI2 + DOT + 20 MHz AVR)
- Debug net highlight / pin history (probe-time only)

See [`README.md`](README.md) (Architecture summary).
