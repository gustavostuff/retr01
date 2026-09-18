# Memory and cart image

What lives where, how the CPU sees PRG and MAP, and the on-cart `.retr01` layout.

Cart image rules below are the baseline for this repo. Soft `$7Fxx` owners follow `hardware.md` (3x AVR128DB28).

**Related:** `hardware.md`, `cartridge.md`, `video-graphics.md`, `palette/`, `world-scrolling.md`, `software-api.md`, `ic-comms-risks.md`.

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

Magic **`retr01`**, **`format_ver` = 4**. Bump only when the layout breaks old tools.

```text
+======================================================================+
|                         .retr01 CART IMAGE                           |
+======================================================================+
| +----------------------+  +----------------------------------------+ |
| | HEADER          16 B |  | POINTER TABLE                     36 B | |
| | magic, format_ver,   |  | slots: PRG, BG pals, SPR pals,         | |
| | flags, ...           |  |        worlds, other screens,          | |
| |                      |  |        global other CHR                | |
| +----------------------+  +----------------------------------------+ |
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
| | GLOBAL OTHER CHR                                           32 KB | |
| |  4 BG banks + 4 SPR banks (256 tiles x 16 B each bank)           | |
| |  Used by other screens (title / interstitial / credits / UI)     | |
| +------------------------------------------------------------------+ |
+----------------------------------------------------------------------+
| +------------------------------------------------------------------+ |
| | OTHER SCREENS (global, max 16 total)                             | |
| |  title / interstitial / credits share one pool (ids 0..15)       | |
| |  each payload 480 B raw or RLE                                   | |
| |  nametable attrs index global other CHR (not world CHR)          | |
| +------------------------------------------------------------------+ |
+----------------------------------------------------------------------+
| +------------------------------------------------------------------+ |
| | WORLD TABLE                                              7 x 8 B | |
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
| | | ENTITY CATALOG                              up to 16 types   | | |
| | | defs only (states/frames/sprites). This world's SPR CHR      | | |
| | | pack format in software-api.md                               | | |
| | +--------------------------------------------------------------+ | |
| | +--------------------------------------------------------------+ | |
| | | optional PA blob (player anim), if used                      | | |
| | +--------------------------------------------------------------+ | |
| +------------------------------------------------------------------+ |
| ... up to 7 world blobs ...                                          |
+======================================================================+
```

### Pointer table (36 B)

Six `(offset, length)` pairs as little-endian **u24** (3+3 bytes each):

| Slot | Points at |
| --- | --- |
| 0 | PRG (**32 KB**) |
| 1 | Global BG palette plane (**128 B**) |
| 2 | Global sprite palette plane (**128 B**) |
| 3 | World table (**56 B**) |
| 4 | Other-screens blob |
| 5 | Global other CHR (**32 KB**: 4 BG + 4 SPR banks) |

### World blob (per present world)

| Piece | Size / note |
| --- | --- |
| World header | **32 B** (spawn cell as nibble-packed col/row, default banks/pal row, BG1/BG0 present counts, flags at byte **7**, entity type count **0..16**, CHR/dir/entity-catalog offsets, player entity + hitbox, camera dead-zone **width/height** at bytes **30-31**) |
| CHR | **4** BG banks + **4** SPR banks x **4096 B** = **32 KB** total |
| BG1 screen directory | **12 B** per present playfield screen (grid cell + payload offset) |
| BG1 screen payloads | **480 B** each (present only, sparse **16x16**, max **32**/world) |
| BG0 directory | **12 B** per present BG0 screen (same shape as BG1 dir). Offset **0** if none |
| BG0 payloads | **480 B** each (up to **8** present screens, sparse on **16x16**) |
| Entity catalog | Up to **16** defs for this world only (pack in `software-api.md`). Offset **0** if none |
| Player anim | Optional `PA` blob when a player entity is marked |

Entity **spawn locations** are **not** on the cart. PRG owns who appears where (tables or code calling `spawn_entity`). Defs live in the **per-world** catalog inside the world blob.

**Grid cell byte:** virtual map is **16x16** (col/row **0-15**). Pack both coords in **1 byte** as nibbles: `col | (row << 4)`. Same packing for BG1/BG0 directory entries and world-header spawn cell.

