# Retr01 Hardware

32-IC Retr01-A motherboard + cart. Through-hole DIP target, ~**14 x 12 cm** minimum 4-layer PCB.

**Related:** [`memory.md`](memory.md) (chips, read/write timing). [`graphics.md`](graphics.md) (VRAM, sprites). [`sound.md`](sound.md) (APU). IC notes: [`hw/md/`](../hw/md/). Bring-up sim: [`retr01_sim/`](../retr01_sim/README.md).

---

## 32 IC at a glance

| Block | Parts | Role on PCB |
|------|-------|-------------|
| CPU | W65C02S | Game logic, `$FExx` writes, MAP/VRAM streaming |
| Helper MCUs | ATmega1284P, ATmega328P | Sprites/pads/EEPROM vs dedicated APU |
| SRAM x3 | AS6C62256 | System RAM, interleaved VRAM, sprite line buffer |
| Cart storage | SST39SF040 + 24C64 | 512 KB flash on cart + I2C save EEPROM |
| Video glue | 5x ATF22V10, 6x HC157, 9x HC573, 3x HC245 | Decode, beam, interleave, latches, bus isolation |
| Color out | AT28C16 (or OTP) | 64-entry R3G3B2 PROM -> R-2R -> RGBS |

**Count:** **32** (31 mobo + 1 cart save). Escape **+1 PLD** if compositor overflows. **74HC14** for reset/clock is outside the 32 if not absorbed.

**Clocks:** CPU **8.000 MHz**, dot **5.369318 MHz**, 1284 **20 MHz**, 328P **16 MHz**. Raster **341x262**, ~**60.098 Hz**.

---

## Bill of materials

| Qty | Part | PCB role |
|-----|------|----------|
| 1 | W65C02S | 8 MHz game CPU |
| 1 | ATmega1284P | 20 MHz: OAM, sprite line fill, pads, machine EEPROM |
| 1 | ATmega328P | 16 MHz: APU (`$FE40`-`$FE5F`) |
| 1 | AS6C62256 | 32 KB system RAM |
| 1 | AS6C62256 | 32 KB interleaved VRAM |
| 1 | AS6C62256 | 32 KB sprite line-buffer SRAM |
| 1 | SST39SF040 | 512 KB cart flash (socket on mobo early) |
| 5 | ATF22V10 | Decode, VRAM interleave, X/Y beam, compositor priority |
| 9 | 74HC573 | Bit-packed `$FExx` latches |
| 6 | 74HC157 | VRAM + line-buffer address mux |
| 3 | 74HC245 | CPU / video / cart-OAM bus isolation |
| 1 | AT28C16 | Color PROM (6-bit index -> R3G3B2) |
| 1 | 24C64 (cart) | Per-game save EEPROM |

Datasheets: [W65C02S](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf), [AS6C62256](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf), [ATF22V10](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf), [ATmega1284P](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf), [ATmega328P](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf), [SST39SF040](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf).

---

## How blocks connect on the PCB

Four compute domains share **5 V** and **never** paint a full framebuffer:

```text
                    +------------------+
  Cart SST39SF040 --| PRG / CHR / MAP  |-- CHR read (BG dots + 1284 HBlank)
                    +--------+---------+
                             |
  W65C02S -------------------+------- $FExx latches (HC573) + PLD decode
       |                     |              |
       | PHI2                |              +---> ATmega328P (APU)
       v                     |
  AS6C62256 sys RAM          +---> ATmega1284P (OAM, pads, machine EEPROM)
       |
       +-- $FE10-$FE12 -----> AS6C62256 VRAM (interleaved with BG fetch)
                                    |
  Dot clock + beam PLDs ------------+---> tile/attr -> cart CHR -> palette -> PROM
                                    |
  ATmega1284P ---------------------> AS6C62256 line buffer (ping-pong 128 px)
                                    |
  Compositor PLD -------------------> RGBS (+ SCALE DIP 1x/2x)
```

**Bus rule:** three HC245 domains + PLD `/OE` ---> one driver at a time per domain.

