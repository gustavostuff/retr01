# Retr01 Graphics, Worlds, and Memory

Display, worlds, VRAM, palettes, cart image, and `$FExx`.

**Authority:** this file is the **software-visible** source of truth (see [`01`](01_architecture_overview.md) *Sources of truth*). Chip lists: v0 [`03`](03_hardware_implementation.md), v1 product BOM [`06`](06_hardware_v1_32ic.md). HW may not invent CPU ports here; proposed v1 port changes live only in [v1 deltas (proposed)](#v1-deltas-proposed-with-06) until promoted into the main tables.

## Display

| Item | Value |
|------|-------|
| Logical | **128x120** (**16x15** tiles). Games/Studio stay here |
| RGBS field | **256x240** inside **341x262** |
| Clocks | CPU **8.000 MHz**, dot **5.369318 MHz** (independent), ~**60.098 Hz** |
| SCALE DIP | **2x** default (fills field). **1x** centers 128x120. Raster only (no `$FExx` / cart bit) |

## Worlds, screens, cart budget

- **8** worlds max. Sparse **8x8** grid, **32** screens/world
- Screen: **480 B** raw (**240** tiles + **240** attrs). Direct MAP `$FE93` -> VRAM `$FE12` (no RLE required)
- Per world: **4 BG + 4 sprite** CHR banks (**32 KB**), optional palette banks, screen directory
- Cart: **512 KB** (SST39SF040). **32 KB** PRG contiguous at `$8000` (no `$FE80` paging)

| Asset | Size at caps |
|-------|----------------|
| CHR (8 x 32 KB) | **256 KB** |
| MAP (8 x 32 x 480 B) | **120 KB** |
| Pals + dirs/headers | **~6 KB** |
| PRG | **32 KB** |
| **Total / free** | **~414 KB** used, **~98 KB** free |

**Banks:** live BG bank = per-tile attr bits 1-0. Live sprite bank = per-OAM attr bits 1-0. `$FE31`-`$FE37` are optional stamp helpers only.

## BG and sprite attributes

```text
BG attr                          OAM attr
7 6 5 4 3 2 1 0                  7 6 5 4 3 2 1 0
| | | | | | |_| BANK 0-3         | | | | | | |_| BANK 0-3
| | | | |_|____ PAL 0-3          | | | | |_|____ PAL 0-3
| | | |________ FLIP_H           | | | |________ FLIP_H
| | |__________ FLIP_V           | | |__________ FLIP_V
| |____________ SOLID (soft)     | |____________ PRIORITY
|______________ ANIM (soft)      |______________ SIZE (0=8x8, 1=8x16)
```

Hardware uses BANK/PAL/FLIP (and sprite PRIORITY/SIZE). Video ignores SOLID/ANIM. `ANIM=1`: 4-frame strip, base `B` 4-aligned (`B..B+3`). `SIZE=1`: even base tile `B`, top `B` / bottom `B+1`. Cap **16** sprites/logical line.

## Camera and VRAM

Slots **0-3** = live 2x2 camera. Slots **4-5** = parallax only. Each slot **512 B** (240+240 used, attrs at `+0xF0`). Scroll `$FE02`/`$FE03`: **0-127** / **0-119**. Hardware does not auto-load MAP. Pan inside loaded slots = scroll only. Seam = software streams ~480 B/screen via `$FE12` (auto-inc). Prefer direct MAP->VRAM.

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

**Color PROM** (board): **64 master indices**. Carts store **indices only**. Active buffer: **4 BG + 4 sprite** via `$FE08`/`$FE09`. Shared color 0. BG+sprite row index always locked together.

**How the PROM stores RGB is HW-revision-specific** (does not change the 64-index cart model):

| Revision | Encoding | Authority |
|----------|----------|-----------|
| v0 | **3x AT28C16** -- separate R/G/B bytes per index | [`03`](03_hardware_implementation.md) |
| v1 (proposed) | **1x** PROM/OTP -- packed `{RRRGGGBB}` (R3 G3 B2) per index; 1-dot pipeline | [`06`](06_hardware_v1_32ic.md) |

Kit / Studio **logical** swatches below are full 24-bit reference colors. On v1 silicon, Studio and burn tools must **quantize** to R3G3B2 when building the PROM image (see [`04`](04_retr01_studio.md)).

```text
#000000 #290514 #2A0507 #230F06 #1E1306 #1A1605 #141807 #061A07 #051A13 #071918 #08181C #071722 #030B3D #16033A #20052D #260420
#363636 #740A40 #77091A #693512 #5D3F0E #514617 #424C19 #13511A #16503F #114E4D #164D58 #164A66 #163794 #472990 #5F167D #6C115F
#949494 #C04A7A #C54A4D #B8601B #A27326 #8F7E2F #77872D #209030 #2E8E72 #318B89 #1F889C #2483B5 #4D77D7 #7E6AD3 #9D5DBF #B352A0
#FFFFFF #F1A2BB #F1A6A1 #F1A983 #EEAC44 #D4BA33 #B0C841 #73D275 #22D0A6 #3BCDC9 #48C9E4 #88C4ED #A4BDEF #BBB5F1 #D5A9EF #F09BDD
```

**Fallback (software, load time):** world bank if present -> else cart globals (4 BG + 4 sprite) -> else kit defaults. Bare metal with no `$FE08`/`$FE09` writes = **garbage colors**. Hardware never auto-loads.

## Cart image (`.retr01`)

24-bit offsets. Magic **`RETR01`**.

```text
+----------------------------------------------------------------+
|  CART HEADER (fixed, starts at offset 0)                       |
|    magic[6]          'R','E','T','R','0','1'                   |
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
| GLOBAL PALETTES (cart-wide, 8 pals = 4 BG + 4 sprite)          |
|    BG set:    4 palettes x 4 master indices = 16 B             |
|    Sprite set: 4 palettes x 4 master indices = 16 B            |
|    Worlds without their own banks use these for all rendering  |
+----------------------------------------------------------------+
|  PRG (one global section, max 32 KB, contiguous at $8000)      |
+----------------------------------------------------------------+
|  WORLD TABLE (up to 8 entries)                                 |
|    each slot: present u8, off_world u24, len_world u24         |
+----------------------------------------------------------------+
|  WORLD 0 BLOB                                                  |
|  +------------------------------------------------------------+|
|  | WORLD HEADER                                               ||
|  |   start_col, start_row, default_bg_bank, default_spr_bank  ||
|  |   default_pal_row, screen_count (0..32)                    ||
|  |   off_chr, off_screen_dir                                  ||
|  |   off_world_pal_bg / off_world_pal_spr (0 = use globals)   ||
|  +------------------------------------------------------------+|
|  | CHR: BG 0..3 + SPR 0..3 (4 KB each)                        ||
|  | optional world pal banks (up to 8 rows x 4 each plane)     ||
|  | SCREEN DIR: col,row,flags, off_payload, off_screen_meta    ||
|  |   flags: parallax, default BG bank, pal row hint           ||
|  | PAYLOADS: 240 tile + 240 attr bytes                        ||
|  +------------------------------------------------------------+|
+----------------------------------------------------------------+
|  WORLD 1 .. N                                                  |
+----------------------------------------------------------------+
```

Boot: magic -> pointers -> world header -> dir -> `off_payload`. MAP port: `$FE90`-`$FE92` addr, `$FE93` data auto-inc.

## CPU map and `$FExx`

**v0 draft (current software SoT for addresses below).** Physical latch packing on a v1 PCB may share HC573 bytes; that does **not** change these **logical** addresses until a packing table is promoted here. See [v1 deltas](#v1-deltas-proposed-with-06).

| Range | Region |
|-------|--------|
| `$0000-$7FFF` | System RAM |
| `$8000-$FDFF` | PRG (low image; I/O hole at `$FE00-$FEFF`) |
| `$FE00-$FEFF` | I/O |
| `$FF00-$FFFF` | PRG high + vectors |

PRG planning cap is **32 KB** total in the cart image; the CPU sees the classic 6502 hole at `$FE00-$FEFF`.

| Addr | Name | Notes |
|------|------|-------|
| `$FE00` | `PPUCTRL` | BG/sprites/NMI, camera mode 1/2H/2V/4 |
| `$FE01` | `PPUSTATUS` | VBlank, raster hit (read clears) |
| `$FE02`/`$FE03` | scroll X/Y | 0-127 / 0-119 |
| `$FE04`/`$FE05` | raster Y / IRQ | |
| `$FE06`/`$FE07` | plane band | any band locks camera axis for the frame |
| `$FE08`/`$FE09` | pal addr/data | active indices 0-63, auto-inc |
| `$FE10`-`$FE12` | VRAM addr/data | auto-inc |
| `$FE20`/`$FE21` | OAM addr/data | auto-inc. Entry `Y,tile,attr,X` x64 |
| `$FE30` | `WORLD` | 0-7 |
| `$FE31`-`$FE37` | bank helpers | optional stamps, not live fetch |
| `$FE38` | `PAL_ROW` | hint. Still copy `$FE08`/`$FE09` |
| `$FE40`-`$FE5F` | APU | **ATmega328P** (v0 and v1) |
| `$FE60`/`$FE61` | pads | bits: R L D U X Y coin start (**1=pressed**) |
| `$FE70`-`$FE72` | board EEPROM | **v0 only** (AT28C64B). v1: see deltas |
| `$FE80` | reserved | unused v0 |
| `$FE90`-`$FE93` | MAP | 24-bit seek + read auto-inc |

## v1 deltas (proposed with 06)

Status: **proposed** with the 32-IC BOM in [`06`](06_hardware_v1_32ic.md). Not frozen. Do not treat as SoT for shipped carts until this section is promoted (and [`05`](05_costs_and_open_questions.md) moves rows from Proposed -> Locked).

| Topic | v0 (tables above) | v1 proposal | Open before freeze |
|-------|-------------------|-------------|--------------------|
| APU `$FE40-$FE5F` | 328P | **Unchanged** (328P kept) | None |
| Board EEPROM `$FE70-$FE72` | AT28C64B parallel | **Removed**. Machine config via **1284 internal EEPROM** handshake (mailbox / new `$FExx` -- TBD) | Exact CPU port + protocol |
| Game saves | Not specified | **Cart I2C EEPROM** (in 32-IC count). Access via HAL (6502 bit-bang or 1284 TWI behind `$FExx` -- TBD) | Port map + HAL API |
| `$FExx` latch silicon | One flag-ish byte per HC573 (14 chips) | **9x HC573**, bit-packed | **Bitfield packing table** must land in this doc before adoption |
| Color PROM encoding | 3x 8-bit R/G/B | 1x `{RRRGGGBB}` | Studio/PROM burn quantize (see Palettes) |
| Worlds / VRAM / MAP / OAM / scroll | As above | **Unchanged** | -- |

**HAL guidance (until ports freeze):** PRG should call thin `machine_eeprom_*` and `cart_save_*` helpers so carts are not welded to AT28C64B or a specific I2C master. APU may keep direct `$FE4x` stores (stable across v0/v1).

**Not changed by v1 BOM alone:** cart flash layout (`.retr01`), 128x120 model, interleaved VRAM, MAP `$FE90`, OAM `$FE20`.
