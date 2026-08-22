# Retr01 Hardware Implementation

**Scope:** Optional **protoboard island** checklist and the **legacy ~52 IC** chip list (also on `main`). Useful for discrete bring-up and bench fallback.

| | |
|--|--|
| **Current Retr01-A HW BOM** | [`06`](06_hardware_v1_32ic.md) -- **32 IC** (norm) |
| **Software-visible map** | [`02`](02_graphics_worlds_memory.md) |
| **Sources of truth** | [`01`](01_architecture_overview.md) |

Adapt islands below to the 32-IC parts (beam PLDs instead of HC161/HC688, one Color PROM, no AT28C64B, keep 328P). Do not treat the 52-IC table as the product BOM.

Software contracts live in [`02`](02_graphics_worlds_memory.md). Legacy layout sketch: [`01`](01_architecture_overview.md).

## Domains

Four active compute domains on one **5 V** board:

- **W65C02S** -- game logic (fills nametables, OAM, latches -- never a framebuffer)
- **BG / beam path** -- PLD + HC157 (current BOM); legacy used more discrete HC beam/glue
- **ATmega1284P** -- OAM, sprite line-buffer fill, pad bytes, machine EEPROM handshake
- **ATmega328P** -- audio

## IC plan (legacy ~52 parts)

Through-hole reference from the older architecture. **Current BOM:** [`06`](06_hardware_v1_32ic.md). Optional **+1 ATF22V10** if equations overflow on either path.

| Part | Qty | Role |
|------|-----|------|
| W65C02S | 1 | CPU, 8 MHz |
| ATmega1284P | 1 | sprites / OAM / pads, 20 MHz |
| ATmega328P | 1 | APU, 16 MHz |
| AS6C62256 | 3 | system RAM, interleaved VRAM, sprite line buffer |
| SST39SF040 | 1 | 512 KB cart flash (PRG/CHR/MAP), v0 on-board socket |
| AT28C64B | 1 | board EEPROM |
| AT28C16 | 3 | Color PROM R/G/B (master palette -> DACs) |
| ATF22V10 | 3 | decode, timing, CHR/VRAM gating |
| 74HC157 | 6 | VRAM mux x4 + line-buffer mux x2 |
| 74HC245 | 3 | CPU bus / OAM / cart isolation |
| 74HC573 | 14 | `$FExx` latches |
| 74HC688 | 1 | raster compare |
| 74HC161 | 4 | beam X/Y |
| DIP-14 glue | 10 | HC14 x1, HC00 x2, HC04 x2, HC08 x2, HC32 x2, HC86 x1 |

