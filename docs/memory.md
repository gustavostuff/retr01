# Memory and cart image

What lives where, how the CPU sees PRG and MAP, and the on-cart `.retr01` layout.

Cart image rules below are the baseline for this repo. Soft `$7Fxx` owners follow `hardware.md` (3x AVR128DB28).

**Related:** `hardware.md`, `cartridge.md`, `video-graphics.md`, `world-scrolling.md`, `software-api.md`.

## CPU address map

| Range | Backing | Access |
| --- | --- | --- |
| `$0000-$7EFF` | System SRAM | CPU read/write anytime |
| `$7F00-$7FFF` | I/O latches / MCUs | CPU read/write per port |
| `$8000-$FFFF` | Cart flash (PRG) | CPU read (fetch). No write in normal play |

**One flat PRG region:** **32 KB** at `$8000-$FFFF`, including reset/IRQ/NMI vectors at the top. No PRG banking. No I/O "hole" inside PRG.

I/O lives in `$7F00-$7FFF` (the 256-byte page immediately before PRG at `$8000`). That page is not system RAM. `$7F80` is reserved for the light-gun roadmap.

## Cart flash (512 KB)

| Topic | Detail |
| --- | --- |
| **CPU read** | Contiguous PRG at `$8000-$FFFF`. MAP via seek/data ports (see below). CHR not in the 6502 map |
| **CPU write** | Flash programming via the **console** program path only (not during gameplay) |
| **CHR read** | Video / helper path by bank+index from flash |
| **MAP read** | 24-bit seek, then data with auto-inc |

## Cart image (`.retr01`)

Magic **`retr01`**, **`format_ver` = 2**. Bump only when the layout breaks old tools.

```text
+======================================================================+
|                         .retr01 CART IMAGE                           |
+======================================================================+
| +----------------------+  +----------------------------------------+ |
| | HEADER          16 B |  | POINTER TABLE                     36 B | |
| | magic, format_ver,   |  | slots: PRG, BG pals, SPR pals,         | |
| | flags, ...           |  |        worlds, other screens,          | |
| +----------------------+  |        entity catalog                  | |
|                           +----------------------------------------+ |
+----------------------------------------------------------------------+
| +------------------------------------------------------------------+ |
| | GLOBAL PALETTES                                           256 B  | |
| |  +---------------------------+  +------------------------------+ | |
| |  | BG plane            128 B |  | Sprite plane           128 B | | |
| |  | 8 rows x 4 pals x 4 idx   |  | 8 rows x 4 pals x 4 idx      | | |
| |  +---------------------------+  +------------------------------+ | |
| +------------------------------------------------------------------+ |
+----------------------------------------------------------------------+
| +------------------------------------------------------------------+ |
| | PRG                                                        32 KB | |
| | flat code + vectors at top ($8000-$FFFF window)                  | |
| | entity *behavior* lives here (C/ASM)                             | |
| +------------------------------------------------------------------+ |
+----------------------------------------------------------------------+
| +------------------------------------------------------------------+ |
| | OTHER SCREENS (global, max 16 total)                             | |
| |  title / interstitial / credits share one pool (ids 0..15)       | |
| |  each payload 480 B raw or RLE                                   | |
| +------------------------------------------------------------------+ |
+----------------------------------------------------------------------+
| +------------------------------------------------------------------+ |
| | GLOBAL ENTITY CATALOG                         up to 128 types    | |
| | defs only (states/frames/sprites). Shared by all worlds          | |
| | pack format in software-api.md                                   | |
| +------------------------------------------------------------------+ |
+----------------------------------------------------------------------+
| +------------------------------------------------------------------+ |
| | WORLD TABLE                                              8 x 8 B | |
| | one directory row per world slot (present flag + blob offset)    | |
| +------------------------------------------------------------------+ |
+----------------------------------------------------------------------+
| +--------------------------- WORLD BLOB (per present world) -------+ |
| | +--------------------+                                           | |
| | | World header  32 B |  spawn, banks, counts, offsets, camera    | |
| | +--------------------+                                           | |
| | +--------------------------------------------------------------+ | |
| | | CHR                                                   32 KB  | | |
| | | 4 BG banks + 4 SPR banks (256 tiles x 16 B each bank)        | | |
| | +--------------------------------------------------------------+ | |
| | +------------------+  +----------------------------------------+ | |
| | | BG1 directory    |  | BG1 screen payloads (480 B each)       | | |
| | | 12 B x present   |  | up to 32 present screens               | | |
| | +------------------+  +----------------------------------------+ | |
| | +------------------+  +----------------------------------------+ | |
| | | BG0 directory    |  | BG0 screen payloads (480 B each)       | | |
| | | 12 B x present   |  | up to 8 present screens                | | |
| | +------------------+  +----------------------------------------+ | |
| | +--------------------------------------------------------------+ | |
| | | optional PA blob (player anim), if used                      | | |
| | +--------------------------------------------------------------+ | |
| +------------------------------------------------------------------+ |
| ... up to 8 world blobs ...                                          |
+======================================================================+
```

