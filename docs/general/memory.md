# Memory and cart image

What lives where, how the CPU sees PRG and MAP, and the on-cart `.retr01` layout.

Cart image rules below are the baseline for this repo. Soft `$7Fxx` owners follow `hardware.md` (3x AVR128DB28).

**Related:** `hardware.md`, `cartridge.md`, `video-graphics.md`, `palette/`, `world-scrolling.md`, `software-api.md`, `sound.md`, `ic-comms-risks.md`.

Logical regions and caps here are SoT. Pointer-table packing lives with Studio / Emu.

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

Magic **`retr01`**. Byte 6 is `format_ver` **6**. Pointer table names the regions. No mapper. CHR is one global pool.

```text
+======================================================================+
|                         .retr01 CART IMAGE                           |
+======================================================================+
| HEADER  |  POINTER TABLE (PRG, pals, CHR, entities, other, worlds,   |
|         |  BGM)                                                      |
+----------------------------------------------------------------------+
| GLOBAL PALETTES  256 B (128 B BG + 128 B SPR)                        |
|  8 rows x 4 pals x 4 kit indices per plane                           |
+----------------------------------------------------------------------+
| PRG  32 KB                                                           |
|  flat code + vectors at top ($8000-$FFFF window)                     |
|  entity behavior, spawn tables, collision solids                     |
|  BGM boot track index at $80FE (stream itself is not in PRG)         |
+----------------------------------------------------------------------+
| GLOBAL CHR  128 KB                                                   |
|  16 BG banks  |  16 SPR banks                                        |
|  256 tiles x 16 B each bank. Title, playfield, and player share this |
+----------------------------------------------------------------------+
| ENTITY CATALOG  (up to 32 defs, pack in software-api.md)             |
+----------------------------------------------------------------------+
| OTHER SCREENS  (max 16 x 480 B raw or RLE)                           |
|  title / interstitial / credits. Attrs index global CHR              |
+----------------------------------------------------------------------+
| WORLD TABLE  7 x 8 B                                                 |
+----------------------------------------------------------------------+
| WORLD BLOB (x7, present worlds only)                                 |
|   header 32 B                                                        |
|   BG1 dir 12 B x present (max 64)  |  BG1 payloads 480 B each        |
|   BG0 dir 12 B x present (max 16)  |  BG0 payloads 480 B each        |
|   optional PA after world 0 maps only (one per cart, max 1031 B)     |
+----------------------------------------------------------------------+
| BGM  compressed FD/FE/FA bytecode + wavetable ids                    |
|  MAP region. AKWF / DPCM samples stay in MCU-S2 flash, not here      |
+======================================================================+
```

### Pointer table

`(offset, length)` pairs as little-endian **u24**. Regions:

| Region | Size / note |
| --- | --- |
| PRG | **32 KB** |
| Global BG palette plane | **128 B** |
| Global sprite palette plane | **128 B** |
| Global CHR | **128 KB** (16 BG then 16 SPR banks) |
| Entity catalog | Up to **32** defs (see `software-api.md`) |
| Other-screens blob | Max **16** screens |
| World table | **7 x 8 B** |
| BGM blob | Compressed tracker streams (see `sound.md`) |

### World blob (per present world)

| Piece | Size / note |
| --- | --- |
| World header | **32 B** (spawn cell as nibble-packed col/row, BG1/BG0 present counts, flags at byte **7** (bit 0 player anim, bits 1-2 BG0 wrap, bit 3 BG0 clip, bit 4 platformer), camera dead-zone **width/height** at bytes **30-31**) |
| BG1 screen directory | **12 B** per present playfield screen (grid cell + payload offset) |
| BG1 screen payloads | **480 B** each (present only, sparse **16x16**, max **64**/world) |
| BG0 directory | **12 B** per present BG0 screen (same shape as BG1 dir). Offset **0** if none |
| BG0 payloads | **480 B** each (up to **16** present screens, sparse on **16x16**) |

World blobs hold maps. CHR and the entity catalog are cart-global. One optional `PA` (player anim) blob sits after **world 0** maps when a player entity is marked. Other worlds do not copy it. Pack: `software-api.md`.

