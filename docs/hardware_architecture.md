# Retr01 Hardware Architecture

Discrete-logic 2D hardware for the Retr01 family (**Retr01-A** arcade, **Retr01-C** console, **Retr01-H** handheld). Same CPU model, memory map, and cartridge format across form factors.

**Related docs:** [`graphics.md`](graphics.md) (software-visible map, cart, VRAM). [`sounds.md`](sounds.md) (APU). [`selling_points.md`](selling_points.md) (product pitch, gameplay modules, PRG headroom). Tool READMEs: [`retr01_studio/`](../retr01_studio/README.md), [`retr01_emu/`](../retr01_emu/README.md), [`retr01_sim/`](../retr01_sim/README.md). IC notes: [`hw/md/`](../hw/md/).

---

## Sources of truth

When docs disagree, use this order.

| Concern | Authority |
|---------|-----------|
| Software-visible behavior (CPU map, `$FExx` logical ports, cart image, worlds/VRAM/palettes) | [`graphics.md`](graphics.md) |
| Retr01-A **HW BOM** (32 IC netlist, PCB, silicon pathways) | **This file** |
| Protoboard island bring-up | **This file** (island checklist below) |
| Audio / APU protocol | [`sounds.md`](sounds.md) |
| Gameplay modules, budgets, Studio product contract | [`selling_points.md`](selling_points.md) |
| Retr01 Studio UI + export | [`retr01_studio/README.md`](../retr01_studio/README.md) |
| Emulator runtime | [`retr01_emu/README.md`](../retr01_emu/README.md) |
| Board IC simulator | [`retr01_sim/README.md`](../retr01_sim/README.md) |

**Current product board:** **32 ICs**, ~**14 x 12 cm** minimum 4-layer THT (chip roles and netlist below).

---

## Core principles

1. **Unified CPU:** W65C02S, planning **8.000 MHz**
2. **Interleaved VRAM only:** CPU and BG path share VRAM on alternating phases. System RAM is CPU-only
3. **CHR from cartridge:** tile art lives in cart CHR-ROM, not VRAM
4. **Software collision:** gameplay collision stays in game code, not hardware sprite-vs-BG hit logic
5. **Raster IRQ, not sprite-0:** mid-frame effects use scanline compare
6. **Binary-first data:** fixed layouts, no runtime allocation assumptions on target
7. **Master palette in Color PROM:** **64 indices** on the motherboard (packed **R3G3B2**), not in the cart
8. **Logical 128x120, fixed RGBS raster:** games use **16x15** screens. The RGBS path keeps a **256x240** active field. Board **SCALE** selects 1x or 2x mapping

---

## Capability snapshot

| Area | Spec |
|------|------|
| Logical resolution | **128x120** (**16x15** tiles, **16:15**) |
| RGBS active field | **256x240** (SCALE **2x** fills field, **1x** = centered 128x120) |
| Tile size | **8x8** |
| Color | **2bpp**, **64-entry Color PROM** on board (packed **R3G3B2**), cart holds **8 global BG rows + 8 global sprite rows** (indices only), **one synced row active** (4 BG + 4 sprite) |
| Worlds | **8** max (indices 0-7) |
| Screens per world | **32 present** max on sparse **8x8** virtual grid (playfield only). **0..8** parallax screens/world. Optional **1..120** plane slices (variable thickness) |
| Cart / PRG | **512 KB** flash. Cart `format_ver` **2** only (8 worlds, other screens). **32 KB PRG** (no banking). Full caps fill **~442 KB**. **~70 KB** free |
| CHR per world | **4 BG banks + 4 sprite banks**, **256 tiles each**, **32 KB** |
| Sprites | **64 OAM**, **16 per logical scanline** max |
| VRAM | **32 KB**, interleaved |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, **128 px**/half used |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Frame timing | **341x262**, about **60.098 Hz** |

Full cart layout, `$FExx` register text, and VRAM slot rules: [`graphics.md`](graphics.md).

---

## High-level hardware (32 IC)

| Block | Role |
|------|------|
| **W65C02S** | Game logic, streaming, register writes |
| **BG / beam / compositor (PLD + HC157)** | Beam, VRAM fetch, BG pixels, priority mux, scale/border |
| **ATmega1284P** | OAM, sprite line-buffer fill, pads, **machine EEPROM** |
| **ATmega328P** | NES-style APU (`$FE40-$FE5F`) |
| **3x AS6C62256** | System RAM, VRAM, sprite line buffer |
| **Color PROM** | **1x** packed R3G3B2 (64 indices) -> DACs |
| **5x ATF22V10** | Decode, interleave, beam X/Y, compositor |
| **9x HC573 + 3x HC245** | `$FExx` latches + bus isolation |
| **Cart** | SST39SF040 flash + I2C save EEPROM |

---

## Retr01-A BOM (32 IC)

**Status:** current Retr01-A system BOM (motherboard + cart). Target: **through-hole DIP**, compact **14 x 12 cm** minimum 4-layer PCB.

