# Retr01 Costs and Open Questions

This doc merges the old cost sheet and decision log into one planning file.

## Locked decisions

| Topic | Decision |
|------|----------|
| Name / family | **Retr01**, rollout **A -> C -> H** |
| Worlds | **8** max |
| World layout | sparse virtual grid up to **16×16**, **64 screens max** per world |
| Screen format | **32×30** tiles + **240-byte** packed attr plane |
| CHR layout | **4 BG banks + 4 sprite banks** per world |
| Bank sizes | **256 tiles** per bank, **4 KB** each |
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
| Q1 | exact `$FE0x` and `$FE4x` bitfields | block families are fixed, byte-level details still need freezing |
| Q2 | RGBS analog levels / sync polarity tuning | digital timing is locked; bench tuning still needed |
| Q3 | Retr01-C 3-wire controller bit protocol | software-visible byte contract is fixed, transport details are not |
| Q4 | OAM byte order final confirmation | default remains NES-like `Y, tile, attr, X` |
| Q5 | root-level old names and paths | some legacy `GameNerd` / old-folder references still exist |

## Practical next decisions

The highest-value open work is:

1. freeze exact `$FE0x` and `$FE4x` bytes
2. rewrite the emulator against the reduced doc set
3. lock cart flash packaging
4. tune RGBS on real hardware
