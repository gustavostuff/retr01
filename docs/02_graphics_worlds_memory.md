# Retr01 Graphics, Worlds, and Memory

Display model, world/MAP layout, VRAM, palettes, and the CPU-visible memory map. Hardware pipelines live in [`03_hardware_implementation.md`](03_hardware_implementation.md).

## Display and timing

| Item | Value |
|------|-------|
| Logical resolution | **128x96** (**16x12** tiles, exact **4:3**) |
| RGBS active field | **256x240** inside **341x262** timing |
| Tile size | **8x8** |
| Dot clock | **5.369318 MHz** |
| Frame / NMI | about **60.098 Hz** |
| CPU clock | **8.000 MHz** (independent of dot) |
| SCALE DIP | **1x** default (centered 128x96: 64 px sides, 72 lines top/bottom). **2x** doubles to **256x192** with **24+24** letterbox |

Games, scroll, OAM, and Studio all stay in **128x96**. SCALE is board glue on the raster only (no `$FExx` bit, no cart bit). See `03` for the scale-agnostic rule.

## Worlds and screens

- Cart: up to **16 worlds**
- World: sparse grid up to **16x16**, up to **64 screens**
- Screen: **16x12** tiles + packed attrs (**192** + **48** bytes), stored in MAP as `(col, row)`
- Parallax screens use the same format, marked non-enterable

Each world also carries:

- **4 BG** + **4 sprite** CHR banks (**256** tiles / **4 KB** each -> **32 KB** CHR per world)
- optional **BG** and **sprite palette banks** (master indices 0-63, never RGB bytes)
- MAP directory + screen payloads

Max CHR if every world is full: **16 x 32 KB = 512 KB** (fills an SST39SF040 by itself). Real carts share flash with PRG/MAP or keep some worlds lighter. **v0:** parallel flash in an on-board 32-pin socket. Later: same image on the cart.

### Runtime bank rules

1. Each screen names a **BG bank** in MAP metadata.
2. Loading a screen into a nametable slot also sets that slot's **BG bank latch**.
3. Camera slots **0-3** and plane slots **4-5** each have their own BG bank latch.
4. **Sprite bank** is separate and global for the current world.
5. Changing BG bank does not change the sprite bank, and vice versa.

## Camera, VRAM, and scroll

- `scroll_x` is **0-127**, `scroll_y` is **0-95** (logical pixels inside the live 2x2 field)
- Camera samples **1, 2, or 4** live screens from slots **0-3**
- Slots **4-5** are optional **parallax** only (not extra walkable rooms)

MAP holds the whole chapter. VRAM only holds what is live: up to **four** camera screens + **two** parallax screens.

Each slot is a **full screen** (192 tile + 48 attr), aligned to **256 bytes**. Not "one room plus a tile strip at the edge."

Hardware does **not** auto-load neighbors when you scroll. Software writes slots from MAP. Changing `scroll_x`/`scroll_y` only moves the 128x96 window over whatever is already in the four slots. That part is cheap.

```text
+-------------+-------------+
|  Slot 0     |  Slot 1     |   top
+-------------+-------------+
|  Slot 2     |  Slot 3     |   bottom
+-------------+-------------+
        ^
   128x96 viewport (scroll_x / scroll_y)
```

| Mode | Slots | What you see |
|------|-------|--------------|
| Pixel scroll, 1 slot | one room | pan inside 128x96 |
| Pixel scroll, 2 slots | H or V pair | viewport can straddle the seam |
| Pixel scroll, 4 slots | 2x2 | up to four rooms at once, including corners |
| Instant switch | load new room(s), reset scroll | hard cut (doors, warps). Same slots, no slide |

Studio **dead zone** / **hybrid** camera rules in `04` are software policy on top of these modes. They do not add PPU scroll hardware.

**Parallax (4-5):** full 16x12 nametables drawn behind the playfield (optional scanline band). Own BG bank latches. Enabling any H/V band locks the main camera to that axis for the **whole frame** (see Raster and parallax).

