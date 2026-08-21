# Retr01 Pitch

**Discrete-logic 8-bit hardware for people who love the look of the classics and want room to build bigger games.**

Retr01 is a family of purpose-built 2D game machines. One architecture, one cartridge format, one software contract - three shells:

| Model | What it is | Status |
|-------|------------|--------|
| **Retr01-A** | Arcade motherboard (through-hole, cabinet-first) | **First hardware target** |
| **Retr01-C** | Home console (same core, smaller board, different controllers) | Planned after A |
| **Retr01-H** | Handheld (SMD, same software map) | Later |

This document is the **product pitch**. Technical specs live in the other `docs/` files. NES comparison tables are at the **bottom** only.

---

## The one-line pitch

**Retr01-A is an arcade-ready 6502 platform that keeps NES-era tile/sprite aesthetics while giving developers faster CPU time, bigger worlds, smoother scrolling, and parallax that does not fight the hardware.**

---

## Why Retr01 exists

Classic 8-bit consoles earned their place in history. The NES in particular proved that tight limits can produce unforgettable art. Retr01 is not a joke about those machines. It is a **respectful successor lane**: same visual vocabulary (8x8 tiles, 2bpp patterns, sprite lists, palette tricks), but with plumbing redesigned for **2020s game design** - large maps, seamless camera movement, layered backgrounds, and enough CPU headroom to run real game logic every frame.

Most retro projects either:

- **Emulate** old hardware exactly (great for preservation, tight for new design), or
- **Abandon** the aesthetic entirely (modern engines, different feel).

Retr01 takes a third path: **new silicon, familiar rules, expanded world model.**

---

## Retr01-A: arcade first

Retr01-A is meant to drop into a cabinet:

- **5 V** barrel power, through-hole build, repair-friendly sockets
- **20-pin IDC** for coin, start, and player controls
- **RGBS**, S-Video, and composite output pads (cabinet monitors today, converters for modern displays tomorrow)
- Real **cartridge** plan: **512 KB** standard image (PRG + CHR + MAP). Full caps + **32 KB** PRG ~**414 KB**, ~**98 KB** free

You get a board you can breadboard in islands, bring up module by module, then integrate - not a black-box FPGA mystery.

**Retr01 Studio** (in development) authors worlds, screens, tile art, and palettes. Later Studio phases compile full `.retr01` cart images. A low-level cycle emulator is planned later for validation before silicon is final.

---

## Retr01-C and Retr01-H (same soul, different shell)

### Retr01-C (console)

Same CPU map, same graphics model, same carts. Intended differences are **industrial design**, not software:

- Smaller PCB/enclosure constraints
- **3-wire controllers** with an **ATtiny85** (draft) MCU in the pad (**VCC, GND, DATA**). **`$FE60`/`$FE61`** stay the same two bytes per player
- Consumer-facing AV and power

Games built for A should port to C with recompilation, not re-architecture.

### Retr01-H (handheld)

Same contract again, SMD build, likely multi-board layout. Battery, display, and controls are hardware problems - **not** a second game platform. If it runs on A, it should run on H.

---

## What players and developers get

### Look and feel

- Logical playfield **128x120** (**16x15** tiles) - chunky pixels. **2x** fills the **256x240** CRT field
- That is a feature: **larger, crisp pixels**, less MAP/VRAM per room than NES density, art that stays readable on cabinet CRTs
- Each room is only **16x15** tiles: **240** tile bytes + **240** attr bytes (**480 B** total). Loaders copy bytes as-is - **no RLE / decompress** required
- RGBS path uses a stable **256x240** active raster. Board **SCALE** DIP: **2x** default doubles to **256x240** (no letterbox); **1x** centers **128x120**
- A **640x640** handheld LCD can show the same games at **5x** (**640x600**, thin letterbox) without a second game resolution
- **8x8** tiles, **2bpp** patterns
- **64-color master palette** in board **Color PROM**, with **4 BG + 4 sprite palettes** active at once from one synced palette row
- Limits on purpose, not accidental mush

### Worlds, not just levels

Retr01 treats **worlds** as a first-class cart concept:

- Up to **8 worlds** per game
- Each world: sparse **8x8** grid, up to **32 stored screens**
- Each screen: **16x15** tilemap + attributes (**128x120**) - **480 B** raw in MAP (**one byte per tile index, one byte per attr**)
- **MAP-ROM** streams that blob through `$FE90` - seek, then copy. No compression step on the hot path

That is the difference between "we drew 32 rooms in CHR and hope the level loader keeps up" and **hardware-backed map chapters** with CHR banks per world - and room data simple enough that streaming is a straight byte copy.

### Scrolling that matches how games are designed

The live camera uses **four VRAM nametable slots (0-3)** as a **2x2 playfield window**. Smooth pixel scroll can show **multiple neighboring screens at once** without the classic "reload everything at the seam" dance.

- **`scroll_x`/`scroll_y`**: wrap in **0-127** / **0-119** (logical pixels)
- **Per-tile BG bank**: each **8x8** cell's attr selects CHR BG bank **0-3** (screens are not locked to one bank)
- **Per-sprite bank / size / priority**: OAM attr bits (mixed banks and 8x8/8x16 in one frame)