### Pointer table (36 B)

Six `(offset, length)` pairs as little-endian **u24** (3+3 bytes each):

| Slot | Points at |
| --- | --- |
| 0 | PRG (**32 KB**) |
| 1 | Global BG palette plane (**128 B**) |
| 2 | Global sprite palette plane (**128 B**) |
| 3 | World table (**64 B**) |
| 4 | Other-screens blob |
| 5 | Global **entity catalog** (up to **128** defs) |

### World blob (per present world)

| Piece | Size / note |
| --- | --- |
| World header | **32 B** (spawn cell as nibble-packed col/row, default banks/pal row, BG1 present count, BG0 present count, CHR/dir offsets, player entity + hitbox, camera dead-zone bytes **30-31**) |
| CHR | **4** BG banks + **4** SPR banks x **4096 B** = **32 KB** total |
| BG1 screen directory | **12 B** per present playfield screen (grid cell + payload offset) |
| BG1 screen payloads | **480 B** each (present only, sparse **16x16**, max **32**/world) |
| BG0 directory | **12 B** per present BG0 screen (same shape as BG1 dir). Offset **0** if none |
| BG0 payloads | **480 B** each (up to **8** present screens, sparse on **16x16**) |
| Player anim | Optional `PA` blob when a player entity is marked |

Entity **spawn locations** are **not** on the cart. PRG owns who appears where (tables or code calling `spawn_entity`). Defs stay in the global catalog.

**Grid cell byte:** virtual map is **16x16** (col/row **0-15**). Pack both coords in **1 byte** as nibbles: `col | (row << 4)`. Same packing for BG1/BG0 directory entries and world-header spawn cell.

**World header notes (BG0):** byte **3** packs present BG0 extent (`cols | rows<<4`). Byte **6** is BG0 present count. Bytes **14-16** are BG0 directory offset (u24), or **0** if none.

**Screen payload:** **480 B** = 240 tile bytes + 240 attr bytes (**16x15**, **128x120**). Same shape for BG1 and BG0.

**World caps:** **8** worlds, **32 present BG1 screens**/world, **0..8** BG0 screens/world, **4** BG + **4** sprite CHR banks/world. No per-world entity-type cap (types are global).

### Flash budget at max fill

Worst case: all fixed image pieces + **8** worlds at full CHR and max present screens (sparse dirs) + **128** fully maxed entity defs + **16** other screens at raw **480 B** each. RLE and unused other-screen slots free more. Spawn locations cost **PRG**, not cart flash.

One maxed world blob (no `PA`) is **52480 B** (~51.2 KB):
**1 x 32** (header) + **1 x 32768** (CHR) + **32 x 12** (BG1 dir) + **32 x 480** (BG1 payloads) + **8 x 12** (BG0 dir) + **8 x 480** (BG0 payloads).

| Item | Number of bytes | Kilobytes |
| --- | ---: | ---: |
| Header (1 x 16 B) | **16** | ~0.0 |
| Pointer table (6 slots x 6 B) | **36** | ~0.0 |
| Global BG palettes (1 plane x 128 B) | **128** | ~0.1 |
| Global sprite palettes (1 plane x 128 B) | **128** | ~0.1 |
| PRG (1 x 32768 B) | **32768** | **32.0** |
| World table (8 worlds x 8 B) | **64** | ~0.1 |
| World headers (8 worlds x 32 B) | **256** | ~0.3 |
| CHR (8 worlds x 32768 B) | **262144** | **256.0** |
| BG1 directories (8 worlds x 32 screens x 12 B) | **3072** | **3.0** |
| BG1 payloads (8 worlds x 32 screens x 480 B) | **122880** | **120.0** |
| BG0 directories (8 worlds x 8 screens x 12 B) | **768** | ~0.8 |
| BG0 payloads (8 worlds x 8 screens x 480 B) | **30720** | **30.0** |
| Entity catalog (128 defs x 356 B maxed) | **45568** | **~44.5** |
| Other screens (16 screens x 480 B raw) | **7680** | **~7.5** |
| **Used (sum of rows above)** | **506228** | **~494.4** |
| Free (524288 flash - 506228 used) | **18060** | **~17.6** |