Datasheets: [W65C02S](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf), [AS6C62256](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf), [ATF22V10](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf), [ATmega1284P](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf), [ATmega328P](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf), [AT28C64B](https://ww1.microchip.com/downloads/en/DeviceDoc/doc4428.pdf), [AT28C16](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf), [SST39SF040](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf), [74HC family](https://www.ti.com/logic-circuit/standard-logic/74hc-family/overview.html).

**Clocks:** CPU **8.000 MHz**, dot **5.369318 MHz** (independent), 1284 **20 MHz**, 328P **16 MHz**.

**SCALE DIP:** **2x** default (128x120 -> 256x240, fills RGBS). **1x** centers 128x120. Beam timing stays **341x262**.

**Color PROM:** current board uses **1x** packed R3G3B2 ([`06`](06_hardware_v1_32ic.md)). Table below lists the legacy **3x AT28C16** path. Carts store indices only. Q14/Q15 in [`05`](05_costs_and_open_questions.md).

## Where state lives (short)

| What | CPU view | Chip |
|------|----------|------|
| Game RAM | `$0000-$7FFF` | AS6C62256 (CPU-only) |
| Nametables | `$FE10`/`$FE11`/`$FE12` | AS6C62256 VRAM (CPU phase write, PPU phase fetch) |
| Sprite line buffer | (no CPU port) | AS6C62256 (1284 writes HBlank, beam reads visible) |
| Cart | `$8000+` PRG, MAP `$FE90`-`$FE93`, CHR via fetch | SST39SF040 |
| Master RGB | (none in play) | Color PROM (current: 1x packed -- `06`; legacy table: 3x AT28C16) |
| Board / machine EEPROM | `$FE70`-`$FE72` | Current: 1284 internal EEPROM handshake. Legacy table: AT28C64B |
| `$FExx` controls | scroll, PPUCTRL, MAP addr, ... | HC573 + PLD decode |
| OAM / pads | `$FE20`/`$FE21`, `$FE60`/`$FE61` | ATmega1284P |
| APU | `$FE40`-`$FE5F` | ATmega328P |
| Active palettes | `$FE08`/`$FE09` | **Current (32 IC):** indices held in packed **HC573** / decode path, then Color PROM (no separate palette RAM IC). Legacy ~52 also used latch/buffer parts for the same ports |

Full register map: [`02`](02_graphics_worlds_memory.md).

**Bus rules:** system RAM = CPU only. VRAM = interleaved on PHI2. CHR = BG on visible dots, 1284 may own in HBlank. Color PROM = video path only.

**Sprites:** 1284 fills a ping-pong **128 px** line buffer one scanline ahead. Not a framebuffer. Cap **16** sprites per logical line.

**BG:** beam + scroll -> VRAM tile/attr -> CHR (attr `BANK`) -> active palette -> Color PROM. Mid-frame scroll takes effect on the next tile fetch. Visible nametable/attr pokes follow tear rules in `02`.

**Input:** `$FE60` / `$FE61`, bits 0-7 = right, left, down, up, X, Y, coin (cabinet) / select (console draft), start.

**Not in hardware:** framebuffer, sprite-0 hit, sprite-vs-BG collision, sprite DMA, on-board HDMI.

## Protoboard islands

Do not breadboard all **52** legacy ICs at once. Prove **islands**, then merge -- preferably against the **32 IC** netlist in `06`. **Pass** = island checks below, not a full game.

**Ground rules:** 5 V only, 100 nF per IC. One bus driver at a time. Start CPU at **1-2 MHz** if wires ring, then **8 MHz**. Do not share CHR between BG and 1284 until each side works alone. W65C02S: **`BE` high**, **`RDY` high**, clock = **`PHI2`**.

```text
A Power
B Clocks + reset
C CPU + system RAM + tiny PRG
D $FExx decode + one latch
E Pads ($FE60/$FE61)
F Machine EEPROM (1284 handshake) [optional early; legacy F used AT28C64B]
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

Parallel: develop **K** and **L** in sim while breadboarding **A-I**. Merge at **N** and **P**.

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

| Island | Success |
|--------|---------|
| **A** Power | Clean 5 V, no smoke |
| **B** Clocks + reset | Stable `PHI2` (later ~5.37 MHz dot). `RESB` low then high |
| **C** CPU + RAM + PRG | Fetches PRG, RAM R/W, no bus fight |
| **D** `$FExx` + latch | `STA $FExx` hits only the latch |
| **E** Pads | `$FE60`/`$FE61`, **1 = pressed** |
| **F** Machine EEPROM | Write, power-cycle, read back (1284 path) |
| **G** VRAM interleave | `$FE10`-`$FE12` R/W, no PHI2 contention |
| **H** Beam | **341x262**, sane HBlank/VBlank/NMI stubs |
| **I** BG fetch | PPU phase walks nametable addrs, CPU still writes on CPU phase |
| **J** Cart stub | One of PRG/CHR/MAP `/CE` at a time. MAP reads test image |
| **K** 328P | Independent tone (sim OK first) |
| **L** 1284 | Runs at 20 MHz (sim OK first) |
| **M** Line buffer | Two 128-byte halves, clean mux |
| **N** Sprites | Expected pixels in line buffer, one-line pipeline |
| **O** Video | Stable RGBS. **2x** = 256x240 or **1x** centered |
| **P** Integration | Stable video, pads, NMI ~60 Hz, no hot chips |

**Integration order:** A,B -> C,D -> G -> E -> H,I,O -> J -> K -> L,M,N -> full compositor. Stop breadboarding and draw KiCad when **A-E**, **G-J**, and **K-O** pass.

| Port | Island | Smoke check |
|------|--------|-------------|
| `$FE02`/`$FE03` | D | Store `$55`, probe latch |
| `$FE10`-`$FE12` | G | Write/read `$AA` at VRAM 0 |
| `$FE20`/`$FE21` | N | OAM addr + data auto-inc |
| `$FE60`/`$FE61` | E | Switch sets matching bit |
| `$FE70`-`$FE72` | F | Machine EEPROM survives power-cycle |
| `$FE80` | J | unused, leave 0 |
| `$FE90`-`$FE93` | J | MAP seek + read known byte |
