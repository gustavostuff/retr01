# Retr01 Hardware

**IC-focused** doc: the 32-chip Retr01 motherboard + cart (shared by arcade and console shells), how those blocks connect, and island bring-up. Through-hole DIP target, ~**14 x 12 cm** minimum 4-layer PCB.

Passives, connectors, stackup, ESD/PPTC, and RF practice live in [`passive_rf_etc.md`](passive_rf_etc.md). Not here.

**Related:** [`memory.md`](memory.md) (chips, read/write timing). [`graphics.md`](graphics.md) (VRAM, sprites). [`sound.md`](sound.md) (APU). [`cart.md`](cart.md) (cartridge + flasher). [`controllers.md`](controllers.md) (pads). [`passive_rf_etc.md`](passive_rf_etc.md) (non-IC). Per-chip notes: [`hw/md/`](../hw/md/). Bring-up sim: [`app/sim/`](../app/sim/README.md).

---

## Runners today vs silicon target

**Emu / Sim Host Play / Studio Play** are the source of truth for what runs in software today. **Silicon** sections in this doc family describe the motherboard and cart we are building toward. When they differ, runners win until code is updated.

| Topic | Runners today | Silicon / roadmap target |
|-------|---------------|---------------------------|
| Cart image | `.retr01` loaded from host file path (Sim default: `output/test_2.retr01` into cart flash) | SST39SF040 on 36-pin cart, USB-C flasher ([`cart.md`](cart.md)) |
| Cart flasher | Sim island **F** is visual only. Programming path tested via `flasher_bench` harness (**WIP** in main sim UI) | USB-C bench programs cart in socket ([`cart.md`](cart.md#usb-c-cartridge-flasher)) |
| Color PROM | Sim chip model **AT28C16** (64-entry table) | **AT27C256R** OTP ([`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md)) |
| `PPUCTRL` (`$FE00`) | Emu honors L1/L0/SPR enable + NMI. Camera-wrap bits stored, not enforced | Full bitfield incl. wrap modes ([`graphics.md`](graphics.md#ppuctrl-fe00)) |
| HC573 map | Sim netlist still ties VRAM addr to FE10/FE11 latches | Nine-chip U5A-U5I map ([`graphics.md`](graphics.md#hc573-latch-map-9-chips)) |
| Cart save | `$FE22`-`$FE24` in-memory 8 KB buffer, no `RDY` stall | 1284 I2C master + `RDY` stall ([`memory.md`](memory.md)) |
| Machine EEPROM | `$FE70`-`$FE72` in-memory 4 KB buffer, no `RDY` stall | 1284 internal EEPROM + `RDY` stall ([`memory.md`](memory.md)) |
| Pads | `$FE60` / `$FE61` from Host Play / UI (Sim: ARCADE direct or PADS via ATtiny85 poll) | Arcade GPIO or Retr01-C UART ([`controllers.md`](controllers.md)) |
| Pad UART netlist | Sim Pads mode: byte-level OD poll/reply on ATtiny85 entities (no TRS / 1284 USART) | 115200 open-drain on TRS DATA |
| BG0 scroll | `$FE06` / `$FE07` latched. Host Play uses proportional scroll unless CPU overrides | Same ports, HC573 silicon path TBD |

Studio Play uses the same Emu core as standalone `./emu`.

---

## 32 IC parts is the current goal

| Block | Parts | Role on PCB |
|------|-------|-------------|
| CPU | W65C02S | Game logic, `$FExx` writes, MAP/VRAM streaming |
| Helper MCUs | ATmega1284P, ATmega328P | Sprites/pads/EEPROM vs dedicated APU |
| SRAM x3 | AS6C62256 | System RAM, interleaved VRAM, sprite line buffer |
| Cart storage | SST39SF040 + 24C64 | 512 KB flash on cart + I2C save EEPROM ([`cart.md`](cart.md)) |
| Video glue | 5x ATF22V10, 6x HC157, 9x HC573, 3x HC245 | Decode, beam, interleave, latches, bus isolation |
| Color out | AT27C256R OTP | 64-entry R3G3B2 PROM -> binary-weighted DAC -> RGBS (75 ohm to GND) |
| Composite (support) | AD725 + 14.31818 MHz can | RGB->NTSC into J9. Outside the 32-IC logic count. Chip is SOIC-16 only. **First spin: THT via SOIC-to-DIP adapter** ([`passive_rf_etc.md`](passive_rf_etc.md)) |

**Count:** **32** (31 mobo + 1 cart save). Escape **+1 PLD** if compositor overflows. **74HC14** (reset/clock) and **AD725** (composite) are outside the 32 if not absorbed.

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
| 1 | AT27C256R | Color PROM (6-bit index -> R3G3B2, 45 ns OTP) |
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
  ATmega1284P ---------------------> AS6C62256 (sprite field + BG0 ping-pong)
                                    |
  Compositor PLD -------------------> RGBS (+ SCALE DIP 1x/2x)
```

**Bus rule:** three HC245 domains + PLD `/OE` ---> one driver at a time per domain.

**SCALE DIP:** **2x** default (128x120 logical fills **256x240** RGBS). **1x** centers 128x120. Raster timing unchanged.

**VRAM interleave (island G):** PHI2 high = CPU may R/W `$FE10`-`$FE12`. PHI2 low = BG fetch owns VRAM. Three HC157 mux low address bits between CPU latch and beam VA (line-buffer uses the other three). Details: [`memory.md`](memory.md).

---

## Signal paths

### Background video

1. Beam PLDs step **341x262** and fetch tile/attr from **VRAM** on PPU phases (BG1 slots 0-3).
2. BG1 scroll latches (`$FE02`/`$FE03`) offset into the BG1 2x2 workbench ([`graphics.md`](graphics.md)).
3. Attr **BANK** picks CHR tile from cart flash. Active palette indices -> **Color PROM** -> DAC.
4. Compositor PLD muxes sprite vs BG1 vs BG0 (show-through when BG1 index is **0**) vs backdrop.

### Second background (BG0)

1. Software keeps BG0 screens in VRAM slots **4-7** and sets `$FE06`/`$FE07` (often proportional to BG1 scroll).
2. Target silicon / sim: **HBlank** fills the next BG0 line from slots 4-7 + cart CHR (sim uses cart BG0 cache into linebuf). Sprites use **VBlank** for a full playfield field so they do not steal HBlank.
3. Emu Host Play composites BG0 under BG1 color 0 from the cart BG0 cache in the full-frame renderer (not a separate HBlank worker). See [`graphics.md`](graphics.md).

### Sprites

1. CPU fills **OAM** in 1284 via `$FE20`/`$FE21`.
2. Locked split with BG0: **VBlank** plots the full **120x128** sprite field. **HBlank** fills the next BG0 line. Active dots apply BG1 color-0 show-through. Cap **16** sprites per logical scanline.

### Pads

1. All pad paths feed the **ATmega1284P**. CPU reads packed bits at **`$FE60`** (P1) and **`$FE61`** (P2). Bit set = pressed ([`controllers.md`](controllers.md)).
2. **Silicon / PCB target:** one motherboard carries **both** I/O styles (arcade vs console is shell / population, not a different PCB):
 - **Retr01-A (arcade):** **J5/J6** 1x10 microswitch headers + **J7** 1x4 power/reset ([`controllers.md`](controllers.md#locked-headers-schematic-freeze)).
 - **Retr01-C (console):** **2x Switchcraft 35RAPC** TRS for aux pads. Pad boards use **ATtiny85** on a **3-wire** (5 V / DATA / GND) half-duplex UART link ([`controllers.md`](controllers.md)). Populate jacks for console. DNP OK on pure arcade builds.
3. **Runners today (Emu / Sim):** Host Play drives `$FE60` / `$FE61`. Sim HUD **ARCADE** = direct inject. **PADS** = ATtiny85 poll/reply into those ports. TRS jack netlist / 1284 USART not modeled yet.
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
| Compositor | Sprite vs BG1 vs BG0 show-through + Color PROM index |

---

## Protoboard bring-up

Prove **islands** on breadboard before full PCB. Pass = island smoke test, not a shipped game.

```text
A Power --> B Clocks --> C CPU+RAM+PRG --> D $FExx latch --> E Pads
                              |
                              +--> G VRAM interleave --> H Beam --> I BG fetch ----+
                              |                                                    |
J Cart flash -----------------+                                                    +--> O RGBS --> P Integration
K 328P APU ------------------------------------------------------------------------+
L 1284 --> M Line buf --> N Sprites -----------------------------------------------+
```

| Island | Pass |
|--------|------|
| **G** VRAM | `$FE10`-`$FE12` R/W, no PHI2 fight |
| **H** Beam | Stable 341x262, NMI stub |
| **N** Sprites | Expected pixels in line buffer |
| **O** Video | RGBS stable at 2x or 1x SCALE |

Full letter list, sim canvas grouping (11 frames), and port smoke checks: [`app/sim/README.md`](../app/sim/README.md). Sim canvas **N** is the cart module. Silicon bring-up **N** in the diagram below is the sprite path (wired via 1284/L, not a separate sim frame).

---

## Form factors

| Variant | What differs |
|---------|----------------|
| **Retr01-A** | Arcade shell. Wire **arcade controller** headers to cabinet microswitches. RGBS + composite + mono audio, 5 V barrel. TRS jacks optional (DNP OK). |
| **Retr01-C** | Console shell. **Same motherboard.** Populate **2x 35RAPC** TRS for aux pads ([`controllers.md`](controllers.md)). Arcade headers still present for DIY sticks. Same AV + cart ([`cart.md`](cart.md)). |
| **Retr01-H** | Handheld SMD later, same cart / `$FExx` software contract |

**Cart programming:** USB-C **cartridge flasher** (ATmega32U4 bench board). Not on the motherboard. See [`cart.md`](cart.md). **Sim:** flasher island is on the canvas for layout. USB programming is **WIP** (harness tests only).

Ports and passives: [`passive_rf_etc.md`](passive_rf_etc.md).

---

## Resolved / deferred topics

| Topic | Resolution |
|-------|------------|
| Color PROM speed | **AT27C256R** 45 ns OTP ([`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md)). Unused address pins tied **GND**. Binary-weighted outputs through **75 ohm** to ground -> **~0.7 Vpp** RGBS. |
| Composite | **AD725** NTSC from RGBS + CSYNC ([`passive_rf_etc.md`](passive_rf_etc.md)). **AV outs: RGBS + composite.** **First PCB: THT adapter** (no direct SOIC on board). |
| HC573 latch packing | Nine chips -> nine `$FExx` bytes ([`graphics.md`](graphics.md#hc573-latch-map-9-chips)). |
| Cart I2C save API | `$FE22`-`$FE24` via 1284 master ([`memory.md`](memory.md#cart-save-eeprom-24c64-on-cartridge)). |
| Machine EEPROM API | `$FE70`-`$FE72` + `RDY` stall ([`memory.md`](memory.md#atmega1284p-internal-eeprom-4-kb)). |
| Aux pad protocol | Retr01-C 3-wire UART ([`controllers.md`](controllers.md)). |
| Cart edge + flasher | 36-pin pinout + USB-C flasher ([`cart.md`](cart.md)). |
| VBlank sprite budget | 1284 @ 20 MHz: ~25k cycles in VBlank. Full 64-entry 8x16 field ~9.6k cycles ([`graphics.md`](graphics.md#sprites)). |

| Topic | Still open |
|-------|------------|
| **U725 first-spin THT** | Swap SOIC footprint for DIP-16 + SOIC-to-DIP adapter before fab ([`passive_rf_etc.md`](passive_rf_etc.md)) |
| RGBS / AD725 analog tuning | Targets locked in [`passive_rf_etc.md`](passive_rf_etc.md). Bench on first spin |
| Flasher firmware | Protocol locked in [`cart.md`](cart.md). Implement on 32U4 |
| UPLDA pin 23 | **Accepted this spin:** `SEL_FE10`/`SEL_FE11` share until fuse map or +1 PLD |
| HC245 DIR/OE | **Accepted this spin:** driven from UPLDB (UPLDA I/O budget) |