### Worked example: 3x3 world, standing on the center room

World MAP (nine rooms). Player is on room **E**, scroll at **(0, 0)** so the viewport shows E exactly.

```text
MAP (1 world)                             VRAM slots (workbench), 4-slot mode
+-----+-----+-----+                       +---------+---------+
|  A  |  B  |  C  |   player here ------> | Slot 0  | Slot 1  |
+-----+-----+-----+                       |   E     |   F     |
|  D  |  E  |  F  |                       +---------+---------+
+-----+-----+-----+                       | Slot 2  | Slot 3  |
|  G  |  H  |  I  |                       |   H     |   I     |
+-----+-----+-----+                       +---------+---------+

What the screen actually renders: all the 128x96 pixels that belong to screen E, and only that.
```

VRAM holds **E + F + H + I** (center, right, bottom, bottom-right). A/B/C/D/G are still only in cart MAP.

Scroll right and the hardware just samples a bit of F (**no cart/MAP load.**):

```text
VRAM unchanged: E F / H I
This is what the TV renders:

scroll_x = 0                     scroll_x = 32 px (for instance)
+-----------------------+        +-----------------------+
|                       |        |                 |     |
|                       |        |                 |     |
|                       |        |                 |     |
|           E           |        |         E       |  F  |
|                       |        |                 |     |
|                       |        |                 |     |
|                       |        |                 |     |
+-----------------------+        +-----------------------+
```

Same idea scrolling down into H, or diagonally toward I. Still no load until the viewport would need a room that is not in the four slots.

### What about 1 px left from scroll (0, 0)?

At scroll_x **0** you are already on the **left edge** of the live 2x2 VRAM slots. One more pixel west is not "hardware loads A/D/G." It is a **software camera shift**: rewrite some slots from MAP, then set scroll so the **resulting** picture keeps moving left.

Typical pattern (shift west by one column):

```text
Before (need to go left)              After software reload
+-----+-----+                         +-----+-----+
|  E  |  F  |                         |  D  |  E  |
+-----+-----+                         +-----+-----+
|  H  |  I  |                         |  G  |  H  |
+-----+-----+                         +-----+-----+
scroll (0,0)                          scroll near right, viewport still shows mostly E,
                                      now with D peeking from the left
```

You do **not** unload F/H/I and load three brand-new rooms on every pixel. You reload when the **workbench must reload slots**.

### Why this is not too much work

Two different CPU jobs. Do not mix them up:

| Action | What the CPU does |
|--------|-------------------|
| Pan inside the already-loaded 2x2 | Write `$FE02`/`$FE03` (scroll). **No** nametable rewrite. BG hardware keeps fetching the slots on its own |
| Stream a room into a slot (boundary / shift) | Point `$FE10`/`$FE11` at the slot start (or a run inside it), then poke ~**240** bytes through `$FE12`. **Auto-inc** means you do **not** rewrite the address for every tile. Usually: set addr once per contiguous run, then `STA $FE12` in a loop. Attr plane is the same idea at `slot+0xC0` |

So for a full screen load you are pushing **192 tile bytes + 48 attr bytes**, not "192 separate address setups." A column shift is that pattern times two (~480 data writes), still only when software slides the workbench, not every pixel.

| Action | Rough cost |
|--------|------------|
| Change scroll by 1 px | 1-2 register writes. No VRAM stream |
| Stream **one** screen from MAP | ~240 `$FE12` writes (+ a few addr setup writes) |
| Shift a column (2 screens) | ~480 data writes |
| Worst corner prep (3 screens) | ~720 data writes |

Spread across one or more VBlanks at ~60 Hz with an **8 MHz** CPU that is normal engine work. Scratch at `$0600+` can hold a decompress/copy buffer if MAP payloads are packed. After a slot is filled, the CPU can leave it alone until the next shift.

Worry about **when** software schedules the shift (dead zone, hybrid), not about rewriting VRAM every frame.

