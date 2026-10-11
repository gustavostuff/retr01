# Hardware

One shared motherboard for home console shells and arcade cabinets. Same PCB. Populate arcade microswitch headers, TRS pad jacks, or both. The board outline is **170 x 170 mm** (Mini-ITX). The motherboard is **4-layer**: signal, GND, GND, signal. Cart and pad PCBs are **2-layer**.

**Packages (initial DIY board):** The board is mostly through-hole, with counted motherboard ICs using DIP / SPDIP / PDIP footprints. **AD724** is the sole surface-mount exception: Analog only sells **SOIC-16**, so **U725** mounts directly on a narrow **SOIC-16** land pattern on the top side (`Retr01_Lib:SOIC-16_3.9x9.9mm_P1.27mm`) without a DIP adapter. Cart and pad stay THT.

Cart image layout: `memory.md`. Physical cart notes: `cartridge.md`. Video rules: `video-graphics.md`.

## Clocks and raster

| Net | Rate / shape |
| --- | --- |
| CPU | **8.000 MHz** (W65C02S) |
| Dot | **5.369318 MHz** |
| Each AVR128DB28 | **24 MHz** internal HFOSC |
| Raster | **341 x 262**, about **60.098 Hz** |
| Composite subcarrier | FSC crystal for **AD724** (NTSC **3.579545 MHz** or PAL **4.433618 MHz**) |

Discrete crystal clock generation:
- **PHI2 (8.000 MHz):** An 8.000 MHz crystal with an unbuffered **74HCU04** inverter gate and 1 M ohm feedback resistor forms the Pierce oscillator, buffered by a second gate to feed the CPU clock.
- **DOT (5.369318 MHz):** A 21.47727 MHz master crystal with a second 74HCU04 gate and 1 M ohm feedback resistor feeds an **SN74HC74** dual flip-flop. The flip-flop divides the frequency by 4 to produce a symmetrical 50% duty-cycle 5.369318 MHz square wave.
- **FSC (3.579545 MHz):** A 3.579545 MHz crystal connects directly to the on-chip oscillator pins of the **AD724** composite video encoder.

Logical playfield **128 x 120**, hardware-scaled **2x** to **256 x 240** by default (`SCALE` open). Closing `SCALE_1X` to +5 V selects 1x. Raster size stays the same.

## IC budget

**19** ICs on the motherboard + **2** on the cart = **21** counted parts.

| Scope | Count |
| --- | --- |
| Motherboard | 19 |
| Cart (flash + save EEPROM) | 2 |
| Outside the 21 | Crystals, reset supervisor (MCP130). **Adafruit's UPDI Friend** is the DIY programming accessory, not a BOM IC |

### Bus discipline

Several chips can touch the CPU data bus **D[7:0]** (cart flash, system RAM, MCU-M soft ports, and so on). Only one driver may be active at a time.

The PLD decode asserts the right `/OE` (and related selects) for the current address. The cart flash `/OE` is gated the same way. When MCU-M is not serving a soft `$7Fxx` cycle, it keeps its CPU data pins in **hi-Z**. That three-way rule (PLD `/OE` + cart `/OE` + MCU 3-state) is the whole bus discipline story.

**Idle-safe defaults (locked):** undriven enables must not fight the bus. Board pull-ups / pull-downs so that with AVRs still booting (or crashed) the safe state is:

| Net | Idle-safe |
| --- | --- |
| Cart **`WE#`** | Pull-up (high = no program pulse) |
| **`CART_ARM`** | Compositor MAP latch D7, cleared by **RESB** (low = `WE#` gated off) |
| Cart **`OE#`** | Inactive unless decode asserts it |
| **`/SS_S1`**, **`/SS_S2`** | Pull-up (deselected) |
| Field **ALE** | Low (latch holding) |
| Field **`/WE`** | Pull-up (high = no write) |
| **`CPU_RDY`** | Pull-up. MCU-M drives **open-drain only** (never push-pull) |

MCU-M after reset: CPU D pins stay **inputs / hi-Z** until a proven soft-read window. Soft `$7Fxx` handling is a hard real-time path (edge / CCL / ISR). If the PHI2 window is too tight, assert **`CPU_RDY`** before PHI2 fall, finish the work, then release. SPI and I2C stay out of a soft-read cycle unless RDY is already low.

If soft decode ever runs out of PLD room, preferred escapes in order: demux more SELs on MCU-M, add a fourth ATF22V10, move MAP onto M GPIO with `RDY` stalls, and only then add an HC245 on CPU D (that last step breaks the 19-mobo count).

Full failure modes and bring-up order: `ic-comms-risks.md`.

