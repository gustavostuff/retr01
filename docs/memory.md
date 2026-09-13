# Memory and cart image

What lives where, how the CPU sees PRG and MAP, and the on-cart `.retr01` layout.

Adapted from an earlier Retr01 `memory.md`. Cart image rules below are the baseline for this repo. Soft `$7Fxx` owners follow `hardware.md` (3x AVR128DB28).

**Related:** `hardware.md`, `cartridge.md` (physical cart), `video-graphics.md`, `world-scrolling.md`.

## CPU address map

| Range | Backing | Access |
| --- | --- | --- |
| `$0000-$7EFF` | System SRAM | CPU read/write anytime |
| `$7F00-$7FFF` | I/O latches / MCUs | CPU read/write per port |
| `$8000-$FFFF` | Cart flash (PRG) | CPU read (fetch). No write in normal play |

**One flat PRG region:** **32 KB** at `$8000-$FFFF`, including reset/IRQ/NMI vectors at the top. No PRG banking. No I/O hole inside PRG.

The prior project put I/O at `$FE00-$FEFF` and split flash into "PRG low" / "PRG high". **Do not bring that back.** I/O lives in `$7F00-$7FFF` (last page below PRG). That page is not system RAM.

Within the I/O page, keep the same low-byte offsets as the old `$FExx` map where useful (example: old `$FE90` becomes `$7F90`). `$7F80` unused for now.

## Cart flash (512 KB)

| Topic | Detail |
| --- | --- |
| **CPU read** | Contiguous PRG at `$8000-$FFFF`. MAP via seek/data ports (see below). CHR not in the 6502 map |
| **CPU write** | Flash programming only (not runtime gameplay) |
| **CHR read** | Video / helper path by bank+index from flash |
| **MAP read** | 24-bit seek, then data with auto-inc |

One `.retr01` image holds PRG, global palettes, world blobs (CHR + screen MAP + entities), and optional title/credits screens.

## Cart image (`.retr01`)

Magic **`retr01`**, **`format_ver` = 2** (from the prior project). Bump only when the layout breaks old tools.

```text
+------------------------------------------------------------------+
| HEADER 16 B | POINTER TABLE 36 B (u24 offsets + lengths)         |
| GLOBAL PALS 256 B (8 BG rows + 8 SPR rows, indices only)         |
| PRG 32 KB                                                        |
| OTHER SCREENS (title / interstitial / credits, raw or RLE)       |
| WORLD TABLE 8 x 8 B                                              |
| WORLD BLOBS: header, CHR 32 KB, BG1 screen dir + payloads,       |
|   BG0 dir + payloads, entity types/instances, player anim        |
+------------------------------------------------------------------+
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
| 5 | Reserved (legacy ASCII credits, stay 0) |

### World blob (per present world)

| Piece | Size / note |
| --- | --- |
| World header | **32 B** (spawn cell as nibble-packed col/row, default banks/pal row, BG1 present count, BG0 present count, CHR/dir offsets, entity counts, player entity + hitbox, camera dead-zone bytes **30-31**) |
| CHR | **4** BG banks + **4** SPR banks x **4096 B** = **32 KB** total |
| BG1 screen directory | **12 B** per present playfield screen (grid cell + payload offset) |
| BG1 screen payloads | **480 B** each (present only, sparse **16x16**, max **32**/world) |
| BG0 directory | **12 B** per present BG0 screen (same shape as BG1 dir). Offset **0** if none |
| BG0 payloads | **480 B** each (up to **8** present screens, sparse on **16x16**) |
| Entity types / instances | Packed records. Metasprite catalog is Studio-only and flattened here |
| Player anim | Optional `PA` blob when a player entity is marked |

**Grid cell byte:** virtual map is **16x16** (col/row **0-15**). Pack both coords in **1 byte** as nibbles: `col | (row << 4)`. Same packing for BG1/BG0 directory entries and world-header spawn cell.

**World header notes (BG0):** byte **3** packs present BG0 extent (`cols | rows<<4`). Byte **6** is BG0 present count. Bytes **14-16** are BG0 directory offset (u24), or **0** if none.

**Screen payload:** **480 B** = 240 tile bytes + 240 attr bytes (**16x15**, **128x120**). Same shape for BG1 and BG0.

**World caps:** **8** worlds, **32 present BG1 screens**/world on sparse **16x16**, **0..8** BG0 screens/world, **4** BG + **4** sprite CHR banks/world (**256** tiles x **16 B** each bank).

### Flash budget at max fill

Worst case (PRG + header/pals/world table + full unique CHR and max screens for all 8 worlds, sparse dirs, no entity/other blobs yet):

| Item | Bytes | KB |
| --- | ---: | ---: |
| Used | ~452980 | ~442 |
| Free in 512 KB | ~71308 | ~69.6 |

That free window is for **other screens**, **entity / PA blobs**, and padding. About **247** maxed entity defs @ 288 B fit in the free space alone (see `software-api.md`). Real games that leave CHR or screens unused free even more.

### Other screens (global ROM)

Not on the world grid. Read through the MAP port like world data.

| Id | Kind |
| --- | --- |
| **0** | Title |
| **1** | Level interstitial |
| **2+** | Credits pages (0..46 max) |

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
| System RAM `$0000-$7EFF` | 32 KB minus the I/O page (**32512 B**). CPU only. Game state, stacks, helpers |
| VRAM 32 KB interleaved | CPU and video take turns by PHI2 phase. Camera window holds BG1 slots and BG0 slots (see `world-scrolling.md`). CPU data port low bytes follow the old map under `$7Fxx` (was `$FE10`-`$FE12`) |

Exact VRAM slot sizes follow `world-scrolling.md` / video bring-up. Soft port owners are in `hardware.md`.

## Cart save EEPROM

Small I2C EEPROM on the cart for per-game saves. **24C64**, mailbox **`$7F22`-`$7F24`**, MCU-M as I2C master with `RDY` stall. See `hardware.md`.

## Color master table

**AT27C256R** on the motherboard holds the 64 RGB values. Cart global palette planes are indices only. See `hardware.md` and `video-graphics.md`.

## Notes

- Flat contiguous 32 KB PRG at `$8000-$FFFF` (no I/O hole)
- I/O page `$7F00-$7FFF` (old `$FExx` low bytes kept where useful)
- `.retr01` header, pointer table, world blob shape, sparse dirs, other-screens, MAP port
- World caps (8 worlds / 32 BG1 / 0..8 BG0)
- Three AVR128DB28 helpers (MCU-M / S1 / S2) as in `hardware.md`
