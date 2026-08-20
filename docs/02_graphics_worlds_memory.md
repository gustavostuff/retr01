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
| Frame rate/NMI | about **60.098 Hz** |

CPU and dot clocks are **independent**. The CPU runs at **8.000 MHz**. The beam and BG path run on the dot clock.

## Worlds and screens

- A cart has up to **8 worlds**
- A world uses a sparse virtual grid up to **16x16**
- A world may store up to **64 screens**
- A screen is **32x30** tile indices plus packed attrs
- A screen is stored in MAP-ROM and identified by `(col, row)`

### What a world contains

- **4 CHR BG banks** and **4 CHR sprite banks** (tile patterns)
- optional **BG palette bank** and **sprite palette bank** (master **indices** 0-63, not RGB bytes)
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

- max CHR budget: about **256 KB** (8 worlds x 32 KB)
- planning flash class **SST39SF040** is **512 KB**. **v0:** parallel flash in an on-board **32-pin socket** for bring-up. **Later:** same image lives on the **cartridge**. Max CHR is about **256 KB** (8x32 KB). PRG + MAP share the rest (exact bank map still flexible)

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

### What VRAM holds (camera and scroll modes)

This section answers a common design question: when the camera scrolls or switches rooms, **how much tile data is actually in VRAM?**

#### Technical summary

| Storage | Size per slot | What it is |
|---------|---------------|------------|
| Camera slots **0-3** | **2 KB** each (32x30 tiles + attrs) | Up to **four full screens** in a 2x2 camera field |
| Plane slots **4-5** | **2 KB** each | Up to **two full parallax backgrounds** (not walkable) |
| Scratch `$2000-$2FFF` | **4 KB** | Streaming temp, not part of the live picture |

Each camera slot holds a **complete screen** (960 tile bytes + 240 attr bytes). It is **not** one screen plus a small tile border or "2-tile perimeter" around a single room.

The TV always shows **256x240 pixels**. `scroll_x` and `scroll_y` slide a **viewport** over whatever tile field is active in slots 0-3. The hardware can use **1, 2, or 4** of those slots for the live camera (see scroll modes below).

**MAP-ROM** can store up to **64 screens per world**. **VRAM** only holds the **live** set: up to four camera screens plus two optional parallax screens. Software streams new screens from MAP into slots when the player moves through the world.

#### Camera slot arrangement (4-screen/pixel scroll)

When all four camera slots are in use, they form a fixed **2x2 grid** of whole rooms:

```text
+-------------+-------------+
|  Slot 0     |  Slot 1     |   top row
|  full screen|  full screen|
+-------------+-------------+
|  Slot 2     |  Slot 3     |   bottom row
|  full screen|  full screen|
+-------------+-------------+
        ^
        |
   256x240 viewport (scroll_x, scroll_y pick the window)
```

Smooth scrolling moves that viewport across the **internal seams** between these full screens. When the player crosses a room boundary, both neighboring rooms are already loaded - the picture slides, it does not wait for a one-tile-wide strip to be filled in at the edge.

#### Scroll modes (camera)

| Mode | Slots 0-3 in use | Scroll | What the player sees |
|------|------------------|--------|----------------------|
| **Pixel scroll, 1 slot** | One full screen | `scroll_x`/`scroll_y` pan inside that room | One room, camera pans within 256x240 |
| **Pixel scroll, 2 slots** | Two full adjacent screens (horizontal or vertical pair) | Scroll across the seam | Viewport can show part of room A and part of room B |
| **Pixel scroll, 4 slots** | Four full screens in 2x2 | Scroll across any internal seam, including corners | Viewport can straddle up to four rooms at once |
| **Instant screen switch** | New screen(s) loaded into slot(s) | Reset `scroll_x`/`scroll_y` to **0** (or another fixed position), no smooth pan | Picture **cuts** to the new room(s) - doors, warps, screen-at-a-time movement |

Instant switch uses the **same VRAM slots** as pixel scroll. The difference is **behavior**: no sliding viewport. Software loads the next room(s), resets scroll registers, and the image jumps.

Hardware scroll contract is only the rows above: **1/2/4 live slots** with pixel scroll, or **instant cut**. Studio constraints such as **dead zone** and **hybrid** (see `04_retr01_studio.md`) are **software camera policy** on top of those modes - they do not add new PPU scroll hardware.

#### Parallax slots (4-5)

Slots **4** and **5** are also **full 32x30 nametables**, but they are **background-only**:

- not part of the walkable 2x2 camera
- drawn **behind** the main playfield (optional **scanline band** chooses when they appear)
- use their own **BG bank latches**
- parallax setup locks the main camera to **one axis for the whole frame** when any H/V band is enabled (see Raster and parallax below)

