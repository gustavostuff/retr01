# Retr01 Emulation and Bring-up

This doc merges the old emulator spec, hardware simulator notes, simulation plan, and proto-board test plan.

## Purpose

Use two benches:

1. **software bench**: strict low-level C emulator
2. **hardware bench**: digital logic simulation, then island-by-island proto bring-up

## Emulator goals

The emulator is for **architectural validation**, not for “run as fast as possible.”

It should match:

- memory decode
- interleaved VRAM ownership
- world/MAP streaming behavior
- BG bank per slot
- sprite next-line behavior
- raster IRQ timing
- NES-style APU register contract

## Emulator scope

### In scope

- W65C02S core
- bus / decode
- VRAM port rules
- CHR fetch rules
- OAM and sprite line behavior
- framebuffer output
- host I/O glue

### Out of scope

- old World Studio assumptions
- allocator-heavy architecture
- pretending VRAM is a free framebuffer

## Suggested emulator layout

```text
retr01_emu/
  cpu/
  bus/
  mem/
  cart/
  ppu/
  apu/
  host/
  main.c
```

Rule: CPU code should go through the **bus**, not index RAM arrays directly.

## Core emulator state

- `system_ram[0x8000]`
- `vram[0x8000]`
- `io_regs[0x100]`
- `prg[]`, `chr[]`, `map[]`
- `ppu.bg_slot_bank[6]`
- `ppu.spr_bank`
- `ppu.world`
- `ppu.oam[256]`

## Important emulator rules

### VRAM

- wrong-phase CPU VRAM access should be a **debug hard error**
- PPU fetches VRAM only on PPU phase
- CPU touches VRAM only through `$FE1x`

### BG fetch

- tile byte is `0–255`
- active BG bank comes from the current slot's bank latch
- slots **0–3** are camera
- slots **4–5** are optional parallax plane

### Sprites

- OAM lives logically in the 1284 side, not VRAM
- sprite evaluation is **next-line**
- max **16 sprites/scanline**
- sprite bank is separate from BG banks

### Raster

- `raster_hit` when scanline equals `raster_y` at dot 0
- optional IRQ when enabled
- no NES sprite-0 hit emulation

## Simulation tools

### Use

- **Digital (HNeemann)** for gate-level logic
- **simavr** or **Wokwi** for 1284 and 328P firmware
- the **C emulator** for ROM behavior and fast iteration

### Avoid

- using schematic AIs as the source of truth
- trying to SPICE the whole machine

## Bring-up stages

1. **CPU island**: clock, reset, RAM, tiny PRG ROM
2. **I/O page + pads**
3. **BG path**: beam counters, VRAM, fetch, palette, raster
4. **Sprites as a behavioral box**
5. **1284 firmware**
6. **328P APU firmware**
7. **C emulator rewrite**
8. **KiCad from known-good logic**
9. **Proto boards / physical islands**

## Physical proto islands

Recommended solder-up order:

- power
- clocks and reset
- CPU + system RAM + tiny ROM
- `$FExx` decode + one latch
- pads
- EEPROM
- VRAM port
- BG video
- sprite subsystem
- APU

## Pass / fail mindset

Do not treat “some output” as good enough.

Examples of true pass conditions:

- no bus fight
- clean reset
- VRAM port reads back correctly
- raster IRQ triggers at the expected line
- sprite appears one line late, not one frame late
- controller bits match the documented layout

## Current build path

The intended order is:

1. architecture docs
2. Digital logic model
3. proto islands
4. emulator rewrite
5. full board work

## Practical takeaway

If you need confidence:

- use the emulator to check **software contract**
- use Digital to check **wiring and timing**
- use islands to check **real-world electrical sanity**
