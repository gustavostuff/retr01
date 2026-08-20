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
| Master palette | one **64-color** table for Retr01-A/C/H |
| Cart global palette banks | minimum **1 BG Palette + 1 sprite Palette** (one 4-color set each) for the whole cart |
| World palette banks | optional **BG palette bank** and/or **sprite palette bank**, sparse, up to **8 rows x 4 palettes** each |
| Active palette buffer | **4 BG + 4 sprite palettes** from one selected **palette row** |
| Palette row selection | BG palette row **N** and sprite palette row **N** are always selected together |
| Shared backdrop | all **8** active palettes use the same **color 0** master index (software must write it into every slot when loading a row) |
| Palette fallback | world palette bank -> cart global -> system default. Resolved in software at load time |
| Runtime BG banking | per **nametable slot** |
| Runtime sprite banking | separate **global latch** |
| VRAM | **32 KB**, interleaved |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, sprite ping-pong |
| OAM | in **ATmega1284P**, no DMA |
| Sprite cap | **16 / scanline** |
| MAP access | **`$FE90`** only |
| PRG banking | **`$FE80`** only |
| CPU map | RAM at `$0000-$7FFF`, I/O at `$FE00-$FEFF` |
| Controls | one byte per player at `$FE60/$FE61` |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Raster | scanline compare + IRQ |
| Glue | **ATF22V10CQZ-20PU** + 74HC family |
| APU | separate **ATmega328P** |
| Near-term software | **Retr01 Studio** only (visual authoring + compile) |
| Future software | low-level hardware emulator (planned, not current work) |

## Cost snapshot

Qty-1 planning numbers, not a quote.

### Motherboard core

| Item | Ballpark |
|------|----------|
| CPU, SRAMs, AVRs, PLDs, glue | about **$108** |
| sockets, passives, connectors | about **$50-$60** |
| proto motherboard PCB share | about **$20** |

### Cartridge

Depends on final flash strategy, but motherboard + cart proto still targets roughly the old **~$200** band.

## Why cost is still approximate

- final cart flash packaging is not frozen
- board dimensions may still move
- analog output details may still shift resistor and connector choices
- distributor pricing changes

## Still open

| ID | Topic | Note |
|----|-------|------|
| Q1 | exact `$FE0x` and palette-register bitfields | block families are fixed, byte-level details still need freezing |
| Q2 | RGBS analog levels / sync polarity tuning | digital timing is locked. Bench tuning still needed |
| Q3 | Retr01-C 3-wire controller bit protocol | software-visible byte contract is fixed, transport details are not |
| Q4 | OAM port bytes + entry layout | Default entry order remains NES-like `Y, tile, attr, X`. Which of `$FE20`/`$FE21` is address vs data (and any auto-inc) still needs freezing |
| Q5 | root-level old names and paths | some legacy `GameNerd` / old-folder references still exist |
| Q6 | exact cartridge encoding for palette banks and palette-row IDs | terminology and row-sync rules are locked. Field layout still needs freezing |
| Q7 | board EEPROM at `$FE70-$FE7F` | AT28C64B is in the v0 chip plan. Exact decode nibble, size window, and whether it ships on every board still open |
| Q8 | parallax vs 1-axis camera gating | docs require 1-axis main camera when parallax planes are used; whether that is whole-frame or band-only still open |
| Q9 | SST39SF040 ownership (on-board socket vs cart package) | v0 ASCII plans a 32-pin flash footprint for bring-up; final cart BOM packaging still open |

## Practical next decisions

The highest-value open work is:

1. freeze exact `$FE0x`, OAM `$FE20`/`$FE21` roles, and palette-register bytes
2. define Retr01 Studio phase 0 project/format layout
3. lock palette-bank encoding and palette-row IDs in cartridge data
4. lock cart flash packaging (on-board socket vs cart BOM) and PRG/MAP vs CHR split
5. freeze board EEPROM `$FE7x` decode or drop it from v0
6. tune RGBS on real hardware
7. decide parallax 1-axis gating (whole-frame vs band-only)
