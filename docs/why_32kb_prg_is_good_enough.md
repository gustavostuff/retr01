# why 32KB of PRG is good enough

Retr01 uses a fixed 32 KB PRG window at $8000-$FFFF.
That is the same raw size many classic NES NROM titles used. On Retr01 it buys far more game logic.

## Why the same 32 KB goes further

| Factor | NES (NROM-era) | Retr01 |
|--------|----------------|--------|
| CPU clock | ~1.79 MHz | 8.000 MHz |
| Cycles per frame (~60 Hz) | ~29 800 | ~133 000 (~4.5x) |
| Graphics work in PRG | Heavy: software scrolling, sprite multiplexing, VBlank-only nametable updates, attribute tables | Minimal: hardware 2x2 camera window, interleaved VRAM, sprite line-buffer (1284), MAP streaming via $FE93, per-tile bank/attr already in hardware |
| System RAM | 2 KB | 32 KB |
| PRG role | Code + data + rendering hacks | Mostly pure game logic and tables |

On the NES a large fraction of the 32 KB (and of every frame's cycles) was spent just making the picture appear and scroll.
On Retr01 those costs are already paid by the discrete-logic path and the two AVRs. The 32 KB PRG and the 133 kcycles/frame are therefore available almost entirely for gameplay systems.

## What fits comfortably in 32 KB PRG

- 30-60 active entities with individual state machines, simple pathfinding or flocking, and data-driven behavior tables
- Solid physics and collision: tile-based or soft-pixel, platforms, slopes, one-way platforms, multiple hitboxes per entity
- Full player systems: platformer or top-down movement, wall-jumps, dashes, inventory, equipment, status effects, multi-stage attacks
- World and camera logic: multi-screen seamless scrolling (helped by the live 2x2 VRAM window), doors/warps, triggers, cutscene scripting, simple dialogue
- Game flow: title to world select to up to 8 worlds x 32 screens, save points, quest flags, ending sequence
- Audio driver: modest music + SFX bytecode that the ATmega328P mixes

Classic NES 32 KB titles already delivered complete games under far tighter constraints.
On Retr01 the same designs leave headroom for richer AI, better physics, inventory systems, and multi-world structure while still fitting in the fixed 32 KB window.

## Design posture

Keep bank 0 as the fixed home for vectors, interrupt stubs, common library code and any tiny HAL helpers.
Put the bulk of game systems and tables in the remaining space.
Because graphics and streaming are hardware-assisted, most of the 32 KB can stay readable, modular 6502 (or compiled C) instead of cycle-counting rendering tricks.

32 KB of PRG on Retr01 is enough for a complete, polished 8-bit action, platform or adventure game with multiple worlds and modern-feeling camera work.
