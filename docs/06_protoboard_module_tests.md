# Retr01-A Protoboard Module Tests

Step-by-step bring-up for **Retr01-A v0** on solderless protoboards (or small proto PCBs). Each section is one **island**: a subset of ICs you can test **before** wiring the full 52-chip motherboard.

Hardware context: [`03_hardware_implementation.md`](03_hardware_implementation.md). Memory map and `$FExx` ports: [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md).

Do **not** try to fit the entire 8 MHz machine on one breadboard. Build islands, prove each one, then combine on a larger board or PCB.

---

## 1. Before you start

### Tools

| Tool | Use |
|------|-----|
| Bench supply | 5 V, current limit ~200 mA to start |
| Logic probe or cheap 8-channel analyzer | Address/data activity, `/CS`, PHI2 |
| Oscilloscope (2 ch minimum) | Clocks, reset, `/WE`, dot clock |
| Multimeter | Continuity, rail voltage |
| TL866 or similar | Burn ATF22V10 `.jed`, cart flash, AVR firmware |
| LED + 1 k ohm | Power-on indicator |

Optional: 15.7 kHz **RGBS** monitor or scope with TV line mode for video islands.

### Ground rules

- **5 V only** on v0. No 3.3 V **74LV** parts.
- **W65C02S** (not NMOS 6502): pin **36 `BE` = high**, pin **`RDY` = high**, pins **1 `VPB`** and **5 `MLB`** NC. Full DIP-40 table: **section 3.2**.
- Clock input is **`PHI2`**, not phi0/phi1 split like some older docs.
- **One bus driver at a time.** If two chips fight `D0-D7`, stop - fix mux/`/OE` before adding ICs.
- **100 nF ceramic** from VCC to GND at **every IC**, as close as the breadboard allows.
- Do **not** connect **1284 CHR** and **BG PPU CHR** until each side works alone.
- First CPU bring-up: clock **1-2 MHz** is fine if **8 MHz** rings on long wires. Move to 8 MHz when stable.

### Pass definition

**Pass** means the checks listed for that module - not "the game runs."

---

## 2. Recommended order

Build and validate in this sequence. Later islands assume earlier ones pass.

```text
A Power
B Clocks + reset
C CPU + system RAM + tiny PRG ROM
D $FExx decode + one latch
E Controller pads ($FE60)
F Board EEPROM ($FE7x) [optional early]
G VRAM port + interleave mux [critical - no PPU yet]
H Dot clock + beam counters
I BG nametable fetch (minimal) [uses G + H]
J Cart flash stub (PRG/CHR/MAP) [can split from C / I]
K ATmega328P APU alone [simavr/Wokwi first]
L ATmega1284P alone [simavr/Wokwi first]
M Line-buffer SRAM
N 1284 + line buffer + CHR stub
O Palette + Color PROM + compositor + RGBS [minimal DAC]
P Integration board
```

**Parallel paths:** **K** and **L** firmware can be developed on **simavr** or **Wokwi** while you breadboard **A-I**. Merge at **N** and **P**.

Each module below lists **which ICs** it uses. Pin names, datasheet links, and Retr01 wiring notes are in **section 3**.

---

## 3. IC reference - datasheets and pins

Use the **official PDF** for timing and AC specs. This section lists **Retr01-A v0** pin connections for breadboard islands.

### 3.1 Datasheet index

