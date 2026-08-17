# Retr01 Low-Level Emulator Specification

## 1. Objective

A **hardware-restricted emulator in C** whose job is architectural validation, not max host FPS. Behavior that passes here should match Retr01-A silicon for: memory decode, interleaved VRAM phases, sprite drop rules, bank timing, and NES-style APU register traffic.

**In scope now:** emulator core + host display/audio glue.  
**Out of scope now:** PPUX, cc65, and other asset/game authoring toolchains.

## 2. Components

### 2.1 CPU core

- Cycle-accurate 6502 core with **W65C02S** ops as needed.
- Two-phase clock model drives VRAM ownership (polarity frozen with the schematic).

### 2.2 Memory

| Store | Emulation rule |
|-------|----------------|
| System RAM 32 KB | Always CPU-accessible at `$0000–$7EFF` |
| I/O page | `$7F00–$7FFF` register file |
| VRAM 32 KB | Only via `$7F1x`; **phase-locked** with PPU |
| PRG / CHR / MAP | Cart image; PRG at `$8000–$FFFF`; CHR PPU-fetched |

See [08_memory_map.md](08_memory_map.md) and [09_address_decoding.md](09_address_decoding.md).

### 2.3 Interleaved VRAM

- PPU phase: background/sprite nametable & attr fetches (CHR from cart).
- CPU phase: VRAM port R/W legal.
- Debug: **hard fail** on wrong-phase CPU VRAM access.

### 2.4 Virtual PPU

- 2bpp; per-tile BG attributes; 32×30 × up to 4 slots.
- Independent BG / sprite banks; allow mid-frame bank writes.
- 64 OAM; **16 sprites/scanline** drop.
- Sprite non-transparent pixel over BG.

### 2.5 APU

Trap `$7F40–$7F5F` writes; synthesize **NES-style** channels (2 pulse + triangle + noise + DMC) on the host (e.g. SDL2).

## 3. Bring-up workflow

1. Assemble or generate a minimal ROM image (hand-made is fine).
2. Run under the C emulator; iterate on core accuracy.
3. Later: flash the same image class to hardware.

## 4. Non-goals

- Analog DAC / encoder simulation.
- Gate-level GAL fuse simulation (use Digital/Logisim for that — [10_hardware_simulators.md](10_hardware_simulators.md)).
- Full game toolchain integration in this phase.