## BOM (locked 21)

| Qty | Part | Role | THT (v1) | SMD also? |
| --- | --- | --- | --- | --- |
| 1 | W65C02S | Game CPU @ 8 MHz | PDIP-40 | Yes (PLCC-44, QFP-44) |
| 3 | AVR128DB28-I/SP | MCU-M / MCU-S1 / MCU-S2 @ 24 MHz | SPDIP-28 | Yes (SOIC-28, SSOP-28, plus larger pin-count QFN/TQFP siblings) |
| 3 | AS6C62256 | Sys RAM, interleaved VRAM, sprite field + BG0 ping-pong. Prefer **-55** (55 ns) | PDIP-28 | Yes (SOP-28, sTSOP-28) |
| 1 | SST39SF040 | 512 KB cart flash | PDIP-32 | Yes (PLCC-32, TSOP-32) |
| 1 | 24C64 | Cart save EEPROM (8 KB I2C) | DIP-8 | Yes (SOIC/SOP/TSSOP) |
| 3 | ATF22V10 | Beam X, Beam Y, Compositor + decode | PDIP-24 | Yes (SOIC-24, PLCC-28) |
| 3 | 74HC157 | VRAM A[11:0] mux (CPU vs beam) | DIP-16 | Yes (SOIC-16, TSSOP) |
| 1 | 74HC573 | Field A[7:0] latch (ALE from S1) | DIP-20 | Yes (SOIC/TSSOP) |
| 1 | 74HC574 | BG1 scroll X `$7F02` | DIP-20 | Yes (SOIC/TSSOP) |
| 1 | AT27C256R | Color PROM (45 ns OTP, packed R3G3B2) | PDIP-28 | Yes (SOIC-28, PLCC-32, TSOP-28) |
| 1 | AD724 | RGB to NTSC/PAL composite encoder | **SOIC-16** SMD on motherboard | Same (only package Analog sells) |
| 1 | 74HCU04 | Crystal oscillator driver and clock buffer | DIP-14 | Yes (SOIC-14, TSSOP) |
| 1 | 74HC74 | Dual D-type flip-flop (divide-by-4 for 5.369318 MHz dot clock) | DIP-14 | Yes (SOIC-14, TSSOP) |

**Still outside the count:** crystals (common THT HC-49/US), reset supervisor (MCP130 in TO-92). Pad **ATtiny85**: DIP-8 and SOIC-8 both exist. **Adafruit's UPDI Friend** is an accessory, not a BOM IC.

### Reset generation: MCP130 supervisor

A dedicated 3-pin supervisor IC (**Microchip MCP130** in TO-92) controls system reset. It monitors the 5 V rail, provides brown-out detection, enforces a 350 ms power-on delay, and debounces manual reset switches directly on the **RESB** net with a sharp rising edge.

### What kind of system is this?

Retr01 is a **multi-chip 8-bit gaming system** (separate CPU, RAM, glue, video path), not an FPGA soft system and also **not** a fully discrete-logic machine in the TTL-only sense. Game behavior and helper work live in programmable parts. Fixed 74xx-class chips only do mux/latch glue and clock generation.

| Class | Parts | Programmable? | Flashed through the console? |
| --- | --- | --- | --- |
| **CPU** | W65C02S | Yes (runs cart **PRG**) | No (executes cart code, is not flashed) |
| **MCU helpers** | 3x AVR128DB28 | Yes (firmware) | **No** (breadboard SerialUPDI, then install) |
| **PLDs** | 3x ATF22V10 | Yes (beam / decode equations) | **No** (pre-programmed or DIY PLD tool) |
| **OTP color table** | AT27C256R | Program once (factory/DIY blow) | **No** (pre-programmed or DIY PROM tool) |
| **Cart memories** | SST39SF040, 24C64 | Yes (game image / saves) | **Yes** (cart flash via MCU-M bridge) |
| **Motherboard memories** | 3x AS6C62256 | No logic. Volatile storage only | No |
| **Fixed glue logic** | 3x 74HC157, 74HC573, 74HC574 | **No.** Hardwired mux / latch | No |
| **Clock generation** | 74HCU04, 74HC74 | **No.** Discrete crystal oscillator and divider | No |
| **Composite encoder** | AD724 | Fixed analog (RGB to NTSC/PAL) | No |
| **Reset supervisor** | MCP130 | Fixed supervisory timing | No |
| **Outside the 21** | crystals | Fixed timing | No |
| **Pad MCU** | ATtiny85 (in controller) | Yes (pad firmware) | **No** (pre-programmed or DIY ISP) |

### Composite encoder (frozen): AD724

