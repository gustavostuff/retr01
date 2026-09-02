# Retr01 KiCad Schematic Guide (32-IC Motherboard)

Chip-by-chip, pin-by-pin guide to capture the **shared Retr01 motherboard** schematic in KiCad. Scope is the locked **32-IC** silicon target plus passives and connectors. The USB-C cart flasher is a **separate** bench board ([`cart.md`](cart.md)). Do not put flasher ICs on this schematic.

**Authority**

| Layer | Wins when |
|-------|-----------|
| Vendor datasheet | Pin numbers, AC timing, absolute max |
| [`hw/md/`](../hw/md/) | Retr01 role, package, sim-facing pin names |
| This guide | Sheet order, net names, proposed GPIO locks, KiCad workflow |
| [`retr01/sim/`](../retr01/sim/README.md) | Island grouping and bring-up smoke order |
| Emu / Sim Host Play | What runs in software today ([`hardware.md`](hardware.md#runners-today-vs-silicon-target)) |

When runners and silicon differ, mark silicon nets as **silicon target**. Do not invent software contracts that contradict [`graphics.md`](graphics.md) / [`memory.md`](memory.md).

**Related:** [`hardware.md`](hardware.md), [`passive_rf_etc.md`](passive_rf_etc.md), [`cart.md`](cart.md), [`controllers.md`](controllers.md), [`sound.md`](sound.md).

**KiCad:** written for **KiCad 8.x** (Symbol Editor + Schematic Editor). Steps are the same idea in 7.x with slightly different menu names.

**ai-rules:** keyboard-friendly ASCII only. No semicolons. No em dashes.

---

## 1. What you are drawing

### 1.1 IC count (must match BOM)

| Qty | Refdes pattern | Part | Island (sim / bring-up) |
|-----|----------------|------|-------------------------|
| 1 | U1 | W65C02S | C |
| 1 | U2 | AS6C62256 system RAM | C |
| 1 | U3 | ATF22V10 decode | C |
| 1 | U4 | SN74HC245 CPU domain | C |
| 9 | U5A-U5I | SN74HC573 `$FExx` latches | D |
| 1 | U6 | AS6C62256 VRAM | G |
| 3 | U7A-U7C | SN74HC157 VRAM addr mux | G |
| 1 | U8 | ATF22V10 VRAM glue | G |
| 1 | U9 | ATF22V10 beam X | H |
| 1 | U10 | ATF22V10 beam Y | H |
| 1 | U11 | ATF22V10 compositor | O |
| 1 | U12 | AT27C256R Color PROM | O |
| 1 | U13 | SN74HC245 video domain | O |
| 1 | U14 | SN74HC245 cart-OAM domain | J |
| 1 | U15 | ATmega1284P | L |
| 1 | U16 | AS6C62256 line buffer | L |
| 3 | U17A-U17C | SN74HC157 linebuf addr mux | L |
| 1 | U18 | ATmega328P APU | K |
| 1 | U19 | 74HC14 (reset / clock cleanup) | A (outside 32 if counted separately) |
| cart | (on cart PCB) | SST39SF040 + 24C64 | N (module) |

Motherboard IC bodies in the **32** tally: everything above except treating **U19** as support if your BOM follows [`hardware.md`](hardware.md) ("74HC14 outside 32 if not absorbed"). Cart save **24C64** counts in the **32**. Flash lives on the cart module in production (socket on mobo OK for bring-up).

### 1.2 Hierarchical sheets (match sim islands)

Create a root sheet `retr01_mobo.kicad_sch` and **hierarchical sheets**:

| Sheet file | Title | Contents |
|------------|-------|----------|
| `01_power_clk.kicad_sch` | A Power + clocks | Barrel, PPTC, bulk, ferrites, OSC 8 MHz, OSC dot, reset, U19 |
| `02_cpu_ram.kicad_sch` | C CPU + sys RAM + decode | U1, U2, U3, U4 |
| `03_latches.kicad_sch` | D `$FExx` latches | U5A-U5I |
| `04_vram.kicad_sch` | G VRAM interleave | U6, U7A-C, U8 |
| `05_beam.kicad_sch` | H Beam | U9, U10, DOT nets |
| `06_video.kicad_sch` | O Video out | U11, U12, U13, R-2R, RGBS jack, SCALE |
| `07_cart_edge.kicad_sch` | J Cart socket | U14, 36-pin connector, ESD |
| `08_mcu_linebuf.kicad_sch` | L 1284 + linebuf | U15, U16, U17A-C, pads |
| `09_apu.kicad_sch` | K APU | U18, audio R-2R |
| `10_ports.kicad_sch` | Ports | Barrel already in A. Arcade headers, 2x TRS, AV |

Sim canvas letters: see [`retr01/sim/README.md`](../retr01/sim/README.md). Silicon bring-up **N** = sprites. Sim canvas **N** = cart module. On the schematic, put cart **flash + 24C64** on a small `cart_module.kicad_sch` or a separate cart project. Motherboard only needs the **36-pin receptacle**.

### 1.3 Global net name rules

Use these exact net names so ERC and layout stay searchable:

| Net | Meaning |
|-----|---------|
| `+5V` | Digital 5 V pour |
| `+5V_ANA` | Ferrite-isolated analog spur (video + audio DAC) |
| `GND` | Ground |
| `PHI2` | 8.000 MHz CPU clock into W65C02S pin 37 |
| `DOT` | ~5.369318 MHz beam clock |
| `CPU_A0`..`CPU_A15` | 6502 address |
| `CPU_D0`..`CPU_D7` | 6502 data (CPU side of HC245) |
| `RWB` | 6502 R/W (1=read) |
| `BE` | Bus enable (tie high) |
| `RESB` | Active-low reset (CPU + shared) |
| `IRQB` | Active-low IRQ |
| `NMIB` | Active-low NMI |
| `RDY` | CPU ready (pull-up high in normal play) |
| `SEL_FE00` .. (decode outputs) | Active strobes from decode PLD |
| `LE_FE00` etc. | Latch enables into HC573 |
| `VRAM_A0`.. | Address into VRAM SRAM after mux |
| `LB_A0`.. | Address into linebuf SRAM after mux |
| `CART_A0`..`CART_A18` | Cart edge address |
| `CART_D0`..`CART_D7` | Cart edge data |
| `CART_OE#` `CART_WE#` | Cart flash controls |
| `SDA` `SCL` | Cart save I2C |
| `RGB_R` `RGB_G` `RGB_B` `CSYNC` | Analog video |
| `AUDIO_L` | Line-level audio (mono OK for v1) |

Bus aliases in KiCad: place a **Bus** `CPU_A[15..0]` and member aliases `CPU_A0`..`CPU_A15`. Same for `CPU_D[7..0]`, `CART_A[18..0]`, `CART_D[7..0]`.

---

## 2. KiCad project setup

1. **File -> New Project** in an empty folder, e.g. `hw/kicad/retr01_mobo/`.
2. Open the root schematic.
3. **Preferences -> Manage Symbol Libraries**: add a project library `Retr01` pointing at `hw/kicad/symbols/Retr01.kicad_sym` (create the file in Symbol Editor first).
4. **Preferences -> Manage Footprint Libraries**: add `Retr01` footprints for the 36-pin edge and TRS if not using stock.
5. Set schematic **Page Settings** title to `Retr01 Motherboard`, revision `0.1-32IC`.
6. Create hierarchical sheets listed in section 1.2 (**Insert -> Hierarchical Sheet**). Give each sheet a unique **Sheet name** matching the island letter.

Do not start layout until ERC is clean on power flags and hierarchical pins.

---

## 3. Creating symbols (custom parts)

### 3.1 Generic DIP from datasheet (most ICs)

For W65C02S, AS6C62256, ATF22V10, HC573, HC157, HC245, AT27C256R, ATmega1284P, ATmega328P, 74HC14:

1. Open **Symbol Editor**.
2. **File -> New Library** -> save as `Retr01.kicad_sym` under the project (or repo `hw/kicad/symbols/`).
3. **File -> Create Symbol**.
4. Name = manufacturer part, e.g. `W65C02S`.
5. Units: **1 unit**, not interchangeable, pin count matching PDIP.
6. Draw the rectangle. Place pins with **correct electrical type**:

| Electrical type | Use for |
|-----------------|---------|
| Input | Clocks, OE#, LE, DIR, address into SRAM/PROM |
| Output | Dedicated outs (PHI2O unused OK as Not Connected) |
| Bidirectional | Data buses D0-D7, DQ |
| Power input | VCC / VDD |
| Power output | Do not use on these parts |
| Passive | NC pins (W65C02S pin 35) |
| Open collector | Open-drain MCU UART DATA if modeled |

7. Pin **number** = PDIP pin number from [`hw/md/`](../hw/md/). Pin **name** = net-facing name (`A0`, `D0`, `OE#`, `RWB`).
8. Assign footprint: e.g. `Package_DIP:DIP-40_W15.24mm` for U1. Match body width in [`hw/md/packages_dip.md`](../hw/md/packages_dip.md).
9. Save. Place from the `Retr01` library in the schematic.

**Tip:** KiCad already has `74HC573`, `74HC157`, `74HC245`, `74HC14`, `ATMEGA1284P-PU`, `ATMEGA328P-PU` in stock libraries. Prefer stock symbols when the pinout matches. Still verify every pin against `hw/md`.

### 3.2 Custom symbol: 36-pin cart edge (`Conn_Cart36`)

Stock libraries do not ship the Retr01 A/B naming. Build one.

1. Symbol Editor -> Create Symbol `Conn_Cart_Edge_36`.
2. **2 units** optional (Unit A = side A, Unit B = side B) **or** one tall symbol with 36 pins.
3. Recommended: **one unit**, two columns of pins:

**Left column (Side A, cart top)**

| Pin num | Name | Type |
|---------|------|------|
| A1 | GND | Passive |
| A2 | VCC | Passive |
| A3 | SDA | Bidirectional |
| A4 | A0 | Bidirectional |
| A5 | A1 | Bidirectional |
| A6 | A2 | Bidirectional |
| A7 | A3 | Bidirectional |
| A8 | A4 | Bidirectional |
| A9 | A5 | Bidirectional |
| A10 | A6 | Bidirectional |
| A11 | A7 | Bidirectional |
| A12 | A8 | Bidirectional |
| A13 | A9 | Bidirectional |
| A14 | A10 | Bidirectional |
| A15 | A11 | Bidirectional |
| A16 | A12 | Bidirectional |
| A17 | A13 | Bidirectional |
| A18 | GND | Passive |

**Right column (Side B, cart bottom)**

| Pin num | Name | Type |
|---------|------|------|
| B1 | GND | Passive |
| B2 | VCC | Passive |
| B3 | SCL | Output (host drives) or Bidirectional |
| B4 | D0 | Bidirectional |
| B5 | D1 | Bidirectional |
| B6 | D2 | Bidirectional |
| B7 | D3 | Bidirectional |
| B8 | D4 | Bidirectional |
| B9 | D5 | Bidirectional |
| B10 | D6 | Bidirectional |
| B11 | D7 | Bidirectional |
| B12 | OE# | Output |
| B13 | A14 | Bidirectional |
| B14 | A15 | Bidirectional |
| B15 | A16 | Bidirectional |
| B16 | A17 | Bidirectional |
| B17 | A18 | Bidirectional |
| B18 | WE# | Output |

Pin **numbers** in KiCad must be unique. Use `A1`..`A18` and `B1`..`B18` as pin numbers (KiCad allows alphanumeric pin numbers) **or** map to 1..36 with a legend on the sheet. Prefer alphanumeric `A1`/`B1` so the schematic matches [`cart.md`](cart.md).

4. Footprint: start from a 2x18 2.54 mm dual-row or TE Standard Edge II footprint from the vendor. Lock A1 keying against the cart gold-finger drawing before PCB.

5. On the motherboard sheet, connect:

| Connector pin | Net |
|---------------|-----|
| A1, A18, B1 | `GND` |
| A2, B2 | `+5V` (after cart TVS policy) |
| A3 | `SDA` |
| B3 | `SCL` |
| A4-A17 | `CART_A0`-`CART_A13` |
| B13-B17 | `CART_A14`-`CART_A18` |
| B4-B11 | `CART_D0`-`CART_D7` |
| B12 | `CART_OE#` |
| B18 | `CART_WE#` (inactive high in play, pull-up) |

Full table: [`cart.md`](cart.md#36-pin-edge-pinout).

### 3.3 Custom symbol: Switchcraft 35RAPC TRS jack

1. Create symbol `Jack_TRS_35RAPC`.
2. Pins (minimum):

| Pin | Name | Type | Net |
|-----|------|------|-----|
| T | TIP | Passive | Port `+5V` after PPTC |
| R | RING | Bidirectional | `PAD1_DATA` or `PAD2_DATA` |
| S | SLEEVE | Passive | `GND` |
| SH | SHELL | Passive | `GND` (tie to sleeve) |

3. Footprint: use vendor 35RAPC footprint (example part **35RAPC3BH3**). Do not guess pad positions.

4. Populate **two** instances: `J_PAD1`, `J_PAD2`. DNP OK on arcade-only builds. Footprints stay on PCB ([`passive_rf_etc.md`](passive_rf_etc.md)).

**Locked cable map** ([`controllers.md`](controllers.md)):

| Conductor | Signal |
|-----------|--------|
| Tip | 5 V |
| Ring | DATA |
| Sleeve | GND |

### 3.4 Custom symbol: arcade controller header

Arcade pinout is **open** in [`hardware.md`](hardware.md) ("lock at schematic"). This guide **proposes** a default. Change only if cabinet harness needs different order.

Create `Header_Arcade_P1` / `Header_Arcade_P2` (10-pin IDC style):

| Pin | Name | `$FE60`/`$FE61` bit |
|-----|------|---------------------|
| 1 | RIGHT | 0 |
| 2 | LEFT | 1 |
| 3 | DOWN | 2 |
| 4 | UP | 3 |
| 5 | X | 4 |
| 6 | Y | 5 |
| 7 | COIN | 6 |
| 8 | START | 7 |
| 9 | +5V | (optional lamp / unused) |
| 10 | GND | common |

Each switch line: series **22-100 ohm**, then into 1284 GPIO with internal pull-up (firmware) or external **10 kohm** to `+5V`. Switch other side to `GND`.

### 3.5 Power symbols and flags

- Place `+5V` and `GND` power symbols on every sheet.
- Place **PWR_FLAG** on `+5V` at the barrel output (after PPTC / ideal diode) so ERC knows the rail is sourced.
- Place **PWR_FLAG** on `GND` at the barrel return.

---

## 4. Sheet A: Power and clocks

### 4.1 Power entry

Draw in order from jack to pour:

```text
Barrel tip --> PPTC --> reverse diode (or P-FET) --> bulk 100-470 uF --> ferrite --> +5V
Barrel sleeve -------------------------------------------------------> GND
```

| Ref | Part | Notes |
|-----|------|-------|
| J_BARREL | 2.1 mm barrel | Center positive 5 V |
| F1 | PPTC | Ihold above full-board idle |
| D1 | Schottky or ideal diode | Reverse plug |
| C_BULK | 100-470 uF low-ESR | At entry |
| FB1 | Ferrite on 5 V | Before pour |
| C_LOCAL | 100 nF + 1-10 uF | After ferrite into `+5V` |

Analog spur:

```text
+5V --> FB_ANA --> +5V_ANA
              +--> 10 uF to GND
```

Feed Color PROM R-2R and APU DAC from `+5V_ANA` only.

### 4.2 Clocks

| Ref | Part | Out net | Dest |
|-----|------|---------|------|
| Y1 | Canned osc 8.000 MHz | `PHI2_RAW` | Series 22-47 ohm -> `PHI2` -> U1 pin 37. Also to decode / VRAM PLD clocks as needed |
| Y2 | Canned osc 5.369318 MHz | `DOT_RAW` | Series 22-47 ohm -> `DOT` -> beam PLDs |

Optional: run `PHI2_RAW` / `DOT_RAW` through U19 (74HC14) sections for cleanup. If used, U19 is support logic outside or inside BOM per your freeze.

AVR clocks:

| MCU | Clock |
|-----|-------|
| U15 1284 | 20 MHz crystal + load caps on XTAL1/XTAL2 **or** canned osc into XTAL1 (check fuse / clock source) |
| U18 328P | 16 MHz crystal + load caps |

Keep crystal loops tiny. No long stubs.

### 4.3 Reset

```text
+5V --[10k]--+-- RESB (CPU pin 40, AVR RESET pins)
             |
            |/  open-drain supervisor or HC14 + RC
             |
            GND  (hold low until rail OK)
```

| Item | Value |
|------|-------|
| Pull-up on `RESB` | 4.7-10 kohm |
| Pull-up on `IRQB` | 4.7-10 kohm |
| Pull-up on `NMIB` | 4.7-10 kohm |
| Pull-up on `RDY` | 4.7-10 kohm (tie inactive high for play) |

Supervisor example class: MCP120. Exact PN at BOM freeze.

### 4.4 U19 74HC14 (if used)

| Pin | Connection |
|-----|------------|
| 14 | `+5V` |
| 7 | `GND` |
| 1A/1Y .. | Reset Schmitt, clock buffers as drawn |
| Unused inputs | Tie to `GND` or `+5V` (never float) |

Decouple: **100 nF** at pin 14.

---

## 5. Sheet C: CPU, system RAM, decode, CPU HC245

### 5.1 U1 W65C02S (40-pin) pin map

Pinout from [`hw/md/W65C02S.md`](../hw/md/W65C02S.md):

| Pin | Name | Net / tie |
|-----|------|-----------|
| 1 | VPB | NC or test pad |
| 2 | RDY | `RDY` (pull-up) |
| 3 | PHI1O | NC |
| 4 | IRQB | `IRQB` |
| 5 | MLB | NC or test |
| 6 | NMIB | `NMIB` |
| 7 | SYNC | test pad optional |
| 8 | VDD | `+5V` + 100 nF |
| 9-20 | A0-A11 | `CPU_A0`-`CPU_A11` |
| 21 | VSS | `GND` |
| 22-25 | A12-A15 | `CPU_A12`-`CPU_A15` |
| 26-33 | D7-D0 | `CPU_D7`-`CPU_D0` (note PDIP order D0 on 33) |
| 34 | RWB | `RWB` |
| 35 | NC | leave open |
| 36 | BE | `+5V` (bus on) |
| 37 | PHI2 | `PHI2` |
| 38 | SOB | `+5V` (unused, inactive) |
| 39 | PHI2O | NC |
| 40 | RESB | `RESB` |

**Data pin order check:** PDIP lists D0 on pin 33 through D7 on pin 26. Label nets carefully.

### 5.2 U2 AS6C62256 system RAM

Pinout [`hw/md/AS6C62256.md`](../hw/md/AS6C62256.md):

| SRAM pin | Net |
|----------|-----|
| A0-A14 | `CPU_A0`-`CPU_A14` |
| DQ0-DQ7 | `CPU_D0`-`CPU_D7` (through CPU HC245 policy: sys RAM usually on CPU side) |
| CE# | `RAM_CE#` from decode PLD (active when `$0000-$7FFF`) |
| OE# | qualify with read: often `RWB` gated (active low OE when read) via PLD |
| WE# | qualify with write: active when `RWB` low and CE# low, PHI2-safe via PLD |
| VCC / VSS | `+5V` / `GND` + 100 nF |

System RAM is **CPU-only**. Never connect VRAM or linebuf CE# here.

### 5.3 U4 SN74HC245 CPU domain

| Pin | Net |
|-----|-----|
| DIR | From decode PLD: high = CPU drives peripherals, low = read-back path as designed |
| OE | `CPU_245_OE#` from decode (only one domain driver) |
| A1-A8 | `CPU_D0`-`CPU_D7` |
| B1-B8 | `PERIPH_D0`-`PERIPH_D7` (shared peripheral data toward latches / APU / 1284 ports) |
| VCC / GND | `+5V` / `GND` + 100 nF |

**Bus rule:** only one HC245 OE low on a shared segment at a time ([`hardware.md`](hardware.md)).

### 5.4 U3 ATF22V10 decode (role DEC)

Resources: 10 I/O macrocells, CLK pin 1 ([`hw/md/ATF22V10.md`](../hw/md/ATF22V10.md)).

**Required inputs (minimum):**

| Signal | Use |
|--------|-----|
| `CPU_A8`-`CPU_A15` (at least A8-A15 for `$FExx` / RAM / cart) | Address decode |
| `RWB` | Read/write qualify |
| `PHI2` | Timing gates |
| `BE` | Optional qualify |
| `RESB` | Optional clear |

**Required outputs (minimum):**

| Signal | Use |
|--------|-----|
| `RAM_CE#` | System RAM |
| `CART_PRG_OE#` or gate into `CART_OE#` | PRG read when `$8000+` and not `$FExx` |
| `SEL_FE00` .. strobes | Pulse LE on U5A-U5I and select MCU windows |
| `CPU_245_OE#` `DIR` helps | Bus isolation |
| `APU_CS#` | `$FE40-$FE5F` window to U18 |
| `MCU1284_CS#` | `$FE20`/`$FE21`, `$FE60`/`$FE61`, mailboxes |

**Pin assignment worksheet (fill in CUPL later):**

| ATF22V10 pin | Function (assign) |
|--------------|-------------------|
| 1 | CLK = `PHI2` (if registered decode) or unused input |
| 2-11, 13 | Inputs: address, RWB, ... |
| 14-23 | Outputs: CE# / SEL / OE |
| 12 | GND |
| 24 | VCC + 100 nF |

Exact fuse map is **not** in repo yet. Capture the symbol with **named pins** matching your CUPL pin constraints file. Do not leave PLD I/O floating. Unused inputs tie per Atmel keeper guidance (defined level).

Decode also creates the `$FE00-$FEFF` hole in PRG so cart flash is not selected on I/O cycles.

---

## 6. Sheet D: Nine HC573 latches

Silicon map ([`graphics.md`](graphics.md#hc573-latch-map-9-chips)):

| Ref | Port | Q bus feeds |
|-----|------|-------------|
| U5A | `$FE00` | `PPUCTRL[7:0]` to beam / compositor / NMI enable |
| U5B | `$FE02` | `SCR1_X[7:0]` BG1 scroll X |
| U5C | `$FE03` | `SCR1_Y[7:0]` BG1 scroll Y |
| U5D | `$FE04` | `RAST_Y[7:0]` compare to beam Y |
| U5E | `$FE05` | `RAST_CTL[7:0]` |
| U5F | `$FE08` | `PAL_ADDR[7:0]` |
| U5G | `$FE90` | `MAP_A0_7` |
| U5H | `$FE91` | `MAP_A8_15` |
| U5I | `$FE92` | `MAP_A16_23` (low bits used for flash seek) |

### 6.1 Each SN74HC573 wiring template

From [`hw/md/SN74HC573.md`](../hw/md/SN74HC573.md):

| Pin | Net |
|-----|-----|
| OE | `GND` (outputs always enabled toward PLDs) **or** PLD-qualified if you must Hi-Z |
| 1D-8D | `PERIPH_D0`-`PERIPH_D7` (same order every chip) |
| LE | `LE_FExx` from decode (transparent when high, latch on falling edge per HC573) |
| 1Q-8Q | Named byte nets above |
| VCC / GND | `+5V` / `GND` + **100 nF per chip** |

**LE polarity:** decode must present a write strobe compatible with HC573 (LE high while data valid, then low to hold). Match sim `wire_io` intent: pulse on `STA $FExx`.

### 6.2 Sim vs silicon note

Sim still ties some VRAM address bytes to HC573 ([`hardware.md`](hardware.md) runners table). **Silicon latch map above is normative for this schematic.** VRAM `$FE10`-`$FE12` use the interleave path on sheet G, not U5A-U5I.

---

## 7. Sheet G: VRAM + three HC157 + VRAM PLD

### 7.1 Interleave rule (do not break)

From [`memory.md`](memory.md):

```text
PHI2 high (CPU) -> mux selects CPU VRAM address, CPU may R/W data
PHI2 low  (PPU) -> mux selects beam/BG VA, nametable read
```

### 7.2 U7A U7B U7C SN74HC157 (VRAM)

Each chip muxes 4 bits ([`hw/md/SN74HC157.md`](../hw/md/SN74HC157.md)):

| Chip | A inputs (CPU) | B inputs (beam) | Y outputs |
|------|----------------|-----------------|-----------|
| U7A | `VA_CPU0`-`3` | `VA_PPU0`-`3` | `VRAM_A0`-`3` |
| U7B | `VA_CPU4`-`7` | `VA_PPU4`-`7` | `VRAM_A4`-`7` |
| U7C | `VA_CPU8`-`11` | `VA_PPU8`-`11` | `VRAM_A8`-`11` |

| Pin | Net |
|-----|-----|
| A/B select | `VRAM_MUX_SEL` from U8 (VRAM PLD), tracking PHI2 phase |
| G (strobe) | `GND` (always enabled) unless PLD needs force-low |
| VCC / GND | + decoupling |

Extend `VRAM_A12`-`VRAM_A14` with extra mux bits or PLD helpers if full 32 KB used. Slots in docs use low VRAM. Wire all 15 address pins of U6 consistently.

**CPU address source:** latched / registered `$FE10`/`$FE11` path as frozen in your decode (sim today uses HC573 for some of this). Pick one silicon approach and label nets `VA_CPU*`.

**PPU address source:** BG fetch from beam + scroll (U9/U10 + scroll latches). Label `VA_PPU*`.

### 7.3 U6 VRAM AS6C62256

| Pin | Net |
|-----|-----|
| A* | `VRAM_A*` |
| DQ* | `VRAM_D*` (isolated via video HC245 / PLD OE as designed) |
| CE# | `VRAM_CE#` from U8 |
| OE# / WE# | Phase-qualified by U8 (CPU write only on CPU phase) |

### 7.4 U8 ATF22V10 VRAM glue

Jobs: drive `VRAM_MUX_SEL`, `VRAM_CE#`, OE/WE qualifies, cooperate with cart CHR OE so flash is not fought.

Inputs: `PHI2`, decode selects, beam fetch requests, maybe `DOT`.  
Outputs: mux select, SRAM controls, enables toward sheet O/J.

---

## 8. Sheet H: Beam PLDs

### 8.1 U9 beam X (ATF22V10)

| Need | Notes |
|------|-------|
| Clock | `DOT` on pin 1 (registered counters) |
| Outputs | `BEAM_X0`-`BEAM_X8` (0..340), `HBLANK`, maybe `HSYNC` |
| Inputs | `RESB`, maybe `PPUCTRL` bits |

Nine X bits almost fill one 22V10. Keep equations skinny. Escape **+1 PLD** if fit fails ([`hardware.md`](hardware.md)).

### 8.2 U10 beam Y (ATF22V10)

| Need | Notes |
|------|-------|
| Clock | line clock from U9 (HBLANK edge) or shared `DOT` with enables |
| Inputs | `RAST_Y*` from U5D, `PPUCTRL` NMI_EN, `RESB` |
| Outputs | `BEAM_Y*`, `VBLANK`, `EQ#` (raster match), `NMI_PULSE#` |

Wire `EQ#` -> `IRQB` (open-drain style: PLD active low into pulled-up `IRQB`).  
Wire VBlank NMI edge -> `NMIB` when `PPUCTRL` bit 7 set (gate in PLD).

Sim reference: `beam_xy` + `pld_beam_y` in [`retr01/sim/`](../retr01/sim/README.md).

---

## 9. Sheet O: Compositor, Color PROM, video HC245, RGBS

### 9.1 U11 compositor ATF22V10

| Inputs | Priority path |
|--------|---------------|
| Sprite pixel index / opaque | from linebuf read |
| BG1 index | from BG fetch |
| BG0 index | from linebuf BG0 half |
| Enables from `PPUCTRL` | L1/L0/SPR |

| Outputs | |
|---------|--|
| `PROM_A0`-`PROM_A5` | 6-bit master index |
| optional flags | |

Priority ([`graphics.md`](graphics.md)): sprite > BG1 > BG0 (if BG1 index 0) > backdrop.

### 9.2 U12 AT27C256R

| Pin | Net |
|-----|-----|
| A0-A5 | `PROM_A0`-`PROM_A5` |
| A6-A14 | **GND** (unused) |
| DQ0-DQ7 | `PROM_D0`-`PROM_D7` packed R3G3B2 |
| CE# OE# | active for video read (tie appropriately, often both enabled in play) |
| VCC | `+5V_ANA` preferred |
| VSS | `GND` |

### 9.3 R-2R ladder (R3G3B2)

Packing: `{RRRGGGBB}` on `PROM_D7`..`PROM_D0`.

Per gun, classic R-2R (use **1%** metal film, `+5V_ANA` referenced bits through CMOS levels):

**Example red (3 bits D7 D6 D5):**

```text
D7 -- R --+-- R --+-- R --+---- RGB_R
          |       |       |
         2R      2R      2R
          |       |       |
         GND     GND     GND
```

Same for green on D4 D3 D2. Blue is **2 bits** D1 D0 (shorter ladder or weighted R/2R).

Then:

```text
RGB_R --[75 ohm]-- GND   (termination to ground for ~0.7 Vpp into 75 ohm plant)
```

Repeat for G and B. Sync: derive `CSYNC` from HBLANK/VBLANK logic (PLD or HC glue). Feed AV connector. Optional ferrites at jack ([`passive_rf_etc.md`](passive_rf_etc.md)).

Exact R value (e.g. 10k / 20k network) is bench-tuned. Keep impedance consistent per gun.

### 9.4 U13 video HC245

Isolate video data domain from CPU when peeking or when compositor shares nets. DIR/OE from decode or video PLD.

### 9.5 SCALE DIP

| Part | Function |
|------|----------|
| SW_SCALE | 1x / 2x logical scale ([`hardware.md`](hardware.md)) |
| Pull-up/down | Define idle = **2x** default |

SCALE does not change 341x262 timing. It only changes how logical 128x120 maps into the RGBS active field (compositor / sink policy).

---

## 10. Sheet J: Cart transceiver + edge + ESD

### 10.1 U14 cart-OAM HC245

| Side A | Side B |
|--------|--------|
| `PERIPH_D*` or CPU data | `CART_D*` |

OE/DIR from decode so PRG reads, MAP reads, and programming never fight CHR fetches. CHR OE windows: flash OE denied when CHR fetch owns the cart ([`hw/md/ATF22V10.md`](../hw/md/ATF22V10.md) sim notes).

### 10.2 `CART_WE#`

Pull-up **10 kohm** to `+5V`. Motherboard leaves it high in play. Flasher drives it when cart is in the **flasher** socket (not during normal console play).

### 10.3 ESD

At connector: PESD5V0-class arrays on address/data/control as needed. Series 22-100 ohm on slower lines if EMI requires ([`passive_rf_etc.md`](passive_rf_etc.md)).

### 10.4 MAP seek

U5G-U5I Q outputs form the 24-bit seek used with `$FE93` auto-inc read path into flash (decode + OE). Wire seek bits onto `CART_A*` when MAP selected. PRG uses `CPU_A*` mapped into cart address with A15.. for `$8000` region. Document the address mux/PLD equations in the decode/VRAM PLD pair.

---

## 11. Sheet L: ATmega1284P, linebuf, muxes, pads

### 11.1 Proposed GPIO lock (schematic freeze proposal)

Docs say exact GPIO is schematic TBD. **Proposal** for first spin (change only with a doc update):

| 1284 port | Assignment |
|-----------|------------|
| PA0-PA7 | Arcade P1 bits 0-7 (RIGHT..START) |
| PB0-PB7 | Arcade P2 bits 0-7 |
| PC0 | SCL (`TWI`) |
| PC1 | SDA (`TWI`) |
| PD0/PD1 | UART0 open-drain tied for **PAD1** DATA (with external 4.7k pull-up) |
| PD2/PD3 | UART1 open-drain tied for **PAD2** DATA |
| remaining PC/PD/PB | Linebuf address/data bit-bang or parallel as fitted |

Linebuf needs address + data + CE/OE/WE. Prefer a full port for `LB_D0`-`LB_D7` and enough pins for `LB_A*` before mux, **or** multiplex carefully in firmware without missing HBlank. If pins run out, add HC573/HC574 **only** if you accept leaving the 32 count (avoid). Prefer fitting in 32 GPIO.

**VBlank / HBlank:** wire `VBLANK` and `HBLANK` from beam PLDs into 1284 INT pins (e.g. INT0/INT1) so firmware meets budgets in [`graphics.md`](graphics.md).

### 11.2 U16 linebuf SRAM + U17A-C HC157

Same mux pattern as VRAM:

| Select | Master |
|--------|--------|
| A | 1284 linebuf address |
| B | beam read address |
| Y | `LB_A*` into U16 |

1284 writes sprite field in **VBlank**, BG0 line in **HBlank**. Beam reads during active display. Do not let both masters drive Y without mux.

### 11.3 Software ports (no extra ICs)

| Port | Function |
|------|----------|
| `$FE20`/`$FE21` | OAM |
| `$FE22`-`$FE24` | Cart EEPROM mailbox |
| `$FE60`/`$FE61` | Pads |
| `$FE70`-`$FE72` | Machine EEPROM mailbox |

Decode PLD selects 1284. Firmware implements registers.

### 11.4 TRS port passives (per jack)

```text
+5V --[PPTC]--+--[TVS]-- Tip
             +-- 100 nF -- GND
GND ------------- Sleeve
1284 DATA --[R]--[TVS]-- Ring
+5V --[4.7k]-- Ring (pull-up once on mobo DATA)
```

Details: [`passive_rf_etc.md`](passive_rf_etc.md), [`controllers.md`](controllers.md).

---

## 12. Sheet K: ATmega328P APU

### 12.1 Bus window

`$FE40`-`$FE5F` ([`sound.md`](sound.md)). Decode asserts `APU_CS#`. CPU data via CPU HC245 into 328P PORT (proposed: **PORTD** = data, **PB0** = write strobe, **PB1** = addr latch low nibble). Mark final pins on the schematic legend.

### 12.2 Audio out

Firmware mixes to 8-bit code. Hardware:

```text
328P PORT -- R-2R --[AC cap]--[series R]-- AUDIO jack
                 |
              +5V_ANA reference / bias as designed
```

PWM/RC alternate is allowed ([`sound.md`](sound.md)) but pick **one** for v1 schematic.

### 12.3 Power / clock / reset

| Pin | Net |
|-----|-----|
| VCC / AVCC | `+5V` (+ ferrite to AVCC if analog noisy) |
| GND / AGND | `GND` |
| RESET | `RESB` or dedicated AVR reset rail |
| XTAL | 16 MHz |

100 nF at VCC. 1-10 uF island cap.

**Rule:** 1284 does **not** synthesize audio ([`sound.md`](sound.md)).

---

## 13. Passive checklist (every IC)

| Item | Rule |
|------|------|
| 100 nF X7R | Each VCC pin, millimeters from pin |
| 1-10 uF | Per island / large DIP cluster |
| Pull-ups | RESB, IRQB, NMIB, RDY, CART_WE#, I2C if needed |
| Unused CMOS inputs | Tied (HC / PLD / AVR) |
| TVS | Cart edge, TRS, exposed AV |
| Series R | Pad lines, clock dampers 22-47 ohm |

Order-of-magnitude counts: [`passive_rf_etc.md`](passive_rf_etc.md#passive-count-mindset-planning).

---

## 14. Cart module schematic (separate file OK)

On the **cartridge** PCB (not mobo 32 count except 24C64 in system tally):

### 14.1 SST39SF040

Pinout [`hw/md/SST39SF040.md`](../hw/md/SST39SF040.md). Wire A18..A0 and DQ to edge fingers per [`cart.md`](cart.md).  

`CE#` (pin 22): tie **GND** (active) on cart. Mobo gates `OE#` / `WE#`.

### 14.2 24C64

| Pin | Net |
|-----|-----|
| SDA / SCL | Edge A3 / B3 |
| A0 A1 A2 | Address straps (usually all GND -> fixed 7-bit addr) |
| VCC / WP | `+5V` / WP per save policy (WP high or low) |
| VSS | GND |

---

## 15. Capture order (recommended)

Follow sim / bring-up smoke so ERC matches islands you can test:

1. A Power + clocks + reset (LED optional on `+5V`)
2. C CPU + U2 RAM + U3 decode stub + U4 (ROM/cart PRG read next)
3. D latches one-by-one with `LE_*` from decode
4. J cart connector + U14 + PRG path
5. G VRAM interleave
6. H beam -> IRQB / NMIB
7. O compositor + PROM + R-2R
8. L 1284 + linebuf
9. K APU
10. Ports (arcade + TRS)

Island pass criteria: [`hardware.md`](hardware.md#protoboard-bring-up), sim tests `test_island_abcdeghiojklmnp.c`.

---

## 16. ERC and annotation

Before PCB:

1. **Annotate schematic** (U-numbers match section 1.1).
2. **Electrical Rules Check**: fix power flags, disconnected pins, stacked pins.
3. No floating CMOS inputs.
4. Hierarchical labels match parent sheet exactly (`PHI2`, `CPU_D0`, ...).
5. Netlist export and compare bus widths (15-bit VRAM vs 16-bit CPU).
6. Cross-check every IC against `hw/md` pin tables.
7. Confirm **32** IC tally (plus U19 policy) against [`hardware.md`](hardware.md).

---

## 17. Still open (do not silently invent forever)

| Topic | Action on schematic |
|-------|---------------------|
| Arcade header order | Use section 3.4 proposal or revise with cabinet harness |
| 1284 / 328P exact GPIO | Section 11 / 12 proposals. Update `hw/md` when frozen |
| PLD CUPL pin maps | Fill worksheets in sections 5.4, 7.4, 8, 9.1 |
| BG0 scroll `$FE06`/`$FE07` silicon path | Docs: TBD vs Host Play. Add latches or 1284 regs only with a hardware.md update |
| RGBS analog R values | Bench on first spin |
| Flasher USB protocol | Out of scope for this mobo schematic |

---

## 18. Quick reference: sim island -> sheet

| Sim canvas | Schematic sheet |
|------------|-----------------|
| A | `01_power_clk` |
| C | `02_cpu_ram` |
| D | `03_latches` |
| G | `04_vram` |
| H | `05_beam` |
| O | `06_video` |
| J | `07_cart_edge` |
| L | `08_mcu_linebuf` |
| K | `09_apu` |
| N (cart module) | Cart project / `cart_module` |
| F flasher | **Not** on motherboard |

Run `./sim` with the default cart to see islands and refdes placement while you draw. Pin-level behavior must stay consistent with `hw/md` ([`ai-rules.txt`](../ai-rules.txt) rule on sim netlist).

---

## 19. Document maintenance

When you freeze a GPIO map or PLD pin file:

1. Update this doc.
2. Update the matching `hw/md` note.
3. Update [`hardware.md`](hardware.md) open-topics rows.
4. Keep runners-vs-silicon table honest if Sim still differs.
