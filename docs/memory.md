# Retr01 Memory

What each storage chip holds, who reads it, and when. Cart layout lives here too.

**Related:** [`graphics.md`](graphics.md) (VRAM slots, palettes). [`hardware.md`](hardware.md) (BOM, PCB paths). [`cart.md`](cart.md) (edge pinout, flasher). Register text for `$FExx`: [`graphics.md`](graphics.md).

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
| **Typical use** | Entity state, scroll helpers, BG0 proportional scroll temps, stacks, ZP shadows |

### VRAM (32 KB, interleaved)

| | |
|--|--|
| **CPU port** | `$FE10` addr hi, `$FE11` addr lo, `$FE12` data (auto-inc) |
| **CPU writes** | PHI2 high (CPU phase) via HC157 mux + PLD `/OE` |
| **PPU reads** | PHI2 low (PPU phase): beam/BG fetch owns address and data |
| **Layout** | Slots 0-3 BG1 camera (**512 B** each), slots 4-7 BG0 camera (**512 B** each). **480 B** screen data + **32 B** pad per slot ([`graphics.md`](graphics.md)). Rest scratch/reserved |
| **Tear rule** | Do not poke a cell the beam is fetching. Off-screen slots and VBlank are safe |

**How interleave works:** one AS6C62256 serves both the 6502 and the video path. On each **8 MHz** PHI2 cycle the mux picks who owns the VRAM address bus:

```text
  PHI2 high (CPU)   ---> HC157 selects $FE10/$FE11 addr, CPU may R/W $FE12
  PHI2 low  (PPU)   ---> HC157 selects beam/BG fetch VA, nametable OE
```

The CPU never shares a raw fight with the beam. Streaming MAP during active display is intentional: write on CPU phases while dots fetch on PPU phases. Auto-inc on `$FE12` commits on the next PHI2 rising edge in the sim model.

### Sprite line buffer (32 KB SRAM)

| | |
|--|--|
| **CPU port** | None direct |
| **Sprite field** | ATmega1284P fills the full **120x128** playfield in **VBlank** (soft field in sim). Beam/compositor reads per visible dot |
| **BG0 lines** | 1284 fills the next BG0 line in **HBlank** (ping-pong). Active dots use that line where BG1 color index is **0** (show-through mask) |
| **Layout (sim)** | Sprite field at `$0000` (120x128). BG0 ping-pong halves at `$4000` / `$4080` |
| **Content** | Resolved sprite / BG0 master indices, not a full RGB framebuffer |

---

## Cart flash (SST39SF040, 512 KB)

| | |
|--|--|
| **CPU read** | PRG at `$8000+`, MAP via `$FE90`-`$FE93`, CHR indirectly via BG/1284 fetch |
| **CPU write** | Flash programming only (not runtime gameplay) |
| **CHR read** | BG path on visible dots. 1284 uses cart CHR in **VBlank** (sprite field) and **HBlank** (BG0 line) |
| **MAP read** | `$FE90`-`$FE92` set 24-bit seek, `$FE93` data auto-inc |

One `.retr01` image holds PRG, global palettes, world blobs (CHR + screen MAP + entities), and optional title/credits screens. CHR is **not** memory-mapped into the 6502 address space. The CPU seeks MAP bytes through `$FE93`. Video logic fetches CHR tiles by bank+index from flash.

---

## Pads (`$FE60` / `$FE61`)

| | |
|--|--|
| **Who** | ATmega1284P (island E / L path). Not a separate pad IC |
| **Layout** | Bit set = pressed. Same mask for P1 (`$FE60`) and P2 (`$FE61`) |
| **Bits** | 0 Right, 1 Left, 2 Down, 3 Up, 4 X, 5 Y, 6 Coin, 7 Start |
| **Board I/O** | Silicon / PCB: arcade microswitch headers **and** 2x 35RAPC TRS footprints ([`passive_rf_etc.md`](passive_rf_etc.md)). Runners today: `$FE60` / `$FE61` only |

Host Play (emu/sim) samples P1 for move and warps. Game PRG can poll the same ports.

---

## Cart save EEPROM (24C64, on cartridge)

| | |
|--|--|
| **Purpose** | Per-game save data |
| **Host** | **ATmega1284P** is I2C master to cart **SDA** / **SCL** (6502 does not bit-bang) |
| **CPU ports** | Mailbox at **`$FE22`-`$FE24`** |
| **API** | Games use `cart_save_*` HAL, not raw ports |

### `$FE22`-`$FE24` mailbox

| Addr | Name | Write | Read |
|------|------|-------|------|
| `$FE22` | `CARTEE_CMD` | High address byte **or** command: **`0x80`** = read, **`0x40`** = write | Last command / high addr |
| `$FE23` | `CARTEE_ADDR` | Low address byte (13-bit effective into 8 KB device) | Low address |
| `$FE24` | `CARTEE_DATA` | Data byte. **Write** starts I2C **program** sequence | Data byte. **Read** starts I2C **read** |

**Flow:** CPU writes command + address bytes, then touches **`$FE24`**. The 1284 performs the I2C transaction on the cart edge and **asserts `RDY` low** on the 6502 until the EEPROM ACK completes (same stall model as machine EEPROM). Games should use the HAL so retry / busy policy stays in firmware.

