# Retr01 Costs and Open Questions

This doc merges the old cost sheet and decision log into one planning file.

## Locked decisions

| Topic | Decision |
|------|----------|
| Name / family | **Retr01**, rollout **A -> C -> H** |
| Worlds | **8** max |
| World layout | sparse virtual grid up to **16x16**, **64 screens max** per world |
| Screen format | **32x30** tiles + **240-byte** packed attr plane |
| CHR layout | **4 BG banks + 4 sprite banks** per world |
| Bank sizes | **256 tiles** per bank, **4 KB** each |
| Master palette | **64 colors** in board **Color PROM** (**3x AT28C16** R/G/B). Same image on Retr01-A/C/H. **Not** in cart |
| Cart global palette banks | minimum **1 BG Palette + 1 sprite Palette** (indices 0-63 only) for the whole cart |
| World palette banks | optional **BG palette bank** and/or **sprite palette bank**, up to **8 rows x 4 palettes** each |
| Palette cart storage | **uncompressed** master **indices**; **pointer table** locates blobs. **No** master RGB in cart |
| Active palette buffer | **4 BG + 4 sprite palettes** from one selected **palette row** via `$FE08`/`$FE09` (indices into Color PROM) |
| Palette row selection | BG palette row **N** and sprite palette row **N** are always selected together |
| Shared backdrop | all **8** active palettes use the same **color 0** master index (software must write it into every slot when loading a row) |
| Palette fallback | world palette bank -> cart global -> system default **indices**. Master RGB always from Color PROM |
| Color PROM | **3x AT28C16** on every board; 6-bit index -> R/G/B DACs; programmed once |
| Runtime BG banking | per **nametable slot** |
| Runtime sprite banking | separate **global latch** |
| VRAM | **32 KB**, interleaved |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, sprite ping-pong |
| OAM | in **ATmega1284P**, no DMA. **`$FE20`** = addr, **`$FE21`** = data (auto-inc). Entry `Y, tile, attr, X` |
| Sprite cap | **16 / scanline** |
| `$FExx` byte map | **draft v0 frozen** in [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) (PPU, VRAM, OAM, banks, EEPROM, MAP) |
| MAP access | **`$FE90`-`$FE92`** addr, **`$FE93`** data + auto-inc |
| PRG banking | **`$FE80`** only |
| Board EEPROM | **AT28C64B on every Retr01-A v0**. Port **`$FE70`/`$FE71`** addr, **`$FE72`** data |
| Cart flash | **SST39SF040 class**: **on-board 32-pin socket for v0 bring-up**; **on cartridge later**. Same image format |
| Parallax camera lock | if **any** H or V parallax band is enabled, main camera locks to that axis for the **whole frame** |
| CPU map | RAM at `$0000-$7FFF`, I/O at `$FE00-$FEFF` |
| Controls | one byte per player at `$FE60/$FE61` |
| Retr01-C pad transport | **draft:** **ATtiny85** in pad; **VCC, GND, DATA**; 1284 master-polls; same `$FE60/$FE61` bytes |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Raster | scanline compare + IRQ |
| Glue | **ATF22V10CQZ-20PU** + 74HC family |
| APU | separate **ATmega328P** |
| Near-term software | **Retr01 Studio** only (visual authoring + later compile) |
| Studio project files | **JSON** OK; **schema/structure deferred** until Studio coding |
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

v0 uses on-board flash socket; cart PCB comes later. Motherboard + cart proto still targets roughly the **~$200** band.

## Why cost is still approximate

- cart PCB / connector not drawn yet (flash moves off-board later)
- board dimensions may still move
- analog output details may still shift resistor and connector choices
- distributor pricing changes

## Still open

| ID | Topic | Note |
|----|-------|------|
| Q2 | RGBS analog levels / sync polarity tuning | digital timing is locked. Bench tuning still needed |
| Q10 | OAM attr bitfields | entry order locked; flip / priority / palette bits inside attr byte still micro-rev |
| Q11 | `$FE07` plane band end / dual-band detail | start scanline drafted; end-of-band pairing may need a second latch |
| Q12 | PRG / CHR / MAP offsets inside 512 KB flash | socket-now / cart-later locked; exact region map still flexible |
| Q13 | Retr01-C pad bit timing | ATtiny85 + 3-wire draft locked; baud / poll edge details later |
| Q14 | Color PROM byte width per gun | AT28C16 is 8-bit wide; how many MSBs feed each R-2R (6-bit vs 8-bit DAC) still bench-tunable |

## Practical next decisions

1. tune RGBS on real hardware (Q2)
2. start Retr01 Studio Phase 0/1 (JSON project files; freeze schema when code needs it)
3. sketch PRG/CHR/MAP flash region map (Q12)
4. flesh out ATtiny85 poll timing when Retr01-C work starts (Q13)
