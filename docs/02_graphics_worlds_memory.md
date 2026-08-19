# Retr01 Graphics, Worlds, and Memory

This doc is the merged source for the graphics model, world layout, VRAM layout, and CPU-visible memory map.

## Display and timing

| Item | Value |
|------|-------|
| Visible resolution | **256x240** |
| Tile size | **8x8** |
| Dot clock | **5.369318 MHz** |
| Dots per scanline | **341** |
| Scanlines per frame | **262** |
| Frame rate / NMI | about **60.098 Hz** |

CPU and dot clocks are **independent**. The CPU runs at **8.000 MHz**. The beam and BG path run on the dot clock.

## Worlds and screens

- A cart has up to **8 worlds**
- A world uses a sparse virtual grid up to **16x16**
- A world may store up to **64 screens**
- A screen is **32x30** tile indices plus packed attrs
- A screen is stored in MAP-ROM and identified by `(col, row)`

### What a world contains

- **4 CHR BG banks** and **4 CHR sprite banks** (tile patterns)
- optional **BG palette bank** and **sprite palette bank** (color data)
- MAP directory + compressed screen payloads

### What a screen stores

- tile plane: **960 bytes**
- attr plane: **240 bytes**
- flags with **BG bank 0-3**
- MAP payload offset

Parallax screens use the same screen format but are marked non-enterable.

## BG banks and sprite banks (CHR)

These are **CHR tile banks**, not palette banks.

| Term | Meaning |
|------|---------|
| **BG bank** | **256 BG tiles**, **16x16** tile grid, **4 KB** |
| **Sprite bank** | **256 sprite tiles**, **16x16** tile grid, **4 KB** |

Per world:

- **4 BG banks**
- **4 sprite banks**
- total CHR per world: **32 KB**

Across the full cart:

- max CHR budget: about **256 KB**

### Runtime bank rules

1. Each screen names an **authored BG bank** in MAP metadata.
2. When software loads or seam-streams a screen into a nametable slot, it also copies that screen's BG bank into that slot's **BG bank latch**.
3. **Camera slots 0-3** and **plane slots 4-5** each have their own BG bank latch.
4. **Sprite bank** is separate and global within the current world.
5. Changing a BG bank does **not** change the sprite bank, and vice versa.

## Scrolling and live VRAM

### Camera

- `scroll_x` and `scroll_y` are **one byte each** (`0-255`, wrap)
- The camera can sample **1, 2, or 4** live screens from VRAM slots **0-3**
- Smooth scrolling means multiple neighboring screens may be visible at once

### Why VRAM has six slots

- **Slots 0-3**: live **camera** field, up to four playfield screens
- **Slots 4-5**: optional **parallax plane** only

Slots 4-5 are **not** fifth and sixth playfield screens.

### VRAM layout

| VRAM offset | Size | Purpose |
|-------------|------|---------|
| `$0000-$07FF` | 2 KB | nametable slot 0 |
| `$0800-$0FFF` | 2 KB | nametable slot 1 |
| `$1000-$17FF` | 2 KB | nametable slot 2 |
| `$1800-$1FFF` | 2 KB | nametable slot 3 |
| `$2000-$2FFF` | 4 KB | scratch / streaming temp |
| `$3000-$37FF` | 2 KB | plane slot 4 |
| `$3800-$3FFF` | 2 KB | plane slot 5 |
| remainder | reserved | future |

Each 2 KB slot holds:

- tiles at `+0x000` (**960 bytes**)
- packed attrs at `+0x3C0` (**240 bytes**)

One attr byte is a **2x2 attr quadrant** with four 2-bit palette fields, one per tile.

## Palettes and compositing

### Palette terminology

Use these words consistently. Do **not** mix them with CHR **tile banks**.

