# Retr01 Hardware

**IC-focused** doc: the 32-chip Retr01 motherboard + cart (shared by arcade and console shells), how those blocks connect, and island bring-up. Through-hole DIP target, ~**14 x 12 cm** minimum 4-layer PCB.

Passives, connectors, stackup, ESD/PPTC, and RF practice live in [`passive_rf_etc.md`](passive_rf_etc.md). Not here.

**Related:** [`memory.md`](memory.md) (chips, read/write timing). [`graphics.md`](graphics.md) (VRAM, sprites). [`sound.md`](sound.md) (APU). [`passive_rf_etc.md`](passive_rf_etc.md) (non-IC). Per-chip notes: [`hw/md/`](../hw/md/). Bring-up sim: [`retr01/sim/`](../retr01/sim/README.md).

---

## 32 IC parts is the current goal

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

Vendor datasheets (WDC, Alliance, Microchip / Atmel, SST): look up each part by name on the manufacturer site when pinouts or AC timing matter.

---

## How blocks connect on the PCB

Four compute domains share **5 V** and **never** paint a full framebuffer:

```text
                    +------------------+
  Cart SST39SF040 --| PRG / CHR / MAP  |-- CHR read (BG dots + 1284 VBlank/HBlank)
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
  ATmega1284P ---------------------> AS6C62256 (sprite field + L0 ping-pong)
                                    |
  Compositor PLD -------------------> RGBS (+ SCALE DIP 1x/2x)
```

**Bus rule:** three HC245 domains + PLD `/OE` ---> one driver at a time per domain.

**SCALE DIP:** **2x** default (128x120 logical fills **256x240** RGBS). **1x** centers 128x120. Raster timing unchanged.

**VRAM interleave (island G):** PHI2 high = CPU may R/W `$FE10`-`$FE12`. PHI2 low = BG fetch owns VRAM. Three HC157 mux low address bits between CPU latch and beam VA (line-buffer uses the other three). Details: [`memory.md`](memory.md).

---

## Signal paths

### Background video

1. Beam PLDs step **341x262** and fetch tile/attr from **VRAM** on PPU phases (L1 slots 0-3).
2. L1 scroll latches (`$FE02`/`$FE03`) offset into the L1 2x2 workbench ([`graphics.md`](graphics.md)).
3. Attr **BANK** picks CHR tile from cart flash. Active palette indices -> **Color PROM** -> DAC.
4. Compositor PLD muxes sprite vs L1 vs L0 (show-through when L1 index is **0**) vs backdrop.

### Second background (L0)

1. Software keeps L0 screens in VRAM slots **4-7** and sets `$FE06`/`$FE07` (often proportional to L1 scroll).
2. Target silicon / sim: **HBlank** fills the next L0 line from slots 4-7 + cart CHR (sim uses cart BG0 cache into linebuf). Sprites use **VBlank** for a full playfield field so they do not steal HBlank.
3. Emu Host Play composites L0 under L1 color 0 from the cart BG0 cache in the full-frame renderer (not a separate HBlank worker). See [`graphics.md`](graphics.md).

### Sprites

1. CPU fills **OAM** in 1284 via `$FE20`/`$FE21`.
2. Locked split with L0: **VBlank** plots the full **120x128** sprite field. **HBlank** fills the next L0 / BG0 line. Active dots apply L1 color-0 show-through. Cap **16** sprites per logical scanline.

### Pads

1. All pad paths feed the **ATmega1284P**. CPU reads packed bits at **`$FE60`** (P1) and **`$FE61`** (P2). Bit set = pressed ([`graphics.md`](graphics.md)).
2. **Silicon / PCB target:** one motherboard carries **both** I/O styles (arcade vs console is shell / population, not a different PCB):
   - **Arcade controllers:** headers / IDC for sticks and buttons as simple **microswitch-to-GND** circuits (series R + optional TVS at the connector) into the 1284.
   - **Aux pad ports:** PCB footprints / solder holes for **2x Switchcraft 35RAPC** female 3.5 mm TRS. Optional pad boards (ATtiny85 draft) talk a **3-wire** VCC / DATA / GND link over a male-male aux cable. Populate the jacks when using pads. Leave unstuffed in a pure cabinet build if desired.
3. **Runners today (Emu / Sim):** Host Play and pad UI drive the same `$FE60` / `$FE61` contract. They do not model TRS jacks or arcade header pinouts as separate netlist islands yet.
4. Ports / ESD / PPTC: [`passive_rf_etc.md`](passive_rf_etc.md).

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
| Compositor | Sprite vs L1 vs L0 show-through + Color PROM index |

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

Full letter list, sim canvas grouping, and port smoke checks: [`retr01/sim/README.md`](../retr01/sim/README.md).

---

## Form factors

| Variant | What differs |
|---------|----------------|
| **Arcade shell** | Same motherboard. Wire **arcade controller** headers to cabinet microswitches. RGBS / S-Video / composite, 5 V barrel. TRS jacks optional (DNP OK). |
| **Console shell** | **Same motherboard.** Populate **2x 35RAPC** TRS for aux pads. Arcade headers still present for DIY sticks / fight sticks. Same AV + cart. |
| **Retr01-H** | Handheld SMD later, same cart / `$FExx` software contract |

Ports and passives: [`passive_rf_etc.md`](passive_rf_etc.md).

---

## Open topics

| Topic | Note |
|-------|------|
| RGBS analog tuning | Digital timing set, bench levels TBD |
| Color PROM part speed | AT28C16 150 ns vs faster OTP, 1-dot pipeline |
| HC573 bitfield packing | 9-chip map still TBD in [`graphics.md`](graphics.md) |
| Cart I2C / machine EEPROM API | Mailbox protocol TBD in [`memory.md`](memory.md) |
| Aux pad 3-wire poll timing | Edges TBD (ATtiny85 draft) |
| Arcade header pinout | Lock at schematic (switch matrix / common GND) |