**AD724** is on the motherboard BOM (one of the **19**). Solder the **SOIC-16** part on **U725** with local bypass (**C18**) and the datasheet coupling network when the composite path is populated. Hand-solder or reflow is fine for a single narrow SOIC.

It accepts **CSYNC or separate HSYNC+VSYNC**, which matches J2 carrying all three syncs. Clocking is flexible (FSC crystal, FSC clock, or 4FSC). **AD725** stays off the BOM (4FSC-oriented, luma-trap focused, worse fit here).

RGB analog always comes from the color PROM DAC. Composite is AD724 -> J9 RCA.

## MCU roles (3x AVR128DB28)

```text
                    +------------------+
                    |     MCU-M        |
                    |  Soft $7Fxx,     |
                    |  SPI master,     |
                    |  cart I2C, RDY   |
                    +---------+--------+
                              | SPI + RDY flags
              +---------------+---------------+
              |                               |
   +----------v-----------+       +-----------v----------+
   |       MCU-S1         |       |       MCU-S2         |
   |  OAM + field + BG0   |       |  Pads + APU PWM      |
   +----------+-----------+       +----------------------+
              |
         74HC573 ALE --> field AS6C62256
```

| Chip | Owns |
| --- | --- |
| **MCU-M** | Soft `$7Fxx`, OAM/APU mailboxes, machine EEPROM **512 B**, cart **24C64** I2C, `RDY`, SPI master to S1/S2, **cart-flash bridge** when Adafruit's UPDI Friend is on **J10** and **CART_ARM** is set |
| **MCU-S1** | OAM apply, **full sprite field in VBlank**, **BG0 next-line fill in HBlank only** (ping-pong), field SRAM via AD mux + HC573 |
| **MCU-S2** | `$7F60`/`$7F61` pads, `$7F40`-`$7F5F` APU mailbox, DPCM samples in S2 flash, **PWM** audio on PF1 |

OAM `$7F20`/`$7F21` latches on M then SPI to S1. APU forwards M to S2. Cart I2C `$7F22`-`$7F24` on M. Machine EE `$7F70`-`$7F72` on M.

**SPI mailbox rules (locked):** exactly one of `/SS_S1` or `/SS_S2` low at a time. Idle both high. OAM blocks go to S1 in **early VBlank** only, or when **`S1_RDY`** says ready. Do **not** blast OAM SPI during HBlank (that window is for BG0 line fill). Separate message IDs / lengths for S1 vs S2 so a mis-select fails closed.

HBlank is short, so S1 only prepares the **next BG0 line** there. Sprites are composited in one VBlank pass into the field buffer (no sprite line ping-pong). See `video-graphics.md`.

**Field AD rules (locked):** S1 uses a state machine with dead cycles between ALE address latch and data `/WE`. AD[7:0] stay **hi-Z** outside an owned write window. PLD field `/OE` (beam read) and S1 `/WE` are mutually exclusive by equation.

Game **entities** live in system RAM / PRG. Drawing goes through OAM + S1 field fill.

## Pin freeze essentials

**GPIO (SPDIP-28):** `PA[7:0]`, `PC[3:0]`, `PD[7:1]`, `PF[6,1,0]` = **22**. **UPDI** = dedicated **pin 19** (not PF6). **VDDIO2** = VDD (5 V). Cart program header is **J10** (2x2). AVR UPDI stays off J10.

| Lock | Value |
| --- | --- |
| Cart I2C | M TWI0 DEFAULT **PA2/PA3** |
| Master SPI | SPI1 DEFAULT PC0 MOSI, PC1 MISO, PC2 SCK, PC3 `/SS_S1`. `/SS_S2` = PD5. Cart program: USART1 one-wire on **PC1** (J10 DATA) |
| Slave SPI | SPI1 ALT1 PD4 MOSI, PD5 MISO, PD6 SCK, PD7 `/SS` |
| DAC / audio | On-chip DAC on PD6 unused. S2 audio = TCA0 WO1 **PF1** |
| Pad UART | USART2 OD on **PF0** |
| Field | AD[7:0] + ALE -> HC573. A[14:8] direct. S1 drives ALE + `/WE` only. `/OE`/`/CE` from PLD |
| Async into AVRs | Synchronize `VBL`, `SEL_SOFT*`, and other beam/CPU edges with two flops (or the event system) before acting |

**Soft SEL** (I/O page `$7F00-$7FFF`):