### VRAM layout

| Offset | Size | Purpose |
|--------|------|---------|
| `$0000-$00FF` | 256 B | camera slot 0 (192+48 used) |
| `$0100-$01FF` | 256 B | camera slot 1 |
| `$0200-$02FF` | 256 B | camera slot 2 |
| `$0300-$03FF` | 256 B | camera slot 3 |
| `$0400-$04FF` | 256 B | plane slot 4 |
| `$0500-$05FF` | 256 B | plane slot 5 |
| `$0600-$3FFF` | rest of low half | scratch / stream temp |
| `$4000-$7FFF` | 16 KB | reserved (not live camera/plane) |

Per slot: tiles at `+0x000` (192 B), packed attrs at `+0xC0` (48 B). One attr byte = one **2x2** tile group with four 2-bit palette fields.

## Sprite line buffer (not VRAM nametables)

BG nametable slots and the sprite line buffer are different memories. Slots 0-5 hold **tile maps**. Sprites never get painted into those maps by the 6502.

OAM (64 sprites: Y, tile, attr, X) lives with the **ATmega1284P**. Each logical scanline, the 1284 builds **one 128-pixel strip** of sprite color into a small SRAM, then the compositor reads it. Two halves ping-pong so display and prepare overlap. Full pin/timing story: [`03_hardware_implementation.md`](03_hardware_implementation.md).

```text
Logical frame (sprites), one row at a time:

  row 0  ################################  (128 px)
  row 1  ################################
  ...
  row 50 ########....####################  <- beam showing this row
  row 51 (being written into the other half during HBlank)
  ...
  row 95 ################################

Line-buffer SRAM (only two rows of storage, not a full framebuffer):

         Half A ($000-$07F)          Half B ($080-$0FF)
        +------------------+        +------------------+
Line N  | SHOW (beam read) |        | fill next row    |  1284 writes
        +------------------+        +------------------+
Line N+1| fill next row    |        | SHOW (beam read) |
        +------------------+        +------------------+
                    \____ swap roles every logical scanline ____/
```

- Cap: **16** sprites contributing to one logical row.
- Latency: **one scanline** ahead, not a full-frame double buffer.
- CPU job: keep OAM updated (`$FE20`/`$FE21`). 1284 job: evaluate Y, fetch CHR in HBlank, pack the strip.
- SCALE still happens later on the raster path. The line buffer stays **128** wide either way.

## Palettes

Do not mix these with CHR **tile banks**.

| Term | Meaning |
|------|---------|
| **Master palette** | 64 RGB colors in board **Color PROM**. Games only use indices **0-63** |
| **BG / sprite palette bank** | Cart store: up to **32** palettes (**8 rows x 4**). Indices only |
| **Palette row** | One row of 4 palettes (index **0-7**) |
| **Palette** | 4 master indices (4 bytes) |
| **Active palette buffer** | The **8** palettes on screen now: **4 BG + 4 sprite** |

### Color PROM (board)

A/C/H share one master table. Not in the cart. Not loaded by the CPU.

- **3x AT28C16** (R, G, B). Compositor puts a **6-bit** index on all three address buses. Each PROM feeds that gun's R-2R DAC.
- Three chips because one 8-bit PROM cannot drive three guns at pixel rate.

Canonical RGB (docs / Studio preview mirror of what is burned into the PROMs):

```text
#000000 #290514 #2A0507 #230F06 #1E1306 #1A1605 #141807 #061A07 #051A13 #071918 #08181C #071722 #030B3D #16033A #20052D #260420
#363636 #740A40 #77091A #693512 #5D3F0E #514617 #424C19 #13511A #16503F #114E4D #164D58 #164A66 #163794 #472990 #5F167D #6C115F
#949494 #C04A7A #C54A4D #B8601B #A27326 #8F7E2F #77872D #209030 #2E8E72 #318B89 #1F889C #2483B5 #4D77D7 #7E6AD3 #9D5DBF #B352A0
#FFFFFF #F1A2BB #F1A6A1 #F1A983 #EEAC44 #D4BA33 #B0C841 #73D275 #22D0A6 #3BCDC9 #48C9E4 #88C4ED #A4BDEF #BBB5F1 #D5A9EF #F09BDD
```

