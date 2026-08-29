# Retr01 Graphics, Worlds, and Memory

Display, worlds, VRAM, palettes, cart image, and `$FExx`.

**Authority:** this file is the **software-visible** source of truth (see [`01`](01_architecture_overview.md)). Current HW BOM: [`05`](05_hardware_v1_32ic.md) (**32 IC**). HW must not invent CPU ports here. Open mailbox/I2C/bitfield items stay listed as TBD below.

## Display

| Item | Value |
|------|-------|
| Logical | **128x120** (**16x15** tiles). Games/Studio stay here |
| RGBS field | **256x240** inside **341x262** |
| Clocks | CPU **8.000 MHz**, dot **5.369318 MHz** (independent), ~**60.098 Hz** |
| SCALE DIP | **2x** default (fills field). **1x** centers 128x120. Raster only (no `$FExx` / cart bit) |

## Worlds, screens, cart budget

- **8** worlds max (indices **0-7**). Sparse **8x8** grid per world, **32 present screens** max/world (camera / playfield only. Grid slots beyond that stay unused)
- Per world: **0..8** parallax screens (`PARALLAX_MAX` = **8**). Same **480 B** payload as a playfield screen. **Not** on the world grid. Separate MAP directory. Live fetch uses VRAM slots **4-5** only (PRG streams which payloads are resident). See [Parallax](#parallax)
- Screen / plane payload: **480 B** raw (**240** tiles + **240** attrs). Direct MAP `$FE93` -> VRAM `$FE12` (no RLE required)
- Per world: **4 BG + 4 sprite** CHR banks (**32 KB**), screen dir + parallax dir
- Palettes: **8 global BG palette rows** + **8 global sprite palette rows** (see [Palettes](#palettes))
- Cart: **512 KB** (SST39SF040). **32 KB PRG** at `$8000-$FFFF` (I/O hole at `$FE00-$FEFF`). No PRG banking. Cart `format_ver` **2**.

| Asset | Size at caps (8 worlds) |
|-------|--------------------------|
| CHR (8 x 32 KB) | **256 KB** (262144 B) |
| MAP screens (8 x 32 x 480 B) | **120 KB** (122880 B) |
| MAP parallax (8 x **8** x 480 B) | **30 KB** (30720 B) |
| Global pals (8 BG rows + 8 sprite rows) | **256 B** |
| Dirs / headers | **~3956 B** (~3.9 KB) |
| PRG | **32 KB** (32768 B) |
| **Total / free** | **452724 B** (~**442.1 KB**) used. **71564 B** (~**69.9 KB**) free of **512 KB** |

**32 present screens**/world is the playfield cap (sparse **8x8** grid). Full 8-world fill + **8** parallax/world + **32 KB** PRG leaves ~**70 KB** free in **512 KB** flash for ([Other screens](#other-screens-global-rom)), entity tables, and headroom:

| Spare budget (from ~69.9 KB free) | Size |
|----------------------------------|------|
| **Other screens** (title + interstitial + credits pages, RLE or raw) | **up to ~64 KB** soft |
| Entity tables + alignment + unused flash | **remainder** (~**6 KB+** if other uses full soft budget) |

World blobs hold **playfield** screens and **parallax** only. Title, level interstitials, and **credits pages** are **global other screens** (see below). They do **not** count toward the **32 present screens**/world cap.

**Banks:** live BG bank = per-tile attr bits 1-0. Live sprite bank = per-OAM attr bits 1-0. `$FE31`-`$FE37` are optional stamp helpers only.

## BG and sprite attributes

```text
BG attr                            OAM attr
7 6 5 4 3 2 1 0                    7 6 5 4 3 2 1 0
| | | | | | |_|__ BANK 0-3         | | | | | | |_|__ BANK 0-3
| | | | |_|______ PAL 0-3          | | | | |_|______ PAL 0-3
| | | |__________ FLIP_H           | | | |__________ FLIP_H
| | |____________ FLIP_V           | | |____________ FLIP_V
| |______________ SOLID (soft)     | |______________ PRIORITY
|________________ ANIM (soft)      |________________ SIZE (0=8x8, 1=8x16)
```

Hardware uses BANK/PAL/FLIP (and sprite PRIORITY/SIZE). Video ignores SOLID/ANIM. `ANIM=1`: 4-frame strip, base `B` 4-aligned (`B..B+3`). `SIZE=1`: even base tile `B`, top `B` / bottom `B+1`. Cap **16** sprites/logical line.

## Camera and VRAM

Slots **0-3** = live 2x2 camera. Slots **4-5** = parallax only (**two** live plane slots). Each slot **512 B** (240+240 used, attrs at `+0xF0`). A world may store up to **8** parallax payloads in cart. PRG chooses which one or two are loaded into slots **4-5**. Scroll `$FE02`/`$FE03`: **0-127** / **0-119**. Hardware does not auto-load MAP. Pan inside loaded slots = scroll only. Seam = software streams ~480 B/screen via `$FE12` (auto-inc). Prefer direct MAP->VRAM.

```text
+-------------+-------------+
|  Slot 0     |  Slot 1     |
+-------------+-------------+
|  Slot 2     |  Slot 3     |
+-------------+-------------+
        ^ 128x120 viewport (scroll)
```

```text
MAP (example)                         VRAM 4-slot workbench
+-----+-----+-----+                   +---------+---------+
|  A  |  B  |  C  |                   | Slot 0 E| Slot 1 F|
+-----+-----+-----+                   +---------+---------+
|  D  |  E* |  F  |  player on E  --> | Slot 2 H| Slot 3 I|
+-----+-----+-----+                   +---------+---------+
|  G  |  H  |  I  |
+-----+-----+-----+
```

Scroll right into F uses slots already loaded (no MAP). Leaving the workbench west needs a software shift (e.g. load D/G, keep E/H):

```text
Before                         After shift west
+-----+-----+                  +-----+-----+
|  E  |  F  |                  |  D  |  E  |
+-----+-----+                  +-----+-----+
|  H  |  I  |                  |  G  |  H  |
+-----+-----+                  +-----+-----+
```

| Action | Cost |
|--------|------|
| Scroll 1 px | 1-2 latch writes |
| 1 / 2 / 3 screens into VRAM | ~480 / 960 / 1440 B (~**11** / **23** / **34** CRT lines @ ~12 cyc/B, interleaved) |

Interleave allows `$FE12` during active display. Do not rewrite a **visible** cell under the beam (tear). Off-screen slots and VBlank/beam-gated pokes are safe.

| VRAM | Purpose |
|------|---------|
| `$0000`-`$07FF` | camera slots 0-3 (512 B each) |
| `$0800`-`$0BFF` | plane slots 4-5 |
| `$0C00`-`$3FFF` | scratch |
| `$4000`-`$7FFF` | reserved |

## Sprite line buffer

OAM in **1284** (`$FE20`/`$FE21`). Ping-pong **128 px** halves in third SRAM. One scanline pipeline, not a framebuffer.

```text
         Half A ($000-$07F)          Half B ($080-$0FF)
        +------------------+        +------------------+
Line N  | SHOW (beam)      |        | fill N+1 (HBlank)|
        +------------------+        +------------------+
Line N+1| fill N+1         |        | SHOW             |
        +------------------+        +------------------+
```

## Palettes

**Color PROM** (board): **64 master indices**, packed **`{RRRGGGBB}`** (R3 G3 B2) in **one** PROM/OTP chip, 1-dot pipeline ([`05`](05_hardware_v1_32ic.md)). Active buffer: **4 BG + 4 sprite** via `$FE08`/`$FE09` (indices held in packed HC573 / decode path on the 32-IC board, no separate palette RAM IC). Shared color 0. BG+sprite row index always locked together.

**Cart storage:** **8 BG palette rows** + **8 sprite palette rows**. Each row is **4 palettes x 4 master indices = 16 B**. Totals: **128 B** BG + **128 B** sprite = **256 B**. Software selects a row (`$FE38` hint) and copies it into `$FE08`/`$FE09`.

Kit / Studio **logical** swatches below are full 24-bit reference colors. Studio and burn tools **quantize** to R3G3B2 when building the PROM image (Studio README).

(Earlier board sketches used 3x AT28C16 R/G/B, not the current norm.)

```text
#000000 #290514 #2A0507 #230F06 #1E1306 #1A1605 #141807 #061A07 #051A13 #071918 #08181C #071722 #030B3D #16033A #20052D #260420
#363636 #740A40 #77091A #693512 #5D3F0E #514617 #424C19 #13511A #16503F #114E4D #164D58 #164A66 #163794 #472990 #5F167D #6C115F
#949494 #C04A7A #C54A4D #B8601B #A27326 #8F7E2F #77872D #209030 #2E8E72 #318B89 #1F889C #2483B5 #4D77D7 #7E6AD3 #9D5DBF #B352A0
#FFFFFF #F1A2BB #F1A6A1 #F1A983 #EEAC44 #D4BA33 #B0C841 #73D275 #22D0A6 #3BCDC9 #48C9E4 #88C4ED #A4BDEF #BBB5F1 #D5A9EF #F09BDD
```

**Fallback (software, load time):** cart global rows -> else kit defaults. Bare metal with no `$FE08`/`$FE09` writes = **garbage colors**. Hardware never auto-loads.

## Parallax

Per-world MAP payloads used as scrolling backdrop planes. **Not** on the playfield grid. Stored in the world blob parallax dir + payloads.

| Field | Value |
|-------|-------|
| `parallax_count` | **0..8** (`PARALLAX_MIN` = **0**, `PARALLAX_MAX` = **8**) |
| Payload | Same **480 B** as a playfield screen (240 tile + 240 attr), raw |
| Live VRAM | Slots **4-5** only (**2** resident at a time). PRG loads/swaps from the up-to-**8** cart payloads |
| Scroll ports | Plane band `$FE06`/`$FE07` (phase 2+). Main camera may lock per [04](04_costs_and_open_questions.md) when a band is active |
| Slices | **1..120** bands of **variable thickness** on the live plane (H or V). See below. Not playfield camera mid-frame shifts |

**Layouts** (dir `flags`: PRG interprets. Hardware only sees the loaded slot(s)):

| Mode | Screens used | Behavior |
|------|--------------|----------|
| **Single** | **1** payload | One **128x120** tilemap. Scroll and **repeat** on the chosen axis (H, V, or both) |
| **Pair H** | **2** consecutive dir entries | Side-by-side (**256x120** strip). Scroll/repeat horizontally so the period is **two** screens: seam less obvious than a single tilemap |
| **Pair V** | **2** consecutive dir entries | Stacked (**128x240** strip). Scroll/repeat vertically with a **two**-screen period |

A world may mix singles and pairs as long as total payloads stay within **8**. A pair consumes **two** of the `parallax_count` slots. The second entry is the partner (not a second independent layer head).

**Dir entry (8 B):** `{ index u8, flags u8, live_slot u8, pad u8, off_payload u24 }`

| `flags` bits | Meaning |
|--------------|---------|
| **1-0** | Layout: **00** = single, **01** = pair-H head, **10** = pair-V head, **11** = reserved. Partner entries use **00** (or ignore layout) |
| **2** | Scroll/repeat **H** |
| **3** | Scroll/repeat **V** |
| **7-4** | Reserved (**0**) |

| `live_slot` | Meaning |
|-------------|---------|
| **0** / **1** | Preferred VRAM slot **4** / **5** when this payload is resident (PRG may override) |
| other | Unused / PRG-defined |

**Authoring note:** prefer **pair** layouts when a looping backdrop would show an obvious repeat every 128 or 120 px. Singles are fine for sparse stars, distant haze, or intentionally tiled patterns.

### Parallax slices (variable-thickness bands)

For Mode-7-style roads and similar tricks: keep a few **base** parallax screens for forward / side motion, and bend the plane with **independent signed offsets on bands** so the same tiles look curved or sheared.

| Field | Value |
|-------|-------|
| Slice count | **1..120** when enabled (`PARALLAX_SLICE_MAX` = **120**). **0** = off |
| What a slice is | One band: **thickness** (px) + signed **offset** (px). Thicknesses may differ |
| Cover | Band thicknesses along the slice axis must sum to the viewport on that axis (**120** for H-band mode, **128** for V-band mode). Pad with a final band of offset **0** if needed |
| H-band mode | Bands stack along **Y** (rows). Offset is **H** (`dx`). Typical when the plane scrolls / repeats **horizontally** (roads, side-view depth) |
| V-band mode | Bands stack along **X** (columns). Offset is **V** (`dy`). Typical when the plane scrolls / repeats **vertically** |
| Applies to | Live plane slots **4-5** only. **Not** playfield slots **0-3** (collision / camera stay coherent) |
| Storage | Authoring: list of `{thickness, offset}` (max **120** entries). Runtime may expand to per-row / per-col additives in **sys RAM** (or a future `$FExx` port: bitfield TBD). **Not** extra MAP screens |
| Base scroll | Still `$FE06`/`$FE07` band + plane scroll. Slice offsets are **additive** inside each band |
| Typical use | Road: base plane(s) scroll for "driving forward". H-bands ramp L/R down the view for curves |

**Do not** implement this with mid-frame playfield camera shifts. Playfield scroll stays frame-coherent. Bends belong on the parallax plane.

Empty / unused expanded lines read as **0** (no bend). PRG may rewrite the table every frame (or every few frames) for animation.

**Example: horizontal parallax (H-bands):** **62** slices covering **120** rows: sixty **1 px** bands + two **30 px** bands.

```text
  Y
  0  +--+  1 px, dx = d0
     +--+  1 px, dx = d1
     ...   (60 x 1 px bands)
  59 +--+  1 px, dx = d59
  60 +=======+  30 px, dx = d60
  89 +=======+
  90 +=======+  30 px, dx = d61
 119 +=======+
```

Fine **1 px** bands near the horizon (or vanishing point) give smooth curve control. Thick **30 px** bands farther out hold a constant offset cheaply.

**Example: vertical parallax (V-bands):** same mix along **X**: sixty **1 px** + two **30 px** = **120** columns, then one **8 px** pad band (`dy = 0`) to fill **128**.

```text
  X 0                                                         127
     | 1 | 1 | ... x60 ... |==== 30 px ====|==== 30 px ====| pad 8 |
       ^ fine V offsets          shared dy         shared dy   dy=0
```

You may also use fewer thicker bands (e.g. **4** slices of **30 px** for H-band mode) or a full **120** x **1 px** table when every row needs its own offset.

## Other screens (global ROM)

Non-playfield UI that is **not** on the world grid. Lives in cart flash as **MAP-readable data** (same `$FE90`-`$FE93` port as world MAP). **Not** in the **32 KB** PRG image: PRG holds boot/load/scroll/fade **code** only.

| Kind | Id range | Cart storage | Dev workflow |
|------|----------|--------------|--------------|
| **Title** | **0** | Other-screen payload (raw or RLE) | Stream decode -> VRAM slot **0**, scroll **0,0** |
| **Level interstitial** | **1** | One shared other-screen payload | Reuse between levels. PRG may patch tiles after load |
| **Credits pages** | **`CREDITS_FIRST` ..** | Optional other-screen payloads (text + small graphics). **Min 0**, **max 46** (`CREDITS_MAX`) | PRG chooses scroll / fade / hold / cut. No fixed presentation in the cart |

**Caps:**

| Field | Value |
|-------|-------|
| `other_count` max | **48** (`OTHER_MAX`: title + interstitial + credits) |
| Credits pages | **min 0**, **max 46** (`CREDITS_MIN` / `CREDITS_MAX`). Ids start at **2** (`CREDITS_FIRST`) |
| Uncompressed payload | Always **480 B** (240 tile + 240 attr) after decode |
| On-cart payload | **Raw 480 B** or **RLE** (see flags). Dir stores `len_payload` |
| Soft blob budget | Keep `len_other` practical (**~64 KB** typical headroom from free flash) |
| CHR | Reuse existing BG banks (often world **0** / shared UI glyphs). No extra CHR line |

**Dir entry (8 B):** `{ id u8, flags u8, len_payload u16, off_payload u24 }`

| `flags` bit | Meaning |
|-------------|---------|
| **0** (`RLE`) | **1** = RLE-compressed payload. **0** = raw **480 B** |
| 1..7 | Reserved (**0**) |

**RLE (byte runs, whole 480 B stream):** command byte `C`:
- `C < 0x80`: copy next `(C+1)` literal bytes (1..128)
- `C >= 0x80`: repeat next byte `(C-0x7F)` times (1..128)

Studio exports RLE when it shrinks the payload. Otherwise raw. Decode before VRAM fill (PRG or host helper).

**Presentation is PRG-owned.** The cart only stores screen payloads. Custom PRG may scroll credits pages vertically, cross-fade, show a single static page, interleave graphics frames, etc. Host Play / Studio do not imply a credits mode.

**Pointer table:** `off_credits` / `len_credits` are **reserved** and must be **0** (legacy ASCII credits blob removed: use credits **pages** in other screens instead).

## Cart image (`.retr01`)

24-bit offsets. Magic **`retr01`** (lowercase ASCII). **`format_ver` = 2**: **36 B** pointer table, **8** world table slots, **32 present screens**/world max, **other screens** (title / interstitial / credits pages). PRG payload is **32 KB** (no banking). Older images are not loaded: re-export from Studio.

```text
+--------------------------------------------------------------------------+
|  CART HEADER (16 B at offset 0)                                          |
|    magic[6]          'r','e','t','r','0','1'                             |
|    format_ver        u8 (**2**)                                          |
|    world_count       u8 (1..8)                                           |
|    flags / reserved  (pad to 16 B)                                       |
|  POINTER TABLE (36 B -- each field u24)                                  |
|    off_prg, len_prg                                                      |
|    off_pal_bg, len_pal_bg                                                |
|    off_pal_spr, len_pal_spr                                              |
|    off_world_table, len_world_table                                      |
|    off_other, len_other    -- other screens blob                         |
|    reserved u24, reserved u24  -- must be 0 (legacy credits ASCII)       |
+--------------------------------------------------------------------------+
| GLOBAL PALETTES                                                          |
|    BG:     8 rows x 4 pals x 4 master indices = 128 B                    |
|    Sprite: 8 rows x 4 pals x 4 master indices = 128 B                    |
|    Active row N: copy 4 BG + 4 sprite pals into $FE08/$FE09              |
+--------------------------------------------------------------------------+
|  PRG (one global section, 32 KB at $8000 + I/O hole $FE00)               |
|    No banking. Vectors in high page                                      |
|    Code only -- credits *pages* live in OTHER SCREENS below              |
+--------------------------------------------------------------------------+
|  OTHER SCREENS (global -- not in world blobs)                            |
|    other_count u8 (1..48: id0=TITLE, id1=INTER, id2+= credits (0..46))   |
|    pad[3]                                                                |
|    DIR: other_count x 8 B                                                |
|      { id, flags, len_payload u16, off_payload u24 }                     |
|    PAYLOADS: variable; raw 480 B or RLE (flags.RLE). Decode -> 480 B     |
+--------------------------------------------------------------------------+
|  WORLD TABLE (8 slots x 8 B)                                             |
|    each slot: present u8, pad u8, off_world u24, len_world u24           |
+--------------------------------------------------------------------------+
|  WORLD 0 BLOB                                                            |
|  +-----------------------------------------------------------------+     |
|  | WORLD HEADER (32 B)                                             |     |
|  |   start_col, start_row, default_bg_bank, default_spr_bank       |     |
|  |   default_pal_row (0..7), screen_count (present, max **32**)    |     |
|  |   parallax_count (0..8)                                         |     |
|  |   off_chr u24, off_screen_dir u24, off_parallax_dir u24         |     |
|  |   entity_type_count u8, entity_inst_count u8                    |     |
|  |   off_entity_types u24, off_entity_insts u24                    |     |
|  |   pad u8, player_entity u8 (0xFF=stub), hit_x/y/w/h u8          |     |
|  |   reserved to 32 B                                              |     |
|  +-----------------------------------------------------------------+     |
|  | CHR: BG 0..3 + SPR 0..3 (4 KB each; real spr_banks)             |     |
|  | SCREEN DIR: 12 B per present screen                             |     |
|  |   col, row, flags0, flags1 (Phase 1: flags = 0)                 |     |
|  |   off_payload u24, off_screen_meta u24 (0 if unused)            |     |
|  | PARALLAX DIR: parallax_count x 8 B (max 8)                      |     |
|  |   { index, flags, live_slot, pad, off_payload u24 }             |     |
|  |   flags: layout single/pair-H/pair-V + scroll H/V               |     |
|  | SCREEN PAYLOADS: 240 tile + 240 attr each                       |     |
|  | PARALLAX PAYLOADS: same 480 B shape (after screens)             |     |
|  | ENTITY TYPES: 20 B each (state0/frame0 only)                    |     |
|  |   origin_x, origin_y, part_count, pad                           |     |
|  |   4x {tile, attr, dx i8, dy i8} (unused parts zero)             |     |
|  | INSTANCES: 6 B each {type_id, flags, world_x u16, world_y u16}  |     |
|  |   flags bit0 = flip_h, bit1 = flip_v (mirror around origin)     |     |
|  +-----------------------------------------------------------------+     |
+--------------------------------------------------------------------------+
|  WORLD 1 .. N                                                            |
+--------------------------------------------------------------------------+
```

OAM attr packing matches BG: bank bits 1-0, pal bits 3-2, `FLIP_H=0x10`, `FLIP_V=0x20`. Instance `world_x/y` is the **user origin**. Host Play draws parts at `world + (dx,dy) - origin` (Studio `r01_entity_world_x/y`). Instance flags bit0/bit1 (`flip_h`/`flip_v`) mirror each part: `dx' = 2*origin_x - dx - 8` / `dy' = 2*origin_y - dy - 8` and XOR part `FLIP_H` / `FLIP_V` (Studio `r01_entity_part_instance_pose`). World header `player_entity` selects the Play-driven type (state0/frame0); `0xFF` falls back to SPR bank 0 **tile 1** (solid stub). Player collision uses the packed hitbox at `origin + (hit - state_origin)`.

Boot: magic -> pointers -> other screens -> world header -> screen dir / parallax dir -> `off_payload`. Load grid screens into VRAM slots 0-3. Load up to **two** active parallax payloads into slots 4-5 (from the world's up to **8** cart entries). Title/interstitial/credits page: decode chosen **other** payload (RLE or raw) into slot 0 (full **128x120**). MAP port: `$FE90`-`$FE92` addr, `$FE93` data auto-inc.

**Debugging carts:** Studio Play and editor chrome are **not** the cart. Runner helpers differ:

| Runner | MAP / pals into VRAM |
|--------|----------------------|
| **Studio export PRG** | Streams **one** pal row + **start screen** via `$FE93` -> `$FE08`/`$FE09`/`$FE12` |
| **Emulator** | Default: cart PRG pal+start MAP catchup into VRAM (`$FE93`->`$FE12`). Host Play handles camera/player/warps, `sync_camera` reloads a 2x2 workbench during Play. Opt-in host memcpy: `R01E_SOFTBOOT=1` |
| **Board sim** | Default = IC stream from cart PRG. Softboot only if `R01S_SOFTBOOT=1` |

Phase 1 PRG boot streams **one** start screen only (no full 2x2 seam PRG yet). Host Play in emu/sim fills the 2x2 window during preview. Triage ROM vs runner: [`retr01_sim/README.md`](../retr01_sim/README.md#cart-rom-vs-runners-triage).

## CPU map and `$FExx`

Logical CPU addresses below are the software SoT. Silicon uses **9x HC573** with bit-packing ([`05`](05_hardware_v1_32ic.md)). The **bitfield packing table is still TBD** (Q21). Prefer these logical addresses + Zero Page shadows until it lands.

| Range | Region |
|-------|--------|
| `$0000-$7FFF` | System RAM |
| `$8000-$FDFF` | PRG (low image, I/O hole at `$FE00-$FEFF`) |
| `$FE00-$FEFF` | I/O |
| `$FF00-$FFFF` | PRG high + vectors |

PRG is a **32 KB** image mapped at `$8000-$FFFF` with the `$FE00-$FEFF` I/O hole. No bank select. Vectors live in the high page (`$FFFA-$FFFF`).

| Addr | Name | Notes |
|------|------|-------|
| `$FE00` | `PPUCTRL` | BG/sprites/NMI enable bits, camera slot mode (**1 / 2H / 2V / 4**, bitfield TBD) |
| `$FE01` | `PPUSTATUS` | VBlank, raster hit (read clears) |
| `$FE02`/`$FE03` | scroll X/Y | 0-127 / 0-119 |
| `$FE04`/`$FE05` | raster Y / IRQ | `$FE04` = compare scanline (latched). `$FE05` = enable/ack/control (**bitfield TBD**) |
| `$FE06`/`$FE07` | plane band | any band locks camera axis for the frame. Plane slices (**1..120**, variable thickness, H or V) apply inside the band. See [Parallax](#parallax) |
| `$FE08`/`$FE09` | pal addr/data | active indices 0-63, auto-inc |
| `$FE10`-`$FE12` | VRAM addr/data | auto-inc |
| `$FE20`/`$FE21` | OAM addr/data | auto-inc. Entry `Y,tile,attr,X` x64. **Host Play:** X/Y are **viewport-relative signed** coords packed as `int8` in each byte (negative top-left allowed. Raster clips to **128x120**). Unused slot: `tile == 0xFF` |
| `$FE30` | `WORLD` | 0-7 |
| `$FE31`-`$FE37` | bank helpers | optional stamps, not live fetch |
| `$FE38` | `PAL_ROW` | hint. Still copy `$FE08`/`$FE09` |
| `$FE40`-`$FE5F` | APU | **ATmega328P** |
| `$FE60`/`$FE61` | pads | bits 0-7: right, left, down, up, X, Y, **coin** (cabinet) / **select** (console draft), start (**1=pressed**) |
| `$FE70`-`$FE72` | machine EEPROM | **1284 internal EEPROM** handshake (protocol TBD) |
| `$FE80` | (reserved) | Unused. Leave **0**. No PRG banking |
| `$FE90`-`$FE93` | MAP | 24-bit seek + read auto-inc |
| (TBD) | cart save | **Cart I2C EEPROM** via HAL (`cart_save_*`). CPU port TBD (Q20) |

**HAL:** PRG should use `machine_eeprom_*` and `cart_save_*` helpers so games are not welded to a specific mailbox layout. APU may use direct `$FE4x` stores.

**Open before shipping carts that need saves / machine config:** freeze mailbox protocol + cart I2C `$FExx` (Q20), and HC573 bitfield packing (Q21) in this doc.