| SEL | Covers (examples) |
| --- | --- |
| `SEL_SOFT0` | `$7F00` PPUCTRL, `$7F06`/`$7F07` BG0 scroll |
| `SEL_SOFT1` | `$7F05` raster ctrl, `$7F08` PAL_ROW, `$7F09` PAL_DATA |
| `SEL_SOFT2` | OAM `$7F20`/`$7F21`, cart EE `$7F22`-`$7F24`, APU `$7F40`-`$7F5F`, machine EE `$7F70`-`$7F72`, MAP `$7F90`-`$7F93` |

Hard (not soft SEL): scroll/raster latches `$7F02`-`$7F04`, VRAM `$7F10`-`$7F12` (`SEL_VRAM`). Pads `$7F60`/`$7F61` are served by MCU-S2 via the soft path after SPI/GPIO sample.

Rising edge + latched `A[7:0]` via `CPU_A_SAMPLE` (PD4).

**HC573:** D=AD[7:0], Q=field A[7:0], LE=`ALE`, `/OE` low. **HC574:** D=CPU D, CLK=`LE_7F02`, `/OE` low, Q=SX[7:0] (not PHI2 free-run). Field write: A[14:8] + A[7:0] on AD, pulse ALE, data on AD, `/WE`.

### MCU-M

| Net | PORT | Net | PORT |
| --- | --- | --- | --- |
| `CPU_D0`/`D1` | PA0-1 Z/in | `I2C_SDA`/`SCL` | PA2-3 OD |
| `CPU_D2`..`D5` | PA4-7 Z/in | `SPI_MOSI/MISO/SCK` | PC0-2. PC1 also cart DATA |
| `/SS_S1` | PC3 out | `SEL_SOFT0` | PD1 in |
| `CPU_RDY` | PD2 OD | `VBL` | PD3 in |
| `CPU_A_SAMPLE` | PD4 in | `/SS_S2` | PD5 out |
| `CPU_D6`/`D7` | PD6-7 Z/in | `SEL_SOFT1`/`2` | PF0-1 in |
| `S1_RDY` | PF6 in | `UPDI` | pin 19 |

### MCU-S1

| Net | PORT | Net | PORT |
| --- | --- | --- | --- |
| `AD0`..`AD7` | PA0-7 Z | `A8`..`A11` | PC0-3 Z |
| `A12`..`A14` | PD1-3 Z | `SPI_*` `/SS_S1` | PD4-7 |
| `ALE` | PF0 out | `/WE` | PF1 out |
| `S1_RDY` | PF6 out | `UPDI` | pin 19 |

### MCU-S2

| Net | PORT | Net | PORT |
| --- | --- | --- | --- |
| `P1_RIGHT`..`START` | PA0-7 in | `P2_RIGHT`..`UP` | PC0-3 in |
| `P2_X`/`Y`/`COIN` | PD1-3 in | `SPI_*` `/SS_S2` | PD4-7 |
| `PAD_DATA` | PF0 OD | `AUDIO_PWM` | PF1 out |
| `P2_START` | PF6 in | `UPDI` | pin 19 |

## PLD roles (3x ATF22V10)

| PLD | Owns |
| --- | --- |
| **Beam X** | Dot / H inside 341. Scroll Y `$7F03`. HBlank / VBlank / NMI |
| **Beam Y** | Line / V. Raster Y `$7F04` + cascaded EQ -> IRQB |
| **Compositor** | Priority, Color PROM index, MAP A14-A18. `LE_7F02`/`03`/`04`, `SEL_VRAM`, `LE_MAP`, three `SEL_SOFT*`, residual `/OE`. Cart `WE#` gated by **CART_ARM** |

Hard LE and beam stay out of MCU paths. VRAM: PHI2 high = CPU `$7F10`-`$7F12`, PHI2 low = BG fetch (3x HC157). Prefer **AS6C62256-55**. Keep VRAM mux / decode traces short. HC157 **G** must never float (G high forces Y low, not Hi-Z).

**Cart `OE#` (locked):** assert only for PRG `$8000-$FFFF` reads and intentional MAP/CHR fetch windows. Those windows never overlap system RAM or soft `$7Fxx` selects.

Macrocell pressure note: SY(8)+Q(8)+MAP(5)+CART_ARM(1) = **22** vs **30** MC on a 22V10. That budget is a hard limit when adding features. A **1-dot** Color PROM index latch in the Compositor is preferred if fit allows.

## On-board memory (chips)

| Chip | Use |
| --- | --- |
| AS6C62256 #1 | System RAM behind `$0000-$7EFF` |
| AS6C62256 #2 | Interleaved VRAM (CPU PHI2 high, beam PHI2 low) |
| AS6C62256 #3 | Sprite field (filled in VBlank) + BG0 **ping-pong line buffers** (filled in HBlank) |
| AT27C256R | **64** master colors, packed **R3G3B2** `{RRRGGGBB}`. Video reads by 6-bit index. No CPU runtime access. Kit RGB and tool palettes: [`palette/`](palette/README.md) |