So at maximum, VRAM can hold **four whole camera screens + two whole parallax screens** at once. The visible frame is still 256x240. Parallax layers sit under (or in a band under) the camera view.

#### Plain English: VRAM vs the world map

Think of **MAP** as the atlas of every room in the chapter. **VRAM** is the **workbench** with only the rooms you need **right now**.

- **Pixel scroll (up to 4 slots):** tape up to **four complete room blueprints** in a 2x2 square on the bench. The TV is a **fixed-size window** you slide over that square. Walking off the right edge of a room means you see **the right side of one full room and the left side of the next**, because both were already on the bench.
- **Instant switch:** swap the blueprint(s), put the window back at the start, **no sliding** - like turning a page.
- **Parallax (slots 4-5):** two **backdrop paintings** the same size as a room, hung **behind** the bench. The player does not walk on them. They add depth (sky, far hills). They scroll or appear in bands separately from the main camera.

This is **not** "one room plus a couple of extra tiles pasted on the edge." That pattern appears on some older hardware. Retr01 loads **whole screens** into named slots so seams are predictable.

### VRAM layout

| VRAM offset | Size | Purpose |
|-------------|------|---------|
| `$0000-$07FF` | 2 KB | nametable slot 0 |
| `$0800-$0FFF` | 2 KB | nametable slot 1 |
| `$1000-$17FF` | 2 KB | nametable slot 2 |
| `$1800-$1FFF` | 2 KB | nametable slot 3 |
| `$2000-$2FFF` | 4 KB | scratch/streaming temp |
| `$3000-$37FF` | 2 KB | plane slot 4 |
| `$3800-$3FFF` | 2 KB | plane slot 5 |
| `$4000-$7FFF` | 16 KB | reserved/future - **not** part of the live camera or plane contract. Do not rely on this region in software until a later rev assigns it |

Each 2 KB slot holds:

- tiles at `+0x000` (**960 bytes**)
- packed attrs at `+0x3C0` (**240 bytes**)

One attr byte is a **2x2 attr quadrant** with four 2-bit palette fields, one per tile.

## Palettes and compositing

### Palette terminology

Use these words consistently. Do **not** mix them with CHR **tile banks**.

| Term | Meaning | Not the same as |
|------|---------|-----------------|
| **Master palette** | 64 global RGB colors in **board Color PROM**. All game colors are indices 0-63 into it | A palette row or palette bank |
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

### Global master palette (Color PROM)

Retr01-A, C, and H all share one **master palette** of **64 unique colors**. It is **not** stored in the cartridge and is **not** loaded by the CPU.

Hardware:

- **3x AT28C16** parallel EEPROMs act as **Color PROMs** (one each for R, G, B)
- compositor outputs a **6-bit** master color index on the PROM address pins
- each PROM's data bus feeds that gun's **R-2R DAC** (no CPU cycles per pixel)
- the same image is programmed once at board build for A, C, and H

Why three chips: one 8-bit PROM cannot drive three gun DACs at pixel rate. Same 6-bit index on all three. Each holds that gun's level for colors 0-63.

Canonical RGB values (documentation/Studio preview mirror of what is burned into the PROMs):

```text
#000000 #290514 #2A0507 #230F06 #1E1306 #1A1605 #141807 #061A07 #051A13 #071918 #08181C #071722 #030B3D #16033A #20052D #260420
#363636 #740A40 #77091A #693512 #5D3F0E #514617 #424C19 #13511A #16503F #114E4D #164D58 #164A66 #163794 #472990 #5F167D #6C115F
#949494 #C04A7A #C54A4D #B8601B #A27326 #8F7E2F #77872D #209030 #2E8E72 #318B89 #1F889C #2483B5 #4D77D7 #7E6AD3 #9D5DBF #B352A0
#FFFFFF #F1A2BB #F1A6A1 #F1A983 #EEAC44 #D4BA33 #B0C841 #73D275 #22D0A6 #3BCDC9 #48C9E4 #88C4ED #A4BDEF #BBB5F1 #D5A9EF #F09BDD
```

Games and carts only ever store **indices 0-63** into this table (inside BG/sprite Palettes). They never ship the RGB bytes.

### Cart palette storage (pointer table, uncompressed)

Cart ROM stores **palette banks of master indices only** (which of the 64 Color PROM colors each slot uses). Do **not** put the 64 RGB master table in the cart. Do **not** RLE/compress palette index blobs.

Directory model:

- a **pointer table** (cart header/world directory) holds offsets to palette-index blobs
- each blob is plain **master-color indices** (4 bytes per Palette, rows as authored)
- software follows the pointer, copies the needed row into `$FE08`/`$FE09`
- `$FE08`/`$FE09` hold indices. The Color PROM turns those indices into RGB at the DAC

Programmers do **not** need to author every palette in every bank for every world - omit pointers/leave unused.