<img src="img/palette_autogen_80pert_saturation.png" alt="Palette" />

### Cart storage and fallback

Cart holds **palette banks of indices only** (pointer table -> uncompressed blobs). Never the 64 RGB values. Never RLE on palette blobs. Copy a row into `$FE08`/`$FE09` when needed.

Layers: Color PROM always on the board. Cart needs at least **1 BG + 1 sprite** palette. World palette banks are optional.

Resolve at load time (boot / world enter / `load_screen`), not per pixel:

```text
1. world palette bank (if present)
2. else cart global palette bank
3. else Studio/system default index palettes

Master RGB always from Color PROM.
```

### Active buffer

PPU shows **4 BG + 4 sprite** palettes at once (dedicated palette RAM, not nametable VRAM).

- Software picks palette row **0-7**. BG and sprite rows are always the **same** index (no BG-4 / sprite-2 mix).
- Tile attrs and OAM attrs still only pick **0-3** inside that row.
- Screens may optionally name a palette row in MAP (like they name a BG bank). Switching rows means reload `$FE08`/`$FE09`.

**Shared color 0:** all 8 active palettes use the same master index at color 0 (universal backdrop). Software writes that into every slot when loading a row. On BG, color 0 is the backdrop. On sprites, pattern color 0 stays **transparent**.

Sprite priority: transparent sprite -> BG. Sprite-behind + opaque BG -> BG. Else sprite.

Palette fades, interpolates, and cycles are **runtime library** helpers that rewrite the active buffer (rotate inside a 4-color set, or step to another row).

## MAP-ROM

Not memory-mapped into CPU space. Read through `$FE90`:

1. write 24-bit address (`$FE90`/`$FE91`/`$FE92`)
2. read data at `$FE93` (auto-inc)

Directory row sketch: `col`, `row`, `flags` (`bit0` parallax, `bits1-2` BG bank, `bits3-5` palette row), `data_off` (24-bit).

## CPU memory map

| Range | Region |
|-------|--------|
| `$0000-$7FFF` | System RAM (CPU-only) |
| `$8000-$FDFF` | PRG window (banked via `$FE80`) |
| `$FE00-$FEFF` | I/O |
| `$FF00-$FFFF` | PRG high + vectors |

| Block | Purpose |
|-------|---------|
| `$FE00-$FE0F` | PPU, scroll, raster, parallax, palette |
| `$FE10-$FE1F` | VRAM port |
| `$FE20-$FE2F` | OAM (1284) |
| `$FE30-$FE3F` | world + BG/sprite bank latches |
| `$FE40-$FE5F` | APU |
| `$FE60-$FE6F` | controllers / cabinet |
| `$FE70-$FE7F` | board EEPROM (AT28C64B, every v0 board) |
| `$FE80-$FE8F` | PRG bank |
| `$FE90-$FE9F` | MAP port |

### `$FExx` draft v0

Frozen draft for Studio, firmware, and proto. Bitfields may grow reserved bits later. Do not renumber ports without a doc rev.