| Item | Spec |
|------|------|
| **System IC count** | **32** (31 motherboard + 1 cart save). Escape **+1 PLD** -> 33 if compositor overflow |
| CPU / RAM / VRAM / cart flash | W65C02S + 3x AS6C62256 + SST39SF040 |
| Video timing | 341x262, ~5.37 MHz dot |
| Audio MCU | **ATmega328P** (dedicated APU) |
| Sprite / pads / machine EEPROM | **ATmega1284P** (no APU time-share) |
| Board config storage | **1284P internal 4 KB EEPROM** (handshake) |
| Color PROM | **1x AT28C16** (or faster OTP): packed R3-G3-B2, 1-dot pipeline |
| Beam / raster | **2x ATF22V10** (X/Y state machines + compare) |
| Glue logic | Absorbed into PLDs |
| `$FExx` latches | **9x HC573** (bit-packed bytes) |
| Bus transceivers | **3x HC245** (CPU / video / cart-OAM) |
| Cart game saves | **1x I2C EEPROM on cart** (in the 32) |

### Bill of materials

**Processors and MCUs (3 ICs)**

| Qty | Part | Role |
|-----|------|------|
| 1 | W65C02S (DIP-40) | 8 MHz game CPU |
| 1 | ATmega1284P (DIP-40) | 20 MHz: sprites/OAM, pads, **machine EEPROM** |
| 1 | ATmega328P (DIP-28) | 16 MHz: **APU**, `$FE40-$FE5F` |

**Memory and storage (4 ICs)**

| Qty | Part | Role |
|-----|------|------|
| 1 | AS6C62256 (DIP-28) | 32 KB system RAM (`$0000-$7FFF`) |
| 1 | AS6C62256 (DIP-28) | 32 KB interleaved VRAM |
| 1 | AS6C62256 (DIP-28) | 32 KB sprite line-buffer SRAM |
| 1 | SST39SF040 (DIP-32) | 512 KB cart flash (**32 KB** PRG + CHR / MAP) |

**Programmable logic (5 ICs)**

| Qty | Part | Role |
|-----|------|------|
| 1 | ATF22V10 | Address decode, PHI2 / CPU bus gating |
| 1 | ATF22V10 | VRAM interleave mux + absorbed glue |
| 1 | ATF22V10 | X-beam state machine (0-340) |
| 1 | ATF22V10 | Y-beam state machine (0-261) + raster IRQ compare |
| 1 | ATF22V10 | BG/sprite **priority mux** -> 6-bit Color PROM address |

**Registers and latches (9 ICs):** 9x 74HC573 — packed `$FExx` state.

**Video mux, bus, and output (10 ICs):** 6x 74HC157 (VRAM / line-buffer address mux), 1x AT28C16 Color PROM, 3x 74HC245 (CPU / video / cart-OAM isolation).

**Cart save (+1 IC, on cartridge):** 1x I2C EEPROM (e.g. 24C64). Interface: 6502 bit-bang or 1284 as I2C master behind a `$FExx` window (protocol TBD in [`graphics.md`](graphics.md)).

**IC count:** **32** total — 31 motherboard (incl. cart flash in socket) + 1 cart save. **Not in the 32:** **74HC14** reset/clock (absorb if possible, else +1 -> 33).