| Term | Meaning | Not the same as |
|------|---------|-----------------|
| **Master palette** | 64 global RGB colors, source of all color indices | A palette row or palette bank |
| **BG palette bank** | BG-side cartridge store of up to **32 palettes** in **8 palette rows x 4 palettes** | CHR **BG bank** (tile patterns) |
| **Sprite palette bank** | Sprite-side cartridge store of up to **32 palettes** in **8 palette rows x 4 palettes** | CHR **sprite bank** (tile patterns) |
| **Palette row** | One row of **4 palettes** inside a palette bank, row index **0-7** | A screen row or attr row |
| **Palette** | One 4-color set: **4 master-color indices** | The whole palette bank |
| **Active palette buffer** | The **8 palettes** currently on screen: **4 BG + 4 sprite** copied from one selected palette row | VRAM or nametable data |

Layout of one palette bank:

```text
Palette bank (BG or Sprite)
+-- Palette row 0: [Palette] [Palette] [Palette] [Palette]
+-- Palette row 1: [Palette] [Palette] [Palette] [Palette]
+-- ...
+-- Palette row 7: [Palette] [Palette] [Palette] [Palette]
```

Each **Palette** is 4 bytes (4 master indices). A full bank is **32 palettes = 128 bytes** if every slot is authored.

### Global master palette

Retr01-A, C, and H all share one **master palette** of **64 unique colors**. It is the source of all color indices used by the system.

```text
#000000 #290514 #2A0507 #230F06 #1E1306 #1A1605 #141807 #061A07 #051A13 #071918 #08181C #071722 #030B3D #16033A #20052D #260420
#363636 #740A40 #77091A #693512 #5D3F0E #514617 #424C19 #13511A #16503F #114E4D #164D58 #164A66 #163794 #472990 #5F167D #6C115F
#949494 #C04A7A #C54A4D #B8601B #A27326 #8F7E2F #77872D #209030 #2E8E72 #318B89 #1F889C #2483B5 #4D77D7 #7E6AD3 #9D5DBF #B352A0
#FFFFFF #F1A2BB #F1A6A1 #F1A983 #EEAC44 #D4BA33 #B0C841 #73D275 #22D0A6 #3BCDC9 #48C9E4 #88C4ED #A4BDEF #BBB5F1 #D5A9EF #F09BDD
```

This table belongs in cartridge data and is the canonical color source for the whole family.

### Cart palette storage (sparse)

Palettes are stored in cartridge data. Programmers do **not** need to define every palette in every palette bank for every world.

Three layers exist:

| Layer | What it holds | Required? |
|-------|---------------|-----------|
| **Master palette** | 64 RGB colors | optional in cart, system default exists |
| **Cart global palette banks** | at least **1 BG palette + 1 sprite palette** shared across worlds | minimum authoring contract |
| **World palette banks** | optional **BG palette bank** and/or **sprite palette bank** for that world | optional per world |

Within each palette bank, storage is **sparse**: only authored palettes occupy cart bytes.

### Palette fallback chain

Resolution happens in **software at load time** (boot, world enter, or `load_screen`), not in the PPU per pixel. The hardware always reads whatever is already in the **active palette buffer**.

```text
1. world palette bank entry (if the world defines one)
2. else cart global palette bank (at least 1 BG + 1 sprite palette for a valid cart)
3. else system default palettes (baked into Retr01 Studio / system startup)
```

This needs **no extra chips**. The CPU copies the resolved active row into palette registers when the palette row changes.

### Active palette buffer

The PPU can only show **4 BG palettes** and **4 sprite palettes** at one time. Together that is **8 palettes** in the **active palette buffer**.

That buffer is dedicated **palette registers / palette RAM**. It is **not** nametable VRAM.

Selection rule:

- software selects a **palette row** index **0-7**
- **BG palette row N** and **sprite palette row N** are always selected together
- there is no independent "BG row 4 + sprite row 2" mode

When palette row `N` is selected, the active buffer loads:

- all **4 BG palettes** from BG palette row `N`
- all **4 sprite palettes** from sprite palette row `N`

At runtime inside that active row:

- BG tile attrs choose among **BG palettes 0-3** in the active buffer
- sprite OAM attrs choose among **sprite palettes 0-3** in the active buffer
- software may rewrite the active palette buffer during VBlank, or mid-frame with raster timing if needed