**Color DAC** (1% metal film). LSB->MSB: R/G **4.00 / 2.00 / 1.00 kohm**. B **2.00 / 1.00 kohm**. **75.0 ohm** to GND each gun -> **~0.7 Vpp**. Unused PROM address pins to GND.

Cart palettes are **indices only** into this PROM. See `memory.md`, `video-graphics.md`, and [`palette/`](palette/README.md).

## Cartridge

Game Boy-sized (~**55 mm** width). Passive cart: **SST39SF040** + **24C64**. No mapper. `CE#` tied active. Mobo gates `OE#`. `WE#` for program (console flash path) and is idle in normal play (**board pull-up**). Socket: EDAC **395-036-520-201** straight 2x18 (right-angle option **395-036-559-212** for tight shells). Pitch **2.54 mm**. Cart **1.6 mm**, **2-layer** PCB. A0-A13 from CPU. A14-A18 from Compositor MAP.

### Cart edge pinout (2x18 = 36 contacts)

EDAC **395-036-*** style. Looking into the console socket (or at the cart edge fingers): **Side A** is one row, **Side B** the other. Same pin number = opposite faces of the edge.

| Pin | Side A | Side B |
| --- | --- | --- |
| 1 | GND | GND |
| 2 | VCC (+5 V) | VCC (+5 V) |
| 3 | SDA (I2C save) | SCL (I2C save) |
| 4 | A0 | D0 |
| 5 | A1 | D1 |
| 6 | A2 | D2 |
| 7 | A3 | D3 |
| 8 | A4 | D4 |
| 9 | A5 | D5 |
| 10 | A6 | D6 |
| 11 | A7 | D7 |
| 12 | A8 | OE# (mobo gated) |
| 13 | A9 | A14 (MAP) |
| 14 | A10 | A15 (MAP) |
| 15 | A11 | A16 (MAP) |
| 16 | A12 | A17 (MAP) |
| 17 | A13 | A18 (MAP) |
| 18 | GND | WE# (flash path, idle in play) |

**Groups:** A0-A13 from the CPU. A14-A18 from the Compositor MAP port. D0-D7 data. OE# / WE# for flash. SDA/SCL for the 24C64.

**CHR fetch:** CHR is **16** banks on the same flash (**64 KB**). During a CHR window the cell or sprite attr bank field (4 bits) is part of the flash address: bits **0-1** on **A12-A13** (inside a 16 KB page), bits **2-3** on **A14-A15**, CHR region base on MAP **A16-A18**. MCU-S1 can form that address in firmware. If BG1 CHR is built in the Compositor, folding bits 2-3 onto A14/A15 is a 22V10 product-term question. See `video-graphics.md` and `open-questions.md`.

### Console as programmer (locked): cart flash via J10

The console plus **Adafruit's UPDI Friend** is a flasher for a **seated cartridge** only. It does not program the three AVR128DB28 parts, the ATF22V10 PLDs, the AT27C256R color PROM, or the pad ATtiny85. Those parts are programmed off the motherboard, then installed.

USB-C stays on Adafruit's UPDI Friend (PC side only). The console, cart, and pads have **no USB**. Friend wires clip onto **J10**, a 2x2 male header.

Adafruit's UPDI Friend is a CH340E USB-serial with the usual 1K RX/TX loopback. Cart DATA uses that one-wire serial path into running MCU-M firmware (USART1 on **PC1**), not SerialUPDI onto pin 19. No USBASP. No separate flasher PCB for cart work.

**J10** (2x2, 2.54 mm male). Same footprint family as J7. Friend cables carry three wires (PWR, GND, DATA). Pin 4 is a second GND so the housing sits on four holes.

Top view, pin 1 at top-left:

```text
1 (PWR NC)  2 (GND)
3 (DATA)    4 (GND)
```

- Pin 1 PWR: **NC**. Friend 5 V does not feed the motherboard 5 V rail. Console power stays J1.
- Pins 2 and 4: board GND.
- Pin 3 DATA: MCU-M **PC1** (USART1 one-wire). Play uses PC1 as SPI MISO. An unplugged header leaves that pin as an open stub.

**CART_ARM** is D7 of the compositor MAP latch (`LE_MAP`). D0-D4 are A14-A18. **RESB** clears the latch. Play MAP writes keep D7 low. `WE#` stays pulled up.

**Firmware (locked):** MCU-M refuses cart-bridge work unless CART_ARM is high. `/SS_S1` and `/SS_S2` stay high. PC1 PORTMUXes to USART1 as open-drain one-wire UART. Cart mode stays off while a game is running. Friend stays off J10 during play (PC1 is live SPI MISO).