Datasheets: [W65C02S](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf), [AS6C62256](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf), [ATF22V10](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf), [ATmega1284P](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf), [ATmega328P](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf), [AT28C16](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf), [SST39SF040](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf), [74HC family](https://www.ti.com/logic-circuit/standard-logic/74hc-family/overview.html).

**Clocks:** CPU **8.000 MHz**, dot **5.369318 MHz** (independent), 1284 **20 MHz**, 328P **16 MHz**.

**SCALE DIP:** **2x** default (128x120 -> 256x240, fills RGBS). **1x** centers 128x120. Beam timing stays **341x262**.

---

## Inter-chip pathways

### APU path (328P)

1. W65C02S writes `$FE40-$FE5F` (sequencer / bytecode, see [`sounds.md`](sounds.md)).
2. **Bus bridge** = decode + CPU-domain HC245 isolation + 328P port/latch (not a separate IC).
3. ATmega328P services APU continuously (mix / DAC).
4. ATmega1284P does **not** synthesize audio.

### PLD beam and raster IRQ

1. `$FE04` latched in HC573.
2. Y-beam PLD compares to Y counter.
3. Match drives **IRQB**.

### 8-bit color DAC path

1. Compositor PLD -> 6-bit palette index (pipelined one dot if needed).
2. PROM/OTP -> `{RRRGGGBB}`.
3. R-2R ladders -> **RGBS**.

### Bus isolation

Three HC245s + PLD `/OE`: one driver at a time per domain.

---

## Where state lives

Four active compute domains on one **5 V** board:

- **W65C02S:** game logic (fills nametables, OAM, latches). Never paints a framebuffer.
- **BG / beam path:** PLD + HC157 (beam X/Y in ATF22V10)
- **ATmega1284P:** OAM, sprite line-buffer fill, pad bytes, machine EEPROM handshake
- **ATmega328P:** audio

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

Full register map: [`graphics.md`](graphics.md).

**Bus rules:** system RAM = CPU only. VRAM = interleaved on PHI2. CHR = BG on visible dots, 1284 may own in HBlank. Color PROM = video path only.

**Sprites:** 1284 fills a ping-pong **128 px** line buffer one scanline ahead. Not a framebuffer. Cap **16** sprites per logical line.

**BG:** beam + scroll -> VRAM tile/attr -> CHR (attr `BANK`) -> active palette -> Color PROM. Mid-frame scroll takes effect on the next tile fetch. Visible nametable/attr pokes follow tear rules in [`graphics.md`](graphics.md).

**Input:** `$FE60` / `$FE61`, bits 0-7 = right, left, down, up, X, Y, coin (cabinet) / select (console draft), start.

**Not in hardware:** framebuffer, sprite-0 hit, sprite-vs-BG collision, sprite DMA, on-board HDMI.

---

## Protoboard islands

Optional **protoboard island** checklist for the **32 IC** Retr01-A netlist. Prove **islands**, then merge against the netlist. **Pass** = island checks below, not a full game.

**Ground rules:** 5 V only, 100 nF per IC. One bus driver at a time. Start CPU at **1-2 MHz** if wires ring, then **8 MHz**. Do not share CHR between BG and 1284 until each side works alone. W65C02S: **`BE` high**, **`RDY` high**, clock = **`PHI2`**.

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

`retr01_sim` validates the letter milestones above, but the **SDL canvas** uses **9 frames** so related chips sit together. Wiring is unchanged. BOM count stays **32**.

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

---

## Validation gates

Bench/sim gates before first PCB spin:

| Gate | Pass criteria |
|------|----------------|
| **G1 CPU + RAM** | Island C style |
| **G2 VRAM interleave** | Island G style |
| **G3 Beam PLDs** | Stable 341x262, HBlank/VBlank/NMI stubs, raster IRQ |
| **G4 Compositor PLD** | BG/sprite priority @ dot rate, pipelined PROM |
| **G5 328P APU** | Stable output, `$FE4x` smoke |
| **G6 Bus** | No fights with 3x HC245 + PLD `/OE` |
| **G7 Color** | 64-entry PROM, RGBS tuned, 1-dot pipeline |
| **G8 Saves** | 1284 machine EEPROM + cart EEPROM R/W |

```text
Phase A: Prove behavior (island order above)
  CPU+RAM, VRAM interleave, beam PLDs, BG fetch, 1284 sprites,
  328P APU, Color PROM + RGBS (pipelined).

Phase B: Board-specific merges
  - Compositor PLD at dot rate
  - Packed HC573 / $FExx (as bitfields land in graphics.md)
  - 1284 machine-EEPROM handshake
  - Cart I2C save path

Phase C: First integrated PCB
  32 IC system (14 x 12 cm mobo + cart)
```

| Area | Risk | Mitigation |
|------|------|------------|
| Compositor PLD | I/O and timing fit | Priority-mux-only, escape +1 PLD |
| Bus contention | Driver fights | 3x HC245 |
| Color PROM | 150 ns / R3G3B2 | 1-dot pipeline, faster OTP, Studio match |
| `$FExx` / saves | Spec gaps | Finalize mailbox + I2C + bitfields in [`graphics.md`](graphics.md) |
| IC count | Overflow | Norm **32**, +1 PLD or +HC14 only if needed |

---

## Variants (hardware shell)

### Retr01-A

- Through-hole motherboard, **32 IC** system, ~**14 x 12 cm** minimum target
- RGBS + S-Video + composite pads
- **SCALE** DIP (**2x** default / **1x** optional)
- 20-pin IDC for cabinet controls
- 5 V barrel power
- Cart: 512 KB flash + I2C game-save EEPROM

### Retr01-C

- Same core architecture
- 3-wire controllers with **ATtiny85** (draft) in the pad -> `$FE60/$FE61`
- Same software contract

### Retr01-H

- Later SMD handheld
- Same memory map and cartridge model

---

## Open topics (hardware)

These may still change as bring-up continues.

| Topic | Note |
|-------|------|
| RGBS analog levels / sync polarity | Digital timing is set. Bench tuning still needed |
| `$FE07` plane band end / dual-band detail | Start scanline drafted. End-of-band pairing may need a second latch |
| Retr01-C pad bit timing | ATtiny85 + 3-wire draft. Baud/poll edge details later |
| Color PROM DAC depth | Packed R3G3B2 is the norm. Resistor steps / levels still tunable on bench |
| Color PROM part (AT28C16 vs faster OTP) | **Candidate:** AT27C256R-70PU (70 ns) if 150 ns is tight. DIP-24 vs DIP-28 footprint |
| Machine EEPROM handshake + cart I2C API | Protocol / `$FExx` for 1284 mailbox + cart save HAL — see [`graphics.md`](graphics.md) |
| HC573 bitfield packing | 9-chip packed map to document in [`graphics.md`](graphics.md) |
| BG flip+bank silicon timing | Prove on BG fetch island before finalizing attr UI in Studio |
| Cart flash part | Current target: **SST39SF040** (512 KB, DIP-32) |
