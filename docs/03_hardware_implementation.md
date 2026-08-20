# Retr01 Hardware Implementation

This doc merges the board walkthrough, software-engineer explanation, chip-count plan, and schematic-facing hardware notes.

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
| SST39SF040 (cart flash) | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf) |
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

## Output scale (board DIP)

Retr01-A ships with a **SCALE** DIP (or jumper):

- **open = 1x** (default): center **128x96** in the **256x240** active RGBS field (64 px side margins, 72 line top/bottom margins)
- **closed = 2x**: double to **256x192**, letterbox with **24** lines top and **24** bottom inside the same **256x240** active field

The beam counters and **341x262** timing do not change with the DIP. Only logical-to-raster mapping and border blanking change.

## Clocks

| Clock | Value | Job |
|------|-------|-----|
| CPU | **8.000 MHz** | W65C02S + VRAM ownership phase |
| Dot | **5.369318 MHz** | beam counters, fetch, compositor |
| 1284 | **20 MHz** | sprite/input firmware |
| 328P | **16 MHz** | APU firmware |

CPU and dot clocks are **independent**.

## Ownership and buses

### System RAM

- CPU-only
- no interleave

### VRAM

- shared between CPU port and BG fetch path
- CPU owns VRAM only on CPU phase
- BG fetch path owns VRAM on PPU phase

### CHR

- not in VRAM
- fetched from cart CHR-ROM
- visible line: BG fetch path owns CHR
- HBlank: 1284 may own CHR for sprite fetch

### Line buffer

- beam reads visible sprite data
- 1284 writes the other half during HBlank
- halves swap each scanline

See **Sprite line buffer (how it works)** below for a full explanation. BG scrolling and VRAM slots are in [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) (*Camera, VRAM, and scroll*).

## Sprite line buffer (how it works)

Sprites are **not** drawn by the 6502 into a framebuffer. The **ATmega1284P** owns sprite evaluation and fills a **line buffer** in SRAM. The BG path reads that buffer when compositing each scanline.

### Technical summary

| Item | Detail |
|------|--------|
| OAM | **64** sprites. CPU: write index to **`$FE20`**, data to **`$FE21`** (auto-inc). Entry order `Y, tile, attr, X`. Positions are **logical** (128x96 space) |
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
4. slot BG bank latch picks CHR bank
5. active BG palette buffer maps the tile's 2-bit color to a **master index 0-63**
6. tile row fetch returns 2bpp data
7. shifters output that master index into the **Color PROM** -> R/G/B DAC

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

- mid-frame bank/scroll writes take effect on the **next tile fetch**
- allow up to **8 px** delay if a write lands mid-tile
- clean splits should write during **HBlank**
- raster IRQ is the intended split mechanism
- palette-buffer rewrites follow the same rule: safest in VBlank, possible mid-frame with raster timing

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

- write game state in system RAM
- stream screens through MAP and VRAM
- write OAM through the 1284 port
- treat palette changes as writes to an active palette buffer
- treat `$FExx` as the hardware API
- let the board resolve tiles to pixels

Protoboard bring-up: [`06_protoboard_module_tests.md`](06_protoboard_module_tests.md) (island map, interactions, pass criteria).