**Address path (locked):** MCU-M has CPU D[7:0] and no cart address pins. A0-A13 stay on the 6502. A14-A18 stay on the compositor MAP latch (`$7F90`). While CART_ARM is high, `$8000-$FFFF` reads are MCU-M cycles (same shape as soft `$7Fxx`, with `CPU_RDY` if the window is tight). MCU-M serves a small 6502 stub. That stub copies itself into system RAM and jumps there. The RAM stub talks to MCU-M through `$7Fxx`, sets MAP via `$7F90`-`$7F92` (D7 = CART_ARM on the `LE_MAP` byte), and writes `$8000-$FFFF`. The compositor pulses `WE#` only when CART_ARM is high and the CPU is writing cart space.

Host command bytes on the Friend serial link stay TBD. Pin numbers and this bridge contract are locked. See `open-questions.md`.

| Job | Path |
| --- | --- |
| Program cart flash (on board) | Friend on **J10**, CART_ARM high, MCU-M USART plus RAM stub, cart `WE#` |
| Reflash an AVR | Off the motherboard. Friend on a breadboard to that chip **UPDI** pin 19, then install |
| PLDs / color PROM / pad MCU | Off the motherboard (see below) |

Keep each AVR's UPDI pin configured as **UPDI** (not reset/GPIO) so Adafruit's UPDI Friend works on the bench. Adafruit's High Voltage UPDI Friend is only a recovery tool if that fuse is bricked. Pin 19 is not brought to J10.

### PLDs, color PROM, AVRs, and pad MCU (not J10)

J10 does not program these. AVR128DB28 parts use Adafruit's UPDI Friend on a breadboard (SerialUPDI to pin 19), then get installed. **Locked:** there is **no** high-voltage programming path on the motherboard (no ~12 V PLD EDIT / no ~13 V PROM VPP injected on-board). PLDs, the color PROM, and the pad MCU are programmed **off the console PCB**, then installed (sockets recommended so they can be swapped).

Two practical paths:

1. **Buy them pre-programmed.** Blank stock is the default from distributors. Programming services (distributor / MicrochipDirect-style / kit vendor selling Retr01-ready parts) can ship AVR128DB28s with MCU firmware, ATF22V10s with the beam/compositor JEDEC images, an AT27C256R blown with the 64-color table, and pad ATtiny85s with pad firmware. That is the easiest path when a bench programmer is not on hand. Note: the color PROM is **OTP** (one-time). A wrong blow means a new chip.
2. **Program them with a separate tool** (off the console PCB):

| Part | DIY options (examples) |
| --- | --- |
| **AVR128DB28** | Adafruit's UPDI Friend, or USB-serial with 1K TX/RX loopback, SerialUPDI to pin 19, on a breadboard |
| **ATF22V10** | Arduino **Uno/Nano**-based GAL programmers (e.g. Afterburner), or a TL866-class universal programmer that lists ATF22V10 |
| **AT27C256R** | Parallel EPROM/OTP programmer (TL866-class or similar) that supports 27C256 and the required VPP/VCC programming voltages |
| **ATtiny85** (pad) | **ISP**: Arduino as ISP (Nano/Uno), USBasp, USBtinyISP, and so on. Optional ISP header on the pad PCB is fine (5 V only, no HV) |

Prefer programming PLDs, the color PROM, and the three AVRs **before** they go into the motherboard (or drop pre-programmed parts into sockets). Same for the pad MCU before closing the controller shell.

## Controllers

`$7F60` P1 / `$7F61` P2. Bit set = pressed. MCU-S2 samples.

| Bit | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Button | Start | Coin | Y | X | Up | Down | Left | Right |

**Arcade:** J5 **2x10**. Even pins 2-16 are Player 1 bits 0-7 (pin 2 = bit 0). Odd pins 15-1 are Player 2 bits 0-7 (pin 15 = bit 0, pin 1 = bit 7). Pins 17-20 are GND. J7 **2x2** (`+5V`/`GND` / `RESET_N`/`GND`). Microswitch to GND. Series **47 ohm**. P1 -> PA0-7. P2 bits 0-3 -> PC0-3, 4-6 -> PD1-3, Start -> PF6.

