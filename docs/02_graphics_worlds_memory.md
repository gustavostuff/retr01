# Retr01 Graphics, Worlds, and Memory

Display model, world/MAP layout, VRAM, palettes, and the CPU-visible memory map. Hardware pipelines live in [`03_hardware_implementation.md`](03_hardware_implementation.md).

## Display and timing

| Item | Value |
|------|-------|
| Logical resolution | **128x120** (**16x15** tiles, **16:15** storage aspect) |
| RGBS active field | **256x240** inside **341x262** timing |
| Tile size | **8x8** |
| Dot clock | **5.369318 MHz** |
| Frame / NMI | about **60.098 Hz** |
| CPU clock | **8.000 MHz** (independent of dot) |
| SCALE DIP | **2x** default: doubles to **256x240** (fills the RGBS field, **no** letterbox). **1x** optional: center **128x120** (**64** px sides, **60** lines top/bottom) |

Games, scroll, OAM, and Studio all stay in **128x120**. SCALE is board glue on the raster only (no `$FExx` bit, no cart bit). See `03` for the scale-agnostic rule.

**Outputs:** CRT cabinets use **2x** for a full **256x240** picture. A **640x640** LCD can integer-scale **5x** to **640x600** (thin letterbox) without changing game resolution.

## Worlds and screens

- Cart: up to **8 worlds**
- World: sparse grid up to **8x8**, up to **32 screens**
- Screen: **16x15** tiles + **one attr byte per tile** (**240** + **240** bytes = **480 B**), stored in MAP as `(col, row)`
- Parallax screens use the same format, marked non-enterable

Each world also carries:

- **4 BG** + **4 sprite** CHR banks (**256** tiles / **4 KB** each -> **32 KB** CHR per world)
- **default BG bank** and **default sprite bank** (0-3) for loaders to stamp
- optional **world** BG/sprite palette banks (up to 8 rows each; override cart globals when present; if absent, that world uses cart globals)
- sparse screen directory + **480 B** screen payloads (see cart map below)

### MAP screen format (no RLE required)

Each stored screen in flash is a flat **480-byte** blob:

| Order | Bytes | Contents |
|-------|-------|----------|
| 1 | **240** | tile indices, **one byte per tile** (row-major 16x15) |
| 2 | **240** | attr bytes, **one byte per tile** (same order) |

**In other words:** tile index bytes and attr bytes do **not** need compression. A game *can* compress them, but then the developer must ship a decompress step and pay extra CPU (decompress, then copy into VRAM). The simple path is a direct cart-to-VRAM copy: each plane is **240** bytes, which fits in a single-byte counter (**240 < 255**), so the loader is just "read `$FE93`, write `$FE12`, count to 240" for tiles, then the same for attrs.

### Cart flash budget (standard = 512 KB)

**Standard cart / image size: 512 KB (4 Mbit x8 parallel NOR, SST39SF040 class).** Caps below are chosen so a full game (worlds + CHR + MAP + pals + PRG) fits with a little slack.

MAP `$FE90`-`$FE92` is **24-bit**; **512 KB** only needs A0-A18.

#### Caps at full fill (planning max)

| Asset | Cap math | Size |
|-------|----------|------|
| CHR | 8 worlds x 32 KB (4 BG + 4 sprite banks full) | **256 KB** |
| MAP screens | 8 x 32 x 480 B | **120 KB** |
| World palette banks | 8 x 256 B (8 rows x 4 BG + 8 x 4 sprite) | **2 KB** |
| Cart global palettes | 4 BG + 4 sprite pals x 4 B | **32 B** |
| Directories / headers | ~12 B x 256 screen rows + world table/headers + cart pointer table | **~4 KB** |
| PRG (planning) | default reserve (~32 KB `$8000` window + `$FE80` pages) | **96 KB** |
| **Total** | | **~478 KB** |

**On a 512 KB cart:** ~**478 KB** used, ~**34 KB** free. Full architecture caps + **96 KB** PRG fit with comfortable slack for padding and small growth.

**Planning rule of thumb:** **8 worlds / 32 screens / 8x8 grid**; reserve **~96 KB** PRG; treat **~478 KB** as the filled-cart target on **SST39SF040**. **v0:** on-board flash socket, same image later on the cart.

### Runtime bank rules