**World header notes (BG0):** byte **3** packs present BG0 extent (`cols | rows<<4`). Byte **6** is BG0 present count. Bytes **14-16** are BG0 directory offset (u24), or **0** if none. Byte **7** flags: bit0 player-anim blob, bit1 BG0 wrap X, bit2 BG0 wrap Y, bit3 BG0 clip to BG1 (see `world-scrolling.md`), bit4 platformer mode (see `software-api.md`).

**Screen payload:** **480 B** = 240 tile bytes + 240 attr bytes (**16x15**, **128x120**). Same shape for BG1 and BG0.

**World caps:** **7** worlds, **32 present BG1 screens**/world, **0..8** BG0 screens/world, **4** BG + **4** sprite CHR banks/world, **16** entity types/world.

### Flash budget at max fill

Worst case: all fixed image pieces + **global other CHR** (**32 KB**) + **7** worlds at full CHR, max present screens (sparse dirs), and **16** fully maxed entity defs each + **16** other screens at raw **480 B** each. RLE and unused slots free more. Spawn locations cost **PRG**, not cart flash. There is **no** dedicated player CHR bank: the marked player uses the **global other SPR** banks.

One maxed world blob (no `PA`) is **60992 B** (~59.6 KB):
**1 x 32** (header) + **1 x 32768** (CHR) + **32 x 12** (BG1 dir) + **32 x 480** (BG1 payloads) + **8 x 12** (BG0 dir) + **8 x 480** (BG0 payloads) + **16 x 532** (entity defs).

| Item | Number of bytes | Kilobytes |
| --- | ---: | ---: |
| Header (1 x 16 B) | **16** | ~0.0 |
| Pointer table (6 slots x 6 B) | **36** | ~0.0 |
| Global BG palettes (1 plane x 128 B) | **128** | ~0.1 |
| Global sprite palettes (1 plane x 128 B) | **128** | ~0.1 |
| PRG (1 x 32768 B) | **32768** | **32.0** |
| Global other CHR (4 BG + 4 SPR banks) | **32768** | **32.0** |
| World table (7 worlds x 8 B) | **56** | ~0.1 |
| World headers (7 worlds x 32 B) | **224** | ~0.2 |
| World CHR (7 worlds x 32768 B) | **229376** | **224.0** |
| BG1 directories (7 worlds x 32 screens x 12 B) | **2688** | **~2.6** |
| BG1 payloads (7 worlds x 32 screens x 480 B) | **107520** | **105.0** |
| BG0 directories (7 worlds x 8 screens x 12 B) | **672** | ~0.7 |
| BG0 payloads (7 worlds x 8 screens x 480 B) | **26880** | **~26.3** |
| Entity defs (7 worlds x 16 defs x 532 B maxed) | **59584** | **~58.2** |
| Other screens (16 screens x 480 B raw) | **7680** | **~7.5** |
| **Used (sum of rows above)** | **500524** | **~488.8** |
| Free (524288 flash - 500524 used) | **23764** | **~23.2** |

Absolute max fill **fits** with ~**23.2 KB** free. Real carts stay further under because entity defs are variable-length (only live sprites), screens/CHR are rarely all filled, and RLE can shrink other screens. Optional `PA` and the per-world type directory (`u16` x type count, up to **+32 B**/world) are also outside the table above.

### Global other CHR

Cart-global pattern banks for **other screens** (title, interstitial, credits, menus) and for the **marked player** (and inventory icons). Separate from per-world CHR.

| Topic | Value |
| --- | --- |
| Banks | **4** BG + **4** SPR (same shape as one world CHR block) |
| Size | **32 KB** |
| Consumers | Other-screen nametable / attr bank bits. Marked **player** entity parts use the **SPR** banks (author picks among the 4). Not world playfield BG |
| Flash layout | Pointer-table slot **5**. Counted in the max-fill table |

### Player patterns (global SPR)

The marked **player** entity is a normal catalog type in **world 0** (Studio **World 1**). Pixel patterns live in the cart **global other SPR** banks (one or more of the **4**), not in a private fifth bank and not in world CHR.

| Topic | Value |
| --- | --- |
| Pattern home | Global other CHR **SPR** banks (4 x 256 tiles) |
| Catalog | World **0** entity catalog (`player_entity` index) |
| Bank bits | Part bank **0..3** select which global SPR bank (same encoding as world SPR, different CHR base at play/title time) |
| Inventory icons | Same global SPR tiles (or a reserved range). No separate flash region |
| Other worlds | May mark a local player for playtests. Cart SoT player art is global SPR |