#### `$FE00-$FE0F` PPU / raster / palette

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE00` | W | `PPUCTRL` | bit0 BG, bit1 sprites, bit2 NMI, bit3-4 camera mode (`00`=1, `01`=2H, `10`=2V, `11`=4), bit5-7 reserved |
| `$FE01` | R | `PPUSTATUS` | bit0 VBlank, bit1 sprite overflow (opt), bit2 raster hit sticky. Read clears hit |
| `$FE02` | W | `SCROLL_X` | 0-127 wrap |
| `$FE03` | W | `SCROLL_Y` | 0-95 wrap |
| `$FE04` | W | `RASTER_Y` | compare scanline 0-261 |
| `$FE05` | W | `RASTER_CTRL` | bit0 IRQ enable |
| `$FE06` | W | `PLANE_CTRL` | bit0/1 plane 4/5 band, bit2 axis (`0`=H lock, `1`=V lock). Any band locks camera to that axis for the whole frame |
| `$FE07` | W | `PLANE_BAND` | band start scanline (end = next VBlank or later paired latch) |
| `$FE08` | W | `PAL_ADDR` | active buffer index 0-31 |
| `$FE09` | W | `PAL_DATA` | master index 0-63, auto-inc addr |
| `$FE0A-$FE0F` | - | reserved | |

#### `$FE10-$FE1F` VRAM

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE10` | W | `VRAM_ADDR_LO` | bits 7-0 |
| `$FE11` | W | `VRAM_ADDR_HI` | bits 14-8 |
| `$FE12` | R/W | `VRAM_DATA` | auto-inc after access |
| `$FE13-$FE1F` | - | reserved | |

#### `$FE20-$FE2F` OAM

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE20` | W | `OAM_ADDR` | 0-255 |
| `$FE21` | W | `OAM_DATA` | write + auto-inc |
| `$FE22-$FE2F` | - | reserved | |

Entry: `Y, tile, attr, X` x 64. Attr bitfields (palette / flip / priority) TBD.

#### `$FE30-$FE3F` world / CHR

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE30` | W | `WORLD` | 0-15 |
| `$FE31`-`$FE36` | W | `BG_BANK_0`..`5` | latches for slots 0-5 (0-3) |
| `$FE37` | W | `SPR_BANK` | global sprite CHR bank 0-3 |
| `$FE38` | W | `PAL_ROW` | row 0-7 (BG+sprite). Still copy into `$FE08`/`$FE09` |
| `$FE39-$FE3F` | - | reserved | |

#### `$FE70-$FE7F` board EEPROM

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE70` | W | `EE_ADDR_LO` | A7-A0 |
| `$FE71` | W | `EE_ADDR_HI` | A12-A8 |
| `$FE72` | R/W | `EE_DATA` | respect AT28C64B write timing |
| `$FE73-$FE7F` | - | reserved | |

#### `$FE80` / `$FE90` PRG and MAP

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE80` | W | `PRG_BANK` | PRG window |
| `$FE90` | W | `MAP_ADDR_LO` | bits 7-0 |
| `$FE91` | W | `MAP_ADDR_MID` | bits 15-8 |
| `$FE92` | W | `MAP_ADDR_HI` | bits 23-16 |
| `$FE93` | R | `MAP_DATA` | read + auto-inc |

`$FE40-$FE5F` APU and `$FE60`/`$FE61` pads use the contracts below / in `03`.

Controller bytes (**1 = pressed**), same on Retr01-A IDC and Retr01-C pad MCU:

| Bit | `$FE60`/`$FE61` |
|-----|-----------------|
| 0 | right |
| 1 | left |
| 2 | down |
| 3 | up |
| 4 | X |
| 5 | Y |
| 6 | coin/select |
| 7 | start |

## Raster and parallax

Raster IRQ (not NES sprite-0 hit). Registers: `RASTER_Y`, hit sticky in `PPUSTATUS`, `RASTER_CTRL` IRQ gate.

Parallax = scanline band pointing at plane slots **4-5**.

- Any H/V band enable locks main camera scroll to that axis for the **whole frame**. Do not scroll the unlocked axis while a band is active.
- Plane slots are not part of the 2x2 camera.
- BG banks stay per slot. Sprite bank stays global.

## Software cheat sheet

- Nametable bytes are tile indices **0-255**. Bank lives in the slot latch, not in the tile byte.
- MAP picks which screen to load. World picks which 32 KB CHR chapter is active.
- Camera slots hold **whole screens**. Scroll moves the window; MAP loads happen on software slot shifts (`02` worked example).
- Sprites use a ping-pong line buffer one scanline ahead (`02` diagram, `03` detail).