1. **BG CHR bank is per 8x8 tile**. Bits **1-0** of that tile's attr byte select bank **0-3**. Hardware CHR fetch reads `BANK` from the attr fetched with the nametable byte.
2. Each **world** names a **default BG bank** and **default sprite bank** (0-3). Screens may override BG stamp in their metadata. Loaders/Studio stamp into attrs / OAM; live fetch still uses per-tile / per-sprite attr `BANK`. See **Cart image map** below.
3. Slot registers `$FE31`-`$FE36` are optional **bulk helpers** (e.g. stamp a bank into a slot's attrs). They are **not** the live fetch source.
4. Camera slots **0-3** and plane slots **4-5** hold nametable+attr data; mixed banks in one screen are normal.
5. **BG bank needs no mid-frame latch split.** Each tile already carries `BANK`, so a room can mix banks 0-3 in one nametable without raster/line IRQ bank flips. Raster IRQ stays available for status bars, parallax bands, palette row changes, and other splits.
6. **Sprite CHR bank is per OAM entry** (attr bits **1-0**). Mixed sprite banks in one frame are normal. `$FE37` is an optional bulk stamp helper, not the live fetch source.
7. BG tile banks and sprite banks are independent (different CHR halves / bank spaces).

## BG tile attributes

One **attr byte per 8x8 BG tile**. Screen payload: **240** tile bytes + **240** attr bytes = **480 B** (fits a **512 B** VRAM slot with **32 B** pad).

Shared low fields match **OAM sprite attrs** (`BANK` / `PAL` / `FLIP_H` / `FLIP_V`). High bits are BG-only (`SOLID` / `ANIM`).

```text
7 6 5 4 3 2 1 0
1 0 0 1 0 1 0 1
| | | | | | |_|__ Bank index (0-3)
| | | | | |
| | | | |_|______ Palette (0-3)
| | | |
| | | |__________ H flip
| | |____________ V flip
| |______________ Solid (physics flag; software)
|________________ Animation flag (software; 4-frame strip)
```

| Bits | Name | Owner | Role |
|------|------|-------|------|
| 1-0 | `BANK` | **Hardware** | BG CHR bank 0-3 (CHR address mux) |
| 3-2 | `PAL` | **Hardware** | BG palette 0-3 (compositor) |
| 4 | `FLIP_H` | **Hardware** | horizontal mirror (pixel shifter) |
| 5 | `FLIP_V` | **Hardware** | vertical mirror (fine-Y / row) |
| 6 | `SOLID` | **Software** | collision hint; video path does not read it |
| 7 | `ANIM` | **Software** | living-tile mark; CPU advances the nametable index |

Studio, MAP, and the BG fetch path use this layout. Games choose how to drive `SOLID` / `ANIM` (rates, lists, physics). The optional kit follows the same bits.

`FLIP_H` / `FLIP_V` need shifter reverse and fine-Y XOR in the BG path (fit in leftover PLD/74HC if possible; otherwise a 4th ATF22V10). Silicon detail: [`03`](03_hardware_implementation.md).

### Living tiles (`ANIM=1`)

Convention for Studio packer and kit anim: **4** frames as consecutive indices in one BG bank:

```text
index:  ... | B | B+1 | B+2 | B+3 | ...
              f0   f1    f2    f3
```

- Base index `B` is **4-aligned** (`B & 3 == 0`).
- All four frames share the same `BANK`, `PAL`, flips, and `SOLID`.
- Runtime nametable byte is `B + phase` with `phase` in `0..3`.
- Studio / packer places strips on a bank row (16 tiles wide): up to **4** strips per row.
- Static tiles (`ANIM=0`) use a single index; no alignment rule.

Flips cut duplicate mirrored patterns (water edges, grass tips, etc.).

### Load / anim / collision (software)

**On slot load**

1. Stream **240** tiles + **240** attrs into the VRAM slot.
2. Build a **collision shadow** in system RAM from `SOLID` (**30** packed bits/screen, or **32** aligned).
3. Build an **anim list**: each `ANIM=1` cell -> `{vram_tile_addr, base_index}` with `base_index = tile & ~3`. Cap length (recommend **32** or **64** living cells per camera workbench).

**Each NMI (or every 2nd frame)**

1. `phase = (frame_counter >> rate_shift) & 3`
2. For each anim entry, write `base_index + phase` via `$FE10`-`$FE12` (tear-safe: VBlank or beam-Y gate).
3. Attrs stay as loaded unless the cell itself changes (breakable, etc.).

**Collision**

Physics reads the RAM shadow (keep the hot path off `$FE12`). Breakable tiles update VRAM tile/attr and the shadow together.

Recommended default: **axis-separated** move (X then resolve, Y then resolve); probe only tiles overlapping the hitbox (**~2-6** lookups), not all **240** cells. Collide **entities** (one AABB per metasprite), not every OAM piece. Rough budget at **8 MHz** / ~**133k** cycles/frame: **~16** active bodies is roughly **under 10%** of a frame for collide+response if the shadow stays in system RAM.

When scroll straddles rooms, map camera-local pixels into the right slot's shadow (or a stitched buffer updated on tile-boundary scroll). Shadow covers **loaded slots** only; rebuild/blit on slot reload.

### Dev kit (optional)

Cart-linkable C/ASM helpers (cc65 + `.s`) are convenience, not required:

| Module | Role |
|--------|------|
| `retr01_bg_attr` | Bit masks / pack-unpack |
| `retr01_bg_load` | MAP -> VRAM slot + bank stamp |
| `retr01_bg_collide` | Build/query solid shadow; `phys_move_xy` |
| `retr01_bg_anim` | Living-cell list + NMI step |
| `retr01_bg_vram` | Tear-safe nametable/attr poke |

Masks: `BANK` `0x03`, `PAL` `0x0C` (shift 2), `FLIP_H` `0x10`, `FLIP_V` `0x20`, `SOLID` `0x40`, `ANIM` `0x80`.

v0 kit scope: solid queries + 4-frame anim helpers + tear-safe VRAM pokes (not a full physics engine).

## Camera, VRAM, and scroll

- `scroll_x` is **0-127**, `scroll_y` is **0-119** (logical pixels inside the live 2x2 field)
- Camera samples **1, 2, or 4** live screens from slots **0-3**
- Slots **4-5** are optional **parallax** only (not extra walkable rooms)

MAP holds the whole chapter. VRAM only holds what is live: up to **four** camera screens + **two** parallax screens.

Each slot is a **full screen** (**240** tile + **240** attr), aligned to **512 bytes**. Not "one room plus a tile strip at the edge."

Hardware does **not** auto-load neighbors when you scroll. Software writes slots from MAP. Changing `scroll_x`/`scroll_y` only moves the **128x120** window over whatever is already in the four slots. That part is cheap.

```text
+-------------+-------------+
|  Slot 0     |  Slot 1     |   top
+-------------+-------------+
|  Slot 2     |  Slot 3     |   bottom
+-------------+-------------+
        ^
   128x120 viewport (scroll_x / scroll_y)
```

| Mode | Slots | What you see |
|------|-------|--------------|
| Pixel scroll, 1 slot | one room | pan inside 128x120 |
| Pixel scroll, 2 slots | H or V pair | viewport can straddle the seam |
| Pixel scroll, 4 slots | 2x2 | up to four rooms at once, including corners |
| Instant switch | load new room(s), reset scroll | hard cut (doors, warps). Same slots, no slide |

Studio **dead zone** / **hybrid** camera rules in `04` are software policy on top of these modes. They do not add PPU scroll hardware.

**Parallax (4-5):** full **16x15** nametables drawn behind the playfield (optional scanline band). Own nametable+attr data (per-tile `BANK` like the camera). Attrs index the same active BG palette buffer as the playfield (`$FE08`/`$FE09`). MAP palette-row on a parallax-flagged screen is **ignored / inherited**. Enabling any H/V band locks the main camera to that axis for the **whole frame** (see Raster and parallax). Physical latch / ownership detail: [`03`](03_hardware_implementation.md).

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

What the screen actually renders: all the 128x120 pixels that belong to screen E, and only that.
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

Two different CPU jobs:

| Action | What the CPU does |
|--------|-------------------|
| Pan inside the already-loaded 2x2 | Write `$FE02`/`$FE03` (scroll). **No** nametable rewrite. BG hardware keeps fetching the slots on its own |
| Stream a room into a slot (boundary / shift) | Point `$FE10`/`$FE11` at the slot start (or a run inside it), then poke ~**480** bytes through `$FE12`. **Auto-inc** means you do **not** rewrite the address for every tile. Usually: set addr once per contiguous run, then `STA $FE12` in a loop. Attr plane follows tiles at `slot+0xF0` |

**In other words:** walking around inside rooms you already loaded is cheap (just move the camera). The expensive job is only when you cross into a room that is not in the four live slots yet - then you copy that room's tiles into VRAM once.

So for a full screen load you are pushing **240 tile bytes + 240 attr bytes**, not "240 separate address setups." A column shift is that pattern times two (~960 data writes), still only when software slides the workbench, not every pixel.

| Action | Rough cost |
|--------|------------|
| Change scroll by 1 px | 1-2 register writes. No VRAM stream |
| Stream **one** screen from MAP | ~480 `$FE12` writes (+ a few addr setup writes) |
| Shift a column (2 screens) | ~960 data writes |
| Worst corner prep (3 screens) | ~1440 data writes |

**In other words:** one new room ~ half a kilobyte into VRAM. Two rooms side-by-side ~ 1 KB. The nasty corner case (three new rooms) ~ 1.5 KB. You do that when the workbench slides, not every frame.

#### How long that takes while the CRT draws

**VRAM commit timing (planning math):** clocks are **8.000 MHz** CPU and **5.369318 MHz** dot, **341** dots/line -> about **508** CPU cycles per CRT line. Active field **240** lines; VBlank region about **22** lines (~**11.2k** CPU cycles). Assume a tight ASM copy from a system-RAM buffer into `$FE12` at about **12** cycles/byte (auto-inc). Interleave lets those writes run while the beam is drawing (CPU phase); PPU keeps fetching on the other phase.

| Screens written | Bytes | CPU cycles (@12/B) | CRT lines | Share of 240-line active field |
|-----------------|-------|--------------------|-----------|--------------------------------|
| 1 | 480 | ~5760 | **~11** | ~5% |
| 2 | 960 | ~11520 | **~23** | ~9% |
| 3 | 1440 | ~17280 | **~34** | ~14% |

**In other words:** while the picture is on screen, the electron beam paints **240** horizontal lines, then rests for about **22** lines (VBlank). If the CPU may only touch VRAM during that short rest:

- **1** room usually fits in one blank.
- **2** rooms take about as long as the **entire** blank - you are out of time in a single VBlank.
- **3** rooms need **two or more** blanks - the camera hitch lasts more than one frame.

With **interleaved** VRAM, the CPU can copy into VRAM on its phases **while those 240 picture lines are being drawn**. Same copies then look like:

- **1** room ~ **11** picture lines of beam time (a thin slice of the frame).
- **2** rooms ~ **23** lines (still under **10%** of the visible frame).
- **3** rooms ~ **34** lines (about **one seventh** of the visible frame) - done in the **same** frame the seam happens, instead of waiting through multiple VBlanks.

That is the practical difference: VBlank-only makes big camera shifts feel like a stall; interleave makes them a short background copy during normal drawing.

MAP read into scratch (or straight MAP -> `$FE12`) is ordinary byte copying at full CPU speed on `$FE93` / system RAM; the table above is the **VRAM port** cost that interleave unlocks during active display.

**In other words:** there is no decompress step. Cart bytes are already the tile indices and attrs. Prefer pouring into slots the beam is **not** currently showing (or use the tear gates below).

Scratch at `$0600+` can hold a copy buffer if you stage before commit. After a slot is filled, the CPU can leave it alone until the next shift.

Worry about **when** software schedules the shift (dead zone, hybrid), not about rewriting VRAM every frame.

### Live VRAM updates and tear avoidance

Interleave allows `$FE12` writes **during active display**. That does **not** mean every poke is visually safe.

If the CPU changes a **tile index** (or that tile's attr) while the beam is still drawing that tile's **8 logical rows**, the screen can show a **tear**: some lines of the cell use the old CHR fetch, later lines use the new one.

**In other words:** you can write VRAM while the TV is drawing, but if you rewrite the exact tile under the beam mid-draw, that one cell can flicker between old and new art for a moment. Copy into **off-screen** slots (the rooms beside/behind the camera) and you avoid that; or only poke visible cells in VBlank / after the beam has passed.

**Platform rule (software / dev kit):** provide a mechanism so games do not update VRAM for a cell that is **currently under the beam**. Preferred approaches (any one is enough):

| Approach | Behavior |
|----------|----------|
| **VBlank / early NMI only** for visible nametable/attr writes | Simplest. No race with the playfield |
| **HBlank-only** queues | Fine for small bursts; still avoid the tile row being fetched on the upcoming line if unsure |
| **Deferred / double-buffer intent** | Build new indices in system RAM; commit to VRAM only in VBlank, or only for cells whose max Y is **above** the current beam (already drawn -> shows next frame, no tear) or **below** (not yet drawn -> clean this frame) |
| **Beam-Y gate** | Kit helper: given tile `(tx,ty)`, refuse or defer write if beam logical Y is inside `[ty*8, ty*8+7]` (and X span if you also care about horizontal mid-tile fetch) |

**Not required in hardware:** no shadow nametable chip. Tear avoidance is a **CPU/kit contract**.

**Still fine mid-frame without gating:** large streams into non-visible slots; physics against RAM solid maps; OAM; scroll latches (those follow the separate "next tile fetch" rule in `03`). Changing a **visible** cell's attr `BANK` is a VRAM write - gate it like other visible attrs.

Dev-kit anim (`ANIM` cells) should use the same gate or VBlank commit so 4-frame updates never split a tile mid-cell.

### VRAM layout

| Offset | Size | Purpose |
|--------|------|---------|
| `$0000-$01FF` | 512 B | camera slot 0 (**240** tiles + **240** attrs used) |
| `$0200-$03FF` | 512 B | camera slot 1 |
| `$0400-$05FF` | 512 B | camera slot 2 |
| `$0600-$07FF` | 512 B | camera slot 3 |
| `$0800-$09FF` | 512 B | plane slot 4 |
| `$0A00-$0BFF` | 512 B | plane slot 5 |
| `$0C00-$3FFF` | rest of low half | scratch / stream temp |
| `$4000-$7FFF` | 16 KB | reserved (not live camera/plane) |

Per slot: tiles at `+0x000` (**240** B), attrs at `+0xF0` (**240** B), **32 B** pad to 512. **One attr byte per 8x8 tile** (see **BG tile attributes** above). Attr `BANK` selects that tile's BG CHR bank.

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
  row 119 ################################

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

### OAM sprite attributes

Entry order: `Y, tile, attr, X` x **64**. Positions are logical (**128x120**). CHR patterns are always **8x8**; `SIZE` selects how many rows one OAM entry spans.

Shared low fields match **BG tile attrs** (`BANK` / `PAL` / `FLIP_H` / `FLIP_V`). High bits are sprite-only (`PRIORITY` / `SIZE`).

```text
7 6 5 4 3 2 1 0
1 0 0 1 0 1 0 1
| | | | | | |_|__ Bank index (0-3)
| | | | | |
| | | | |_|______ Palette (0-3)
| | | |
| | | |__________ H flip
| | |____________ V flip
| |______________ Priority (0 = in front of BG, 1 = behind opaque BG)
|________________ Size (0 = 8x8, 1 = 8x16)
```

| Bits | Name | Owner | Role |
|------|------|-------|------|
| 1-0 | `BANK` | **Hardware** (1284 CHR fetch) | sprite CHR bank 0-3 |
| 3-2 | `PAL` | **Hardware** | sprite palette 0-3 |
| 4 | `FLIP_H` | **Hardware** | horizontal mirror |
| 5 | `FLIP_V` | **Hardware** | vertical mirror |
| 6 | `PRIORITY` | **Hardware** | `0` = in front of BG; `1` = behind opaque BG |
| 7 | `SIZE` | **Hardware** | `0` = 8x8; `1` = 8x16 (two stacked 8x8 tiles) |

`SIZE=1`: one OAM entry covers **16** logical rows. Tile byte is the base index `B` (**even** / 2-aligned); the 1284 fetches top `B` and bottom `B+1` (or `B|1`) for the matching row. `SIZE=0` uses a single tile at `B` for rows `Y..Y+7`. Mixed 8x8 and 8x16 entries in the same frame are normal. An 8x16 sprite counts as **one** toward the **16**-per-line cap on each line it covers.

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

<img src="../img/palette_autogen_80pert_saturation.png" alt="Palette" />

### Cart storage and fallback

Cart holds **palette banks of indices only** (see **Cart image map**). Never the 64 RGB values. Never RLE on palette blobs. Copy a row into `$FE08`/`$FE09` when needed.

**Cart globals (always present in a normal image):** two sets, **8 palettes** total:

- **1 BG set** = **4** BG palettes (16 B)
- **1 sprite set** = **4** sprite palettes (16 B)

That is one active "palette row" worth, shared by every world that does not override.

**World palette banks (optional, larger):** each world may ship a **BG palette bank** and/or a **sprite palette bank**, each up to **8 rows x 4 palettes** (32 palettes / 128 B per plane). When a world bank is present for a plane, loads for that world use it and **override** the cart global set for that plane. When a world has **no** bank for a plane (`off_world_pal_* = 0`), **all rendering for that world on that plane uses the cart global set**.

Resolve at load time (boot / world enter / `load_screen`), not per pixel. BG and sprite planes resolve independently:

```text
For BG (and the same ladder for sprites):

1. world BG palette bank (if off_world_pal_bg != 0) -> pick row
2. else cart global BG set (off_global_pal_bg)
3. else system default BG indices (kit / Studio / boot ROM helper)

Master RGB always from Color PROM.
```

**If the author defines no palettes at all** (no useful global sets and no world banks):

- **With kit / Studio / a boot helper:** that software writes **system default** master indices into `$FE08`/`$FE09`. You get a boring but stable look.
- **Bare ASM/C with no palette writes:** the active buffer is **not** reset by hardware to a known table. Power-on contents are **undefined** (latches/RAM garbage) -> **random / garbage colors** until your code (or a cart loader you wrote) fills `$FE08`/`$FE09`. Color PROM RGB is always valid; the missing piece is which indices sit in the 8 active palettes.

Hardware never auto-loads cart globals into the buffer. Fallback steps above are **software** conventions. Studio export should normally always emit valid global sets (copying system defaults into the cart image if the user left them blank).
### Active buffer

PPU shows **4 BG + 4 sprite** palettes at once (dedicated palette RAM, not nametable VRAM).

- Software picks palette row **0-7**. BG and sprite rows are always the **same** index (no BG-4 / sprite-2 mix).
- Tile attrs and OAM attrs still only pick **0-3** inside that row.
- Screens may optionally name a palette row in MAP (directory flags / world `default_pal_row`). Switching rows means reload `$FE08`/`$FE09`.

**Shared color 0:** all 8 active palettes use the same master index at color 0 (universal backdrop). Software writes that into every slot when loading a row. On BG, color 0 is the backdrop. On sprites, pattern color 0 stays **transparent**.

Sprite priority: transparent sprite -> BG. Sprite-behind + opaque BG -> BG. Else sprite.

Palette fades, interpolates, and cycles are **runtime library** helpers that rewrite the active buffer (rotate inside a 4-color set, or step to another row).

## Cart image map (`.retr01` / flash)

Everything the CPU finds through the **MAP port** (`$FE90`-`$FE93`) or the **PRG window** lives in one image. The cart starts with a **fixed header + pointer table** so boot code does not hardcode region addresses.

Addresses below are **byte offsets in the cart image** (24-bit, same width as `$FE90`-`$FE92`). Exact packing may pad for alignment; field order is the contract.

```text
+----------------------------------------------------------------+
|  CART HEADER (fixed, starts at offset 0)                       |
|    magic[4]          e.g. 'R','0','1',0x00                     |
|    format_ver        u8                                        |
|    world_count       u8 (1..8)                                 |
|    flags             u8 (reserved)                             |
|    reserved...                                                 |
|    POINTER TABLE (24-bit offsets + optional lengths)           |
|      off_prg / len_prg                                         |
|      off_global_pal_bg     -> 4 BG palettes (16 bytes)         |
|      off_global_pal_spr    -> 4 sprite palettes (16 bytes)     |
|      off_world_table       -> 8 world slots                    |
|      (optional off_strings / off_extra)                        |
+----------------------------------------------------------------+
|  GLOBAL PALETTES (cart-wide; 8 pals = 4 BG + 4 sprite)         |
|    BG set:    4 palettes x 4 master indices = 16 B             |
|    Sprite set: 4 palettes x 4 master indices = 16 B             |
|    Worlds without their own banks use these for all rendering  |
|    (Color PROM stays on board)                                 |
+----------------------------------------------------------------+
|  PRG (one global section for the whole cart)                   |
|    game code / data - not split per world                      |
|    appears in CPU space at $8000+ (see memory map)             |
+----------------------------------------------------------------+
|  WORLD TABLE (up to 8 entries)                                 |
|    each slot: present u8, off_world u24, len_world u24         |
|    empty slots: present=0                                      |
+----------------------------------------------------------------+
|  WORLD 0 BLOB (example; worlds 1..N follow same shape)         |
|  +------------------------------------------------------------+|
|  | WORLD HEADER                                               ||
|  |   start_col, start_row     default screen to load          ||
|  |   default_bg_bank          0-3  (world default BG CHR)     ||
|  |   default_spr_bank         0-3  (world default sprite CHR) ||
|  |   default_pal_row          0-7  (optional start row)       ||
|  |   screen_count             u8 (0..32)                      ||
|  |   off_chr                  -> 4 BG + 4 sprite banks        ||
|  |   off_screen_dir           -> sparse directory             ||
|  |   off_world_pal_bg         0 = none; else optional bank    ||
|  |   off_world_pal_spr        0 = none; else optional bank    ||
|  +------------------------------------------------------------+|
|  | CHR (this world)                                           ||
|  |   BG banks 0..3     4 KB each                              ||
|  |   SPR banks 0..3    4 KB each                              ||
|  +------------------------------------------------------------+|
|  | OPTIONAL WORLD PALETTE BANKS                               ||
|  |   up to 8 rows x 4 BG palettes                             ||
|  |   up to 8 rows x 4 sprite palettes                         ||
|  |   if present: overrides cart globals for this world        ||
|  |   if absent (0): this world uses cart global sets          ||
|  +------------------------------------------------------------+|
|  | SCREEN DIRECTORY (sparse, screen_count entries)            ||
|  |   per stored screen:                                       ||
|  |     col, row                                               ||
|  |     flags (parallax, per-screen default_bg_bank,           ||
|  |            pal_row hint, ...)                              ||
|  |     off_payload            -> 480 B tiles+attrs            ||
|  |     off_screen_meta        optional extra blob (0=none)    ||
|  +------------------------------------------------------------+|
|  | SCREEN PAYLOADS                                            ||
|  |   240 tile index bytes + 240 attr bytes (raw)              ||
|  +------------------------------------------------------------+|
|  | OPTIONAL PER-SCREEN META / PALETTE OVERRIDES               ||
|  |   (spawn hints, local pal row blob, etc. - Studio later)   ||
|  +------------------------------------------------------------+|
+----------------------------------------------------------------+
|  WORLD 1 BLOB ...                                              |
+----------------------------------------------------------------+
|  ...                                                           |
+----------------------------------------------------------------+
```

**How the CPU finds things**

1. Seek MAP to **0**, read magic + version + **pointer table**.
2. Use `off_global_pal_*` for cart-wide palette sets; `off_prg` for the **single** PRG section.
3. Use `off_world_table[N]` to open world N's header.
4. From that header: `off_chr`, `off_screen_dir`, defaults (`default_bg_bank`, `default_spr_bank`, `start_col`/`start_row`).
5. Directory entry -> `off_payload` for the **480 B** screen; optional `off_screen_meta` for extras.

**In other words:** the cart is self-describing. Boot reads a small directory of pointers, then walks world -> screens. No "remember that CHR starts at $3F000" baked into every game.

### World defaults vs live banks

| Item | Where | Role |
|------|-------|------|
| `default_bg_bank` | world header | World-wide default BG CHR bank **0-3** for stamps / Generate bank |
| `default_spr_bank` | world header | World-wide default sprite CHR bank **0-3** for stamps / `$FE37` helper |
| Per-screen `default_bg_bank` in directory flags | screen dir | Optional override when loading that screen |
| Live BG / sprite `BANK` | tile attr / OAM attr | What hardware actually fetches |

### Directory row (per stored screen)

| Field | Meaning |
|-------|---------|
| `col`, `row` | Grid position |
| `flags` | `bit0` parallax. `bits1-2` per-screen **default BG bank** (0-3). `bits3-5` **palette row** hint (0-7). Rest reserved. |
| `off_payload` | 24-bit offset to **480 B** screen data |
| `off_screen_meta` | 24-bit offset to optional screen metadata (0 = none) |

On world enter (typical kit path): cart pointers -> world header -> directory (`start_col`,`start_row`) -> copy payload to VRAM -> apply `default_spr_bank` / palette row as needed.

## MAP port (hardware access)

Not memory-mapped into CPU space. Read any cart offset through `$FE90`:

1. write 24-bit address (`$FE90`/`$FE91`/`$FE92`)
2. read data at `$FE93` (auto-inc)

PRG is **one global section** in the image (`off_prg` / `len_prg`). It is not banked per world and not stored as multiple PRG chapters. The CPU executes through the `$8000-$FDFF` window (~**32 KB** visible) plus `$FF00-$FFFF` (vectors). Planning **~96 KB** PRG is still one section in flash; software pages it with **`$FE80` `PRG_WINDOW`** (high address bits into that section). Small bring-up images that fit in the window may leave `$FE80` at 0.

## CPU memory map

| Range | Region |
|-------|--------|
| `$0000-$7FFF` | System RAM (CPU-only) |
| `$8000-$FDFF` | PRG window (single cart PRG section) |
| `$FE00-$FEFF` | I/O |
| `$FF00-$FFFF` | PRG high + vectors |

| Block | Purpose |
|-------|---------|
| `$FE00-$FE0F` | PPU, scroll, raster, parallax, palette |
| `$FE10-$FE1F` | VRAM port |
| `$FE20-$FE2F` | OAM (1284) |
| `$FE30-$FE3F` | world + optional BG bank helpers / sprite bank |
| `$FE40-$FE5F` | APU |
| `$FE60-$FE6F` | controllers / cabinet |
| `$FE70-$FE7F` | board EEPROM (AT28C64B, every v0 board) |
| `$FE80-$FE8F` | PRG window (`$FE80`); needed when PRG exceeds ~32 KB |
| `$FE90-$FE9F` | MAP port |

### `$FExx` draft v0

Frozen draft for Studio, firmware, and proto. Bitfields may grow reserved bits later. Do not renumber ports without a doc rev.

#### `$FE00-$FE0F` PPU / raster / palette

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE00` | W | `PPUCTRL` | bit0 BG, bit1 sprites, bit2 NMI, bit3-4 camera mode (`00`=1, `01`=2H, `10`=2V, `11`=4), bit5-7 reserved |
| `$FE01` | R | `PPUSTATUS` | bit0 VBlank, bit1 sprite overflow (opt), bit2 raster hit sticky. Read clears hit |
| `$FE02` | W | `SCROLL_X` | 0-127 wrap |
| `$FE03` | W | `SCROLL_Y` | 0-119 wrap |
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

Entry: `Y, tile, attr, X` x 64. Attr bitfields: see **OAM sprite attributes** above.

#### `$FE30-$FE3F` world / CHR

| Addr | R/W | Name | Function |
|------|-----|------|----------|
| `$FE30` | W | `WORLD` | 0-7 |
| `$FE31`-`$FE36` | W | `BG_BANK_0`..`5` | optional bulk helpers for slots 0-5 (0-3); **not** live BG fetch source |
| `$FE37` | W | `SPR_BANK` | optional bulk stamp into OAM attrs (0-3); **not** live sprite CHR fetch source |
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
| `$FE80` | W | `PRG_WINDOW` | high-bits / window into the **single** PRG section when `len_prg` exceeds the ~32 KB `$8000` map (planning ~96 KB uses this) |
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
- BG CHR bank is **per 8x8 tile** (attr `BANK`). Sprite CHR bank is **per OAM entry** (attr `BANK`).
- Plane attrs share the playfield's **4 BG palettes** (locked to the active row). Scrolling the band does not unlock new colors.

## Software cheat sheet

- Nametable bytes are tile indices **0-255** inside the bank named by that tile's attr `BANK` bits.
- Screens are **not** hardware-tied to one BG bank; mixed banks in one screen are normal.
- Per-tile `BANK` covers mixed BG art in one frame (no mid-frame bank-latch split needed for BG CHR).
- Sprite attr carries bank, palette, flips, priority, and size; mixed 8x8/8x16 in one frame is normal.
- MAP / cart pointer table locates PRG, global palettes, worlds, CHR, and screens. World header names the **default start screen**, **default BG bank**, and **default sprite bank**.
- Camera slots hold **whole screens**. Scroll moves the window; MAP loads happen on software slot shifts (`02` worked example).
- Sprites use a ping-pong line buffer one scanline ahead (`02` diagram, `03` detail).