For designers, this means multi-screen chapter layouts without rewriting the engine every time the camera crosses a room boundary.

### Parallax without sprite-0 hacks

Two extra VRAM slots (**4-5**) are **parallax plane storage only** - not fifth and sixth playfield screens. A **scanline band** can point at those slots for layered backgrounds while the main camera uses slots 0-3. Enabling any H or V parallax band **locks camera movement to that axis for the whole frame**.

Combined with **raster IRQ** (scanline compare), you get status bars, split layers, and parallax bands through a **documented register model** - not through counting cycles until sprite 0 hits the beam.

### CPU time you can spend on the game

The **W65C02S runs at 8.000 MHz**. VRAM is **interleaved**: the CPU gets dedicated phases to stream nametables and attrs **during the frame**, not only in a short VBlank window. System RAM (**32 KB**, CPU-only) is separate from video SRAM.

**Why that matters at a seam:** each screen is **480** bytes in VRAM (240 tiles + 240 attrs). A tight copy into `$FE12` is about **12** CPU cycles/byte. With **341**-dot lines at the board clocks, that is about **508** CPU cycles per CRT line:

| Camera workbench reload | VRAM bytes | Beam time while streaming (interleaved) |
|-------------------------|------------|------------------------------------------|
| 1 screen | 480 | **~11** CRT lines (~5% of the 240-line active field) |
| 2 screens (column) | 960 | **~23** CRT lines (~9%) |
| 3 screens (corner) | 1440 | **~34** CRT lines (~14%) |

VBlank is only about **22** CRT lines. VBlank-only streaming can fit **one** screen per blank, but a **2**-screen column already burns roughly an entire VBlank, and a **3**-screen corner needs **multiple** blanks (multi-frame hitch or stalled camera). Interleave finishes that same corner in well under one active frame of beam time - seamless scrolling stays realistic and smooth, every frame.

Sprites are evaluated and line-buffered by an **ATmega1284P**. Audio runs on a **ATmega328P** with a **NES-style APU** model. The 6502 writes game state, OAM, and latches - it does not paint pixels.

### Collision and effects (honest scope)

- **Gameplay collision** is **software** (by design - flexible, not tile-bound)
- **No sprite-0 hit API**, no hardware "did sprite 5 touch the background" flag
- **Palette effects** (fade, flash, row interpolation) are runtime library + register writes, backed by a real **active palette buffer**

Retr01 gives you hooks that match how 8-bit games actually implement physics and FX, without pretending the PPU is a physics engine.

---

## CPU speed in context (fair numbers)

| Platform | CPU | Approx. clock | Notes |
|----------|-----|---------------|-------|
| **NES (NTSC)** | Ricoh 2A03 (6502-based) | **~1.79 MHz** | Iconic library, tight VBlank budget |
| **Retr01-A** | W65C02S | **8.000 MHz** | ~**4.5x** NES clock, CMOS 6502 ISA |
| **SNES** | W65C816 | **~3.58 MHz** | 16-bit successor era (different generation) |

Clock ratio alone is not "4.5x the game." Video timing, DMA absence, and your own code matter. The point is practical: Retr01 targets **headroom for world streaming, AI, and FX** while staying in an 8-bit programming model - not squeezing every opcode into VBlank.

---

## World model vs classic level loading

| Idea | Classic NES-style approach | Retr01 approach |
|------|---------------------------|-----------------|
| Map storage | Often ad-hoc tables in PRG, manual pointers | **Self-describing cart**: header + **pointer table** + worlds/screens (**480 B** raw) |
| Room grid | Engine-specific | Up to **32 screens/world** on an **8x8** sparse grid (**8** worlds) |
| CHR organization | Banks and swaps per game | **4 BG + 4 sprite banks per world** (256 tiles each) |
| Crossing a seam | Reload nametable in VBlank, flicker or stall | **2x2 live slots** + scroll. Interleaved stream (~**11** CRT lines/screen) |
| Parallax layer | Status bar tricks, CHR abuse, IRQ hacks | **Dedicated plane slots 4-5** + raster band |
| Mid-frame splits | Sprite 0 hit timing | **Raster compare IRQ** |

None of this makes NES games bad. Many masterpieces worked within tighter rules. Retr01 assumes you want those rules **loosened in predictable places** so Studio and homebrew code can focus on content.

---

## Tooling story

| Piece | Role |
|-------|------|
| **Retr01 Studio** | Visual authoring: worlds, screens, CHR banks, palettes (phased). Full `.retr01` export in later phases |
| **Future emulator** | Cycle-level validation of `$FExx` and carts (planned, not active yet) |
| **Retr01-A board** | Reference hardware and cabinet deploy target |

---

## Who this is for

- **Homebrew developers** who want a documented map format and scroll model, not a blank 6502 board
- **Arcade builders** who want RGBS, IDC controls, and cart-based games
- **Artists** who like 8x8/2bpp constraints but need **multi-screen worlds**
- **Future console/handheld ports** without rewriting the game twice

