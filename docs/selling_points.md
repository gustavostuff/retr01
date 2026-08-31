# Retr01 Selling Points

Retr01 vs classic 8-bit constraints (especially NES-era). Same spirit, more headroom for game logic.

**Related:** [`graphics.md`](graphics.md), [`memory.md`](memory.md), [`hardware.md`](hardware.md), [`passive_rf_etc.md`](passive_rf_etc.md). Authoring: [`retr01_studio/README.md`](../retr01_studio/README.md).

---

## What the NES proved

The NES showed that **32 KB PRG** and **2 KB RAM** could deliver complete, memorable games. Tile maps, tight sprite budgets, and clever VBlank work were features of the platform, not bugs. Retr01 keeps that craft but moves scrolling, the VBlank sprite field, and world streaming into hardware so PRG can focus on **play**.

---

## CPU and RAM

| | NES (typical) | Retr01 |
|--|---------------|--------|
| CPU | Ricoh 2A03 ~1.79 MHz | W65C02S **8.000 MHz** |
| Cycles / frame (~60 Hz) | ~29 800 | ~133 000 (~**4.5x**) |
| Work RAM | **2 KB** | **32 KB** system RAM |
| PRG ROM | 32 KB (NROM) common | **32 KB** fixed window (same size, different job) |

On NES, a large share of PRG and every frame went to **making the picture move**. On Retr01 that work sits in discrete video logic + AVRs. The same **32 KB** PRG budget buys more gameplay code.

---

## Graphics and scrolling

| | NES | Retr01 |
|--|-----|--------|
| Playfield | 256x240, nametable tricks | **128x120** logical (yes, deliberate chunky pixels), **2x2** live VRAM window (L1) |
| Scroll | Software nametable updates, often VBlank-only | Hardware scroll latches + **480 B** MAP stream into VRAM |
| Multi-screen worlds | Bank switching, manual nametable layout | Sparse world grid, **32** L1 screens/world in cart MAP |
| Second BG | Mapper tricks / limited layers | Structured **L0 / BG0** (1..8 screens), show-through under L1 color **0**, proportional scroll |
| Sprites | 64 OAM, **8** per scanline (typical pain point) | 64 OAM, **16** per scanline, VBlank sprite field in 1284 |
| Background | Tile + attribute tables in VRAM | Per-tile bank/pal/flip in attr byte, CHR on cart |
| Mid-frame effects | Sprite-0 hit | **Raster compare** IRQ (`$FE04`) |
| Master palette | Fixed PPU palette | **64** colors on board Color PROM, cart holds indices |

NES developers became experts at **VBlank choreography**. Retr01 developers stream screens into a hardware workbench and scroll inside it.

---

## Hardware vs software

On silicon, the **picture** is built in discrete video logic and AVRs. The **6502** streams MAP bytes, sets scroll latches, and runs the game. Phase 1 **Host Play** (Emu / Sim) stands in for some CPU work until cart PRG owns it.

**Hardware (motherboard + cart CHR/MAP read path):**

- Beam timing and tile/attr fetch for **L1** from VRAM slots **0-3** using `$FE02` / `$FE03` scroll
- **L0** line fill from VRAM slots **4-7** + cart CHR (HBlank target), scroll via `$FE06` / `$FE07`
- Compositor on every dot: **sprite > L1 > L0 show-through (L1 color 0) > backdrop**
- Color PROM lookup (cart palette indices to **64** RGB masters)
- VRAM interleave (CPU writes on PHI2 high, video fetch on PHI2 low)
- Sprite line buffer (1284 fills, compositor reads per scanline)
- OAM assist, pad ports (`$FE60` / `$FE61`), raster IRQ (`$FE04`)
- APU mix on **328P** from CPU bytecode (`$FE40`-`$FE5F`)

**Software (6502 PRG, or Host Play today):**

- Stream **480 B** screen payloads from cart MAP into VRAM when the camera leaves the **2x2** workbench (L1 slots **0-3**, L0 slots **4-7**)
- Default **proportional L0 scroll** from L1 camera (`cols_L0 / cols_L1`, per axis)
- Copy active palette row into `$FE08` / `$FE09`, world select, boot flow
- Gameplay: movement, camera dead zone, collision, entities, warps, AI
- Audio driver feeding the 328P bytecode protocol

Both BGs appear on screen together because the **compositor** merges them each dot. **Loading** new screens and **parallax scroll math** stay on the CPU unless you add a PLD ratio later.

---

## Audio

| | NES | Retr01 |
|--|-----|--------|
| Sound chip | Ricoh 2A03 (2 pulse, tri, noise, DMC) | **ATmega328P** software mixer, **8** channels |
| Sequencer | CPU drives registers every frame | CPU streams **bytecode** on NMI, AVR mixes |
| Sample playback | DMC DMA from cart | DPCM channel, samples in **328P flash**, CPU sends trigger ID |
| SFX steal music | Shared channels | **Channels 1-5** BGM, **6-8** SFX (design intent) |

Same tracker *feel*, different silicon. Details: [`sound.md`](sound.md).

---

## What fits in 32 KB PRG on Retr01

Because graphics streaming is hardware-assisted:

- Multi-screen exploration with dead-zone or rail camera
- Dozens of entities with simple AI and software collision
- Platformer or top-down movement with inventory and game flow
- Title, world select, up to **8** worlds, credits as cart MAP data
- Audio driver feeding the 328P bytecode protocol

Classic NES **32 KB** games (Balloon Fight, Ice Climber, Excitebike) already shipped full experiences under tighter CPU/RAM. Retr01 targets the same PRG size with **~4.5x** more cycles per frame and **16x** more RAM for game state.

**Soft CPU budget:** Host tools chart game-busy cycles against a soft max of **~50k** cycles/frame (`R01E_CPU_BUDGET_CYCLES`). That is a design guide for Phase 1 games, not a hard silicon clamp. Full frame still has ~**133k** CPU cycles available.

---

## Family

| Variant | Role |
|---------|------|
| **Arcade / console** | **Same motherboard.** Arcade headers (microswitches) + 2x 35RAPC TRS footprints. Shell and BOM population differ |
| **Retr01-H** | Handheld later, same software contract |

### Controllers (shared board)

Every motherboard (silicon / PCB target) has:

1. **Arcade controller** connections. Headers for sticks/buttons as simple microswitch circuits into the 1284 (`$FE60` / `$FE61`).
2. **2x Switchcraft 35RAPC** female 3.5 mm TRS footprints. Optional aux pads (ATtiny85 draft on the pad board, male-male aux cable, VCC/DATA/GND). PPTC + ESD on the TRS path so a mangled cable cannot take down the board.

Populate TRS jacks for console / portable sticks. Leave DNP in a sealed cabinet if you only wire the arcade headers. Details: [`passive_rf_etc.md`](passive_rf_etc.md).

Emu / Sim Host Play today use the `$FE60` / `$FE61` software contract only (not separate TRS or header island models).

Built for people who want to **make** 8-bit games, not only play them. Tools: **Studio** (author), **Emu** (runtime), **Sim** (hardware bring-up).

---

## Rough cost (planning)

32-IC mobo + cart proto targets ~**$200** qty-1 (parts + PCB share). See [`hardware.md`](hardware.md) BOM.

Game module profiles (movement, camera, entities): [`retr01_studio/README.md`](../retr01_studio/README.md).
