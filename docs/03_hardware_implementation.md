# Retr01 Hardware Implementation

This doc merges the board walkthrough, software-engineer explanation, chip-count plan, and schematic-facing hardware notes.

**Start here if you write game code or Studio:** [*Where state lives (address to silicon)*](#where-state-lives-address-to-silicon) - which address hits which chip, what byte is stored, who reads it, and how often it changes. Register bitfields: [`02`](02_graphics_worlds_memory.md). Island bring-up: [`06`](06_protoboard_module_tests.md).

## Board-level picture

Retr01-A is four active compute domains on one 5 V board:

- **W65C02S**: game logic
- **74HC BG path**: beam timing, VRAM fetch, BG pixels
- **ATmega1284P**: OAM, sprite evaluation, line-buffer fill, pad bytes
- **ATmega328P**: audio

The CPU never writes a framebuffer. It fills nametables, OAM, and latches.

## Main chips

| Block | Part | Role |
|------|------|------|
| CPU | W65C02S | game logic |
| System RAM | AS6C62256 | `$0000-$7FFF` |
| VRAM | AS6C62256 | interleaved video SRAM |
| Line buffer | AS6C62256 | sprite ping-pong storage |
| Sprite/input MCU | ATmega1284P-PU | OAM + sprite pipeline + pads |
| Audio MCU | ATmega328P-PU | NES-style APU |
| PLD | 3x ATF22V10CQZ-20PU | decode, timing, CHR/VRAM gating |
| Color PROM | 3x AT28C16 | master palette R/G/B (6-bit index -> DAC) |
| 74HC157 | muxes | VRAM and line-buffer address mux |
| 74HC245 | transceivers | data isolation |
| 74HC573 | latches | scroll, banks, MAP address, OAM capture |
| 74HC161 | counters | beam X/Y |

### Datasheets

Official PDFs for the main silicon:

| Part | Datasheet |
|------|-----------|
| W65C02S | [WDC PDF](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf) |
| AS6C62256 | [Alliance PDF](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf) |
| ATF22V10 | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf) |
| ATmega1284P | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf) |
| ATmega328P | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf) |
| AT28C64B | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/doc4428.pdf) |
| AT28C16 (Color PROM) | [Microchip AT28C16 PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0006.pdf) |
| SST39SF040 (512 KB bring-up) / 2 MB parallel NOR (cart standard) | [Microchip SST39SF0x0 PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf) (family); exact 2 MB part TBD (Q19) |
| SN74HC157/245/573/161/00/04/08/14/32/86/688 | [TI 74HC family](https://www.ti.com/logic-circuit/standard-logic/74hc-family/overview.html) (part-specific PDF) |

Island bring-up order and pass criteria: [`06_protoboard_module_tests.md`](06_protoboard_module_tests.md).

## Frozen v0 board plan

- through-hole only
- planning total: **52 motherboard ICs**
- **3x AT28C16** Color PROM (R/G/B), programmed once with the family master palette
- 3x ATF22V10, not Lattice GAL
- if PLD equations overflow, add a **4th ATF22V10**, not a different family

## Color PROM (master palette)

The **64-color master palette** is hardware on every Retr01 board:

- part: **AT28C16** class parallel EEPROM (DIP-24), **three** devices
- address: **6-bit** master index from the compositor (colors 0-63)
- data: each PROM drives one gun (**R**, **G**, or **B**) into that gun's R-2R DAC
- not on the 6502 data bus during gameplay (video path only)
- not stored in the cartridge. Carts only reference indices 0-63

Studio keeps a software copy of the same RGB table for preview only. Changing the look of the family means reburning the Color PROMs (and updating the doc table), not shipping a new cart header field.

**Pin (not decided):** if AT28C16 supply or access time becomes a problem (obsolete listings; **150 ns** is tight if dot clock rises), candidate replacement is Microchip **AT27C256R / AT27C256R-70PU** - **70 ns** OTP EPROM, **In Production**, DIP-28. Tradeoffs: OTP (not EEPROM), different footprint, still three chips for R/G/B unless packing changes. See `05` Q15.

## Output scale (board DIP)

Retr01-A ships with a **SCALE** DIP (or jumper):

- **closed = 2x** (cabinet default): double **128x120** to **256x240** - fills the RGBS active field with **no** letterbox
- **open = 1x**: center **128x120** in the **256x240** active field (**64** px side margins, **60** lines top/bottom)

The beam counters and **341x262** timing do not change with the DIP. Only logical-to-raster mapping and border blanking change.

## Clocks

| Clock | Value | Job |
|------|-------|-----|
| CPU | **8.000 MHz** | W65C02S + VRAM ownership phase |
| Dot | **5.369318 MHz** | beam counters, fetch, compositor |
| 1284 | **20 MHz** | sprite/input firmware |
| 328P | **16 MHz** | APU firmware |

CPU and dot clocks are **independent**.

## Where state lives (address to silicon)

This is the software-engineer map of the board: **which `$FExx` / memory hits which chip**, what byte lives there, **who reads it**, and **how often it should change**. Full register bitfields stay in [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md). Pin-level PLD equations come later with schematics.

Read each row as: *CPU poke -> physical hold -> video/audio/input consumer*.

### Big memories (SRAM / flash / PROM)

| What | CPU view | Physical store | Who writes | Who reads | How often it changes |
|------|----------|----------------|------------|-----------|----------------------|
| Game state, code scratch | `$0000-$7FFF` | **AS6C62256** system RAM | 6502 | 6502 only | Every frame / as game needs. **Never** on the video bus |
| Live nametables (slots 0-5) | `$FE10`/`$FE11` addr, `$FE12` data | **AS6C62256** VRAM | 6502 on **CPU phase** | BG fetch on **PPU phase** | Slot fill on camera/plane load or seam shift (~480 B/screen). Idle while panning inside the workbench |
| Sprite line buffer | (no CPU port) | **AS6C62256** line buffer, halves `$000-$07F` / `$080-$0FF` | **1284** in HBlank | BG compositor on visible dots | **Every logical scanline** (ping-pong). Not a framebuffer |
| Cart PRG / CHR / MAP | `$8000+` PRG window; CHR via fetch; MAP `$FE90`-`$FE93` | **2 MB** parallel NOR (v0 socket; 512 KB OK for bring-up) | Programmer / cart build | 6502 (PRG, MAP), BG path + 1284 (CHR) | Image is fixed at burn time. Runtime only **banks** and **MAP seek** |
| Master RGB (64 colors) | (no CPU port in gameplay) | **3x AT28C16** Color PROM | Once at board program | Compositor every pixel | Never at runtime. Carts only store **indices 0-63** |
| Board save / config | `$FE70`-`$FE72` | **AT28C64B** | 6502 (slow write timing) | 6502 | Rare (options, high scores). Not video |

VRAM slot layout (same chip, CPU and BG share by PHI2 phase): slots **0-3** camera, **4-5** parallax planes, then scratch - see `02`. Each slot is **512 B** aligned (**240** tiles + **240** attrs used).

### Latched `$FExx` controls (74HC573 class + decode PLD)

These are **physical latches on the PCB**, not VRAM and not "variables in the 1284." Decode (`ATF22V10` + glue) pulses `/LE` on the right **74HC573** when the 6502 writes that address. The latch holds the byte (or packed fields) until the next write. Video logic samples the Q outputs continuously.

| Addr | Name | Typical bits held | Physical hold | Who reads | How often / why |
|------|------|-------------------|---------------|-----------|-----------------|
| `$FE02` | `SCROLL_X` | 0-127 | HC573 (+ glue) | BG fetch (slot pick + fine scroll) | Every pan frame, or less. **No** nametable rewrite |
| `$FE03` | `SCROLL_Y` | 0-119 | HC573 | BG fetch | Same |
| `$FE00` | `PPUCTRL` | BG/sprite enable, NMI, camera mode | HC573 / PLD | BG path, NMI gate | Mode changes, room transitions |
| `$FE04`/`$FE05` | raster compare / IRQ | scanline + enable | HC573 + compare (`HC688` class) | IRQ logic vs beam Y | Setup once per split effect; hit is sticky in `PPUSTATUS` |
| `$FE06`/`$FE07` | plane band | which plane, axis lock, band start | HC573 | BG path (slot 4/5 vs 0-3) | When enabling/moving a parallax band |
| `$FE30` | `WORLD` | 0-15 | HC573 | CHR/MAP region select glue | World change (rare) |
| `$FE31`-`$FE36` | `BG_BANK_0`..`5` | bank **0-3** (optional helpers) | HC573 (may share packages with scroll/MAP) | Software / load helpers; **not** live BG CHR mux | Optional bulk stamp into slot attrs. Live bank is **per-tile attr** |
| `$FE37` | `SPR_BANK` | bank **0-3** (optional helper) | HC573 | Software / load helpers; **not** live sprite CHR mux | Optional bulk stamp into OAM attrs. Live bank is **per-sprite attr** |
| `$FE38` | `PAL_ROW` | row 0-7 (hint) | optional latch / software convention | Software still **must** copy indices into `$FE08`/`$FE09` | Row change |
| `$FE80` | `PRG_WINDOW` | optional window into single PRG | HC573 | PRG `/CE` + high address | Only if PRG exceeds `$8000` window; v0 often unused |
| `$FE90`-`$FE92` | `MAP_ADDR_*` | 24-bit MAP cursor | HC573x3 (or packed) | MAP `/CE` + flash A[23:0] | Seek before streaming a screen |
| `$FE10`/`$FE11` | `VRAM_ADDR_*` | 15-bit VRAM cursor | HC573 | VRAM mux on CPU phase | Before each VRAM run |

**BG bank in one sentence:** each nametable tile byte is an index **0-255** inside a CHR BG bank; that bank is selected by **attr bits 1-0** for that **8x8 tile**. Screens are not hardware-tied to one bank. `$FE31`-`$FE36` may still exist as optional stamp helpers. Mixed banks in one frame come from per-tile attrs (no mid-frame bank-latch split required).

**BG attr bits (hardware vs software):** same low nibble layout as OAM (`BANK` / `PAL` / `FLIP_*`); high bits differ.

| Bits | Name | Path |
|------|------|------|
| 1-0 | `BANK` | Hardware -- CHR address mux (measure timing on BG island) |
| 3-2 | `PAL` | Hardware -- compositor |
| 4-5 | `FLIP_H` / `FLIP_V` | Hardware -- shifter reverse / fine-Y XOR (leftover PLD/74HC, or 4th ATF22V10 if equations run short) |
| 6-7 | `SOLID` / `ANIM` | Software -- video ignores; CPU / kit drive collision and living-tile nametable updates |

Full bit table and software model: [`02`](02_graphics_worlds_memory.md). Dot clock / CRT / sprite HBlank unchanged vs denser attr streams (~480 B/screen); those only affect CPU load cost.

**Parallax and palettes:** plane slots have their **own** nametable+attr data (per-tile `BANK` like the camera). Attrs still index the **same** active BG palette buffer as the playfield (`$FE08`/`$FE09`). No second set of 4 BG colors for parallax. MAP "palette row" on a parallax-flagged screen is **ignored / inherited** from the playfield (see `02`).

Package count note: planning shows roughly **HC573x6** in the "scroll / optional bank helpers / MAP addr" cluster plus more for palette/compositor - bits are packed across chips; schematic will assign exact pin maps. The **address -> latch -> consumer** contract above is what software and Studio must assume.

### Active palette buffer (small dedicated RAM / latches)

| What | CPU view | Physical store | Who reads | How often |
|------|----------|----------------|-----------|-----------|
| 8 palettes x 4 master indices (32 bytes), shared color 0 | `$FE08` addr, `$FE09` data | Palette RAM or HC573 bank on the board (not nametable VRAM) | BG + sprite compositor every pixel | Load a row in VBlank (or mid-frame with raster timing). Same buffer for camera **and** plane BG |

Max unique colors without mid-frame reload: **25** (1 shared backdrop + 8x3). Master pool is still **64** in the Color PROM.

### ATmega1284P domain (not HC573)

| What | CPU view | Physical store | Who writes | Who reads | How often |
|------|----------|----------------|------------|-----------|-----------|
| OAM (64 x Y,tile,attr,X) | `$FE20`/`$FE21` | **1284 internal SRAM** | 6502 (captured into 1284) | 1284 sprite eval | Typically once per frame in VBlank; can update sooner |
| Pad bytes | `$FE60`/`$FE61` | 1284 (poll -> hold) | 1284 firmware | 6502 | Every frame / poll rate |
| Sprite pixels for next line | (none) | Line-buffer SRAM (above) | 1284 | Compositor | Every HBlank |

OAM capture may use an **HC573** (or equivalent) on the data path into the 1284; the **authoritative OAM image** lives in the MCU, not in VRAM.

### ATmega328P domain

| What | CPU view | Physical store | How often |
|------|----------|----------------|-----------|
| APU regs / sound | `$FE40`-`$FE5F` (contract) | **328P** firmware state + DAC/PWM path | Game writes; 328P synthesizes at audio rate |

### Ownership and buses (summary)

| Bus / resource | Owner rules |
|----------------|-------------|
| System RAM | CPU-only. No interleave |
| VRAM | CPU on CPU phase; BG fetch on PPU phase |
| CHR (cart) | BG fetch on visible dots; **1284** may own in HBlank for sprites |
| Line buffer | Beam reads show half; 1284 writes fill half; swap each logical line |
| Color PROM | Video path only; not on 6502 D-bus during play |

See **Sprite line buffer (how it works)** below. Camera / VRAM slot semantics: [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md).

## Sprite line buffer (how it works)

Sprites are **not** drawn by the 6502 into a framebuffer. The **ATmega1284P** owns sprite evaluation and fills a **line buffer** in SRAM. The BG path reads that buffer when compositing each scanline.

### Technical summary

| Item | Detail |
|------|--------|
| OAM | **64** sprites. CPU: write index to **`$FE20`**, data to **`$FE21`** (auto-inc). Entry order `Y, tile, attr, X`. Positions are **logical** (128x120 space). Attr: `BANK`/`PAL`/`FLIP_*`/`PRIORITY`/`SIZE` (see `02`) |

| Per scanline cap | **16** sprites on one **logical** row |
| Line buffer SRAM | **256 bytes** used on the third AS6C62256: two **128-byte** halves |
| Half A | `$000-$07F` - one logical row of sprite data (128 pixels) |
| Half B | `$080-$0FF` - one logical row of sprite data (128 pixels) |
| Roles | **Ping-pong:** while the beam **displays** one half, the 1284 **writes** the other |
| Latency | **One scanline** pipeline, **not** a full-frame delay |
| Scale | Raster path only: duplicate or center into 256x240. Line buffer stays 128-wide |

Per scanline timeline:

1. **Visible line N:** compositor reads sprite pixels for line **N** from the half filled in the previous HBlank.
2. **HBlank after line N:** 1284 scans OAM for sprites on line **N+1**, fetches CHR, writes that row into the idle half.
3. **Visible line N+1:** halves swap.

```text
Logical rows (full frame is NOT stored):

  ... 49 50 51 52 ... 95
         ^  ^
      show  prepare during HBlank

SRAM (two trays only):

      Half A                 Half B
   +-------------+        +-------------+
   | 128 px row  |        | 128 px row  |
   +-------------+        +-------------+
    show N / fill N+1      fill N+1 / show N
         (roles swap every logical line)
```

The 6502 maintains **who** is on screen (OAM). The 1284 does **per-row** work in HBlank. CHR: BG owns the cart during visible dots; 1284 may own CHR in HBlank.

This is **not** a full-frame framebuffer. It is one scanline of pipeline delay.

### How BG and sprites meet on screen

Each **logical** pixel is roughly:

1. **BG path:** VRAM camera slots + scroll -> CHR -> BG palette index
2. **Sprite path:** line buffer at logical X (`0..127`) -> sprite palette index (or transparent)
3. **Compositor:** priority -> **6-bit** master index

Then the **raster path** (SCALE DIP) places that pixel into the 256x240 field. Color PROM + DAC follow.

Full sprite pipeline steps (same frame, different jobs):

1. CPU uploads OAM through **`$FE20` (addr)/`$FE21` (data, auto-inc)**
2. 1284 scans OAM for the **next** line
3. active sprite palette buffer maps indices through the selected sprite palette
4. during HBlank, 1284 fetches sprite CHR and fills the next line-buffer half
5. visible line reads the last-filled half

## Interleaved VRAM model

The key architectural trick:

- CPU phase: CPU may use `$FE10`/`$FE12` VRAM port
- PPU phase: BG fetch reads nametable and attrs

This removes the VBlank-only VRAM update bottleneck common on classic consoles like the NES, while still using one VRAM chip. See [`07_pitch.md`](07_pitch.md) for NES comparison.

## Rendering pipeline

### BG

1. beam counters determine visible position
2. scroll + arrangement choose nametable slot
3. slot tile byte and attr come from VRAM
4. attr `BANK` bits (1-0) pick that tile's CHR BG bank; `FLIP_H` / `FLIP_V` (4-5) reverse the shifter / fine-Y as needed
5. attr `PAL` bits (3-2) pick BG palette 0-3; `SOLID` / `ANIM` are software-only (not wired into video)
6. active BG palette buffer maps the tile's 2-bit color to a **master index 0-63**
7. tile row fetch returns 2bpp data
8. shifters output that master index into the **Color PROM** -> R/G/B DAC

### Sprites

See **Sprite line buffer (how it works)** above. Short version:

1. CPU uploads OAM through `$FE20` (addr)/`$FE21` (data)
2. 1284 scans OAM for the **next** line
3. active sprite palette buffer maps sprite 2bpp to a **master index 0-63** (or transparent)
4. during HBlank, 1284 fetches sprite CHR and fills the next line-buffer half
5. visible line reads the last-filled half. Compositor resolves BG vs sprite, then **Color PROM** -> DAC

One-line pipeline, not a full-frame delay.

## Palette hardware model

**Master RGB** comes from the **Color PROM** (see above). Carts never carry those RGB bytes.

Each cart may store palette **index** blobs in flash, located by a **pointer table** (no palette compression/special packing):

- cart-global minimum: **1 BG Palette + 1 sprite Palette** (one 4-color set of indices each)
- optional per world: **BG palette bank** and/or **sprite palette bank**, each up to **8 palette rows x 4 palettes**

Runtime selection is always by **palette row**, and **BG palette row N** and **sprite palette row N** are locked together.

When palette row `N` is active, the **active palette buffer** holds **8 palettes**:

- 4 BG palettes from BG palette row `N`
- 4 sprite palettes from sprite palette row `N`

All 8 share the same **color 0** master index (universal backdrop for that row). Software must write that shared index into every slot when loading the row (see `02_graphics_worlds_memory.md`).

The CPU-facing model is dedicated palette **index** registers via **`$FE08`/`$FE09`**. Those indices address the Color PROM each pixel. **Fallback resolution for which indices to load is not hardware logic.** Boot/Studio runtime follows cart pointers and copies the selected row into registers.

No extra ICs are required for palette **banks**, synced row selection, or fallback rules beyond the Color PROMs already on the board.

## Timing-facing rules

- mid-frame **scroll** writes take effect on the **next tile fetch** (allow up to **8 px** delay if a write lands mid-tile)
- mid-frame OAM / sprite-attr changes are sampled on the **next** line eval (1284); attr `BANK`/`SIZE`/`PRIORITY`/`FLIP_*`/`PAL` live in OAM, not `$FE37`
- mid-frame **attr `BANK`** (or other attr) changes follow nametable tear rules below - they are VRAM, not a slot latch
- clean splits should write during **HBlank**
- raster IRQ is the intended split mechanism
- palette-buffer rewrites follow the same rule: safest in VBlank, possible mid-frame with raster timing

### Nametable / attr tear avoidance

Changing a tile index or attr in VRAM while the beam is drawing that cell can produce a **torn tile** (mixed old/new pattern lines). Hardware does **not** latch a whole cell for the frame.

**Required software policy** (document for games + implement in the dev kit):

1. Do **not** commit nametable/attr writes for a cell whose logical Y range overlaps the current beam scanline, unless you accept tear.
2. Safe defaults: commit visible BG updates in **VBlank**, or defer until the beam has **passed** the cell (effect next frame) or has **not reached** it yet (effect this frame, clean).
3. Optional helper: `vram_poke_tile_safe(addr, data)` that compares beam Y to the cell's tile row and either writes, queues, or waits for NMI.

Big MAP streams stay VBlank-oriented for CPU budget; tear policy is about **small live pokes** (anim, damage tiles, HUD cells). Full write-up: [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) (*Live VRAM updates and tear avoidance*).

## Input contract

Two bytes only:

- `$FE60` = player 1
- `$FE61` = player 2

Bits:

0 right, 1 left, 2 down, 3 up, 4 X, 5 Y, 6 coin/select, 7 start

## Variant notes

### Retr01-A

- through-hole
- cabinet IDC
- RGBS + S-Video + composite pads

### Retr01-C

- same architecture
- **3-wire controllers** with **ATtiny85** (draft) MCU in each pad
- wires: **VCC, GND, DATA** (open-drain DATA). Console **ATmega1284P** is master: poll, then read one button byte
- software-visible bytes stay **`$FE60`/`$FE61`** with the same bit layout as Retr01-A

### Retr01-H

- later SMD handheld
- same software-facing map

## What is intentionally not in hardware

- no framebuffer
- no sprite-0 hit API
- no sprite-vs-BG gameplay collision
- no hardware sprite DMA
- no on-board HDMI

## Practical takeaway

For software people:

- treat `$FExx` as the hardware API; see **Where state lives** for which poke hits SRAM vs HC573 vs MCU
- write game state in system RAM
- stream screens through MAP -> VRAM (attrs already carry per-tile `BANK`); optional `$FE31`-`$FE36` only if you use bulk stamp helpers
- write OAM through the 1284 port; sprites do not touch nametable VRAM
- treat palette changes as writes to one shared active buffer (parallax included)
- let the board resolve tiles to pixels

Protoboard bring-up: [`06_protoboard_module_tests.md`](06_protoboard_module_tests.md) (island map, interactions, pass criteria).


## Extra: Summary of parts needed.

~52 ICs on the Retr01-A motherboard (v0 plan). Optional +1 ATF22V10 if equations overflow.

Compute

- 1x W65C02S -- game CPU (8 MHz)
- 1x ATmega1284P -- OAM, sprite eval, line-buffer fill, pad bytes
- 1x ATmega328P -- NES-style APU

Memory

- 3x AS6C62256 (32 KB each) -- system RAM, interleaved VRAM, sprite line buffer
- 1x AT28C64B -- board EEPROM (save/config)
- 1x **2 MB** parallel NOR (v0 socket; SST39SF040 512 KB OK for early bring-up) -- cart image: PRG / CHR / MAP
- 3x AT28C16 -- Color PROM (R/G/B master palette to DACs)

Glue / video

- 3x ATF22V10 -- decode, timing, CHR/VRAM gating (4th if needed for flip/bank)
- 74HC157 -- VRAM / line-buffer address mux
- 74HC245 -- data bus isolation
- 74HC573 -- scroll, MAP addr, optional bank helpers, OAM capture, other $FExx latches
- 74HC161 -- beam X/Y counters
- Plus more 74HC gates/comparators (00/04/08/14/32/86/688 class) for compositing, raster compare, SCALE, etc. -- most of the rest of the ~52

Roles in one line: 6502 runs the game; discrete 74HC+PLD draws BG; 1284 draws sprites into a line buffer; 328P makes sound; three SRAMs split CPU / nametables / sprites; three Color PROMs turn palette indices into RGB.