### Entity catalog (per world, cart flash)

**Definition:** an entity is a game being/object built from up to 4 states x 4 frames x 6 sprites. Full wording and **byte pack format** in `software-api.md`.

Each world blob owns its own catalog (up to **16** types). Types are **not** shared across worlds. Reuse the same enemy look in world 2 by packing another def (and tiles) there. For normal entities, sprite attr bank bits mean this world's SPR banks. Wrong world CHR loaded = wrong pixels (the intentional glitch tell). The marked **player** uses the same 0..3 field against **global other SPR**.

| Piece | Lives in |
| --- | --- |
| Entity **definitions** (looks / anim metadata) | Per-world entity catalog (inside that world blob) |
| Entity **behavior** (what it does) | **PRG**, authored in **C/ASM** |
| Entity **spawn locations** (who appears where) | **PRG** (tables and/or `spawn_entity` calls). Not packed in the world blob |
| Entity **pixel patterns** | This world's SPR CHR |

| Topic | Value |
| --- | --- |
| Hard cap (per world) | **16** entity **types** (catalog). Not an on-screen instance cap |
| On-screen instances | Soft: share **64** hardware sprites (OAM). As many entities as fit that sprite budget. See `software-api.md` |
| Global / cart type pool | **None** (no shared catalog) |
| Maxed def size (locked pack) | **532 B** |
| Worst case 7 worlds x 16 maxed defs | **59584 B** (~58.2 KB) |
| CHR zero-reuse unique-maxed / world | Soft art pressure ~**10** (1024 sprite tiles / 96 slots). Below the type cap when every type is fully unique-tiled |

Studio: author up to **16** types per world, with art in that world's SPR banks. Live/on-screen count is OAM-budgeted, not type-capped.

### Other screens (global ROM)

Not on the world grid. Read through the MAP port like world data.

**Hard cap: 16 screens** total (ids **0..15**). Title, interstitial, credits, and any other non-world pages share that one pool. There is no separate credits budget.

Roles are labels on indexes inside the pool (Studio / PRG convention). Example: id **0** = title, id **1** = interstitial, credits = indexes **N..M** within **0..15**.

Payload **480 B** raw or **RLE** (`flags` bit 0). RLE: `C < 0x80` copy `C+1` literals, `C >= 0x80` repeat next byte `C-0x7F` times.

**CHR:** other-screen nametable / attr bank bits index the **global other CHR** banks (4 BG + 4 SPR). Showing an other screen selects that CHR set, not a world blob's CHR.

### MAP port

| Addr | Role |
| --- | --- |
| `$7F90`-`$7F92` | 24-bit seek into cart flash (lo, mid, hi) |
| `$7F93` | Read data, auto-inc |

Typical boot: seek palette + start MAP via `$7F90`-`$7F93`, copy an active palette row into the palette ports, stream the start screen into VRAM (`$7F10`-`$7F12`).

## Soft `$7Fxx` map (summary)

Owners and timing live in `hardware.md` / `ic-comms-risks.md`. Port roles:

| Addr | Role |
| --- | --- |
| `$7F00` | PPUCTRL (layer enables, NMI enable) |
| `$7F01` | PPUSTATUS (VBlank bit, and so on) |
| `$7F02` / `$7F03` | BG1 scroll X / Y (hard latches) |
| `$7F04` / `$7F05` | Raster compare / ctrl |
| `$7F06` / `$7F07` | BG0 scroll X / Y |
| `$7F08` / `$7F09` | PAL_ROW / PAL_DATA (see `video-graphics.md`) |
| `$7F10`-`$7F12` | VRAM addr lo/hi + data (PHI2 high) |
| `$7F20` / `$7F21` | OAM addr / data (**64** sprites x 4 B) |
| `$7F22`-`$7F24` | Cart save EEPROM mailbox |
| `$7F30` | WORLD select (0-6), soft helper |
| `$7F40`-`$7F5F` | APU mailbox (8 voices x 4 regs, MCU-S2) |
| `$7F60` / `$7F61` | Pad P1 / P2 bitfields |
| `$7F70`-`$7F72` | Machine EEPROM mailbox |
| `$7F90`-`$7F93` | MAP seek + auto-inc data |