Entity **spawn locations** live in **PRG** (tables or code calling `spawn_entity`). Defs live in the **global** catalog.

**Grid cell byte:** virtual map is **16x16** (col/row **0-15**). Pack both coords in **1 byte** as nibbles: `col | (row << 4)`. Same packing for BG1/BG0 directory entries and world-header spawn cell.

**World header notes (BG0):** byte **3** packs present BG0 extent (`cols | rows<<4`). Byte **6** is BG0 present count. Bytes **14-16** are BG0 directory offset (u24), or **0** if none. Byte **7** flags: bit0 player-anim blob, bit1 BG0 wrap X, bit2 BG0 wrap Y, bit3 BG0 clip to BG1 (see `world-scrolling.md`), bit4 platformer mode (see `software-api.md`).

**Screen payload:** **480 B** = 240 tile bytes + 240 attr bytes (**16x15**, **128x120**). Same shape for BG1 and BG0. Attr pack in `video-graphics.md`. Bank bits **0-3** index global CHR.

### BGM blob (cart flash)

Compressed tracker bytecode. Not in the 32 KB PRG window. `$80FE` is the 1-based boot track (0 = none). At play, PRG copies that track into system RAM (emu follows the same cart pointer). AKWF cycles and DPCM samples stay in MCU-S2 flash. Opcode details: `sound.md`. Rough play time vs leftover flash is in the budget section below.

| Off | Field |
| --- | --- |
| +0 | `'B' 'G'` |
| +2 | u8 track count (0..8) |
| +3 | u8 **1** = instrument table at +36 |
| +4 | u16 off[8] from blob base |
| +20 | u16 len[8] |
| +36 | ins[8][5] wavetable ids when byte 3 is 1 |
| +76 | FD/FE/FA payloads |

**Caps:** **7** worlds, **64** present BG1 screens/world, **0..16** BG0 screens/world, **16** BG + **16** SPR banks cart-wide, **32** entity types cart-wide.

### Flash budget at max fill

Worst case: all 7 worlds present, every world at 64 BG1 + 16 BG0, all 16+16 CHR packed, 32 maxed entity defs, 16 other screens at raw **480 B**, one maxed `PA` blob, full type directory. RLE and unused slots free more. Spawn locations cost **PRG**, not cart flash. Compressed BGM/SFX bytecode uses the leftover flash (outside PRG).

One maxed world blob (maps only) is **39392 B** (~38.5 KB):
**1 x 32** (header) + **64 x 12** (BG1 dir) + **64 x 480** (BG1 payloads) + **16 x 12** (BG0 dir) + **16 x 480** (BG0 payloads).

| Item | Number of bytes | Kilobytes |
| --- | ---: | ---: |
| Header + pointer table + palettes | **320** | ~0.3 |
| PRG (1 x 32768 B) | **32768** | **32.0** |
| World table (7 worlds x 8 B) | **56** | ~0.1 |
| World headers (7 worlds x 32 B) | **224** | ~0.2 |
| Global CHR (16 BG + 16 SPR banks) | **131072** | **128.0** |
| BG1 directories (7 worlds x 64 screens x 12 B) | **5376** | **~5.3** |
| BG1 payloads (7 worlds x 64 screens x 480 B) | **215040** | **210.0** |
| BG0 directories (7 worlds x 16 screens x 12 B) | **1344** | **~1.3** |
| BG0 payloads (7 worlds x 16 screens x 480 B) | **53760** | **52.5** |
| Entity defs (32 x 1044 B maxed) | **33408** | **~32.6** |
| Entity type directory (32 x u16) | **64** | **~0.1** |
| Player anim `PA` (one cart-wide, 4x8x6 maxed) | **1031** | **~1.0** |
| Other screens (16 screens x 480 B raw) | **7680** | **~7.5** |
| **Used (sum of rows above)** | **482143** | **~470.8** |
| Free (524288 flash - 482143 used) | **42145** | **~41.2** |