**TRS (home shell):** 2x CUI Devices **SJ1-3515N** (5-pin horizontal). Tip=5 V, Ring=DATA, Sleeve=GND. **4.7 kohm** pull-up on DATA (PF0). OD half-duplex UART (pad and host both **open-drain**, never push-pull). Pad MCU = **ATtiny85** (in the controller, not on the 21). Pad PCB is **2-layer**. **115200** 8N1. **< 200 us**/exchange with a hard timeout. Poll `0x55`=P1, `0xAA`=P2 in **VBlank**. Reply = 1 byte bitfield. On timeout, keep last good or clear. Arcade headers and TRS pads are alternate input paths. Pads are optional when the cabinet uses microswitches.

## Video out / sync header

Analog RGB from the PROM DAC always. **CSYNC**, **HSYNC**, and **VSYNC** all live on **one** 2x4 header so RGBS and RGBHV cables pick a story without a mode jumper or a second connector family.

### J2 sync-capable RGB header (locked with AD724)

2x4 male, 2.54 mm. KiCad numbering, pin 1 at top-left:

```text
1 (R)       2 (G)
3 (B)       4 (CSYNC)
5 (HSYNC)   6 (VSYNC)
7 (GND)     8 (GND)
```

| Pin | Net |
| --- | --- |
| 1 | Red |
| 2 | Green |
| 3 | Blue |
| 4 | CSYNC |
| 5 | HSYNC |
| 6 | VSYNC |
| 7 | GND |
| 8 | GND |

**RGBS** cables use R/G/B + CSYNC + GND. **RGBHV** cables use R/G/B + HSYNC + VSYNC + GND. AD724 on **U725** can take CSYNC (HSYNC pin, VSYNC held) or the separate H/V pair from the same header.

**Composite:** AD724 -> J9 RCA. An S-video pair can hang off AD724 Y/C later if those pads are needed.

## PCB layout practices

**Stackup:** The motherboard is **4-layer**. Layers 2 and 3 are solid GND planes, so noisy copper and quiet copper each have a ground return directly beside them. Layers 1 and 4 also get a GND zone fill in the copper that is not a trace. Every layer carries GND.

| Layer | Copper |
| --- | --- |
| 1 | Clock-rate digital, +5V, and a GND fill |
| 2 | Solid GND plane |
| 3 | Solid GND plane |
| 4 | Analog, slow digital, I/O, and a GND fill |

The GND copper is one net. Planes and fills are not cut into a digital region and an analog region. Noise stays down by placement, by which outer layer a signal uses, and by the analog keepout on layer 4. **+5V** is a routed net on layer 1 (0.8 mm to 1.2 mm). It is not an inner plane. Stitching vias tie GND on all four layers every 10 mm to 15 mm, and next to each IC ground pin. KiCad zone fill drops orphan copper.

Net-by-net assignment, the half/half outer-layer target, the analog keepout, and the KiCad **Layer4** air-wire class: [`docs/bring-up-v2/main-pcb-layers.md`](../bring-up-v2/main-pcb-layers.md).

**Layer 1:** buffered PHI2 and DOT, CPU and cart address and data, VRAM address and the 74HC157 ports, beam-counter lines, the color index into the Color PROM, U24 digital outputs to the DAC resistors, S1 `AD[7:0]` / ALE / field `/WE`, SPI MOSI/MISO/SCK, scroll X data, and +5V.

**Layer 4:** Pierce loops (Y1, Y2, U04 analog pins, feedback resistors, load caps), Y3 and FSC into the AD724, the R-2R gun nodes to J2, composite to J9, audio to J8, pad UART, I2C, `RESB`, `CPU_RDY`, UPDI, arcade and pad GPIO, LED anodes, line/frame strobes, soft SELs, SPI chip-selects, and MAP A14-A18.

Cart and pad PCBs are **2-layer**.

| Ref | Locked |
| --- | --- |
| J1 | GCT **DCJ200-10-A** barrel 5 V |
| J2 | 2x4 RGB + CSYNC/HSYNC/VSYNC (table above) |
| J3/J4 | CUI Devices **SJ1-3515N** 5-pin horizontal TRS |
| J5 | 2x10 arcade |
| J7 | 2x2 power/reset |
| J8 | CUI **RCJ-012** audio RCA |
| J9 | CUI **RCJ-014** composite RCA |
| J36 | EDAC **395-036-520-201** |
| Y1 / Y2 / Y3 | HC-49/US crystals: **8.000 MHz** / **21.47727 MHz** / **3.579545 MHz** (74HCU04 + 74HC74 clock stage) |

Series **33 ohm** on PHI2 and DOT. Entry bulk **220 uF**. Cart OE#/WE#/SDA/SCL series **33 ohm**. Cart **D[7:0]** ties straight to J36. Hold **RESB** until PHI2/DOT are up (RC or supervisor). Prefer sockets for the three ATF22V10s so a bad JEDEC can be swapped.

### Test points