That free slice is for optional `PA`, packing slack, and anything else that does not fit the capped blobs above.

### Entity catalog (global, cart flash)

**Definition:** an entity is a game being/object built from up to 4 states x 4 frames x 4 sprites. Full wording and **byte pack format** in `software-api.md`.

One **global catalog** for the whole cart (pointer table slot **5**). Worlds do **not** own their own type lists. Any world can spawn any catalog entry, so enemies and props can be shared. World 1 uses A/B/C, world 2 uses C/D/E (for instance).

| Piece | Lives in |
| --- | --- |
| Entity **definitions** (looks / anim metadata) | Global entity catalog (cart-wide) |
| Entity **behavior** (what it does) | **PRG**, authored in **C/ASM** |
| Entity **spawn locations** (who appears where) | **PRG** (tables and/or `spawn_entity` calls). Not packed in the world blob |

| Topic | Value |
| --- | --- |
| Hard cap (global / cart) | **128** entity types |
| Per-world type cap | **None** (reference any global id) |
| Maxed def size (locked pack) | **356 B** |
| Worst case 128 maxed defs | **45568 B** (~44.5 KB) |
| CHR zero-reuse unique-maxed / world | Soft art pressure ~**16** (1024 sprite tiles / 64 slots). Not a catalog cap |

Studio: author up to **128** types once, then pick which to place in each world.

### Other screens (global ROM)

Not on the world grid. Read through the MAP port like world data.

**Hard cap: 16 screens** total (ids **0..15**). Title, interstitial, credits, and any other non-world pages share that one pool. There is no separate credits budget.

Roles are labels on indexes inside the pool (Studio / PRG convention). Example: id **0** = title, id **1** = interstitial, credits = indexes **N..M** within **0..15**. Credits do not need a large contiguous run beyond what you allocate inside the 16.

Payload **480 B** raw or **RLE** (`flags` bit 0). RLE: `C < 0x80` copy `C+1` literals, `C >= 0x80` repeat next byte `C-0x7F` times.

### MAP port

| Addr | Role |
| --- | --- |
| `$7F90`-`$7F92` | 24-bit seek into cart flash (lo, mid, hi) |
| `$7F93` | Read data, auto-inc |

Typical boot: seek palette + start MAP via `$7F90`-`$7F93`, copy an active palette row into the palette ports, stream the start screen into VRAM.

## System RAM and VRAM (summary)

| Topic | Detail |
| --- | --- |
| System RAM `$0000-$7EFF` | 32 KB minus the I/O page (**32512 B**). CPU only. Game state, stacks, helpers, anim-tile delay setting |
| VRAM 32 KB interleaved | CPU and video take turns by PHI2 phase. Camera window holds BG1 slots and BG0 slots (see `world-scrolling.md`). CPU data port `$7F10`-`$7F12` |

Soft port owners are in `hardware.md`.

## Cart save EEPROM

Small I2C EEPROM on the cart for per-game saves. **24C64**, mailbox **`$7F22`-`$7F24`**, MCU-M as I2C master with `RDY` stall. See `hardware.md`.

## Color master table

**AT27C256R** on the motherboard holds the 64 RGB values. Cart global palette planes are indices only. See `hardware.md` and `video-graphics.md`.

## Notes

- Flat contiguous 32 KB PRG at `$8000-$FFFF` (no I/O hole)
- I/O page `$7F00-$7FFF`
- World caps: 8 worlds / 32 BG1 / 0..8 BG0
- Global entity catalog: **128** types, shared across worlds
- Other screens: max **16** (shared pool)
- Console programs the cart (and can program AVRs). Details TBD in `hardware.md`