Absolute max fill **fits** with ~**41.2 KB** free. That leftover is cart room for compressed BGM (and SFX bytecode). Rough play time at typical tracker tempo: about **15 minutes** of busy 5-channel BGM, about **25 minutes** at a sparser 3-channel density, closer to **8 minutes** if every sixteenth is a unique row. Sparse loops and unused world or CHR slots go further. AKWF wavetables and DPCM samples stay in MCU-S2 flash. Real carts stay further under because entity defs are variable-length (only live sprites), screens/CHR are rarely all filled, and RLE can shrink other screens.

### Global CHR

One cart-wide pattern pool. Playfields, other screens, and the marked player use it.

| Topic | Value |
| --- | --- |
| Banks | **16** BG + **16** SPR, independent pools |
| Size | **128 KB** (32 x 4096 B) |
| Consumers | Playfield nametables, other screens, marked player, inventory icons |
| Addressing | Each cell or sprite names bank **0-15** in its attr byte. See `video-graphics.md` |

The tile or sprite has authority: its attr bank field may call any bank in that plane.

### Player patterns

The marked **player** entity is a normal catalog type. Pixel patterns live in the **global SPR** banks (any of the **16**), same as every other entity.

| Topic | Value |
| --- | --- |
| Pattern home | Global SPR banks (16 x 256 tiles) |
| Catalog | Global entity catalog (`player_entity` index) |
| Bank bits | Part attr bits **0-3** select SPR bank **0-15** |
| Inventory icons | Same global SPR tiles |

### Entity catalog (global, cart flash)

**Definition:** an entity is a game being/object built from up to 4 states x 8 frames x 6 sprites. Full wording and **byte pack format** in `software-api.md`.

One catalog for the cart (up to **32** types). The same type may spawn in any world. Sprite attr bank bits index **global SPR**. A wrong bank index shows the wrong tiles.

| Piece | Lives in |
| --- | --- |
| Entity **definitions** (looks / anim metadata) | Global entity catalog |
| Entity **behavior** (what it does) | **PRG**, authored in **C/ASM** |
| Entity **spawn locations** (who appears where) | **PRG** (tables and/or `spawn_entity` calls) |
| Entity **pixel patterns** | Global SPR CHR |
| Collision solids | **RAM** (bank + tile pattern list, copied from PRG at boot) |
| Player anim (`PA`) | One cart blob after world-0 maps. Play player frames. See `software-api.md` |

| Topic | Value |
| --- | --- |
| Hard cap | **32** entity **types** (catalog). Not an on-screen instance cap |
| On-screen instances | Soft: share **64** hardware sprites (OAM). As many entities as fit that sprite budget. See `software-api.md` |
| Maxed def size (locked pack) | **1044 B** (4 x 8 x 6) |
| Worst case 32 maxed defs | **33408 B** (~32.6 KB) |
| Unique-maxed art pressure | Up to **192** SPR tiles per type if nothing is reused (4 x 8 x 6). Global SPR is **4096** tiles |

Live/on-screen count is OAM-budgeted, not type-capped.

### Other screens (global ROM)

Not on the world grid. Read through the MAP port like world data.

**Hard cap: 16 screens** total (ids **0..15**). Title, interstitial, credits, and any other non-world pages share that one pool. There is no separate credits budget.

Roles are labels on indexes inside the pool (Studio / PRG convention). Example: id **0** = title, id **1** = interstitial, credits = indexes **N..M** within **0..15**.

Payload **480 B** raw or **RLE** (`flags` bit 0). RLE: `C < 0x80` copy `C+1` literals, `C >= 0x80` repeat next byte `C-0x7F` times.

**CHR:** other-screen nametable / attr bank bits index the **same global CHR** as playfields (16 BG + 16 SPR).

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
| `$7F30` | WORLD select (0-7), soft helper |
| `$7F40`-`$7F5F` | APU mailbox (8 voices x 4 regs, MCU-S2). See `sound.md` |
| `$7F60` / `$7F61` | Pad P1 / P2 bitfields |
| `$7F70`-`$7F72` | Machine EEPROM mailbox |
| `$7F90`-`$7F93` | MAP seek + auto-inc data |