| IC | Package | Role | Datasheet |
|----|---------|------|-----------|
| **W65C02S6TPG-14** | DIP-40 | CPU | [WDC W65C02S PDF](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf) |
| **AS6C62256-55PCN** | DIP-28 | 32 KB SRAM (x3) | [Alliance AS6C62256 PDF](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf) |
| **ATF22V10CQZ-20PU** | DIP-24 | PLD decode / timing / PPU gating | [Microchip ATF22V10 PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf) |
| **ATmega1284P-PU** | DIP-40 | Sprites + pads | [Microchip ATmega1284P PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf) |
| **ATmega328P-PU** | DIP-28 | APU | [Microchip ATmega328P PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf) |
| **AT28C64B-15PU** | DIP-28 | 8 KB board EEPROM | [Microchip AT28C64B PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/doc4428.pdf) |
| **AT28C16** (class) | DIP-24 | Color PROM x3 (R/G/B master palette) | [Microchip AT28C16 PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0006.pdf) |
| **SST39SF040** (class) | DIP-32 | Cart parallel flash (planning) | [Microchip SST39SF040 PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf) |
| **SN74HC157N** | DIP-16 | 2:1 address mux | [TI SN74HC157 PDF](https://www.ti.com/lit/ds/symlink/sn74hc157.pdf) |
| **SN74HC245N** | DIP-20 | Bus transceiver | [TI SN74HC245 PDF](https://www.ti.com/lit/ds/symlink/sn74hc245.pdf) |
| **SN74HC573N** | DIP-20 | Octal latch | [TI SN74HC573 PDF](https://www.ti.com/lit/ds/symlink/sn74hc573.pdf) |
| **SN74HC161N** | DIP-16 | 4-bit counter | [TI SN74HC161 PDF](https://www.ti.com/lit/ds/symlink/sn74hc161.pdf) |
| **SN74HC14N** | DIP-14 | Schmitt inverter (reset) | [TI SN74HC14 PDF](https://www.ti.com/lit/ds/symlink/sn74hc14.pdf) |
| **SN74HC688N** | DIP-20 | 8-bit identity compare | [TI SN74HC688 PDF](https://www.ti.com/lit/ds/symlink/sn74hc688.pdf) |
| **SN74HC00N** | DIP-14 | NAND (glue) | [TI SN74HC00 PDF](https://www.ti.com/lit/ds/symlink/sn74hc00.pdf) |
| **SN74HC04N** | DIP-14 | Inverter (glue) | [TI SN74HC04 PDF](https://www.ti.com/lit/ds/symlink/sn74hc04.pdf) |
| **SN74HC08N** | DIP-14 | AND (glue) | [TI SN74HC08 PDF](https://www.ti.com/lit/ds/symlink/sn74hc08.pdf) |
| **SN74HC32N** | DIP-14 | OR (glue) | [TI SN74HC32 PDF](https://www.ti.com/lit/ds/symlink/sn74hc32.pdf) |
| **SN74HC86N** | DIP-14 | XOR (glue) | [TI SN74HC86 PDF](https://www.ti.com/lit/ds/symlink/sn74hc86.pdf) |

Pinout diagrams for 74HC parts: see the **Pin Configuration** section in each TI PDF.

KiCad symbols (community): [Alarm-Siren 6502 library](https://github.com/Alarm-Siren/6502-kicad-library) (W65C02S).

### 3.2 W65C02S - DIP-40 (module C+)

**Do not** use NMOS 6502 or W65C816 pinouts. Retr01 straps:

| Pin | Name | Retr01 wiring |
|-----|------|----------------|
| 1 | VPB | **NC** |
| 2 | RDY | **3.3 k ohm -> +5 V** |
| 3 | PHI1O | NC (optional scope) |
| 4 | IRQB | **3.3 k ohm -> +5 V**. GAL-TIM can pull low |
| 5 | MLB | **NC** |
| 6 | NMIB | **3.3 k ohm -> +5 V**. GAL-TIM VBlank pulse |
| 7 | SYNC | NC (debug optional) |
| 8 | VDD | +5 V + **100 nF** |
| 9-16 | A0-A7 | Address bus |
| 17-20 | A8-A11 | Address bus |
| 21 | VSS | GND |
| 22-25 | A12-A15 | Address bus |
| 26-33 | D7-D0 | Data bus (D7 = pin 26) |
| 34 | RWB | To decode / RAM `/WE` qualify |
| 35 | NC | - |
| 36 | BE | **3.3 k ohm -> +5 V** (always enabled) |
| 37 | **PHI2** | **8 MHz oscillator out** |
| 38 | SOB | **3.3 k ohm -> +5 V** |
| 39 | PHI2O | NC (optional) |
| 40 | RESB | HC14 output. RC + button per section 4 |

Active levels: **`RESB`**, **`IRQB`**, **`NMIB`** are **low** active. **`RWB` high = read**.

### 3.3 AS6C62256 - DIP-28 (modules C, G, M)

One chip = **32 K x 8** (address **A0-A14**). Three chips on Retr01-A: system RAM, VRAM, line buffer.

| Pin | Name | Retr01 wiring |
|-----|------|----------------|
| 1-7 | A7-A1 | Address (see PDF pin order) |
| 8 | A0 | Address |
| 9-11, 13-17 | I/O0-I/O7 | Data bus (module-specific) |
| 12 | VSS | GND |
| 18 | `/CE` | From GAL decode (active **low**) |
| 19 | A10 | Address |
| 20 | `/OE` | Read strobe (active **low**) |
| 21 | `/WE` | Write strobe (active **low**. On VRAM, qualify with PHI2 / GAL-PPU) |
| 22-27 | A9, A8, A11-A14 | Address |
| 28 | VCC | +5 V + **100 nF** |

**System RAM (module C):** A0-A14 <- CPU A0-A14. I/O0-7 <-> CPU D0-7. `/CE` <- `!A15`. `/OE` <- read cycle. `/WE` <- write cycle + `RWB`.

**VRAM (module G):** A0-A14 <- **4x 74HC157** mux (CPU latched addr vs PPU addr). I/O <-> **74HC245** to CPU on CPU phase only. `/CE`, `/OE`, `/WE` qualified by GAL-PPU + PHI2.

**Line buffer (module M):** Same SRAM pinout. Only addresses **$000-$1FF** used. A8 <- ping-pong half bit. A0-7 <- beam X or 1284 writer.

Full pin-to-pin order is on **PDF page 1** - match **PCN** silkscreen, not generic 62256 clones, if labels differ.

### 3.4 SN74HC573 - DIP-20 (modules D, G, O)

Octal D latch. Used for scroll, VRAM addr, `$FExx` latches, palette.

| Pin | Name | Use |
|-----|------|-----|
| 1 | `/OE` | Tie **GND** (outputs always enabled) or CPU read gating |
| 2-9 | D0-D7 | CPU data bus (write) |
| 10 | GND | |
| 11-18 | Q0-Q7 | To register / PPU inputs |
| 19 | **LE** | **Latch enable** (active **high**, not `/LE`): high = transparent, **high->low** latches |
| 20 | VCC | +5 V |

**Module D example (`$FE02` scroll X):** `LE` <- decode(`$FE02`) AND write AND PHI2. D0-7 <- CPU D0-7. Q0-7 -> scroll X bits.

### 3.5 SN74HC245 - DIP-20 (module G)

8-bit bus transceiver. **`DIR`** high = A->B (CPU->VRAM for writes in the usual island wiring). **`/OE`** low = active.

| Pin | Name | Retr01 VRAM island |
|-----|------|---------------------|
| 1 | **DIR** | Direction (not `/DIR`). High = A->B toward VRAM for CPU writes |
| 2-9 | **A0-A7** | To CPU D0-D7 |
| 10 | **GND** | |
| 11-18 | **B0-B7** | To VRAM I/O |
| 19 | `/OE` | **High = high-Z**. Enable only on CPU VRAM phase |
| 20 | VCC | |

See [SN74HC245 PDF](https://www.ti.com/lit/ds/symlink/sn74hc245.pdf) for the complete pin map.

### 3.6 SN74HC157 - DIP-16 (modules G, M)

Quad 2-input mux. **One IC = 4 bits.** VRAM needs **4x 157** for 15-bit address (+1 bit tied or expanded).

| Pin | Name | Use |
|-----|------|-----|
| 1 | `/E` | **GND** (enable) |
| 2 | I0a | CPU address bit |
| 3 | I1a | PPU address bit |
| 4-6 | Za, I0b, I1b | Next bit pair |
| 7 | Zb | Out |
| 14 | S | **Select:** 0 = I0, 1 = I1 - tie to **PHI2** or **!PHI2** (document choice) |
| 15 | VCC | |
| 8 | GND | |

**Module G:** S <- PHI2 polarity so CPU phase selects latched `$FE10`/`$FE11` address. PPU phase selects beam/nametable address.

### 3.7 SN74HC161 - DIP-16 (module H)

4-bit sync counter. **Four ICs** chain for X/Y (or two for X, two for Y).

| Pin | Name | Use |
|-----|------|-----|
| 1 | `/CLR` | High run, GAL sync at frame/line start |
| 2 | CLK | **Dot clock** ~5.369 MHz |
| 3-6 | **A, B, C, D** | Parallel **load inputs** (not outputs). Tie or drive only if `/LOAD` used |
| 7 | ENP | Count enable |
| 8 | GND | |
| 9 | `/LOAD` | High (don't parallel load in basic island) |
| 10 | ENT | Count enable |
| 11-14 | **QA-QD** | Counter **outputs** (Q0-Q3) |
| 15 | RCO | Ripple carry **out** -> next 161 ENT/ENP or GAL wrap |
| 16 | VCC | |

Wrap at **341** (X) and **262** (Y) via **74HC688** compare or GAL-TIM, not by letting 161 free-run forever.

### 3.8 ATF22V10CQZ-20PU - DIP-24 (modules C, D, H, J)

Programmable logic. **Must be burned** with `.jed` from CUPL/WinCUPL/GALasm-compatible flow.

| Role | Typical outputs (names in equations) |
|------|--------------------------------------|
| **GAL-DEC** | `RAM_CS`, `IO_CS`, `PRG_OE`, EEPROM `/CE` |
| **GAL-TIM** | `HBLANK`, `VBLANK`, `NMI`, `RASTER_IRQ` |
| **GAL-PPU** | VRAM `/CE`, CHR `/CE` BG vs 1284, line-buffer `/WE` |

Pin numbers vary by fuse map - after programming, print a **pinout report** from your programmer and paste into your island notebook. [Microchip ATF22V10 PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf) section pin diagram.

**Module C stub without PLD:** replace with 74HC688 (`A8-A15` compare to `$00` for RAM) + 74HC00 for `/CE` combine.

### 3.9 ATmega328P / ATmega1284P (modules K, L)

Standard AVR **PDIP** pinouts in Microchip PDFs. Retr01 minimum:

| Signal | 328P | 1284P |
|--------|------|-------|
| VCC | +5 V | +5 V |
| GND | GND | GND |
| AVCC / AREF | per PDF | per PDF |
| **XTAL1/2** | 16 MHz + 22 pF | 20 MHz + 22 pF |
| `/RESET` | 10 k ohm pull-up + ISP | same |
| ISP | MISO/MOSI/SCK | same |

1284P future Retr01 nets (module N): GPIO to OAM strobe, CHR data bus buffer, line-buffer `/WE`, pad sampling - assign in firmware schematic when integrating.

Arduino pinout charts **do not** match DIP-40 1284P 1:1 - use **40002047A** pin table only.

### 3.10 AT28C64B - DIP-28 (module F)

Parallel EEPROM. Port **`$FE70`/`$FE71`** address, **`$FE72`** data. Address/data tie to CPU bus when `/CE` active. See [AT28C64B PDF](https://ww1.microchip.com/downloads/en/DeviceDoc/doc4428.pdf) for write pulse timing (long `/WE`).

### 3.10b AT28C16 Color PROM - DIP-24 (module O)

Three chips: **PROM_R**, **PROM_G**, **PROM_B**. Same wiring pattern; only the programmed RGB bytes differ.

| Signal | Retr01 wiring |
|--------|----------------|
| A0-A5 | **6-bit master color index** from compositor |
| A6-A10 | tie **GND** (only 64 entries used) |
| I/O0-7 | to that gun's R-2R DAC (use top bits per Q14) |
| `/CE` | GND (always selected) or gated with video blank if desired |
| `/OE` | active during visible dots |
| `/WE` | tie **high** (read-only in circuit; program off-board) |

Burn the canonical 64-color table from [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) once. Not on the 6502 bus.

### 3.11 Cart flash - SST39SF040 class (modules C, J)

32-pin DIP parallel NOR. Typical signals:

| Signal | Connection |
|--------|------------|
| A0-A18 | CPU (PRG/MAP) or PPU/1284 (CHR) address mux |
| I/O0-7 | Shared data bus with transceiver |
| `/CE` | **`/CE_PRG`**, **`/CE_CHR`**, or **`/CE_MAP`** (mutually exclusive) |
| `/OE` | Read strobe |
| `/WE` | Tie **high** on ROM-only cart. Programming off-board |

### 3.12 Module -> IC checklist

| Module | ICs (see section 3 datasheets) |
|--------|---------------------------|
| A | - (passives only) |
| B | HC14, oscillator cans |
| C | W65C02S, AS6C62256, ATF22V10 or HC688, flash/EPROM |
| D | + HC573, ATF22V10 |
| E | + switches, HC04/08 (invert) |
| F | + AT28C64B |
| G | + AS6C62256, 4x HC157, HC245, HC573 |
| H | + 4x HC161, ATF22V10, HC688 |
| I | + HC573 (scroll), glue from section 3.4 |
| J | + flash, HC573 (mapper), ATF22V10 |
| K | ATmega328P |
| L | ATmega1284P |
| M | AS6C62256, 2x HC157 |
| N | 1284 + M + J CHR path |
| O | HC573, **3x AT28C16** Color PROM, R-2R |

---

## 4. Shared passives (all islands)

Use the same values on every island that needs them.

### Power input

- **5.5 x 2.1 mm** barrel, center **+**, or bench supply direct.
- **Polyfuse** ~500 mA-1 A in series.
- Reverse-protection **diode** across 5 V (cathode to +).
- **100 uF** bulk + **100 nF** at the entry point.
- **LED + 1 k ohm** from +5 V to GND as power-on marker.

### CPU reset (`RESB`)

- **10 k ohm** pull-up `RESB` -> +5 V.
- **10 uF** `RESB` -> GND (slow POR).
- **74HC14** Schmitt: raw RC node -> HC14 in -> CPU `RESB`.
- Tact switch: RC node -> GND for manual reset.

### CPU straps (W65C02S)

- **`RDY`**, **`BE`**, **`SOB`**: **3.3 k ohm** -> +5 V.
- **`NMIB`**, **`IRQB`**: **3.3 k ohm** -> +5 V.
- **`VPB`**, **`MLB`**: no connect.

### Pad switches (island E)

- Each switch: **10 k ohm** pull-up to +5 V, switch to GND (active low).
- Invert in glue so **`$FE60`/`$FE61`** read **1 = pressed**.

### Clocks (real silicon)

| Signal | Source |
|--------|--------|
| CPU `PHI2` | **8.000 MHz** canned oscillator (or 1-2 MHz for first tests) |
| Dot | **21.477 MHz** can / **4** -> ~**5.369 MHz** (74HC161 or 393) |
| 1284 | **20 MHz** HC-49 + **22 pF** x2 to GND |
| 328P | **16 MHz** HC-49 + **22 pF** x2 to GND |

100 nF on each oscillator/can VCC pin.

### Video DAC (island O, real silicon)

- **6-bit R-2R** per gun (1 k ohm / 2 k ohm), **75 ohm** series to output.
- Target ~**0.7 V** into 75 ohm. Tune on bench.
- **CSYNC** negative.

### AVR ISP

- 6-pin header: MISO, MOSI, SCK, `/RESET`, VCC, GND.
- **10 k ohm** on AVR `/RESET` -> +5 V.

---

## 5. Module A - Power

### Parts

Barrel or screw terminal, polyfuse, diode, bulk cap, LED.

### Test

1. Set supply **5.0 V**, current limit **200 mA**.
2. Measure rail: **5.0 V ± 0.25 V** under a **100 ohm** dummy load.
3. Reverse bench leads briefly (or use sacrificial fuse): nothing else on the board should die.
4. Idle current with no logic: **milliamps**, not amps.

### Pass

Clean 5 V rail, fuse/diode behave, no smoke.

---

## 6. Module B - Clocks and reset

### Parts

8 MHz can (or 1 MHz for early work), HC14, reset RC, CPU **not** required yet - probe the nets that will go to the CPU.

### Test

1. Scope **PHI2**: square wave, expected frequency.
2. Power-on: **`RESB`** low ~**100 ms**, then high.
3. Reset button pulls **`RESB`** low, releases high.
4. (Optional separate sub-island) Dot clock can + /4 -> ~**5.37 MHz** on scope.

### Pass

Stable clocks. Reset timing sane.

---

## 7. Module C - CPU + system RAM + tiny PRG ROM

### Parts

| Part | Role |
|------|------|
| W65C02S | CPU |
| AS6C62256 | System RAM (`$0000-$7FFF`) |
| 32 KB flash or EPROM | PRG at `$8000` |
| ATF22V10 **or** 74HC glue stub | Decode: `RAM_CS = !A15`, ROM when `A15` and not `$FE` |

**Pin reference:** W65C02S **section 3.2**, AS6C62256 **section 3.3**, ATF22V10 **section 3.8**, cart flash **section 3.11**

### Wiring (minimal decode)

- CPU **A0-A14** -> RAM **A0-A14**.
- CPU **A0-A14** -> ROM low address (bank at `$8000`).
- **`RAM_CS`**: active when **A15 = 0**.
- **`ROM_CS`**: active when **A15 = 1** and address not in `$FE00-$FEFF` (stub `$FE` with 74HC688 or switch later).
- **`PHI2`** -> CPU clock. **`R/W`**, **`RESB`**, straps per section 3.
- ROM **`/WE`** tied high (read only).

### ROM contents

Minimal test image:

- **`$8000`**: `JMP $8000` or NOP sled.
- **`$FFFC-$FFFD`**: reset vector -> **`$8000`**.

### Test

1. Apply reset, scope **A15** toggles (code running).
2. On **`PHI2`** rising edge during read, **D0-D7** show **`$EA`** (NOP) or your opcode.
3. Write **`$55`** to **`$0000`**, read back **`$55`**.
4. Supply current stays reasonable (no hot chips).

### Pass

CPU fetches from ROM, system RAM read/write works, no bus fight.

### Common failures

- Wrong CPU pinout (W65C02S vs NMOS 6502).
- **`BE`** or **`RDY`** floating.
- ROM `/CE` or `/OE` wrong -> open bus / `$FF` reads.

---

## 8. Module D - `$FExx` decode + one latch

### Parts

Island **C** plus:

- ATF22V10 programmed as **GAL-DEC** (or 74HC138/688 stub).
- One **74HC573** at **`$FE02`** (scroll X latch).

**Pin reference:** HC573 **section 3.4**, ATF22V10 **section 3.8**

### Wiring

- **`IO_CS`**: assert when **A15..A8 = `$FE`**.
- **`$FE02` write**: clock 573. **`$FE02` read** optional (probe **Q** pins if not readable).

### Test (guest ROM loop)

```text
LDA #$55
STA $FE02
; optional: LDA $FE02
```

1. **`STA $0000`** still hits RAM only.
2. **`STA $FE02`** toggles 573 outputs only.
3. **`STA $8000`** does not change RAM or latch.

### Pass

I/O decode isolated from RAM and PRG.

---

## 9. Module E - Controller pads (`$FE60`)

### Parts

Island **D** plus:

- 8 switches + pull-ups + invert -> **`$FE60`** (P1).
- Repeat for P2 at **`$FE61`** when ready.

### Test

Guest: **`LDA $FE60`**. Press **Right** (bit 0): result has bit 0 set. No keys: **`$00`**.

### Pass

Bit layout matches [`03_hardware_implementation.md`](03_hardware_implementation.md) (also listed under `$FE60` in [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md)) - **1 = pressed**.

---

## 10. Module F - Board EEPROM (`$FE7x`) [optional]

### Parts

Island **D** plus **AT28C64B** (8 KB). Decode port: **`$FE70`/`$FE71`** address, **`$FE72`** data (ships on every v0 board - see [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md)).

**Pin reference:** AT28C64B **section 3.10**

### Test

1. Write byte **`$A5`** at a chosen address.
2. Power-cycle or follow EEPROM `/WE` timing.
3. Read back **`$A5`**.

### Pass

Non-volatile read/write. Stays off the video path.

---

## 11. Module G - VRAM port + interleave (no beam yet)

**Most important island after CPU/RAM.** Proves the **PHI2 mux mutex** before any PPU.

### Parts

| Part | Role |
|------|------|
| AS6C62256 #2 | VRAM |
| 4x 74HC157 | 15-bit address mux (CPU vs "PPU side") |
| 74HC245 | CPU data to VRAM (CPU phase only) |
| 74HC573 | VRAM address latch from **`$FE10`/`$FE11`**, data **`$FE12`** |
| Island **C** or **D** | CPU to run test code |

**Pin reference:** AS6C62256 **section 3.3**, HC157 **section 3.6**, HC245 **section 3.5**, HC573 **section 3.4**

### Wiring

- **PHI2** (or inverted PHI2 - document your choice) selects mux:
  - **CPU phase**: latched **`$FE10`/`$FE11`** address on VRAM **A**, 245 connects CPU **D** to VRAM **D** via **`$FE12`**.
 - **PPU phase**: PPU side of 157s tied to a **fixed** address (all 0) or DIP switch for manual poke.
- VRAM **`/WE`**, **`/OE`** qualified so CPU and PPU never drive **D** at once.

### Test

1. CPU writes **`$AA`** to VRAM offset **`$0000`** via **`$FE10`/`$FE11`/`$FE12`**.
2. CPU reads back **`$AA`**.
3. Scope **245 `/OE`** or VRAM **D**: high-Z on opposite phase from CPU write.
4. (Optional) Halt **PHI2** single-step if your setup allows - only one driver on **D**.

### Pass

Read/write through **`$FE10`/`$FE12`**, no bus contention on VRAM data.

### Common failures

- Mux select inverted -> wrong phase owns bus.
- **`/WE`** active on wrong phase -> corrupted cells.
- Missing 157 - need **4x** 157 for 15 bits (one 157 = 4 bits).

**Do not stack the full PPU on a failing G island.**

---

## 12. Module H - Dot clock + beam counters

### Parts

- **21.477 MHz** can / **4** -> dot clock.
- **4x 74HC161** (or equivalent) for **X** and **Y**.
- ATF22V10 **GAL-TIM** or 74HC688 for wrap at **341** / **262**.
- LEDs or scope on **HBlank**, **VBlank**, **NMI** stub.

**Pin reference:** HC161 **section 3.7**, HC688 **section 3.1**, ATF22V10 **section 3.8**

### Test

1. Dot frequency ~**5.369 MHz**.
2. **X** wraps at **341**, **Y** increments.
3. **Y** wraps at **262**.
4. **NMI** stub pulses at start of line **240** (if wired).

### Pass

341x262 timing matches [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md).

---

## 13. Module I - BG nametable fetch (minimal)

### Parts

Islands **G** + **H** plus:

- Glue for nametable address from **X/Y + scroll** latches (573s).
- Optional: fixed scroll **0**, single nametable slot.

### Test

1. CPU fills VRAM nametable slot 0 with a checkerboard tile pattern via **`$FE10`/`$FE12`**.
2. With dot counters running, scope/logic-analyzer VRAM **A** during PPU phase - addresses walk the nametable region.
3. (Optional) latch tile bytes sequentially.

Full **RGBS picture** comes in module **O**. Here you only need correct **VRAM fetch addresses**.

### Pass

PPU phase reads expected nametable range. CPU can still write on CPU phase.

---

## 14. Module J - Cart flash stub (PRG / CHR / MAP)

### Parts

- Parallel flash (or 4x 512 KB DIP) on a **cart connector** or breadboard "cart."
- **`/CE_PRG`**, **`/CE_CHR`**, **`/CE_MAP`** - **only one active** at a time (GAL-DEC / GAL-PPU).
- **`$FE80`** -> PRG bank latch (573).
- **`$FE90`** -> MAP address latch + read data.

### Tests

| Region | Test |
|--------|------|
| **PRG** | Island **C** already uses this |
| **CHR** | CPU **does not** read CHR directly. Toggle **`/CE_CHR`** with BG fetch or 1284 only |
| **MAP** | CPU writes 24-bit addr to **`$FE90`/`$FE91`/`$FE92`**, reads **`$FE93`**, verify auto-inc against known flash image |

### Pass

Three **`/CE`** lines mutually exclusive. MAP port reads expected bytes from burned test image.

---

## 15. Module K - ATmega328P APU (standalone)

Develop firmware in **simavr** or **Wokwi** first, then breadboard.

### Parts

- ATmega328P, 16 MHz crystal, ISP header.
- **`$FE40-$FE5F`** can be simulated with switches or a 573 fed by a second MCU / pattern generator for now.

**Pin reference:** ATmega328P **section 3.9**, [full PDF pinout](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf)

### Test

1. Flash NES-style test tone firmware.
2. Scope **PWM** or RC-filtered audio out.
3. Changing "register" inputs changes pitch/duty.

### Pass

Independent audio. No 6502 bus required yet.

---

## 16. Module L - ATmega1284P (standalone)

Same as **K**: sim first, then proto.

### Parts

- ATmega1284P, 20 MHz crystal, ISP header.
- GPIO for future: OAM write strobe, CHR data in, line-buffer **`/WE`**.

**Pin reference:** ATmega1284P **section 3.9**, [full PDF pinout](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf)

### Test (firmware loopback)

1. Firmware writes a known **256-byte** pattern into "line buffer" (SRAM or internal buffer).
2. Verify bytes on scope or UART debug.

### Pass

1284 runs at 20 MHz. Firmware loads. Loopback pattern correct.

---

## 17. Module M - Line-buffer SRAM

### Parts

- AS6C62256 #3 (only **512 bytes** used: **`$000-$0FF`**, **`$100-$1FF`** ping-pong).
- **2x 74HC157**: address = **`{display_half, X[7:0]}`** vs 1284 address.

### Test

1. Manual write **half 0** with test pattern (DIP switches or 1284).
2. Mux select "display" half, read back pattern on **D** with **`/OE`**.
3. Swap half select, repeat.

### Pass

Ping-pong halves independent. Mux selects beam vs writer cleanly.

---

## 18. Module N - 1284 + line buffer + CHR stub

### Parts

Islands **L** + **M** + CHR bus from **J** (flash with known tile bytes).

### Wiring rules

- **`/CE_CHR`**: **BG path** owns during visible dots. **1284** owns during **HBlank** - exclusive (GAL-PPU + NAND).
- 1284 writes **next** ping-pong half during HBlank.

### Test

1. Load OAM test list into 1284 (via **`$FE20`** addr / **`$FE21`** data, or direct firmware load).
2. One known sprite tile in CHR flash.
3. After one frame time, line-buffer half contains non-transparent sprite pixels at expected **X**.

### Pass

One-line delay behavior: buffer filled for line **N+1** while line **N** would display (same class as spec in **`03_hardware_implementation.md`**).

---

## 19. Module O - Palette + Color PROM + compositor + RGBS (minimal)

### Parts

- 74HC573 palette **index** latches (**`$FE08`/`$FE09`** - see draft map in [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md)).
- **3x AT28C16** Color PROM (R/G/B), pre-programmed with the master 64-color table. See **section 3.10b**.
- Compositor mux: BG vs line-buffer pixel (start with **BG only**, add sprite later) -> **6-bit** master index into all three PROMs.
- **R-2R** + **75 ohm** + **CSYNC** on each PROM data bus.

Do **not** put the master RGB table in line-buffer SRAM for the final design. A temporary resistor ladder without PROMs is OK only as a pre-PROM smoke test.

### Test

1. Force master index **0** and **63** on PROM address pins -> expected black / white-ish DAC levels.
2. Solid BG color via `$FE08`/`$FE09` index -> stable voltage on **R/G/B**.
3. Module **I** nametable + CHR stub -> checkerboard on RGBS monitor or scope line view.
4. Add line-buffer input from **N** -> sprite dots appear.

### Pass

256x240-ish stable image, **~15.7 kHz** horizontal rate class. Color PROM outputs match burned table. No illegal bus contention.

---

## 20. Module P - Integration

Combine proven islands on one board or PCB:

```text
C + D + E + G + H + I + J + O -> BG + CPU + cart + video
+ N (1284 + line buffer) -> sprites
+ K (328P) -> audio
+ F (EEPROM) -> operator/settings
```

### Integration order on one board

1. Power, clocks, reset (**A**, **B**).
2. CPU, RAM, ROM, decode (**C**, **D**).
3. VRAM port (**G**) - **must pass** before video.
4. Pads (**E**).
5. Dot counters + BG fetch + RGBS (**H**, **I**, **O**).
6. Cart (**J**).
7. 328P audio (**K**).
8. 1284 + line buffer + CHR merge (**L**, **M**, **N**).
9. Full compositor.

### System smoke test (6502 ROM)

1. Init stack, RAM test **`$0000`-`00FF`**.
2. Write scroll / palette / nametable via **`$FExx`**.
3. Read **`$FE60`**, react on loop.
4. Wait for **NMI**. Increment frame counter in RAM.
5. (Later) OAM upload loop: `$FE20` addr, `$FE21` data.

### Pass

Stable video, responsive pads, NMI ~60 Hz, no hot chips, current within budget.

---

## 21. When to stop breadboarding and draw PCB

When **A-E**, **G**, **H**, **I**, **J**, **K**, **L**, **M**, **N**, and **O** pass on **separate** islands (or staged integration), draw the KiCad motherboard from those nets - not from an AI-generated full schematic.

---

## 22. Quick reference - `$FExx` smoke targets

| Port | Island | Quick test |
|------|--------|------------|
| **`$FE02/03`** scroll | D | Store **`$55`**, probe latch |
| **`$FE10-$FE12`** VRAM | G | Write/read **`$AA`** at VRAM **0** via addr lo/hi + data |
| **`$FE20`/`$FE21`** OAM | N | `$FE20`=addr, `$FE21`=data auto-inc |
| **`$FE60/61`** pads | E | Switch -> bit set |
| **`$FE70-$FE72`** EEPROM | F | Addr + data R/W on AT28C64B |
| **`$FE80`** PRG bank | J | Bank change moves **`$8000`** window |
| **`$FE90-$FE93`** MAP | J | 24-bit seek + read known byte |

---

## 23. Related docs

| Doc | Content |
|-----|---------|
| [`03_hardware_implementation.md`](03_hardware_implementation.md) | Block diagram, chip list, pipelines, sprite line buffer |
| [`02_graphics_worlds_memory.md`](02_graphics_worlds_memory.md) | `$FExx` map, VRAM layout, scroll modes, timing |
| [`05_costs_and_open_questions.md`](05_costs_and_open_questions.md) | Open register bitfields, RGBS tuning |
| **section 3 above** | Datasheet links + Retr01 pin wiring for every island IC |

Future: gate-level **Digital** simulation can mirror islands **G**, **H**, **I** before soldering. That is optional and separate from this protoboard guide.
