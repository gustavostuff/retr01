# Hardware Simulators (Gate-Level)

For validating discrete logic (muxes, interleaved VRAM bus) beside the instruction-level C emulator.

Prefer a **digital** simulator (not SPICE) for whole-board logic.

## Tools

| Tool | Strength |
|------|----------|
| **Digital** (HNeemann) | Fast; 74xx libs; custom components |
| **Logisim-Evolution** | Visual; teaching / layout clarity |

## Retr01 mapping

| Block | Approach |
|-------|----------|
| W65C02S | Drop-in cycle-accurate CPU component |
| 32 KB system RAM + 32 KB VRAM | Two SRAM blocks; mux VRAM by clock phase |
| GAL | LUT / truth-table or explicit AND/OR subcircuit |
| CHR | Model as ROM the PPU reads; not in VRAM |
| Video | Tap digital X/Y + color -> matrix widget (256x240) |
| Audio | Log PCM from APU model; listen offline |

Gate-level sims are slow vs 8 MHz silicon - use for bus correctness; use the [C emulator](07_emulator_specification.md) for ROM behavior.
