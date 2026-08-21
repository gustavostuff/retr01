# Retr01 Costs and Open Questions

This doc merges the old cost sheet and decision log into one planning file.

## Locked decisions

| Topic | Decision |
|------|----------|
| Name/family | **Retr01**, rollout **A -> C -> H** |
| Worlds | **16** max |
| World layout | sparse virtual grid up to **16x16**, **64 screens max** per world |
| Screen format | **16x15** tiles (**128x120**), **240-byte** tile plane + **240-byte** attr plane (one attr/tile) |
| Pixel aspect | square logical pixels; storage **16:15**; **2x** fills **256x240** RGBS with no letterbox |
| RGBS active field | always **256x240** inside **341x262** timing |
| Scale | board **SCALE** DIP: **2x** default (**256x240**), **1x** optional (centered **128x120**) |
| CHR layout | **4 BG banks + 4 sprite banks** per world |
| Bank sizes | **256 tiles** per bank, **4 KB** each |
| Master palette | **64 colors** in board **Color PROM** (**3x AT28C16** R/G/B). Same image on Retr01-A/C/H. **Not** in cart |
| Cart global palette banks | minimum **1 BG Palette + 1 sprite Palette** (indices 0-63 only) for the whole cart |
| World palette banks | optional **BG palette bank** and/or **sprite palette bank**, up to **8 rows x 4 palettes** each |
| Palette cart storage | **uncompressed** master **indices**. **Pointer table** locates blobs. **No** master RGB in cart |
| Active palette buffer | **4 BG + 4 sprite palettes** from one selected **palette row** via `$FE08`/`$FE09` (indices into Color PROM) |
| Palette row selection | BG palette row **N** and sprite palette row **N** are always selected together |
| Shared backdrop | all **8** active palettes use the same **color 0** master index (software must write it into every slot when loading a row) |
| Palette fallback | world palette bank -> cart global -> system default **indices**. Master RGB always from Color PROM |
| Color PROM | **3x AT28C16** on every board. 6-bit index -> R/G/B DACs. Programmed once |
| Runtime BG banking | per **8x8 tile** (attr `BANK` bits); screens may stamp a default only at load |
| BG attr byte | per tile: `PAL`/`FLIP_*`/`BANK` **hardware**; `SOLID`/`ANIM` **software** - see [`02`](02_graphics_worlds_memory.md) |
| BG living tiles | `ANIM=1`: **4** consecutive CHR indices `B..B+3`, `B` 4-aligned; software advances nametable |
| BG collision | `SOLID` bit + **RAM shadow**; video ignores `SOLID` |
| Runtime sprite banking | per **OAM entry** (attr `BANK` bits); `$FE37` optional stamp only |
| OAM attr byte | `BANK` (1-0), `PAL` (3-2), `FLIP_H` (4), `FLIP_V` (5), `PRIORITY` (6), `SIZE` (7) - see [`02`](02_graphics_worlds_memory.md) |
| VRAM | **32 KB**, interleaved. Slots **512 B** aligned (240+240 used) |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, sprite ping-pong, **128 px**/half used |
| OAM | in **ATmega1284P**, no DMA. **`$FE20`** = addr, **`$FE21`** = data (auto-inc). Entry `Y, tile, attr, X` in logical space |
| Sprite cap | **16** per **logical** scanline |
| Scroll | `scroll_x` **0-127**, `scroll_y` **0-119**, wrap |
| `$FExx` byte map | **draft v0 frozen** in [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) (PPU, VRAM, OAM, banks, EEPROM, MAP) |
| MAP access | **`$FE90`-`$FE92`** addr, **`$FE93`** data + auto-inc |
| PRG banking | **`$FE80`** only |
| Board EEPROM | **AT28C64B on every Retr01-A v0**. Port **`$FE70`/`$FE71`** addr, **`$FE72`** data |
| Cart flash | **SST39SF040 class**: **on-board 32-pin socket for v0 bring-up**, then **on cartridge later**. Same image format |
| Parallax camera lock | if **any** H or V parallax band is enabled, main camera locks to that axis for the **whole frame** |
| CPU map | RAM at `$0000-$7FFF`, I/O at `$FE00-$FEFF` |
| Controls | one byte per player at `$FE60/$FE61` |
| Retr01-C pad transport | **draft:** **ATtiny85** in pad. Wires **VCC, GND, DATA**. 1284 master-polls. Same `$FE60/$FE61` bytes |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Raster | scanline compare + IRQ |
| Glue | **ATF22V10CQZ-20PU** + 74HC family |
| APU | separate **ATmega328P** |
| Near-term software | **Retr01 Studio** only (visual authoring + later compile) |
| Studio project files | **JSON** OK. **Schema/structure deferred** until Studio coding |
| Future software | low-level hardware emulator (planned, not current work) |

## Cost snapshot

Qty-1 planning numbers, not a quote.

### Motherboard core

| Item | Ballpark |
|------|----------|
| CPU, SRAMs, AVRs, PLDs, Color PROMs, glue | about **$115** |
| sockets, passives, connectors | about **$50-$60** |
| proto motherboard PCB share | about **$20** |

### Cartridge

v0 uses on-board flash socket. Cart PCB comes later. Motherboard + cart proto still targets roughly the **~$200** band.

## Why cost is still approximate

- cart PCB/connector not drawn yet (flash moves off-board later)
- board dimensions may still move
- analog output details may still shift resistor and connector choices
- distributor pricing changes

## Still open

| ID | Topic | Note |
|----|-------|------|
| Q2 | RGBS analog levels/sync polarity tuning | digital timing is locked. Bench tuning still needed |
| Q10 | OAM attr bitfields | **Locked:** bank/pal/flip/priority/size in [`02`](02_graphics_worlds_memory.md). 8x16 tile-pair fetch detail on 1284 firmware still micro-rev |
| Q11 | `$FE07` plane band end/dual-band detail | start scanline drafted. End-of-band pairing may need a second latch |
| Q12 | PRG/CHR/MAP offsets inside 512 KB flash | socket-now/cart-later locked. Exact region map still flexible. Full CHR for all 16 worlds is 512 KB alone |
| Q13 | Retr01-C pad bit timing | ATtiny85 + 3-wire draft locked. Baud/poll edge details later |
| Q14 | Color PROM byte width per gun | AT28C16 is 8-bit wide. How many MSBs feed each R-2R (6-bit vs 8-bit DAC) still bench-tunable |
| Q15 | Color PROM part (AT28C16 vs faster OTP) | **Pinned candidate:** **AT27C256R / AT27C256R-70PU** (70 ns, In Production, DIP-28). Current plan still AT28C16. Revisit if EOL/stock or a higher dot clock needs <150 ns |
| Q16 | Default living-tile list cap | Recommend **32** vs **64** cells per camera workbench (`RETR01_ANIM_MAX`) |
| Q17 | BG anim rate | Fixed global `rate_shift`, or per-game constant only? |
| Q18 | BG flip+bank silicon timing | Prove on BG fetch island before locking Studio Phase 2 attr UI |

## Practical next decisions

1. tune RGBS on real hardware (Q2)
2. start Retr01 Studio Phase 0/1 (JSON project files. Freeze schema when code needs it)
3. sketch PRG/CHR/MAP flash region map (Q12)
4. flesh out ATtiny85 poll timing when Retr01-C work starts (Q13)
