# Retr01 Hardware Implementation

This doc merges the board walkthrough, software-engineer explanation, chip-count plan, and schematic-facing hardware notes.

## Board-level picture

Retr01-A is three active compute domains on one 5 V board:

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
| 74HC157 | muxes | VRAM and line-buffer address mux |
| 74HC245 | transceivers | data isolation |
| 74HC573 | latches | scroll, banks, MAP address, OAM capture |
| 74HC161 | counters | beam X/Y |

### Datasheets and pin wiring

Official PDFs and **Retr01-A v0 breadboard pin tables** (which pins to strap, bus connections, module-by-module checklist) live in [`06_protoboard_module_tests.md` section 3](06_protoboard_module_tests.md#3-ic-reference--datasheets-and-pins).

Quick links for the main silicon:

| Part | Datasheet |
|------|-----------|
| W65C02S | [WDC PDF](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf) |
| AS6C62256 | [Alliance PDF](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf) |
| ATF22V10 | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf) |
| ATmega1284P | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf) |
| ATmega328P | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf) |
| AT28C64B | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/doc4428.pdf) |
| SST39SF040 (cart flash) | [Microchip PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf) |
| SN74HC157 / 245 / 573 / 161 | [TI 74HC family](https://www.ti.com/logic-circuit/standard-logic/74hc-family/overview.html) - use the part-specific PDF linked in section 3 |

For **74HC glue** (00, 04, 08, 14, 32, 86, 688), see the full index in section 3.1 of the protoboard doc.

## Frozen v0 board plan

- through-hole only
- planning total: **49 motherboard ICs**
- 3x ATF22V10, not Lattice GAL
- if PLD equations overflow, add a **4th ATF22V10**, not a different family

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

See **Sprite line buffer (how it works)** below for a full explanation. BG scrolling and VRAM slots are in [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) section *What VRAM holds*.

## Sprite line buffer (how it works)

Sprites are **not** drawn by the 6502 into a framebuffer. The **ATmega1284P** owns sprite evaluation and fills a **line buffer** in SRAM. The BG path reads that buffer when compositing each scanline.

### Technical summary

| Item | Detail |
|------|--------|
| OAM | **64** sprites, uploaded by CPU via **`$FE20` / `$FE21`** |
| Per scanline cap | **16** sprites on one row |
| Line buffer SRAM | **512 bytes** on the third AS6C62256: two **256-byte halves** |
| Half A | `$000-$0FF` - one row of sprite data (256 pixels) |
| Half B | `$100-$1FF` - one row of sprite data (256 pixels) |
| Roles | **Ping-pong:** while the beam **displays** one half, the 1284 **writes** the other |
| Latency | **One scanline** pipeline, **not** a full-frame delay |

Per scanline timeline:

1. **Visible line N:** compositor reads sprite pixels for line **N** from the half that was filled during the **previous** HBlank.
2. **HBlank after line N:** 1284 scans OAM for sprites on line **N+1**, fetches CHR from cart, writes line **N+1** into the **idle** half.
3. **Visible line N+1:** halves have **swapped** - display uses the half just written, 1284 prepares the next row in the other half.

```text
         Half A                    Half B
    +----------------+        +----------------+
    | row of sprites |        | row of sprites |
    +----------------+        +----------------+
           |                          |
    Line N:   READ (show)              WRITE (prepare N+1)
    Line N+1: WRITE (prepare N+2)      READ (show)
           |                          |
           +-------- swap every line --+
```

The 6502 maintains **who** is on screen (OAM positions, tiles, attrs). The 1284 does **per-row** work: which sprites hit this Y, fetch tile bits, pack one horizontal strip. That work happens in **HBlank**, the short gap after each drawn line.

CHR bus note: during the visible line, the **BG path** owns CHR for tile fetch. During **HBlank**, the **1284** may own CHR for sprite tile fetch into the line buffer (see CHR ownership above).

### Plain English

Imagine **two narrow trays**, each holding exactly **one horizontal row** of sprite pixels (256 pixels wide - one per column on the screen).

- While the TV paints **row 100**, it uses the tray that was filled **just before** row 100 started.
- In the **short gap** after row 100 finishes, the sprite chip fills the **other** tray with everything needed for row **101**.
- When row 101 starts, the TV uses that tray, and the chip goes back to preparing row **102** in the first tray.

So the system is always **one row ahead in preparation**: show this line / prepare the next line, **swap trays every line**. The CPU does not paint sprites pixel by pixel each frame - it updates the **sprite list**, and the 1284 handles rows in the gaps between scanlines.

This is **not** "draw the whole frame while calculating the next frame" (that would be double buffering entire screens). It is **one scanline of delay** between preparing a row and displaying it - enough time to fetch CHR and respect the 16-sprites-per-line limit without blocking the 6502.

### How BG and sprites meet on screen

Each output pixel is roughly:

1. **BG path:** pick tile from VRAM camera slots + scroll, fetch CHR, apply palette -> BG color
2. **Sprite path:** read line buffer at current X -> sprite color (or transparent)
3. **Compositor:** transparency and priority rules -> final pixel

Full sprite pipeline steps (same frame, different jobs):

1. CPU uploads OAM through **`$FE20` / `$FE21`**
2. 1284 scans OAM for the **next** line
3. active sprite palette buffer maps indices through the selected sprite palette
4. during HBlank, 1284 fetches sprite CHR and fills the next line-buffer half
5. visible line reads the last-filled half

## Interleaved VRAM model

The key architectural trick:

- CPU phase: CPU may use `$FE1x` VRAM port
- PPU phase: BG fetch reads nametable and attrs

This removes the VBlank-only VRAM update bottleneck common on classic consoles like the NES, while still using one VRAM chip. See [`07_pitch.md`](07_pitch.md) for NES comparison.

## Rendering pipeline

### BG

1. beam counters determine visible position
2. scroll + arrangement choose nametable slot
3. slot tile byte and attr come from VRAM
4. slot BG bank latch picks CHR bank
5. active BG palette buffer maps the tile's 2-bit color through the selected BG palette
6. tile row fetch returns 2bpp data
7. shifters output BG pixel

### Sprites

See **Sprite line buffer (how it works)** above. Short version:

1. CPU uploads OAM through `$FE20/$FE21`
2. 1284 scans OAM for the **next** line
3. active sprite palette buffer maps sprite color indices through the selected sprite palette
4. during HBlank, 1284 fetches sprite CHR and fills the next line-buffer half
5. visible line reads the last-filled half

One-line pipeline, not a full-frame delay.

## Palette hardware model

Each cart may store sparse **palette banks** in flash:

- cart-global minimum: **1 BG palette + 1 sprite palette**
- optional per world: **BG palette bank** and/or **sprite palette bank**, each up to **8 palette rows x 4 palettes**

Runtime selection is always by **palette row**, and **BG palette row N** and **sprite palette row N** are locked together.

When palette row `N` is active, the **active palette buffer** holds **8 palettes**:

- 4 BG palettes from BG palette row `N`
- 4 sprite palettes from sprite palette row `N`

All 8 share the same **color 0** master index (universal backdrop for that row).

The hardware-facing model is dedicated palette registers or palette RAM. It is **not** nametable VRAM. **Fallback resolution is not hardware logic.** Boot code or Retr01 Studio export/runtime code chooses the source palette bank entry and copies the selected row into registers.

No extra ICs are required for palette banks, synced row selection, or fallback rules. That is cartridge encoding plus a burst of CPU stores when the palette row changes.

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
- 3-wire controllers with MCU in pad

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

Protoboard bring-up: [`06_protoboard_module_tests.md`](06_protoboard_module_tests.md) - test ICs in separate islands before full integration. **Pin-level wiring:** [section 3 IC reference](06_protoboard_module_tests.md#3-ic-reference--datasheets-and-pins).
