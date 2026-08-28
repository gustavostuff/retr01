# retr01 Graphics, Worlds, and Memory

Display, worlds, VRAM, palettes, cart image, and `$FExx`.

**Authority:** this file is the **software-visible** source of truth (see [`01`](01_architecture_overview.md)). Current HW BOM: [`06`](06_hardware_v1_32ic.md) (**32 IC**). HW must not invent CPU ports here. Open mailbox/I2C/bitfield items stay listed as TBD below.

## Display

| Item | Value |
|------|-------|
| Logical | **128x120** (**16x15** tiles). Games/Studio stay here |
| RGBS field | **256x240** inside **341x262** |
| Clocks | CPU **8.000 MHz**, dot **5.369318 MHz** (independent), ~**60.098 Hz** |
| SCALE DIP | **2x** default (fills field). **1x** centers 128x120. Raster only (no `$FExx` / cart bit) |

## Worlds, screens, cart budget

- **7** worlds max (indices **0-6**). Sparse **8x8** grid per world, **32** screens/world (camera / playfield only)
- Per world: up to **2 parallax planes** (same 480 B payload as a screen). **Not** on the world grid. Separate MAP directory. Maps to VRAM slots **4-5**
- Screen / plane payload: **480 B** raw (**240** tiles + **240** attrs). Direct MAP `$FE93` -> VRAM `$FE12` (no RLE required)
- Per world: **4 BG + 4 sprite** CHR banks (**32 KB**), screen dir + parallax dir
- Palettes: **8 global BG palette rows** + **8 global sprite palette rows** (see [Palettes](#palettes))
- Cart: **512 KB** (SST39SF040). **128 KB** PRG in cart image (CPU window still `$8000-$FFFF` with I/O hole, banked via `$FE80`, see PRG). Studio Phase 1 export still emits **32 KB** until cart `format_ver` bump lands.

| Asset | Size at caps (7 worlds) |
|-------|--------------------------|
| CHR (7 x 32 KB) | **224 KB** |
| MAP screens (7 x 32 x 480 B) | **105 KB** |
| MAP parallax (7 x 2 x 480 B) | **~6.6 KB** |
| Global pals (8 BG rows + 8 sprite rows) | **256 B** |
| Dirs / headers | **~3.6 KB** |
| PRG | **128 KB** |
| **Total / free** | **~478 KB** used, **~46 KB** free |

At **32 KB** PRG (current export): **~380 KB** used, **~144 KB** free at the same world caps.

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

**Color PROM** (board): **64 master indices**, packed **`{RRRGGGBB}`** (R3 G3 B2) in **one** PROM/OTP chip, 1-dot pipeline ([`06`](06_hardware_v1_32ic.md)). Active buffer: **4 BG + 4 sprite** via `$FE08`/`$FE09` (indices held in packed HC573 / decode path on the 32-IC board, no separate palette RAM IC). Shared color 0. BG+sprite row index always locked together.

**Cart storage:** **8 BG palette rows** + **8 sprite palette rows**. Each row is **4 palettes x 4 master indices = 16 B**. Totals: **128 B** BG + **128 B** sprite = **256 B**. Software selects a row (`$FE38` hint) and copies it into `$FE08`/`$FE09`.

Kit / Studio **logical** swatches below are full 24-bit reference colors. Studio and burn tools **quantize** to R3G3B2 when building the PROM image ([`04`](04_retr01_studio.md)).

(Earlier board sketches used 3x AT28C16 R/G/B, not the current norm.)

```text
#000000 #290514 #2A0507 #230F06 #1E1306 #1A1605 #141807 #061A07 #051A13 #071918 #08181C #071722 #030B3D #16033A #20052D #260420
#363636 #740A40 #77091A #693512 #5D3F0E #514617 #424C19 #13511A #16503F #114E4D #164D58 #164A66 #163794 #472990 #5F167D #6C115F
#949494 #C04A7A #C54A4D #B8601B #A27326 #8F7E2F #77872D #209030 #2E8E72 #318B89 #1F889C #2483B5 #4D77D7 #7E6AD3 #9D5DBF #B352A0
#FFFFFF #F1A2BB #F1A6A1 #F1A983 #EEAC44 #D4BA33 #B0C841 #73D275 #22D0A6 #3BCDC9 #48C9E4 #88C4ED #A4BDEF #BBB5F1 #D5A9EF #F09BDD
```

**Fallback (software, load time):** cart global rows -> else kit defaults. Bare metal with no `$FE08`/`$FE09` writes = **garbage colors**. Hardware never auto-loads.

## Cart image (`.retr01`)

24-bit offsets. Magic **`retr01`** (lowercase ASCII). **`format_ver` = 1** (frozen in current Studio/Emu/Sim packers). **`format_ver` = 2** (planned): **7** worlds max, **128 KB** PRG, `$FE80` bank register.

```text
+----------------------------------------------------------------+
|  CART HEADER (16 B at offset 0)                                |
|    magic[6]          'r','e','t','r','0','1'                   |
|    format_ver        u8 (= 1)                                  |
|    world_count       u8 (1..7)                                 |
|    flags / reserved  (pad to 16 B)                             |
|  POINTER TABLE (24 B, each field u24)                          |
|    off_prg, len_prg                                            |
|    off_pal_bg, len_pal_bg                                      |
|    off_pal_spr, len_pal_spr                                    |
|    off_world_table, len_world_table                            |
+----------------------------------------------------------------+
| GLOBAL PALETTES                                                |
|    BG:     8 rows x 4 pals x 4 master indices = 128 B          |
|    Sprite: 8 rows x 4 pals x 4 master indices = 128 B          |
|    Active row N: copy 4 BG + 4 sprite pals into $FE08/$FE09     |
+----------------------------------------------------------------+
|  PRG (one global section, max 128 KB, CPU window $8000 + I/O hole $FE00) |
|    Phase 1 export: 32 KB stub. Planned: 128 KB banked (`$FE80`)        |
+----------------------------------------------------------------+
|  WORLD TABLE (7 slots x 8 B)                                   |
|    each slot: present u8, pad u8, off_world u24, len_world u24 |
+----------------------------------------------------------------+
|  WORLD 0 BLOB                                                  |
|  +------------------------------------------------------------+|
|  | WORLD HEADER (32 B)                                        ||
|  |   start_col, start_row, default_bg_bank, default_spr_bank  ||
|  |   default_pal_row (0..7), screen_count (= present count)   ||
|  |   parallax_count (0..2)                                    ||
|  |   off_chr u24, off_screen_dir u24, off_parallax_dir u24    ||
|  |   reserved to 32 B                                         ||
|  +------------------------------------------------------------+|
|  | CHR: BG 0..3 + SPR 0..3 (4 KB each)                        ||
|  | SCREEN DIR: 12 B per present screen                        ||
|  |   col, row, flags0, flags1 (Phase 1: flags = 0)            ||
|  |   off_payload u24, off_screen_meta u24 (0 if unused)       ||
|  | PARALLAX DIR: slot (0..1 -> VRAM 4..5), flags, off_payload ||
|  | SCREEN PAYLOADS: 240 tile + 240 attr each                  ||
|  | PARALLAX PAYLOADS: same 480 B shape (after screens)        ||
|  +------------------------------------------------------------+|
+----------------------------------------------------------------+
|  WORLD 1 .. N                                                  |
+----------------------------------------------------------------+
```

Boot: magic -> pointers -> world header -> screen dir / parallax dir -> `off_payload`. Load grid screens into VRAM slots 0-3. Load parallax dir entries into slots 4-5. MAP port: `$FE90`-`$FE92` addr, `$FE93` data auto-inc.

**Debugging carts:** Studio Play and editor chrome are **not** the cart. Runner helpers differ:

| Runner | MAP / pals into VRAM |
|--------|----------------------|
| **Studio export PRG** | Streams **one** pal row + **start screen** via `$FE93` -> `$FE08`/`$FE09`/`$FE12` |
| **Emulator** | Default: cart PRG pal+start MAP catchup into VRAM (`$FE93`->`$FE12`). Host Play handles camera/player/warps, `sync_camera` reloads a 2x2 workbench during Play. Opt-in host memcpy: `R01E_SOFTBOOT=1` |
| **Board sim** | Default = IC stream from cart PRG. Softboot only if `R01S_SOFTBOOT=1` |

Phase 1 PRG boot streams **one** start screen only (no full 2x2 seam PRG yet). Host Play in emu/sim fills the 2x2 window during preview. Triage ROM vs runner: [`retr01_sim/README.md`](../retr01_sim/README.md#cart-rom-vs-runners-triage).

## CPU map and `$FExx`

Logical CPU addresses below are the software SoT. Silicon uses **9x HC573** with bit-packing ([`06`](06_hardware_v1_32ic.md)). The **bitfield packing table is still TBD** (Q21). Prefer these logical addresses + Zero Page shadows until it lands.

| Range | Region |
|-------|--------|
| `$0000-$7FFF` | System RAM |
| `$8000-$FDFF` | PRG (low image, I/O hole at `$FE00-$FEFF`) |
| `$FE00-$FEFF` | I/O |
| `$FF00-$FFFF` | PRG high + vectors |

PRG planning cap is **128 KB** in the cart image (4 x 32 KB banks). The CPU still sees one **32 KB** window at `$8000-$FFFF` with the `$FE00-$FEFF` I/O hole. **`$FE80`** selects the bank (2-bit latch, flash A16/A17). Vectors read from **bank 0** (decode forces bank 0 for `$FFFA-$FFFF`).

| Addr | Name | Notes |
|------|------|-------|
| `$FE00` | `PPUCTRL` | BG/sprites/NMI enable bits, camera slot mode (**1 / 2H / 2V / 4**, bitfield TBD) |
| `$FE01` | `PPUSTATUS` | VBlank, raster hit (read clears) |
| `$FE02`/`$FE03` | scroll X/Y | 0-127 / 0-119 |
| `$FE04`/`$FE05` | raster Y / IRQ | `$FE04` = compare scanline (latched). `$FE05` = enable/ack/control (**bitfield TBD**) |
| `$FE06`/`$FE07` | plane band | any band locks camera axis for the frame |
| `$FE08`/`$FE09` | pal addr/data | active indices 0-63, auto-inc |
| `$FE10`-`$FE12` | VRAM addr/data | auto-inc |
| `$FE20`/`$FE21` | OAM addr/data | auto-inc. Entry `Y,tile,attr,X` x64 |
| `$FE30` | `WORLD` | 0-6 |
| `$FE31`-`$FE37` | bank helpers | optional stamps, not live fetch |
| `$FE38` | `PAL_ROW` | hint. Still copy `$FE08`/`$FE09` |
| `$FE40`-`$FE5F` | APU | **ATmega328P** |
| `$FE60`/`$FE61` | pads | bits 0-7: right, left, down, up, X, Y, **coin** (cabinet) / **select** (console draft), start (**1=pressed**) |
| `$FE70`-`$FE72` | machine EEPROM | **1284 internal EEPROM** handshake (protocol TBD) |
| `$FE80` | `PRG_BANK` | PRG bank select **0-3** (128 KB PRG). Write-only latch |
| `$FE90`-`$FE93` | MAP | 24-bit seek + read auto-inc |
| (TBD) | cart save | **Cart I2C EEPROM** via HAL (`cart_save_*`). CPU port TBD (Q20) |

**HAL:** PRG should use `machine_eeprom_*` and `cart_save_*` helpers so games are not welded to a specific mailbox layout. APU may use direct `$FE4x` stores.

**Open before shipping carts that need saves / machine config:** freeze mailbox protocol + cart I2C `$FExx` (Q20), and HC573 bitfield packing (Q21) in this doc.