Layers:

| Layer | What it holds | Required? |
|-------|---------------|-----------|
| **Color PROM (board)** | 64 RGB colors | always present on A/C/H motherboards |
| **Cart global palette banks** | at least **1 BG Palette + 1 sprite Palette** (indices into 0-63) | minimum authoring contract |
| **World palette banks** | optional **BG palette bank** and/or **sprite palette bank** for that world | optional per world |

### Palette fallback chain

Resolution happens in **software at load time** (boot, world enter, or `load_screen`), not in the PPU per pixel. The hardware always reads whatever is already in the **active palette buffer**.

```text
1. world palette bank entry (if the world defines one)
2. else cart global palette bank (at least 1 BG + 1 sprite palette for a valid cart)
3. else system default **index** palettes (baked into Retr01 Studio/system startup)

Master RGB always comes from the board Color PROM, never from this chain.
```

This needs **no extra chips**. The CPU copies the resolved active row into palette registers when the palette row changes.

### Active palette buffer

The PPU can only show **4 BG palettes** and **4 sprite palettes** at one time. Together that is **8 palettes** in the **active palette buffer**.

That buffer is dedicated **palette registers/palette RAM**. It is **not** nametable VRAM.

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

This is a **software load rule**, not a separate backdrop IC: when the CPU (or Studio runtime) copies a palette row into the active buffer, it must write the **same** master index into color 0 of all **8** palette slots. Hardware does not auto-tie eight independent color-0 fields unless a later rev adds that.

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

### Screen/MAP metadata

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

1. write 24-bit MAP address (`$FE90`/`$FE91`/`$FE92`)
2. read MAP data at **`$FE93`**
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
| `$FE00-$FE0F` | PPU control, scroll, raster, parallax band, palette port (**draft** below) |
| `$FE10-$FE1F` | VRAM port (**draft** below) |
| `$FE20-$FE2F` | OAM port into 1284 |
| `$FE30-$FE3F` | world select + BG bank latches + sprite bank |
| `$FE40-$FE5F` | APU |
| `$FE60-$FE6F` | controllers/cabinet |
| `$FE70-$FE7F` | board EEPROM (AT28C64B) - **ships on every Retr01-A v0** |
| `$FE80-$FE8F` | PRG bank |
| `$FE90-$FE9F` | MAP port |

### `$FExx` register map (draft v0)

Byte-level layout below is a **frozen draft** for Studio, firmware, and proto bring-up. Bit meanings may gain reserved fields later. Do not renumber ports without a doc rev.

#### `$FE00-$FE0F` - PPU/raster/palette

| Addr | R/W | Name | Draft function |
|------|-----|------|----------------|
| `$FE00` | W | `PPUCTRL` | bit0 BG enable, bit1 sprites enable, bit2 NMI enable, bit3-4 camera slot mode (`00`=1 slot, `01`=2 H, `10`=2 V, `11`=4), bit5-7 reserved |
| `$FE01` | R | `PPUSTATUS` | bit0 VBlank, bit1 sprite overflow (optional), bit2 raster hit sticky, bit3-7 reserved. Read clears raster hit |
| `$FE02` | W | `SCROLL_X` | camera `scroll_x` (0-255 wrap) |
| `$FE03` | W | `SCROLL_Y` | camera `scroll_y` (0-255 wrap) |
| `$FE04` | W | `RASTER_Y` | compare scanline (0-261) |
| `$FE05` | W | `RASTER_CTRL` | bit0 IRQ enable, bit1-7 reserved |
| `$FE06` | W | `PLANE_CTRL` | bit0 enable plane slot 4 band, bit1 enable plane slot 5 band, bit2 band axis (`0`=H scroll lock, `1`=V scroll lock), bit3-7 reserved. **Any** band enable locks main camera to that axis for the **whole frame** (see Raster and parallax) |
| `$FE07` | W | `PLANE_BAND` | bits0-7 = band start scanline (end = next VBlank or paired latch in a later rev) |
| `$FE08` | W | `PAL_ADDR` | index into active palette buffer, **0-31** (8 palettes x 4 colors). Write sets pointer |
| `$FE09` | W | `PAL_DATA` | **master color index 0-63** (Color PROM address). Write stores and **auto-increments** `PAL_ADDR` |
| `$FE0A-$FE0F` | - | reserved | leave unimplemented on v0 |

#### `$FE10-$FE1F` - VRAM port

| Addr | R/W | Name | Draft function |
|------|-----|------|----------------|
| `$FE10` | W | `VRAM_ADDR_LO` | VRAM address bits 7-0 |
| `$FE11` | W | `VRAM_ADDR_HI` | VRAM address bits 14-8 (15-bit space) |
| `$FE12` | R/W | `VRAM_DATA` | read/write data. **Auto-increment** address after each access |
| `$FE13-$FE1F` | - | reserved | |