---

## Project status (honest)

Retr01 is in **architecture and documentation** phase. Retr01-A silicon is being brought up as **protoboard islands** before a motherboard PCB. Retr01 Studio Phase 1 (world grid + BG paint + Generate bank) is the active software track.

The pitch is the destination. The other docs are the blueprint.

---

## NES comparison: what is the same

Retr01 deliberately rhymes with the NES where it helps learning and art direction:

| Topic | NES (NTSC) | Retr01-A |
|-------|------------|----------|
| CPU family | 6502-derived (2A03) | **6502-derived (W65C02S)** |
| Tile size | **8x8** | **8x8** |
| Tile depth | **2bpp** (4 colors per tile) | **2bpp** (4 colors per tile) |
| Pattern storage | **CHR in cartridge** | **CHR in cartridge** |
| Attribute idea | Packed palette bits per tile group | **One attr byte per 8x8 tile** (palette, flip, bank, solid, anim) |
| Sprite list | **64** OAM entries | **64** OAM entries |
| OAM entry layout | **Y, tile, attr, X** | **Y, tile, attr, X** (`$FE20` addr/`$FE21` data) |
| Frame rate class | **~60.098 Hz** (262 lines) | **~60.098 Hz** (341x262 timing) |
| Game media | **Cartridge** | **Cartridge** (`.retr01`) |
| Background + sprites | Tile BG + movable sprites | Tile BG + movable sprites |
| Palette model | Indices into system colors | **Color PROM** (64 RGB on board) + cart palette **index** rows |
| Audio direction | Period-accurate square/noise/DPCM style | **NES-style APU** on dedicated MCU |
| Developer culture | Asm, fixed layouts, no heap | Asm-friendly, **binary-first** layouts |

If you know how NES tiles, attrs, and sprites work, Retr01 art pipelines will feel familiar - just on a **tighter, chunkier** playfield.

---

## NES comparison: what is different

| Topic | NES (NTSC) | Retr01-A |
|-------|------------|----------|
| Logical playfield | **256x240** (32x30 tiles) | **128x120** (16x15 tiles) - chunky. **2x** fills CRT |
| Why smaller logical | - | **Bigger pixels**, cheaper rooms, more worlds in flash |
| RGBS/CRT active field | **256x240** | **256x240** raster. Board **SCALE**: **2x** = full field, or **1x** centered |
| Cart / ROM size | Often 128-512 KB class | **512 KB** standard (~**414 KB** full fill + slack). **32 KB** PRG |
| CPU clock | **~1.79 MHz** | **8.000 MHz** |
| Work RAM | **2 KB** | **32 KB** system RAM (CPU-only) |
| Video RAM | **2 KB** PPU internal | **32 KB** interleaved VRAM + separate line-buffer SRAM |
| CPU access to nametables | Mostly **VBlank/forced blank** | **Interleaved CPU phases** every frame |
| Sprites per scanline | **8** | **16** (logical lines) |
| On-screen palette slots | **4 BG + 4 sprite** (with shared backdrop rules) | **4 BG + 4 sprite** active row. **64** master colors in **Color PROM**. Cart holds **index** banks only |
| Nametables live at once | **2** for scroll tricks | **6 slots**: **4** camera + **2** parallax plane |
| World/map hardware | None (game code) | **MAP-ROM**, **8 worlds**, **32 screens/world**, **480 B**/screen raw |
| CHR banking | Mapper-dependent, game-defined | **4 BG + 4 sprite banks/world**, **per-tile** BG bank, **per-sprite** bank (attrs) |
| Mid-frame effects | **Sprite 0 hit** + timed code | **Raster compare IRQ** |
| Gameplay collision | Software (same) | Software (explicit - **no** hardware sprite-BG hit) |
| PPU integration | Single Ricoh PPU | **74HC BG path** + **ATmega1284P** sprite line buffer + **ATmega328P** APU |
| CPU vs video clock | Derived/coupled | **Independent** CPU and dot clocks |
| Output | RF/composite (retail) | **RGBS**, S-Video, composite **pads** (arcade-first) |
| Form factor | Consumer console | **Retr01-A** arcade, **Retr01-C** console, **Retr01-H** handheld |
| Authoring tool | Historical third-party | **Retr01 Studio** (in development) |

Different does not mean better for every game. A tight NES-style single screen is still valid on Retr01. The hardware just removes a few famous bottlenecks when you want more world.

---

## Where to read next

| Doc | Content |
|-----|---------|
| [`01_architecture_overview.md`](01_architecture_overview.md) | Terminology, capability snapshot, variants |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Worlds, VRAM, MAP, palettes, scrolling |
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | Chips, buses, pipelines |
| [`04_retr01_studio.md`](04_retr01_studio.md) | Authoring tool roadmap |
| [`05_costs_and_open_questions.md`](05_costs_and_open_questions.md) | Locked decisions, costs, open questions |
| [`06_protoboard_module_tests.md`](06_protoboard_module_tests.md) | Hardware bring-up |

---

*Retr01: classic pixels, room for bigger worlds.*
