# Graphics & Cartridge Architecture

Canonical description of Retr01 tile graphics, pattern banks, and cartridge ROM budget. Worlds, the sparse screen atlas, and `load_screen` live in [04_worlds_and_screens.md](04_worlds_and_screens.md).

## 1. Display geometry and timing

| Item | Value |
|------|-------|
| Visible resolution | 256x240 |
| Tile size | 8x8 |
| Nametable | **32x30** tile indices (960 bytes) + **240-byte** attr plane (1 byte per 2x2 cell, 2 bits per tile) |
| Color depth | 2 bpp (3 colors + transparency per draw unit) |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** (NTSC PPU-rate, same class as NES) |
| Dots per scanline | **341** (256 visible + 85 HBlank) |
| Scanlines per frame | **262** (240 visible + 22 VBlank) |
| Frame rate | **~60.098 Hz** (NMI metronome) |

CPU and dot clocks are **independent**. The W65C02S runs at 8 MHz for game logic and interleaved VRAM phases. The video beam advances on the 5.369318 MHz dot clock.

That dot clock **is** the CRT refresh if Retr01-A drives analog **RGBS** into a 15.7 kHz arcade/CGA monitor: 341 dots per line gives ~15.7 kHz horizontal, 262 lines gives **~60.1 Hz** vertical. NMI fires at that same rate. A modern TV is different. The panel has its own 60 Hz. An external analog-to-HDMI (or similar) converter on the RGBS pads resamples our output. The PPU still runs 341x262 either way. Game code is paced by NMI, not by the TV.

PPU VRAM fetches occur only on PPU-owned CPU phases (with line buffers / shift registers bridging the two domains as on real hardware). In other words, even though the PPU only accesses VRAM 50% of the time, the display never flickers. The hardware uses shift registers to cache the graphics data during the PPU's phase, continuously streaming pixels to the screen while the CPU executes game logic during its phase.


See also [08_memory_map.md](08_memory_map.md) for the VRAM port and interleave rules.

## 2. Scroll, banks, and CHR

World / grid / screen atlas: [04_worlds_and_screens.md](04_worlds_and_screens.md). Do not mix those with the PPU camera.

| Word | What it is here |
|------|-----------------|
| **Scroll** | `scroll_x` / `scroll_y`, one byte each (0-255). Pixel offset of the 256x240 window across the live VRAM slots. PPU latches (`$FE0x`). |
| **CHR bank** | 512 patterns (256 BG + 256 sprites). 4 banks per world. `$FE30` picks BG bank and sprite bank separately. |

Retr01 does **not** define NES-style **pattern tables/pages** as an extra runtime layer. Software sees a **BG bank** and a **sprite bank**. The screen's tile bytes are just **0-255** into the selected BG bank's BG half.

### Bank rules

1. Each world has **4** pattern banks.
2. Each bank: **first 256 patterns BG**, **second 256 sprites** -> **512** patterns / bank (8 KB @ 16 bytes/pattern).
3. **Authored bank:** `load_screen` sets the BG bank that screen was drawn against. That is the default at the start of the frame. Software may still switch banks mid-frame (see section 8).
4. **Runtime banks:** PPU may select **BG bank** and **sprite bank** independently. Either may change **mid-frame**. Nametable bytes stay 0-255 into whichever BG bank's BG half is **currently** latched.
5. PPU fetches CHR **from cartridge CHR-ROM** (not from VRAM).

## 3. CHR size math

| Unit | Patterns | Bytes |
|------|----------|-------|
| BG half or sprite half | 256 | 4 KB |
| Bank | 512 | 8 KB |
| World (4 banks) | 2048 | 32 KB |
| Cart max (8 worlds) | 16384 | **256 KB** CHR |

## 4. Sprites vs background

These are two fetch paths. Scroll only affects which nametable byte is under the beam.

- **BG:** scroll picks a pixel in the live nametable field. That byte is a tile index **0-255** into the **active BG tile set** (the BG half of the BG bank on cart).
- **Sprites:** OAM (64 entries) holds tile indices into the **active sprite tile set** (the sprite half of the sprite bank). OAM does not use scroll.
- Max **16 sprites per scanline**. Extras are dropped.
- Compositor: if the sprite pixel is pattern color 0, skip it (transparent). Else if the OAM **priority** bit is set, opaque BG wins. Else the sprite wins. Pattern color 0 is **not** OAM sprite #0. OAM entry 0 is a normal sprite.

## 5. Palettes and attributes

- **8 palettes** total: **4 background + 4 sprite**. Each palette has 4 entries because the art is 2bpp (pattern bits 00, 01, 10, 11).
- **Index 0 means two different things**, same as the NES:
  - **Background:** index 0 is a real color, the **shared backdrop**. All 4 BG palettes use the same color 0. A "transparent" hole in a BG tile just shows that backdrop. Normal screens are opaque.
  - **Sprites:** index 0 is **true transparency**. That sprite pixel is skipped. You see whatever BG (or backdrop) is behind it.