**SCALE DIP:** **2x** default (128x120 logical fills **256x240** RGBS). **1x** centers 128x120. Raster timing unchanged.

**VRAM interleave (island G):** PHI2 high = CPU may R/W `$FE10`-`$FE12`. PHI2 low = BG fetch owns VRAM. Three HC157 mux low address bits between CPU latch and beam VA (line-buffer uses the other three). Details: [`memory.md`](memory.md).

---

## Signal paths

### Background video

1. Beam PLDs step **341x262** and fetch tile/attr from **VRAM** on PPU phases.
2. Scroll latches (`$FE02`/`$FE03`) offset into the 2x2 slot workbench ([`graphics.md`](graphics.md)).
3. Attr **BANK** picks CHR tile from cart flash. Active palette indices -> **Color PROM** -> DAC.
4. Compositor PLD muxes BG vs sprite line-buffer pixel by priority.

### Sprites

1. CPU fills **OAM** in 1284 via `$FE20`/`$FE21`.
2. During **HBlank**, 1284 reads CHR from cart and writes the **next** scanline into one half of line-buffer SRAM.
3. Beam reads the **other** half for the visible line. Cap **16** sprites per logical scanline.

### Pads

1. Cabinet / console sticks and buttons feed the **1284**.
2. CPU reads packed bits at **`$FE60`** (P1) and **`$FE61`** (P2). Bit set = pressed ([`graphics.md`](graphics.md)).
3. Retr01-C may insert a 3-wire pad MCU later. Software contract stays the same two ports.

### Audio

1. CPU writes **`$FE40`-`$FE5F`** (bytecode to 328P). See [`sound.md`](sound.md).
2. Path = PLD decode + CPU HC245 + 328P latch/port. **1284 does not synthesize.**

### Raster IRQ

1. `$FE04` compare value latched in HC573.
2. Y-beam PLD match ---> **IRQB** to 6502.

### Five ATF22V10 roles (target)

| PLD | Job |
|-----|-----|
| Decode | Chip-selects for `$FExx` windows and bus `/OE` |
| VRAM glue | Interleave enable with PHI2 + HC157 AB |
| Beam X | Dot counter / H timing inside 341 |
| Beam Y | Line counter / `$FE04` compare / NMI edges |
| Compositor | BG vs sprite priority + Color PROM index |

---

## Protoboard bring-up

Prove **islands** on breadboard before full PCB. Pass = island smoke test, not a shipped game.

```text
A Power --> B Clocks --> C CPU+RAM+PRG --> D $FExx latch --> E Pads
                              |
                              +--> G VRAM interleave --> H Beam --> I BG fetch --+
                              |                                                    |
J Cart flash -----------------+                                                    +--> O RGBS --> P Integration
K 328P APU ------------------------------------------------------------------------+
L 1284 --> M Line buf --> N Sprites ------------------------------------------------+
```

| Island | Pass |
|--------|------|
| **G** VRAM | `$FE10`-`$FE12` R/W, no PHI2 fight |
| **H** Beam | Stable 341x262, NMI stub |
| **N** Sprites | Expected pixels in line buffer |
| **O** Video | RGBS stable at 2x or 1x SCALE |

Full letter list, sim canvas grouping, and port smoke checks: [`retr01_sim/README.md`](../retr01_sim/README.md).

---

## Form factors

| Variant | Shell |
|---------|-------|
| **Retr01-A** | Arcade mobo, RGBS/S-Video/composite, cabinet IDC, 5 V barrel |
| **Retr01-C** | Console, same core, 3-wire pads (ATtiny85 draft) |
| **Retr01-H** | Handheld SMD later, same cart contract |

---

## Open topics

| Topic | Note |
|-------|------|
| RGBS analog tuning | Digital timing set, bench levels TBD |
| Color PROM part speed | AT28C16 150 ns vs faster OTP, 1-dot pipeline |
| HC573 bitfield packing | 9-chip map still TBD in [`graphics.md`](graphics.md) |
| Cart I2C / machine EEPROM API | Mailbox protocol TBD in [`memory.md`](memory.md) |
| Retr01-C pad timing | 3-wire poll edges TBD |
