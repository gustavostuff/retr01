# Hardware Simulators (Gate-Level)

For validating discrete logic (muxes, interleaved VRAM bus) beside the instruction-level C emulator.

Prefer a **digital** simulator (not SPICE) for whole-board logic.

## Tools

| Tool | Strength |
|------|----------|
| **Digital** (HNeemann) | Fast, 74xx libs, custom components |
| **Logisim-Evolution** | Visual, teaching / layout clarity |

## Retr01 mapping

| Block | Approach |
|-------|----------|
| W65C02S | Drop-in cycle-accurate CPU component |
| 3× 32 KB SRAM | System + interleaved VRAM + line-buffer (512 B used) |
| ATmega1284P | Firmware block: OAM eval + pad latch (or treat as opaque next-line sprite buffer) |
| GAL | LUT / truth-table or explicit AND/OR subcircuit |
| CHR | Model as ROM the PPU reads, not in VRAM |
| Video | Tap digital X/Y + color -> matrix widget (256x240) |
| Audio | Log PCM from APU model, listen offline |

Gate-level sims are slow vs 8 MHz silicon. Use them for bus correctness. Use the [C emulator](07_emulator_specification.md) for ROM behavior.

**Sequence:** which simulator when, plus frozen passives: [16_simulation_and_bringup_plan.md](16_simulation_and_bringup_plan.md). Do not start from a schematic-AI BOM.
