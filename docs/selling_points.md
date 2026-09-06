# Retr01 Selling Points

Retr01 vs classic 8-bit constraints (especially NES-era). Same spirit, more headroom for game logic.

**Related:** [`graphics.md`](graphics.md), [`memory.md`](memory.md), [`hardware.md`](hardware.md), [`passive_rf_etc.md`](passive_rf_etc.md), [`lightgun.md`](lightgun.md) (CRT accessory, roadmap). Authoring: [`app/studio/README.md`](../app/studio/README.md).

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
| Playfield | 256x240, nametable tricks | **128x120** logical (yes, deliberate chunky pixels), **2x2** live VRAM window (BG1) |
| Scroll | Software nametable updates, often VBlank-only | Hardware scroll latches + **480 B** MAP stream into VRAM |
| Multi-screen worlds | Bank switching, manual nametable layout | Sparse **16x16** world grid, **48** BG1 screens/world in cart MAP |
| Second BG / parallax | Mapper tricks, split-screen hacks, fake "depth" | **True second layer (BG0)**: authored map, live 2x2 window, compositor show-through |
| Sprites | 64 OAM, **8** per scanline (typical pain point) | 64 OAM, **16** per scanline, VBlank sprite field in 1284 |
| Background | Tile + attribute tables in VRAM | Per-tile bank/pal/flip in attr byte, CHR on cart |
| Mid-frame effects | Sprite-0 hit | **Raster compare** IRQ (`$FE04`) |
| Master palette | Fixed PPU palette | **64** colors on board Color PROM, cart holds indices |

NES developers became experts at **VBlank choreography**. Retr01 developers stream screens into a hardware workbench and scroll inside it.

---

## SNES-like parallax (BG0)

Classic NES games sold depth with **one** background and clever art. The SNES sold depth with **layers that actually move at different rates**. Retr01 takes that second idea seriously on a 6502-class cart:

```text
  sprites  (players, enemies, pickups)
     |
  BG1      (playfield: collision, platforms, foreground art)
     |        color 0 = "window" into the far plane
  BG0      (sky, mountains, city silhouette, scrolls slower)
     |
  backdrop
```

**What you get (not a hack):**

- A **real second tilemap** (BG0), up to **8** screens per world, authored in Studio beside BG1. Same tile/attr pipeline, own scroll latches (`$FE06` / `$FE07`).
- **Color-0 show-through:** wherever BG1 is transparent (palette index **0**), the compositor reveals BG0. Paint caves, arches, and canopy gaps in BG1. The far plane stays continuous underneath.
- **Automatic parallax ratio:** Host Play / PRG default is `scroll_BG0 = scroll_BG1 * (BG0 extent / BG1 extent)` per axis. Make the far world **smaller** than the playfield (e.g. BG0 **2x2**, BG1 **4x4**) and the mountains slide at half speed for free. Equal-or-larger BG0 on an axis -> that axis stays locked (foreground-only scroll).
- **Layer merge every dot:** sprite > BG1 > BG0 > backdrop. Silicon / Sim use the compositor path. Emu Host Play composites the same order in the full-frame renderer today. Either way both layers are on screen together, not a fake backdrop blit into nametable VBlank.

**Vs NES "parallax":** no mid-frame nametable rewrite, no status-bar split pretending to be a second world, no mapper IRQ gymnastics to fake depth. You place a second map. The machine scrolls it.

**Authoring path:** Studio Worlds -> **BG0** sub-button -> drop up to 8 screens on the 16x16 chess -> leave BG1 color 0 where the far plane should show -> Play. Details and ports: [`graphics.md`](graphics.md#second-background-bg0).

---

## Hardware vs software

On silicon, the **picture** is built in discrete video logic and AVRs. The **6502** streams MAP bytes, sets scroll latches, and runs the game. Phase 1 **Host Play** (Emu / Sim) stands in for some CPU work until cart PRG owns it.

**Hardware (motherboard + cart CHR/MAP read path):**

- Beam timing and tile/attr fetch for **BG1** from VRAM slots **0-3** using `$FE02` / `$FE03` scroll
- **BG0** line fill from VRAM slots **4-7** + cart CHR (HBlank target), scroll via `$FE06` / `$FE07`
- Compositor on every dot: **sprite > BG1 > BG0 show-through (BG1 color 0) > backdrop**
- Color PROM lookup (cart palette indices to **64** RGB masters)
- VRAM interleave (CPU writes on PHI2 high, video fetch on PHI2 low)
- Sprite line buffer (1284 fills, compositor reads per scanline)
- OAM assist, pad ports (`$FE60` / `$FE61`), raster IRQ (`$FE04`)
- APU mix on **328P** from CPU bytecode (`$FE40`-`$FE5F`)

**Software (6502 PRG, or Host Play today):**

- Stream **480 B** screen payloads from cart MAP into VRAM when the camera leaves the **2x2** workbench (BG1 slots **0-3**, BG0 slots **4-7**)
- Default **proportional BG0 scroll** from BG1 camera (`cols_BG0 / cols_BG1`, per axis)
- Copy active palette row into `$FE08` / `$FE09`, world select, boot flow
- Gameplay: movement, camera dead zone, collision, entities, warps, AI
- Audio driver feeding the 328P bytecode protocol

Both BGs appear on screen together because the **compositor** merges them each dot (Sim / silicon target). Emu Host Play matches that order in software today. **Loading** new screens and the default **parallax scroll ratio** stay on the CPU (or Host Play) unless you add a PLD auto-ratio later. See [SNES-like parallax](#snes-like-parallax-bg0).

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
- **Dual-layer worlds:** BG1 playfield + BG0 far plane with automatic parallax ratio
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
| **Retr01-A** / **Retr01-C** | **Same motherboard.** Arcade headers (microswitches) + 2x 35RAPC TRS footprints. Shell and BOM population differ ([`controllers.md`](controllers.md)) |
| **Retr01-H** | Handheld later, same software contract |

### Controllers (shared board)

Every motherboard (silicon / PCB target) has:

1. **Arcade controller** connections. **J5/J6** 1x10 headers (bitfield order) + **J7** 1x4 power/reset into the 1284 (`$FE60` / `$FE61`). See [`controllers.md`](controllers.md#locked-headers-schematic-freeze).
2. **2x Switchcraft 35RAPC** female 3.5 mm TRS footprints. **Retr01-C** aux pads: **ATtiny85** + 3-wire half-duplex UART at **115200** ([`controllers.md`](controllers.md)). PPTC + ESD on the TRS path.

Populate TRS jacks for console / portable sticks. Leave DNP in a sealed cabinet if you only wire the arcade headers. Details: [`passive_rf_etc.md`](passive_rf_etc.md).

Emu / Sim Host Play today use the `$FE60` / `$FE61` software contract only (not separate TRS or header island models).

**Light gun (roadmap):** A native CRT accessory on the same **3-wire** TRS port is specced in [`lightgun.md`](lightgun.md). Buffered timer in the gun enables **2-frame** multi-target hits vs classic per-sprite flashing. Not in Emu / Sim yet.

Built for people who want to **make** 8-bit games, not only play them. Tools: **Studio** (author), **Emu** (runtime), **Sim** (hardware bring-up).

---

## Rough cost (planning)

23-IC mobo + cart proto targets ~**$200** qty-1 (parts + PCB share). See [`hardware.md`](hardware.md) BOM.

Game module profiles (movement, camera, entities): [`app/studio/README.md`](../app/studio/README.md).
