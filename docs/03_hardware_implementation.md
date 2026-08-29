# Retr01 Hardware Implementation

**Scope:** Optional **protoboard island** checklist for the **current 32 IC** Retr01-A netlist. Useful for discrete bring-up and bench fallback.

| | |
|--|--|
| **Current Retr01-A HW BOM** | [`05`](05_hardware_v1_32ic.md), **32 IC** (norm) |
| **Software-visible map** | [`02`](02_graphics_worlds_memory.md) |
| **Sources of truth** | [`01`](01_architecture_overview.md) |

Do not breadboard every IC at once. Prove **islands**, then merge against the **32 IC** netlist in `06`. **Pass** = island checks below, not a full game.

Software contracts live in [`02`](02_graphics_worlds_memory.md).

## Domains

Four active compute domains on one **5 V** board:

- **W65C02S:** game logic (fills nametables, OAM, latches). Never paints a framebuffer.
- **BG / beam path:** PLD + HC157 (beam X/Y in ATF22V10)
- **ATmega1284P:** OAM, sprite line-buffer fill, pad bytes, machine EEPROM handshake
- **ATmega328P:** audio

## Board summary (points to 06)

| Item | Spec |
|------|------|
| System IC count | **32** (31 motherboard + 1 cart I2C save). Escape +1 PLD -> 33 |
| Color PROM | **1x** packed R3G3B2 ([`05`](05_hardware_v1_32ic.md)) |
| Machine config | **1284 internal EEPROM** (`$FE70`-`$FE72` handshake). No parallel board EEPROM IC |
| `$FExx` latches | **9x HC573** bit-packed |
| Bus | **3x HC245** |

Datasheets: [W65C02S](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf), [AS6C62256](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf), [ATF22V10](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf), [ATmega1284P](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf), [ATmega328P](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf), [AT28C16](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf), [SST39SF040](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf), [74HC family](https://www.ti.com/logic-circuit/standard-logic/74hc-family/overview.html). Full part list: [`05`](05_hardware_v1_32ic.md).

**Clocks:** CPU **8.000 MHz**, dot **5.369318 MHz** (independent), 1284 **20 MHz**, 328P **16 MHz**.

**SCALE DIP:** **2x** default (128x120 -> 256x240, fills RGBS). **1x** centers 128x120. Beam timing stays **341x262**.

## Where state lives (short)

| What | CPU view | Chip |
|------|----------|------|
| Game RAM | `$0000-$7FFF` | AS6C62256 (CPU-only) |
| Nametables | `$FE10`/`$FE11`/`$FE12` | AS6C62256 VRAM (CPU phase write, PPU phase fetch) |
| Sprite line buffer | (no CPU port) | AS6C62256 (1284 writes HBlank, beam reads visible) |
| Cart | `$8000+` PRG, MAP `$FE90`-`$FE93`, CHR via fetch | SST39SF040 |
| Master RGB | (none in play) | Color PROM (1x packed R3G3B2) |
| Board / machine EEPROM | `$FE70`-`$FE72` | 1284 internal EEPROM handshake |
| `$FExx` controls | scroll, PPUCTRL, MAP addr, ... | HC573 + PLD decode |
| OAM / pads | `$FE20`/`$FE21`, `$FE60`/`$FE61` | ATmega1284P |
| APU | `$FE40`-`$FE5F` | ATmega328P |
| Active palettes | `$FE08`/`$FE09` | Packed **HC573** / decode path, then Color PROM |

Full register map: [`02`](02_graphics_worlds_memory.md).

**Bus rules:** system RAM = CPU only. VRAM = interleaved on PHI2. CHR = BG on visible dots, 1284 may own in HBlank. Color PROM = video path only.

**Sprites:** 1284 fills a ping-pong **128 px** line buffer one scanline ahead. Not a framebuffer. Cap **16** sprites per logical line.

**BG:** beam + scroll -> VRAM tile/attr -> CHR (attr `BANK`) -> active palette -> Color PROM. Mid-frame scroll takes effect on the next tile fetch. Visible nametable/attr pokes follow tear rules in `02`.

**Input:** `$FE60` / `$FE61`, bits 0-7 = right, left, down, up, X, Y, coin (cabinet) / select (console draft), start.

**Not in hardware:** framebuffer, sprite-0 hit, sprite-vs-BG collision, sprite DMA, on-board HDMI.

## Protoboard islands

**Ground rules:** 5 V only, 100 nF per IC. One bus driver at a time. Start CPU at **1-2 MHz** if wires ring, then **8 MHz**. Do not share CHR between BG and 1284 until each side works alone. W65C02S: **`BE` high**, **`RDY` high**, clock = **`PHI2`**.

Letter islands below are the **electrical bring-up checklist** (bench order). The board simulator may **co-locate** several letters on one canvas frame without changing these pass criteria. See [Sim canvas grouping](#sim-canvas-grouping).

```text
A Power
B Clocks + reset
C CPU + system RAM + tiny PRG
D $FExx decode + one latch
E Pads ($FE60/$FE61)
F Machine EEPROM (1284 handshake) [optional early]
G VRAM port + PHI2 interleave   [critical before video]
H Dot clock + beam (PLD state machines)
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

### Sim canvas grouping

`retr01_sim` still validates the letter milestones above (layer-2 smoke in `test_island_abcdeghiojklmnp.c`), but the **SDL canvas** uses **9 frames** so related chips sit together. Wiring is unchanged. BOM count stays **32** ([`05`](05_hardware_v1_32ic.md)).

| Canvas frame (UI) | Bring-up letters on that frame |
|-------------------|-------------------------------|
| **O** VIDEO RGBS *(top-left)* | O (+ video HC245) |
| **A** POWER+CLK | A + B |
| **C** CPU RAM PLD | C (+ CPU HC245) |
| **D** FExx LATCH | D |
| **G** VRAM+PLD | G (I stays wired-only) |
| **H** BEAM NMI | H |
| **J** CART FLASH | J (+ cart/OAM HC245) |
| **K** APU 328P | K |
| **L** 1284+LINEBUF | L + M (N/P stay wired/stats) |

Not drawn as frames (still in the netlist / health milestones): **E** pads, **F** EEPROM (deferred), **I** BG fetch, **N** sprites, **P** integration. Details: [`retr01_sim/README.md`](../retr01_sim/README.md).

| Port | Island | Smoke check |
|------|--------|-------------|
| `$FE02`/`$FE03` | D | Store `$55`, probe latch |
| `$FE10`-`$FE12` | G | Write/read `$AA` at VRAM 0 |
| `$FE20`/`$FE21` | N | OAM addr + data auto-inc |
| `$FE60`/`$FE61` | E | Switch sets matching bit |
| `$FE70`-`$FE72` | F | Machine EEPROM survives power-cycle |
| `$FE80` | J | unused (no PRG banking), leave 0 |
| `$FE90`-`$FE93` | J | MAP seek + read known byte |