**Bring-up:** Emu implements an in-memory 8 KB buffer with instant access (no `RDY` stall). Sim not yet. Silicon target is 1284 I2C master + `RDY` stall ([`hardware.md`](hardware.md#runners-today-vs-silicon-target)).

---

## ATmega1284P internal EEPROM (4 KB)

| | |
|--|--|
| **Purpose** | Machine config (not game saves) |
| **CPU port** | **`$FE70`-`$FE72`** handshake |
| **API** | `machine_eeprom_*` HAL |

### `$FE70`-`$FE72` mailbox

| Addr | Name | Role |
|------|------|------|
| `$FE70` | `MEEPROM_AL` | Address low byte |
| `$FE71` | `MEEPROM_AH` | Address high byte (4 KB, use low 12 bits) |
| `$FE72` | `MEEPROM_DATA` | Data byte. **Read or write** triggers the 1284 EEPROM access |

On **`$FE72`** access the 1284 runs the internal EEPROM read/program cycle and **holds `RDY` low** on the W65C02S until the AVR finishes (~3-4 ms program). CPU code should poll `RDY` or use the HAL blocking helper.

Cart saves use the **cart 24C64** at `$FE22`-`$FE24`, not this window.

**Runners today:** Emu uses in-memory 4 KB storage at `$FE70`-`$FE72` with no `RDY` stall. Silicon target is 1284-backed access with stall ([`hardware.md`](hardware.md#runners-today-vs-silicon-target)).

---

## OAM (in 1284 SRAM, not a separate IC)

| | |
|--|--|
| **CPU port** | `$FE20` addr, `$FE21` data (auto-inc) |
| **Size** | **64** entries, 4 bytes each: `Y, tile, attr, X` |
| **Who reads** | 1284 firmware each scanline for sprite evaluation |
| **Unused slot** | `tile == 0xFF` |

---

## Color PROM (AT27C256R OTP)

| | |
|--|--|
| **Part** | **AT27C256R** (45 ns OTP). Replaces slower AT28C16-class parts |
| **Content** | **64** master colors as packed **R3G3B2** `{RRRGGGBB}` |
| **CPU access** | None at runtime |
| **Video read** | Compositor supplies 6-bit palette index each dot |
| Analog | Binary-weighted DAC -> **75 ohm** termination to GND -> **~0.7 Vpp** RGBS |
| **Wiring** | Use **A5-A0** for index. Unused address pins tied **GND** |

**Runners today:** Sim chip model is still **AT28C16**. Silicon target is **AT27C256R** ([`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md)).

Details: [`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md).

Active palette indices use `$FE08`/`$FE09` (1284 soft addr + data path). Not a separate palette RAM chip.

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
| WORLD BLOBS: header, CHR 32 KB, BG1 screen dir + payloads,       |
|   BG0 dir + payloads, entity types/instances, player anim        |
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
| World header | **32 B** (spawn cell as nibble-packed col/row, default banks/pal row, BG1 present count, BG0 present count, CHR/dir offsets, entity counts, player entity + hitbox, camera dead-zone bytes **30-31**) |
| CHR | **4** BG banks + **4** SPR banks x **4096 B** = **32 KB** total |
| BG1 screen directory | **12 B** per present playfield screen (grid cell + payload offset) |
| BG1 screen payloads | **480 B** each (present only, sparse **16x16**) |
| BG0 directory | **12 B** per present BG0 screen (same shape as BG1 dir). Offset **0** if none |
| BG0 payloads | **480 B** each (up to **8** present screens, sparse on **16x16**) |
| Entity types / instances | Packed records. Metasprite catalog is Studio-only and flattened here |
| Player anim | Optional `PA` blob when a player entity is marked |

**Grid cell byte:** virtual map is **16x16** (col/row **0-15**). Pack both coords in **1 byte** as nibbles: `col | (row << 4)`. Same packing for BG1/BG0 directory entries and world-header spawn cell.

**World header notes (BG0):** byte **3** packs present BG0 extent (`cols | rows<<4`). Byte **6** is BG0 present count (was legacy parallax count). Bytes **14-16** are BG0 directory offset (u24), or **0** if none.

**Screen payload:** **480 B** = 240 tile bytes + 240 attr bytes (**16x15**, **128x120**). Same shape for BG1 and BG0.

**World caps:** **8** worlds, **48 present BG1 screens**/world on sparse **16x16** grid, **0..8** BG0 screens/world (free layout on **16x16**), **4** BG + **4** sprite CHR banks/world (**256** tiles x **16 B** each bank).

**Flash budget at max fill:** ~**504 KB** used (**515700 B**), ~**8.4 KB** free (**8588 B**) in **512 KB** (tight headroom for other screens / entity data).

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
| `$FE60`-`$FE61` | Pads (1284) ([`controllers.md`](controllers.md)) |
| `$FE22`-`$FE24` | Cart save EEPROM mailbox (1284 I2C master) |
| `$FE70`-`$FE72` | Machine EEPROM mailbox (1284 internal EEPROM) |
| `$FE90`-`$FE93` | Cart MAP seek/read |

`$FE80` unused. `$FExx` ownership (PLD vs 1284 soft) is in [`graphics.md`](graphics.md#fexx-ownership). Other `$FExx` ports (VRAM, OAM, BG0 scroll, palette data) use decode-qualified paths documented in [`graphics.md`](graphics.md).

---

## Resolved topics

| Topic | Resolution |
|-------|------------|
| Cart save API | `$FE22`-`$FE24` (above) |
| Machine EEPROM | `$FE70`-`$FE72` + `RDY` (above) |
| `$FExx` ownership | [`graphics.md`](graphics.md#fexx-ownership) |
| Cart hardware | [`cart.md`](cart.md) |