- **BG attributes are per tile:** each of the 960 nametable tiles has its own 2-bit palette select (which of the 4 BG palettes). NES instead shares one select across a 2x2 tile group.
- **Storage:** **240 bytes** per screen/slot. One byte covers a **2x2 cell of tiles** (16x15 cells on a 32x30 screen). Four 2-bit fields, one per tile:
  - bits 0-1: top-left tile
  - bits 2-3: top-right tile
  - bits 4-5: bottom-left tile
  - bits 6-7: bottom-right tile
  Index: `attrs[(ty / 2) * 16 + (tx / 2)]`, then shift by `((ty & 1) * 2 + (tx & 1)) * 2`.
- Sprite attributes: **NES-like OAM attr byte** (palette, flips, priority). Pattern color 0 = transparent. The priority bit puts the sprite behind opaque BG.
- **Master palette:** 64-entry custom Retr01 ramp — [`retr01_world_studio/retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt) (see [planning/09_master_palette.md](../retr01_world_studio/planning/09_master_palette.md)).

## 6. Live VRAM vs cartridge maps

**Cartridge MAP region** holds the world atlas and compressed screens. See [04_worlds_and_screens.md](04_worlds_and_screens.md). CPU reads MAP only through the **`$FE90` MAP port**. PRG/CHR share the same ~2 MB flash.

**On-board VRAM (32 KB)** live set:

| Contents | Planning size |
|----------|----------------|
| 4 nametable slots x (960 tiles + 240 attrs), 2 KB-aligned | 4 x 2 KB = **8 KB** (2x2 camera) |
| 2 parallax **plane** slots (same 2 KB format) | **4 KB** |
| Streaming / decompress scratch | ~4 KB |
| Reserved / future | remainder of 32 KB |

OAM lives in the **ATmega1284P** (internal RAM), reached via `$FE20`/`$FE21`. It is not in the VRAM chip. Visible sprite pixels come from a **line-buffer SRAM** (third AS6C62256, ping-pong). Coprocessor: [14_reduced_number_of_chips.md](14_reduced_number_of_chips.md).

### Scrolling model

- `scroll_x` and `scroll_y` are **one byte each** (0-255, wrap). That is the pixel camera inside the live field.
- Live field: up to **four** nametable slots (2x2). Arrangement/mirroring chooses **1, 2, or 4** distinct screens. As the window crosses a slot boundary, those neighbor tiles **are** on screen. That is how pixel scrolling looks continuous.
- **Planes:** two extra slots (see section 8). Not part of the 2x2 camera. Raster IRQ may point the top (or other) scanline band at a plane.
- **Streaming cue:** software, **2 tiles (16 px)** before a seam. Neighbor lookup, empty-template fill, and `load_screen` are in [04_worlds_and_screens.md](04_worlds_and_screens.md).

## 7. Cartridge ROM budget (~2 MB)

| Region | Role | Ceiling |
|--------|------|---------|
| PRG-ROM | Game code | ~512 KB |
| CHR-ROM | Pattern banks | ~256 KB |
| MAP-ROM | Compressed nametables + attributes | ~1.17 MB |
| **Total** | Example parallel flash | **~2 MB** |

Uncompressed map upper bound: `8 x 64 x (960 + 240) = 600 KB` before RLE (tile plane + packed attr plane).

## 8. Mid-frame banks, parallax, and raster IRQ

Nametable indices are always **one byte** (0-255). You do not store a bank number per tile. More unique tiles on screen, parallax, and status-bar splits are the same class of trick as the NES: change a latch **while the beam is running**.

Retr01 makes that latch change **easier** than the NES. We do **not** use sprite-0 hit.

### Why not NES sprite-0

NES games wait until a non-transparent pixel of OAM sprite 0 overlaps opaque BG, then spin until that flag, then write scroll or CHR banks. That burns a sprite, depends on art, and races the PPU. Retr01 also cannot copy NES cycle-counted waits: CPU clock (8.000 MHz) and dot clock (5.369318 MHz) are **independent**, not a 3:1 pair.

Gameplay collision stays AABB in PRG ([01_system_overview.md](01_system_overview.md) principle 5). Raster timing is a beam compare, not a compositor collision.

### Raster compare (the sprite-0 replacement)

In `$FE0x` (exact bytes still `B2`):

| Field | Role |
|-------|------|
| `raster_y` | Scanline to match (**0-255**). Visible splits are 0-239. Write this. |
| `beam_y` | Live beam Y. Read-only. Fine for debug. Do not spin on this for splits. |
| `raster_hit` | Status bit. Sets when `beam_y == raster_y` at **start of that scanline** (dot 0). Sticky until software acks. |
| `raster_irq_enable` | If set, that match asserts **IRQ** (W65C02S `IRQB`), not NMI. |

NMI stays the VBlank metronome (start of line 240). IRQ is optional and only for raster. Ack the hit in the IRQ handler, then write the **next** `raster_y` if you have another split this frame.

Hardware is a compare of the existing Y counters (74HC161) against one latch. A GAL or a 74HC688 is enough. No extra sprite.

### When a write shows up on screen

`$FE30` (banks / world) and scroll latches are live. The **next PPU pattern fetch** uses the new value. Shift registers already hold the current tile, so expect up to **8 px (one tile)** of delay. For a clean split, write during **HBlank** (85 dots, ~126 CPU cycles at 8 MHz). The IRQ at dot 0 of line N is early enough to prepare the next line, or fire the compare on line N-1 and write in that HBlank.

### More than 256 unique BG tiles

One nametable still names tiles 0-255. The active BG tile set is whichever BG bank `$FE30` currently selects.

- At `load_screen`, set the **authored** BG bank (the set that nametable was drawn against).
- In a raster IRQ, switch to another of the world's **4** BG banks. From that scanline down, the same 0-255 indices are a **different** 256 pictures.
- Four horizontal bands => up to **1024** unique BG tiles on one frame, still one nametable.
- Status bar vs playfield is the one-split version of the same trick.
- This is **not** MMC3 1 KB CHR granules. A bank switch replaces the whole 256-tile BG half. Splits are horizontal bands, not per-column banks. Per-tile bank IDs would need another attr plane. We are not adding that.

Sprite bank may switch on the same IRQ (boss art, HUD icons) independently of BG.

`$FE30` world select may also change mid-frame (legal, next CHR fetch). Usually leave it. It is a whole chapter of CHR, not a small tile set.

### Parallax (special cells + `set_parallax`)

**What it is.** Distant hills crawl, the ground races. Retr01 fakes that with a Y-band: top of the frame samples a **plane** nametable, below that the normal playfield camera.

**Camera axis (H, V, or both).** Pixel-scroll is a software policy, not a PPU bit. PRG keeps a mode:

```
#define CAM_H     0   /* scroll_x, east/west seams only */
#define CAM_V     1   /* scroll_y, north/south seams only */
#define CAM_BOTH  2   /* 1/2/4 field, both axes (default) */

void set_camera_axis(uint8_t axis);
uint8_t get_camera_axis(void);
```

Use this even **without** a plane (a side-scroller with no sky still wants `CAM_H`). The player may still walk 4/8 ways. This only limits the **camera**.

**Parallax vs `CAM_BOTH`.** A plane cannot run with a 2-axis camera (sky glued to the CRT would become a HUD). Do **not** treat that as a compile error: `set_parallax` is a runtime call, and 6502 has no exceptions. The helper **sets the camera to match** and remembers the old mode:

- `set_parallax(..., PARALLAX_H, ...)` -> `set_camera_axis(CAM_H)` (saves previous)
- `set_parallax(..., PARALLAX_V, ...)` -> `set_camera_axis(CAM_V)`
- `clear_parallax()` -> restore the saved axis (often `CAM_BOTH`)

If game code then calls `set_camera_axis(CAM_BOTH)` **while a plane is still on**: ignore BOTH, keep the 1-axis lock. In the emulator, **debug warning** (same class as wrong-phase VRAM: loud in debug, not a crash on silicon). Shipping games just no-op that store.

| Camera mode | Playfield pixel camera | Seam streaming |
|-------------|------------------------|----------------|
| `CAM_H` | `scroll_x` only. `scroll_y` frozen | East / west only |
| `CAM_V` | `scroll_y` only. `scroll_x` frozen | North / south only |
| `CAM_BOTH` | both | all 4 (and empty-neighbor peeks) |

Live playfield field is 1 or 2 nametable slots on the live axis when H or V (not the 2x2). Plane still uses slot 4 or 4+5. VRAM is not the reason for the lock. The lock is the compositor.

**Not locked:** warps and `load_screen` to another col/row (doors, stairs). After a warp you may keep the plane (camera stays 1-axis) or `clear_parallax()`.

**Not locked:** the player. They may walk in 4 or 8 directions. Walking does not always move the camera. Example: walk north in an H-parallax vista, the sprite moves, `scroll_y` stays put. Game code decides when the camera actually scrolls (edge of the window, rail, etc.).

**Authoring.** Parallax nametables are normal 32x30 .bins with directory `flags = 1`. Not enterable: `load_screen`, warp, and seam lookup treat them as holes. They still count toward the 64 stored nametables.

**Span** is how many of those cells make **one looping plane** (not two depth layers).

| `span` | Period | VRAM |
|--------|--------|------|
| 1 | 256 px (H) or 240 px (V) | slot 4 only |
| 2 | 512 px (H) or 480 px (V) | slots 4 and 5 as **one** 2-screen field |

`span` is 1 or 2. That is the two plane slots. A 768 px loop would need a third slot or streaming the plane. Not v1.

Cells are consecutive from `(col, row)` along the axis, all `flags = 1`:

- H, span 2: `(col, row)` and `(col+1, row)` -> slot 4 left, slot 5 right
- V, span 2: `(col, row)` and `(col, row+1)` -> slot 4 top, slot 5 bottom

Park them off the playfield line (e.g. row 0 while the stage is row 5). The flag is what blocks walking, not the coordinates.

`set_parallax_height` is the **visible band** in scanlines (how much sky you see). Independent of span. Span is the loop period. Height is the raster split.

**Runtime is a PRG helper**, not a PPU register.

```
#define PARALLAX_H       0   /* loop scroll_x */
#define PARALLAX_V       1   /* loop scroll_y */

#define PARALLAX_CAMERA  0   /* factor = camera divisor (2 = half, 4 = quarter) */
#define PARALLAX_AUTO    1   /* factor = signed pixels per frame, ignores camera */

void set_parallax(uint8_t col, uint8_t row, uint8_t axis,
                  uint8_t drive, int8_t factor, uint8_t span);
void set_parallax_height(uint8_t scanlines); /* from top, default 80, multiple of 8 */
void clear_parallax(void);
```

`set_parallax` also calls `set_camera_axis` to match `axis` (see above). It fails (no-op) if `span` is not 1 or 2, or a needed cell is missing / not `flags = 1`.

```
set_camera_axis(CAM_BOTH);      /* overworld, no plane */
load_screen();
set_parallax_height(80);
set_parallax(0, 0, PARALLAX_H, PARALLAX_CAMERA, 4, 2);  /* forces CAM_H */
/* ... vista ... */
clear_parallax();               /* CAM_BOTH again */
```

v1 is **one** plane (one raster split). Do not call `set_parallax` twice for sky-then-hills. That would need more plane slots.

**Drive** is one choice, not two knobs at once.

| `drive` | `factor` | When |
|---------|----------|------|
| `PARALLAX_CAMERA` | Divisor of **camera** delta on that axis (1 = lock, 2 = half, 4 = quarter). Plane only moves when the camera moves. | On the ground: distant trees / clouds crawl as you walk. Stand still, they stand still. |
| `PARALLAX_AUTO` | Signed pixels **per frame**. Ignores camera. | In the air: clouds always drift, even if the camera is parked. |

Pick one. Ground vs flying is the usual split. (A later helper could add both at once. Not v1.)

For `span = 2`, keep `plane_x` / `plane_y` in RAM wider than a byte (0-511 H, 0-479 V). Hardware `scroll_x` / `scroll_y` stay one byte. The library writes the low 8 bits to the scroll latch and uses `nt_arrange` so slots 4-5 are a 2-wide (H) or 2-tall (V) field, origin from the high bit. Same idea as playfield pixel scroll over two screens. Wrap at the span period so the pattern loops.

6502: zeropage `par_col`, `par_row`, `par_axis`, `par_drive`, `par_factor`, `par_span`, then `jsr set_parallax`. Guest PRG, not an emulator syscall.

```
 scanline
    0  +------------------------------+
       | plane (1 or 2 cells, looping)|  slots 4 / 4+5, scroll = plane
   80  +------------------------------+
       | playfield camera (1 axis)    |  slots 0-1 (H) or 0+2 (V), other axis frozen
  240  +------------------------------+
```

NMI (library):

```
nmi:
    if drive == PARALLAX_AUTO
        plane += factor                  ; constant wind
    else
        plane += camera_delta / factor   ; CAMERA, factor is divisor
    plane wrap 0 .. (span * period - 1)
    nt_arrange = plane field (1 or 2 slots)
    scroll = plane low 8 bits
    raster_y = par_height
    enable raster IRQ
    ...
irq:
    nt_arrange = PLAYFIELD_CAMERA
    scroll_x = camera_x
    scroll_y = camera_y
    disable raster IRQ
    ack raster_hit
    rti
```

**What this is not.** Full-screen two-layer parallax (far *and* near at every pixel). Pixel-scrolling X *and* Y while a plane is on. A third span cell. Left/right wallpaper columns (mid-line X split). H and V planes at the same time.

**What not to do.** Do not steal camera slots 0-3. Do not `load_screen` onto a parallax cell. Do not omit `flags`. Do not seam-stream the perpendicular axis until `clear_parallax`. The emulator should debug-warn if guest writes the frozen scroll while a plane is active.



