# Hardware for Software Engineers — Retr01 Architecture

Software mental models ↔ discrete logic on Retr01-A.

## 1. Logic and glue

| Software idea | Hardware |
|---------------|----------|
| Program on a clock | W65C02S |
| Bitwise stdlib | 74-series |
| Custom decode function | **GAL22V10** |

## 2. Two RAMs, one interleaved

| Chip | Software idea |
|------|----------------|
| 32 KB system RAM | Normal heap/BSS/stack arrays — always yours |
| 32 KB VRAM | Shared framebuffer-like nametable store with a **mutex keyed to clock phase** |

**74HC157** muxes = ternary select CPU vs PPU address. **74HC245** = bus direction. Fighting drivers = bus contention (short). Only **VRAM** is interleaved; system RAM is not.

CHR patterns are **cart ROM fetches**, like a memory-mapped asset file the PPU reads — not a third writable frame buffer.

## 3. MMIO as the hardware API

All devices live in **`$7Fxx`** ([08_memory_map.md](08_memory_map.md)):

- Scroll / banks / world = latches (**74HC573**-class)
- Beam X/Y = counters (**74HC161**-class)
- APU = async “audio microservice” on ATmega

## 4. Render pipeline

1. Beam counters → nametable coords (with scroll across up to 4 slots).
2. Tile index + **per-tile** attribute → palette.
3. Pattern row from **cart CHR** (BG bank page).
4. HBlank: OAM → ≤16 sprites; patterns from **sprite bank** page on cart.
5. Mux sprite over BG when sprite pixel ≠ transparent.

## 5. Game loop

NMI ≈ **60 Hz** metronome. Logic can run all frame; VRAM updates go through the interleaved data port. Collision stays in software (AABB).
