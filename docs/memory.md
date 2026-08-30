# Retr01 Memory

What each storage chip holds, who reads it, and when. Cart layout lives here too.

**Related:** [`graphics.md`](graphics.md) (VRAM slots, palettes). [`hardware.md`](hardware.md) (BOM, PCB paths). Register text for `$FExx`: [`graphics.md`](graphics.md).

---

## CPU address map

| Range | Backing | Access |
|-------|---------|--------|
| `$0000-$7FFF` | System SRAM | CPU read/write anytime |
| `$8000-$FDFF` | Cart flash (PRG low) | CPU read (fetch). No write in normal play |
| `$FE00-$FEFF` | I/O latches / MCUs | CPU read/write per port |
| `$FF00-$FFFF` | Cart flash (PRG high + vectors) | CPU read |

PRG is **32 KB** mapped at `$8000-$FFFF` with an I/O hole at `$FE00-$FEFF`. No PRG banking. `$FE80` unused.

---

## On-board SRAM (3x AS6C62256)

### System RAM (`$0000-$7FFF`)

| | |
|--|--|
| **Who writes** | W65C02S only |
| **Who reads** | W65C02S only |
| **When** | Any CPU cycle. Never touched by video or AVRs |
| **Typical use** | Entity state, scroll helpers, parallax slice tables, stacks, ZP shadows |

### VRAM (32 KB, interleaved)

| | |
|--|--|
| **CPU port** | `$FE10` addr hi, `$FE11` addr lo, `$FE12` data (auto-inc) |
| **CPU writes** | PHI2 high (CPU phase) via HC157 mux + PLD `/OE` |
| **PPU reads** | PHI2 low (PPU phase): beam/BG fetch owns address and data |
| **Layout** | Slots 0-3 camera (512 B each), 4-5 parallax (512 B each), rest scratch/reserved ([`graphics.md`](graphics.md)) |
| **Tear rule** | Do not poke a cell the beam is fetching. Off-screen slots and VBlank are safe |

**How interleave works:** one AS6C62256 serves both the 6502 and the video path. On each **8 MHz** PHI2 cycle the mux picks who owns the VRAM address bus:

```text
  PHI2 high (CPU)   ---> HC157 selects $FE10/$FE11 addr, CPU may R/W $FE12
  PHI2 low  (PPU)   ---> HC157 selects beam/BG fetch VA, nametable OE
```

The CPU never shares a raw fight with the beam. Streaming MAP during active display is intentional: write on CPU phases while dots fetch on PPU phases. Auto-inc on `$FE12` commits on the next PHI2 rising edge in the sim model.

### Sprite line buffer (32 KB SRAM, 128 px used per half)

| | |
|--|--|
| **CPU port** | None direct |
| **Who writes** | ATmega1284P during HBlank (scanline N+1) |
| **Who reads** | Compositor/beam for scanline N |
| **Layout** | Ping-pong halves at `$000-$07F` and `$080-$0FF` per bank |
| **Content** | Resolved sprite pixels for one logical scanline, not a framebuffer |

---

## Cart flash (SST39SF040, 512 KB)

| | |
|--|--|
| **CPU read** | PRG at `$8000+`, MAP via `$FE90`-`$FE93`, CHR indirectly via BG/1284 fetch |
| **CPU write** | Flash programming only (not runtime gameplay) |
| **CHR read** | BG path on visible dots. 1284 during HBlank for sprites |
| **MAP read** | `$FE90`-`$FE92` set 24-bit seek, `$FE93` data auto-inc |

One `.retr01` image holds PRG, global palettes, world blobs (CHR + screen MAP + entities), and optional title/credits screens. CHR is **not** memory-mapped into the 6502 address space. The CPU seeks MAP bytes through `$FE93`. Video logic fetches CHR tiles by bank+index from flash.

---

## Pads (`$FE60` / `$FE61`)

| | |
|--|--|
| **Who** | ATmega1284P (island E / L path). Not a separate pad IC |
| **Layout** | Bit set = pressed. Same mask for P1 (`$FE60`) and P2 (`$FE61`) |
| **Bits** | 0 Right, 1 Left, 2 Down, 3 Up, 4 X, 5 Y, 6 Coin, 7 Start |

Host Play (emu/sim) samples P1 for move and warps. Game PRG can poll the same ports.

---

## Cart save EEPROM (24C64, on cartridge)

| | |
|--|--|
| **Purpose** | Per-game save data |
| **Host** | 6502 bit-bang or 1284 as I2C master via `$FExx` window (protocol TBD) |
| **API** | Games use `cart_save_*` HAL, not raw GPIO |

---

## ATmega1284P internal EEPROM (4 KB)

| | |
|--|--|
| **Purpose** | Machine config (not game saves) |
| **CPU port** | `$FE70`-`$FE72` handshake (protocol TBD) |
| **API** | `machine_eeprom_*` HAL |

---

## OAM (in 1284 SRAM, not a separate IC)