Exposed copper pads (circles or plated holes) for multimeter / scope probes. Prefer one accessible side. Label in silkscreen. Keep clear of tall THT bodies and board-edge keepouts.

Minimum set (expand as layout needs):

| TP | Net / use |
| --- | --- |
| Several **GND** | Probe return (spread around the board) |
| **+5V** | After barrel / regulator entry |
| **PHI2**, **DOT** | Clock sanity |
| **RESET_N** | Bring-up |
| **CART_PROG_DATA** (J10 DATA) | Cart program path alive |
| Cart **WE#**, **OE#** | Flash / bus checks |
| Soft I/O sample (ex. one `$7Fxx` SEL) | Decode smoke test |

Aim for about **1.0 mm** pad diameter and comfortable probe spacing (about **1.27 mm**+ center-to-center).

### Status LEDs (THT only for now)

Full-size **through-hole** LEDs only in v1 (no SMD indicators yet). Series resistors as usual. Clear cathode mark on silkscreen. Place where a shell window or open chassis can see them.

Starter set (roles can grow):

| LED | Meaning |
| --- | --- |
| Power | +5 V present |
| Heartbeat M | MCU-M alive (firmware toggles on a timer even while `CPU_RDY` pulses) |
| Heartbeat S1 | MCU-S1 alive |
| Heartbeat S2 | MCU-S2 alive |
| Prog / activity | Optional. Blinks while the J10 / MCU-M cart-bridge path is busy (if firmware can drive it) |

### Layout rules (bring-up friendly)

These track common practice for this **4-layer** digital and video board:

- **Decoupling:** **100 nF** (or similar) at every IC VCC pin, within 5 mm of the pin, with a short via into the GND plane. Bulk **220 uF** at the 5 V entry. Smallest HF caps closest to the pin.
- **Return paths:** High-frequency return flows in the GND copper under the trace. Layer 2 returns layer 1. Layer 3 returns layer 4. Stitching vias tie GND on all four layers every 10 mm to 15 mm and next to each IC ground pin.
- **Keep clocks short:** PHI2, DOT, AVR clocks, and FSC stays. Crystals and their load caps next to the part. Series **33 ohm** already noted on PHI2/DOT.
- **Board edges:** High-speed and clock traces stay off the PCB perimeter. Edge copper couples into chassis and EMI. Clocks sit toward the middle of the board. Connectors and video out may sit on the edge by nature. Their stub lengths stay short.
- **Spacing / corners:** Prefer 45-degree bends over sharp 90s on faster nets. Give PHI2 / DOT / RGB analog some clearance from noisy switching and from each other where layout allows.
- **Analog video:** AD724 / DAC / RCA area quieter. Local decoupling. Short RGB and sync runs to J2/J9. Digital buses and layer 4 control traces stay out of that island. Full keepout: [`docs/bring-up-v2/main-pcb-layers.md`](../bring-up-v2/main-pcb-layers.md).
- **Power:** +5V stays on layer 1 at 0.8 mm to 1.2 mm. Feed from the barrel. Do not daisy a thin trace through the whole board.
- **Mounting / ESD:** Leave keepout around mounting holes. Tie chassis/mounting strategy deliberately (not accidental floating metal next to edge traces).
- **Silkscreen:** Refdes, polarity, J10 `CART`, TP names, LED names.

**Cart and pad PCBs (2-layer).** Same spirit: local caps next to the ICs, short stubs to the edge connector or TRS jack, one side mostly ground pour with stitching vias, labeled TPs for `+5V` / `GND` (and cart `WE#` if space allows).

### IC communication risks

Different clocks, shared buses, and Hi-Z hand-offs make IC-to-IC traffic the fragile part of this design. Full risk catalog and mitigations: **`ic-comms-risks.md`**.

## Light gun (roadmap)

Same TRS bus as pads. ATtiny85 + photodiode + LM393 + 16-bit beam timer (1 us ticks).

| Byte | Role |
| --- | --- |
| Host `0xFF` | Identify. Pad `0x01`. Gun `0x02` |
| `0x55`/`0xAA` | VBlank poll. Stop bit resets timer. Reply bit5 = Trigger |
| Host `0x5A` | Timer HI, LO. Miss = `0xFFFF` |

Black anti-spoof then white hitboxes on all targets, then `0x5A`. Two flash frames + one read. Proposed `$7F80`/`$7F81` (GUN_HI/LO).

## Related

`memory.md` | `cartridge.md` | `video-graphics.md` | `palette/` | `world-scrolling.md` | `software-api.md` | `sound.md` | `open-questions.md` | `../ic_behavior/` | `ic-comms-risks.md`