(Proto Module G historically used `$FE11`/`$FE12`/`$FE13` naming - treat this table as canonical going forward.)

#### `$FE20-$FE2F` - OAM (1284)

| Addr | R/W | Name | Draft function |
|------|-----|------|----------------|
| `$FE20` | W | `OAM_ADDR` | byte index into 256-byte OAM (0-255) |
| `$FE21` | W | `OAM_DATA` | write byte at `OAM_ADDR`, then **auto-increment** addr |
| `$FE22-$FE2F` | - | reserved | |

OAM entry layout (NES-like), 4 bytes x 64 sprites:

| Offset | Field |
|--------|--------|
| `4n + 0` | Y |
| `4n + 1` | tile index |
| `4n + 2` | attr (palette, flip, priority - bitfields TBD in a later micro-rev) |
| `4n + 3` | X |

#### `$FE30-$FE3F` - world/CHR banks

| Addr | R/W | Name | Draft function |
|------|-----|------|----------------|
| `$FE30` | W | `WORLD` | world select 0-7 |
| `$FE31` | W | `BG_BANK_0` | BG bank latch for nametable slot 0 (0-3) |
| `$FE32` | W | `BG_BANK_1` | slot 1 |
| `$FE33` | W | `BG_BANK_2` | slot 2 |
| `$FE34` | W | `BG_BANK_3` | slot 3 |
| `$FE35` | W | `BG_BANK_4` | plane slot 4 |
| `$FE36` | W | `BG_BANK_5` | plane slot 5 |
| `$FE37` | W | `SPR_BANK` | global sprite CHR bank 0-3 |
| `$FE38` | W | `PAL_ROW` | selected palette row 0-7 (BG and sprite together). Software still copies the row into `$FE08`/`$FE09` |
| `$FE39-$FE3F` | - | reserved | |

#### `$FE70-$FE7F` - board EEPROM (AT28C64B)

Ships on **every** Retr01-A v0 board (settings/high scores/operator data - not cart).

| Addr | R/W | Name | Draft function |
|------|-----|------|----------------|
| `$FE70` | W | `EE_ADDR_LO` | EEPROM A7-A0 |
| `$FE71` | W | `EE_ADDR_HI` | EEPROM A12-A8 (8 KB device) |
| `$FE72` | R/W | `EE_DATA` | data. Respect AT28C64B write timing on write |
| `$FE73-$FE7F` | - | reserved | |

#### `$FE80`/`$FE90` - PRG and MAP

| Addr | R/W | Name | Draft function |
|------|-----|------|----------------|
| `$FE80` | W | `PRG_BANK` | PRG window bank select |
| `$FE90` | W | `MAP_ADDR_LO` | MAP address bits 7-0 |
| `$FE91` | W | `MAP_ADDR_MID` | bits 15-8 |
| `$FE92` | W | `MAP_ADDR_HI` | bits 23-16 |
| `$FE93` | R | `MAP_DATA` | read MAP byte. **Auto-increment** 24-bit MAP address |

(`$FE40-$FE5F` APU and `$FE60`/`$FE61` pads stay as previously specified.)

Controller bytes (software contract, **1 = pressed**):

| Bit | `$FE60`/`$FE61` |
|-----|-------------------|
| 0 | right |
| 1 | left |
| 2 | down |
| 3 | up |
| 4 | X |
| 5 | Y |
| 6 | coin/select |
| 7 | start |

Same layout on Retr01-A (cabinet IDC -> 1284) and Retr01-C (pad MCU -> these two bytes).

## Raster and parallax

Retr01 uses **raster IRQ**, not NES sprite-0 hit (see [`07_pitch.md`](07_pitch.md) for NES comparison).

- `raster_y`: target scanline
- `raster_hit`: status
- `raster_irq_enable`: IRQ gate

Parallax is implemented as a **separate scanline band** that points to plane slots **4-5**.

Rules:

- if **any** H or V parallax band is enabled (`PLANE_CTRL` bits), main **camera movement locks to that axis for the whole frame** (not band-only). Software must not scroll the unlocked axis while a band is active
- plane slots are not part of the 2x2 camera
- BG banks stay per slot
- sprite bank remains separate/global

## What software should remember

- nametable bytes are tile indices `0-255`
- tile bytes do **not** store a bank number
- the slot's BG bank latch chooses the CHR bank
- MAP chooses which screen to load
- the current world chooses which 32 KB CHR chapter is active
- **VRAM camera slots hold full screens**, not single-room plus a tile border (see *What VRAM holds*)
- **sprites use a ping-pong line buffer**, one scanline ahead (see `03_hardware_implementation.md` *Sprite line buffer*)