To use palette row 4 instead of row 0, software changes the selected palette row and reloads the active buffer. Tile attrs and OAM attrs still only ever index **0-3** within the current row.

### Shared backdrop across the active buffer

All **8 palettes** in the active buffer share the same **color 0** master index.

That gives one universal backdrop color for the current palette row, NES-style, across both BG and sprite planes.

Rules:

- every **Palette** in the active row uses that same master index at color 0
- on **BG**, color 0 is the visible shared backdrop
- on **sprites**, pattern color 0 is still **transparent**

So the shared color is a palette-definition and selection rule, not a rule that forces visible sprite pixels to draw color 0.

### Practical authoring rule

A screen may effectively use only part of the active row. For example:

- one screen may only use BG palettes 0-1 inside the active row
- another may use all 4 BG palettes in that row

That is fine. The hardware contract stays **one synced palette row** -> **4 BG + 4 sprite palettes active**.

Sprite priority rule:

1. transparent sprite pixel -> show BG
2. sprite-behind bit + opaque BG -> show BG
3. otherwise show sprite

### Screen / MAP metadata

Screens may optionally name a **palette row** index **0-7** in MAP metadata, similar to how they already name a CHR BG bank.

Because attrs only index **0-3**, reaching another palette row means **changing the selected palette row** and reloading the active buffer, not widening attr bits.

### Palette effects

These should be exposed as software routines in the Retr01 runtime library (used by Studio-generated games and future tooling), backed by active-palette-buffer writes:

1. **Fade all active palettes to black**
2. **Fade all active palettes to white**
3. **Interpolate active palettes toward another palette row**
4. **Cycle colors or palette rows** for effects such as waterfalls, power-ups, warning flashes, and glowing objects

Recommended scope:

- hardware provides the active palette buffer and the ability to rewrite it
- Retr01 Studio and the runtime library provide helpers that compute and upload the new palette contents

Palette cycling may work in two useful ways:

- rotate colors **inside** one 4-color palette
- step to another **palette row** (BG and sprite together)

## MAP-ROM model

MAP-ROM is **not** directly memory-mapped into the CPU space.

Software reads MAP through **`$FE90`**:

1. write 24-bit MAP address
2. read MAP data
3. hardware auto-increments

Recommended MAP directory row:

- `col`
- `row`
- `flags` (`bit0 = parallax`, `bits1-2 = CHR BG bank`, `bits3-5 = palette row 0-7`)
- `data_off` (24-bit)

## CPU memory map

| Range | Region | Notes |
|-------|--------|-------|
| `$0000-$7FFF` | System RAM | CPU-only |
| `$8000-$FDFF` | PRG window | banked through `$FE80` |
| `$FE00-$FEFF` | I/O page | PPU, VRAM port, OAM, CHR bank latches, APU, MAP, input |
| `$FF00-$FFFF` | PRG high | vectors included |

### Important I/O blocks

| Range | Purpose |
|-------|---------|
| `$FE00-$FE0F` | PPU control, scroll, raster compare |
| `$FE10-$FE1F` | VRAM port |
| `$FE20-$FE2F` | OAM port into 1284 |
| `$FE30-$FE3F` | world select + BG bank latches + sprite bank |
| `$FE40-$FE5F` | APU |
| `$FE60-$FE6F` | controllers / cabinet |
| `$FE80-$FE8F` | PRG bank |
| `$FE90-$FE9F` | MAP port |

## Raster and parallax

Retr01 uses **raster IRQ**, not NES sprite-0 hit.

- `raster_y`: target scanline
- `raster_hit`: status
- `raster_irq_enable`: IRQ gate

Parallax is implemented as a **separate scanline band** that points to plane slots **4-5**.

Rules:

- parallax forces a **1-axis camera**
- plane slots are not part of the 2x2 camera
- BG banks stay per slot
- sprite bank remains separate/global

## What software should remember

- nametable bytes are tile indices `0-255`
- tile bytes do **not** store a bank number
- the slot's BG bank latch chooses the CHR bank
- MAP chooses which screen to load
- the current world chooses which 32 KB CHR chapter is active
