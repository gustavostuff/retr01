# Retr01 Selling Points

Retr01 vs classic 8-bit constraints (especially NES-era). Same spirit, more headroom for game logic.

**Related:** [`graphics.md`](graphics.md), [`memory.md`](memory.md), [`hardware.md`](hardware.md). Authoring: [`retr01_studio/README.md`](../retr01_studio/README.md).

---

## What the NES proved

The NES showed that **32 KB PRG** and **2 KB RAM** could deliver complete, memorable games. Tile maps, tight sprite budgets, and clever VBlank work were features of the platform, not bugs. Retr01 keeps that craft but moves scrolling, sprite line fill, and world streaming into hardware so PRG can focus on **play**.

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
| Playfield | 256x240, nametable tricks | **128x120** logical, **2x2** live VRAM window |
| Scroll | Software nametable updates, often VBlank-only | Hardware scroll latches + **480 B** MAP stream into VRAM |
| Multi-screen worlds | Bank switching, manual nametable layout | Sparse world grid, **32 screens**/world in cart MAP |
| Sprites | 64 OAM, **8** per scanline (typical pain point) | 64 OAM, **16** per scanline, line-buffer fill in 1284 |
| Background | Tile + attribute tables in VRAM | Per-tile bank/pal/flip in attr byte, CHR on cart |
| Mid-frame effects | Sprite-0 hit | **Raster compare** IRQ (`$FE04`) |
| Master palette | Fixed PPU palette | **64** colors on board Color PROM, cart holds indices |

NES developers became experts at **VBlank choreography**. Retr01 developers stream screens into a hardware workbench and scroll inside it.

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

## Cartridge and worlds

| | NES | Retr01 |
|--|-----|--------|
| Typical cart | 32 KB PRG + 8 KB CHR (NROM) | **512 KB** flash: PRG + CHR + MAP + palettes |
| Worlds | Usually one game world | Up to **8** worlds, **32** screens each |
| Tile art banks | Often fixed CHR page | **4** BG + **4** sprite banks per world (**256** tiles each, **32 KB** CHR total) |
| Saves | Battery RAM (mapper-dependent) | **I2C EEPROM** on cart |
| Authoring | Assembler + tile editors | **Retr01 Studio** -> `.r01proj` + export |

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
| **Retr01-A** | Arcade motherboard, first hardware target |
| **Retr01-C** | Home console shell, same cart |
| **Retr01-H** | Handheld later, same software contract |

Built for people who want to **make** 8-bit games, not only play them. Tools: **Studio** (author), **Emu** (runtime), **Sim** (hardware bring-up).

---

## Rough cost (planning)

32-IC mobo + cart proto targets ~**$200** qty-1 (parts + PCB share). See [`hardware.md`](hardware.md) BOM.

Game module profiles (movement, camera, entities): [`retr01_studio/README.md`](../retr01_studio/README.md).