`$7F80` stays reserved (light-gun roadmap).

## System RAM and VRAM (summary)

| Topic | Detail |
| --- | --- |
| System RAM `$0000-$7EFF` | 32 KB minus the I/O page (**32512 B**). CPU only. Game state, stacks, helpers |
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
| `+$00E0` | `$80E0` | Boot MAP (16 B): pal_bg u24, pal_spr u24, start screen u24, BGM u24, world0 u24, reserved |
| `+$0100` | `$8100` | Present-screen bitmasks (32 B, 16x16 grid) |
| `+$0120` | `$8120` | Spawn cell (`col | row<<4`) |
| `+$0121` | `$8121` | Collision dir count (legacy linear dir, first present screens) |
| `+$0122` | `$8122` | Collision dir entries (stops before `$81C0`) |
| `+$0500` | `$8500` | Collision grid: 16×16 little-endian u16 probe-table addresses (0 = no screen). Play uses this, not the linear dir |
| `+$0700` | `$8700` | Solid pattern list: u8 count, then `count` x (bank, tile). Packed from project JSON `solid_patterns` (Studio Set Solid). Boot copies to RAM `$0200`. Probe tables follow |
| `+$01C0` | `$81C0` | Instance count (u8, max **64**) |
| `+$01C1` | `$81C1` | Instance table (`count` x **6 B**, pack below) |
| `+$00F0` | `$80F0` | `R01P` marker + version byte |
| `+$00F7` | `$80F7` | Platformer gravity (u8, 1/16 px per frame^2, **0** = `R01_PLAT_GRAVITY_DEFAULT`) |
| `+$00F8` | `$80F8` | Platformer jump impulse (u8, **0** = `R01_PLAT_JUMP_DEFAULT`) |
| `+$00F9` | `$80F9` | Platformer meter px (u8, **0** = `R01_PLAT_METER_DEFAULT`) |
| `+$00FA` | `$80FA` | Crouch state index (u8, **$FF** = unmapped) |
| `+$00FB` | `$80FB` | Idle state index (u8, **$FF** = unmapped, freeze state 0 frame 0) |
| `+$00FC` | `$80FC` | Walk state index (u8, **$FF** = unmapped) |
| `+$00FD` | `$80FD` | Jump state index (u8, **$FF** = unmapped) |
| `+$00FE` | `$80FE` | BGM boot track (u8, **0** = none, **1..8** = track). Stream bytes live in the cart BGM region, not in PRG |

**Spawn instance (6 B, little-endian):** a placed copy of a catalog type (who, facing, world XY). Live pose and the player-anim (`PA`) blob: `software-api.md`.

| Off | Size | Field |
| --- | ---: | --- |
| 0 | 1 | `type_id` (global catalog index) |
| 1 | 1 | flags: bit **0** flip H, bit **1** flip V |
| 2 | 2 | `world_x` |
| 4 | 2 | `world_y` |

Table grows toward the collision grid at `$8500`. **64** records need **384 B** and fit. The grid is **512 B** (`$8500`–`$86FF`); probe tables start at `$8700`.

Probe tables are **240 B** per present screen (max **64**). With a full solid-pattern list they end by `$C381`. llvm-mos C occupies `$C400`–`$FFF9`.

Full entity defs use the locked pack in `software-api.md` (type directory + EntityDefs). Collision solids are a bank+tile list in system RAM (copied from PRG `$8700` at boot). Studio Set Solid stores `solid_patterns` in the project JSON. `r01_solid_pattern_add` in `game_logic.c` is the author API.

## Notes

- Flat contiguous 32 KB PRG at `$8000-$FFFF` (no I/O hole)
- I/O page `$7F00-$7FFF`
- World caps: **7** worlds / 64 BG1 / 0..16 BG0
- CHR: **16** BG + **16** SPR banks, cart-global, **128 KB**
- Other screens: max **16** (shared pool), same global CHR
- Entity types: **32** global (4 states x 8 frames x 6 sprites)
- Marked player: global SPR banks