| | |
|--|--|
| **CPU port** | `$FE20` addr, `$FE21` data (auto-inc) |
| **Size** | **64** entries, 4 bytes each: `Y, tile, attr, X` |
| **Who reads** | 1284 firmware each scanline for sprite evaluation |
| **Unused slot** | `tile == 0xFF` |

---

## Color PROM (AT28C16 or OTP)

| | |
|--|--|
| **Content** | **64** master colors as packed **R3G3B2** `{RRRGGGBB}` |
| **CPU access** | None at runtime |
| **Video read** | Compositor supplies 6-bit palette index each dot (1-dot pipeline) |
| **Cart** | Holds palette **indices** only. Master RGB lives on the board |

Active palette indices are latched via `$FE08`/`$FE09` (HC573 path), not a separate palette RAM chip.

---

## Cart image (`.retr01`)

Magic **`retr01`**, **`format_ver` = 2**. Studio re-export required for older images.

```text
+------------------------------------------------------------------+
| HEADER 16 B | POINTER TABLE 36 B (u24 offsets + lengths)         |
| GLOBAL PALS 256 B (8 BG rows + 8 SPR rows, indices only)         |
| PRG 32 KB                                                        |
| OTHER SCREENS (title / interstitial / credits, raw or RLE)       |
| WORLD TABLE 8 x 8 B                                              |
| WORLD BLOBS: header, CHR 32 KB, screen dir, parallax dir,        |
|   screen payloads 480 B each, entity types/instances, player anim|
+------------------------------------------------------------------+
```

### Pointer table (36 B)

Six `(offset, length)` pairs as little-endian **u24** (3+3 bytes each):

| Slot | Points at |
|------|-----------|
| 0 | PRG (**32 KB**) |
| 1 | Global BG palette plane (**128 B**) |
| 2 | Global sprite palette plane (**128 B**) |
| 3 | World table (**64 B**) |
| 4 | Other-screens blob |
| 5 | Reserved (legacy ASCII credits, stay 0) |

### World blob (per present world)

| Piece | Size / note |
|-------|-------------|
| World header | **32 B** (spawn col/row, default banks/pal row, present count, CHR/dir offsets, entity counts, player entity + hitbox, camera dead-zone bytes **30-31**) |
| CHR | **4** BG banks + **4** SPR banks x **4096 B** = **32 KB** total |
| Screen directory | **12 B** per present screen (grid col/row + payload offset) |
| Screen payloads | **480 B** each (present only, sparse) |
| Entity types / instances | Packed records. Metasprite catalog is Studio-only and flattened here |
| Player anim | Optional `PA` blob when a player entity is marked |

**Screen payload:** **480 B** = 240 tile bytes + 240 attr bytes (**16x15**, **128x120**).

**World caps:** **8** worlds, **32 present screens**/world on sparse **8x8** grid, **0..8** parallax payloads/world, **4** BG + **4** sprite CHR banks/world (**256** tiles x **16 B** each bank).

**Flash budget at max fill:** ~**442 KB** used, ~**70 KB** free in **512 KB** (room for other screens, entity data).

### Other screens (global ROM)

Not on the world grid. Read through MAP port like world data.

| Id | Kind |
|----|------|
| **0** | Title |
| **1** | Level interstitial |
| **2+** | Credits pages (0..46 max) |

Payload **480 B** raw or **RLE** (`flags` bit 0). RLE: `C < 0x80` copy `C+1` literals, `C >= 0x80` repeat next byte `C-0x7F` times.

### MAP port

| Addr | Role |
|------|------|
| `$FE90`-`$FE92` | 24-bit seek into cart flash (lo, mid, hi) |
| `$FE93` | Read data, auto-inc |

Boot flow (Phase 1 PRG): seek palette + start MAP via `$FE90`-`$FE93` -> copy active palette row into `$FE08`/`$FE09` -> stream start screen tile/attr bytes into VRAM via `$FE12`. Scroll, player, and warps still run in Host Play today.

### `$FExx` window (storage view)

| Range | Backing |
|-------|---------|
| `$FE00`-`$FE12` | Video latches / VRAM port ([`graphics.md`](graphics.md)) |
| `$FE20`-`$FE21` | OAM in 1284 |
| `$FE30`-`$FE38` | World + bank helpers + `PAL_ROW` hint |
| `$FE40`-`$FE5F` | APU (328P) ([`sound.md`](sound.md)) |
| `$FE60`-`$FE61` | Pads (1284) |
| `$FE70`-`$FE72` | Machine EEPROM handshake (TBD) |
| `$FE90`-`$FE93` | Cart MAP seek/read |

`$FE80` unused. Silicon packs many of these into **9x HC573** (bitfield map TBD).

---

## Open topics

| Topic | Note |
|-------|------|
| Cart save `$FExx` | Port and mailbox layout TBD |
| Machine EEPROM handshake | `$FE70`-`$FE72` protocol TBD |
| HC573 bit packing | Logical `$FExx` vs silicon bitfields TBD |
