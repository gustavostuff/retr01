# Retr01-A Protoboard Islands

Bring-up plan for **Retr01-A v0** on solderless protoboards (or small proto PCBs). Do not try to fit the full **52**-IC machine on one breadboard. Build **islands**, prove each one, then combine.

Hardware: [`03_hardware_implementation.md`](03_hardware_implementation.md). Memory map and `$FExx`: [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md).

**Pass** means the island checks below, not "a full game runs."

## Ground rules

- **5 V only** on v0. Cap every IC (100 nF VCC to GND).
- One bus driver at a time on shared data lines. Stop and fix mux/`/OE` if two chips fight.
- Start CPU clocks at **1-2 MHz** if long wires ring. Move to **8 MHz** when stable.
- Do not share **CHR** between BG path and 1284 until each side works alone.
- W65C02S straps: **`BE` high**, **`RDY` high**. Clock is **`PHI2`**.

## Island map and order

```text
A Power
B Clocks + reset
C CPU + system RAM + tiny PRG
D $FExx decode + one latch
E Pads ($FE60/$FE61)
F Board EEPROM ($FE7x)          [optional early]
G VRAM port + PHI2 interleave   [critical before video]
H Dot clock + beam counters
I BG nametable fetch            [needs G + H]
J Cart flash stub (PRG/CHR/MAP)
K ATmega328P APU                [sim first OK]
L ATmega1284P                   [sim first OK]
M Line-buffer SRAM
N 1284 + line buffer + CHR      [needs L + M + J]
O Color PROM + compositor + RGBS
P Integration
```

**Parallel:** develop **K** and **L** firmware in simavr/Wokwi while breadboarding **A-I**. Merge at **N** and **P**.

```text
        A --> B --> C --> D --> E
                     |     |
                     |     +--> F (optional)
                     |
                     +--> G --> I --> O --\
                     |     ^              |
                     +--> H -/            +--> P
                                          |
        J --------------------------------+
        K --------------------------------+
        L --> M --> N --------------------+
```

## How islands interact

| From | Into | What crosses the boundary |
|------|------|---------------------------|
| A | everything | 5 V rail and ground |
| B | C, H, AVRs | `PHI2`, reset, later dot clock |
| C | D, G, J | CPU address/data/`R/W`, PRG fetch |
| D | E, F, G, O, N | `$FExx` chip-selects and write strobes |
| G | I, O | Shared VRAM: CPU phase vs PPU phase on PHI2 |
| H | I, N, O | Beam X/Y, HBlank, VBlank, NMI stub |
| I | O | BG tile/index stream for compositing |
| J | C, I, N | Gated `/CE_PRG`, `/CE_CHR`, `/CE_MAP` (one at a time) |
| M | N, O | Ping-pong line buffer (128 + 128 logical pixels) |
| N | O | Sprite pixels for the visible half |
| K | P | Audio out (can stay off the video bus) |
| O | monitor | RGBS (SCALE DIP is raster glue only) |

## Islands (role + success)

| Island | Looks like | Success |
|--------|------------|---------|
| **A** Power | Supply, fuse, diode, bulk cap, LED | Clean **5 V**, safe current, no smoke |
| **B** Clocks + reset | Crystal/can, HC14, RC reset | Stable `PHI2` (and later ~5.37 MHz dot). `RESB` low then high on power-on |
| **C** CPU + RAM + PRG | W65C02S, system SRAM, tiny ROM/flash, minimal decode | CPU fetches PRG. System RAM R/W works. No bus fight |
| **D** `$FExx` + latch | Decode PLD/glue + one 74HC573 (e.g. `$FE02`) | `STA $FExx` hits only the latch. RAM/PRG untouched |
| **E** Pads | Switches into `$FE60`/`$FE61` | Read port: **1 = pressed**, bit layout matches docs |
| **F** Board EEPROM | AT28C64B on `$FE70-$FE72` | Write, power-cycle or wait, read back same byte |
| **G** VRAM interleave | VRAM SRAM, 4x HC157, HC245, addr/data latches | CPU R/W via `$FE10`/`$FE11`/`$FE12`. No D-bus contention across PHI2 phases |
| **H** Beam counters | Dot clock, HC161s, wrap at 341/262 | Timing matches **341x262**. HBlank/VBlank/NMI stubs sane |
| **I** BG fetch | G + H + nametable address glue | PPU phase walks expected nametable addresses. CPU still writes on CPU phase |
| **J** Cart stub | Parallel flash, three `/CE` gates, bank/MAP latches | Only one of PRG/CHR/MAP `/CE` active. MAP port reads known test image |
| **K** 328P APU | ATmega328P alone (sim OK first) | Independent tone/PWM. No 6502 bus required yet |
| **L** 1284 alone | ATmega1284P alone (sim OK first) | Runs at 20 MHz. Firmware loads. Loopback pattern OK |
| **M** Line buffer | Third SRAM, two 128-byte halves, HC157 mux | Halves independent. Mux picks display vs writer cleanly |
| **N** Sprites | L + M + CHR during HBlank | Line buffer shows expected sprite pixels. One-line prepare/display pipeline |
| **O** Video out | Palette indices, 3x AT28C16, compositor, R-2R, SCALE DIP | Stable RGBS (~15.7 kHz H). **2x** = 256x240 (fills field) or **1x** centered 128x120 |
| **P** Integration | Proven islands on one board | Stable video, pads, NMI ~60 Hz, audio optional, no hot chips |

## Integration sketch (island P)

1. A, B
2. C, D
3. G (must pass before video)
4. E
5. H, I, O
6. J
7. K
8. L, M, N
9. Full compositor (BG + sprites)

Stop breadboarding and draw KiCad when **A-E**, **G-J**, and **K-O** pass on separate islands (or staged merges). Build the motherboard from proven nets, not from a full schematic guess.

## Quick `$FExx` smoke targets

| Port | Island | Quick check |
|------|--------|-------------|
| `$FE02`/`$FE03` | D | Store `$55`, probe scroll latch |
| `$FE10`-`$FE12` | G | Write/read `$AA` at VRAM 0 |
| `$FE20`/`$FE21` | N | OAM addr + data auto-inc |
| `$FE60`/`$FE61` | E | Switch sets matching bit |
| `$FE70`-`$FE72` | F | EEPROM R/W survives power-cycle |
| `$FE80` | J | reserved (`PRG_WINDOW` unused in v0, leave 0) |
| `$FE90`-`$FE93` | J | MAP seek + read known byte |

## Related docs

| Doc | Role |
|-----|------|
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | Chip plan, buses, pipelines, datasheet links |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | Timing, VRAM, `$FExx` map |
| [`05_costs_and_open_questions.md`](05_costs_and_open_questions.md) | Open bitfields, RGBS tuning |