`$7F80` stays reserved (light-gun roadmap).

## System RAM and VRAM (summary)

| Topic | Detail |
| --- | --- |
| System RAM `$0000-$7EFF` | 32 KB minus the I/O page (**32512 B**). CPU only. Game state, stacks, helpers, anim-tile delay setting |
| VRAM 32 KB interleaved | CPU and video take turns by PHI2 phase. Camera window holds BG1 slots and BG0 slots (see `world-scrolling.md`). CPU data port `$7F10`-`$7F12` |

Soft port owners are in `hardware.md`.

## Cart save EEPROM

Small I2C EEPROM on the cart for per-game saves. **24C64**, mailbox **`$7F22`-`$7F24`**, MCU-M as I2C master.

**Rules (locked):**

- Saves are **explicit** PRG calls (not background during physics). Typical UI: pause, fade, or a dedicated saving screen.
- **Multi-frame / multi-VBlank saves are OK.** A 24C64 page program is milliseconds. Full saves naturally take more than one frame.
- MCU-M uses **short** `CPU_RDY` pulses for mailbox / page handoff (with fail-safe timeout), then **releases** so the 6502 can keep running.
- The picture stays live for the whole save. Beam keeps scanning. PRG may update a spinner or other saving UI every frame while the save state machine advances.
- Machine / cabinet config stays on MCU-M internal EEPROM (`$7F70`-`$7F72`). It must not depend on a seated cart.
- Series **33 ohm** on SDA/SCL (see `hardware.md`).

See `hardware.md` and `ic-comms-risks.md`.

## Color master table

**AT27C256R** on the motherboard holds the **locked 64-color kit** (packed R3G3B2). Cart global palette planes are **indices only** into that kit. Preview RGB SoT: `apps/common/r01_kit_palette.c`. List, tool palettes, and regenerate script: [`palette/`](palette/README.md). `$7F08`/`$7F09` fill rules and `_prom.bin` burn note: `video-graphics.md`.

## Phase 1 PRG play tables (Studio / Emu)

Authoring spawns live in the project JSON. Packed carts put **placements in PRG**, not the world blob (see entity catalog above). Phase 1 PRG layout (CPU `$8000` = PRG+$0000):

| PRG off | CPU | Role |
| --- | --- | --- |
| `+$0100` | `$8100` | Present-screen bitmasks (32 B) |
| `+$0120` | `$8120` | Spawn cell (`col | row<<4`) |
| `+$0121` | `$8121` | Collision dir count |
| `+$0122` | `$8122` | Collision dir entries |
| `+$01C0` | `$81C0` | Instance count (u8) |
| `+$01C1` | `$81C1` | Instance table (`count` x 6 B: type, flip flags, world_x/y LE) |
| `+$00F0` | `$80F0` | `R01P` marker + version byte |
| `+$00F7` | `$80F7` | Platformer gravity (u8, 1/16 px per frame^2, **0** = `R01_PLAT_GRAVITY_DEFAULT`) |
| `+$00F8` | `$80F8` | Platformer jump impulse (u8, **0** = `R01_PLAT_JUMP_DEFAULT`) |
| `+$00F9` | `$80F9` | Platformer meter px (u8, **0** = `R01_PLAT_METER_DEFAULT`) |
| `+$00FA` | `$80FA` | Crouch state index (u8, **$FF** = unmapped) |
| `+$00FB` | `$80FB` | Idle state index (u8, **$FF** = unmapped; freeze state 0 frame 0) |
| `+$00FC` | `$80FC` | Walk state index (u8, **$FF** = unmapped) |
| `+$00FD` | `$80FD` | Jump state index (u8, **$FF** = unmapped) |

Full entity defs use the locked pack in `software-api.md` (type directory + EntityDefs at `OFF_TYPES`).

## Notes

- Flat contiguous 32 KB PRG at `$8000-$FFFF` (no I/O hole)
- I/O page `$7F00-$7FFF`
- World caps: **7** worlds / 32 BG1 / 0..8 BG0 / **16** entity types per world
- Other screens: max **16** (shared pool), CHR from **global other** banks
- Global other CHR: **4** BG + **4** SPR (**32 KB**). Other screens + marked player SPR
- Marked player: global other **SPR** banks (not a private bank, not world CHR)
