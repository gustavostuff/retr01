# Hardware

One shared motherboard for home console shells and arcade cabinets. Same PCB. Populate arcade microswitch headers, TRS pad jacks, or both. Board outline **170 x 120 mm** (locked for the initial design). Initial design: motherboard, cart, and pad PCBs all **2-layer** (locked for now). Revisit **4-layer** later only if bring-up or a commercial SMD revision really needs it (EMI / RF / regulatory).

**Packages (initial THT DIY board):** The motherboard is **100% through-hole** at the board level. Counted BOM ICs use DIP / SPDIP / PDIP footprints. **AD724** only exists as **SOIC-16**, so it mounts on a **SOIC-16 to DIP-16 adapter** that plugs into a DIP-16 socket (or soldered DIP pads) on the motherboard. Cart and pad stay THT. A later commercial spin can place AD724 (and other SMD parts) directly.

Cart image layout: `memory.md`. Physical cart notes: `cartridge.md`. Video rules: `video-graphics.md`.

## Clocks and raster

| Net | Rate / shape |
| --- | --- |
| CPU | **8.000 MHz** (W65C02S) |
| Dot | **5.369318 MHz** |
| Each AVR128DB28 | **24 MHz** internal HFOSC |
| Raster | **341 x 262**, about **60.098 Hz** |
| Composite subcarrier | FSC crystal for **AD724** (NTSC **3.579545 MHz** or PAL **4.433618 MHz**) |

Logical playfield **128 x 120**, hardware-scaled **2x** to **256 x 240** by default (`SCALE` open). Closing `SCALE_1X` to +5 V selects 1x. Raster size stays the same.

## IC budget

**17** ICs on the motherboard + **2** on the cart = **19** counted parts.

| Scope | Count |
| --- | --- |
| Motherboard | 17 (includes **AD724**) |
| Cart (flash + save EEPROM) | 2 |
| Outside the 19 | Crystals. **74HC14** optional (see skip conditions below). **Adafruit's UPDI Friend** is the DIY programming accessory, not a BOM IC |

### Bus discipline

Several chips can touch the CPU data bus **D[7:0]** (cart flash, system RAM, MCU-M soft ports, and so on). Only one driver may be active at a time.

The PLD decode asserts the right `/OE` (and related selects) for the current address. The cart flash `/OE` is gated the same way. When MCU-M is not serving a soft `$7Fxx` cycle, it keeps its CPU data pins in **hi-Z**. That three-way rule (PLD `/OE` + cart `/OE` + MCU 3-state) is the whole bus discipline story.

**Idle-safe defaults (locked):** undriven enables must not fight the bus. Board pull-ups / pull-downs so that with AVRs still booting (or crashed) the safe state is:

| Net | Idle-safe |
| --- | --- |
| Cart **`WE#`** | Pull-up (high = no program pulse) |
| Cart **`OE#`** | Inactive unless decode asserts it |
| **`/SS_S1`**, **`/SS_S2`** | Pull-up (deselected) |
| Field **ALE** | Low (latch holding) |
| Field **`/WE`** | Pull-up (high = no write) |
| **`CPU_RDY`** | Pull-up. MCU-M drives **open-drain only** (never push-pull) |

MCU-M after reset: CPU D pins stay **inputs / hi-Z** until a proven soft-read window. Soft `$7Fxx` handling is a hard real-time path (edge / CCL / ISR). If the PHI2 window is too tight, assert **`CPU_RDY`** before PHI2 fall, finish the work, then release. SPI and I2C stay out of a soft-read cycle unless RDY is already low.

If soft decode ever runs out of PLD room, preferred escapes in order: demux more SELs on MCU-M, add a fourth ATF22V10, move MAP onto M GPIO with `RDY` stalls, and only then add an HC245 on CPU D (that last step breaks the 17-mobo count).

Full failure modes and bring-up order: `ic-comms-risks.md`.

## BOM (locked 19)

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
| 1 | AD724 | RGB to NTSC/PAL composite encoder | DIP-16 via **SOIC-16 to DIP adapter** | Native SOIC-16 (direct later) |

**Still outside the count:** crystals (common THT). Pad **ATtiny85**: DIP-8 and SOIC-8 both exist. **Adafruit's UPDI Friend** is an accessory, not a BOM IC. The SOIC-to-DIP adapter is a mechanical carrier, not an extra counted IC.

### Optional: 74HC14 (hex Schmitt inverter)

**Not in the counted 19.** Skip it on the first board when all of these hold:

- **PHI2** and **DOT** come from **canned oscillators** (or other already-square CMOS clock sources), not a raw crystal amp that needs squaring
- Series **33 ohm** (already planned) is enough damping on those clock nets
- **Reset** is a simple pull-up + switch, or a small supervisor IC, with short traces and no visible bounce/chatter on a scope

Add the 74HC14 (or populate its footprint) if bring-up shows soft clock edges, unavoidable crystal-buffer duty, or a noisy/slow reset rail that needs Schmitt cleanup.

### What kind of system is this?

Retr01 is a **multi-chip 8-bit gaming system** (separate CPU, RAM, glue, video path), not an FPGA soft system and also **not** a fully discrete-logic machine in the TTL-only sense. Game behavior and helper work live in programmable parts. Fixed 74xx-class chips only do mux/latch glue.

| Class | Parts | Programmable? | Flashed through the console? |
| --- | --- | --- | --- |
| **CPU** | W65C02S | Yes (runs cart **PRG**) | No (executes cart code, is not flashed) |
| **MCU helpers** | 3x AVR128DB28 | Yes (firmware) | **Yes** (Adafruit's UPDI Friend header) |
| **PLDs** | 3x ATF22V10 | Yes (beam / decode equations) | **No** (pre-programmed or DIY PLD tool) |
| **OTP color table** | AT27C256R | Program once (factory/DIY blow) | **No** (pre-programmed or DIY PROM tool) |
| **Cart memories** | SST39SF040, 24C64 | Yes (game image / saves) | **Yes** (cart flash via MCU-M bridge) |
| **Motherboard memories** | 3x AS6C62256 | No logic. Volatile storage only | No |
| **Fixed glue logic** | 3x 74HC157, 74HC573, 74HC574 | **No.** Hardwired mux / latch | No |
| **Composite encoder** | AD724 | Fixed analog (RGB to NTSC/PAL) | No |
| **Outside the 19** | crystals | Fixed timing | No |
| **Optional glue** | 74HC14 (hex Schmitt) | Skip if canned PHI2/DOT + simple reset. Add if edges/reset need cleanup | No |
| **Pad MCU** | ATtiny85 (in controller) | Yes (pad firmware) | **No** (pre-programmed or DIY ISP) |

### Composite encoder (frozen): AD724

**AD724** is on the motherboard BOM (one of the **17**). Analog only sells it as **SOIC-16**. For the initial DIY board, mount it on a **SOIC-16 to DIP-16 adapter** so the motherboard footprint stays **DIP-16** (socket recommended). That keeps the main PCB **100% THT**. Direct SOIC footprint is fine on a later SMD commercial spin.

It accepts **CSYNC or separate HSYNC+VSYNC**, which matches the dual-sync J2 header. Clocking is flexible (FSC crystal, FSC clock, or 4FSC). **AD725** stays off the BOM (4FSC-oriented, luma-trap focused, worse fit here).

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
| **MCU-M** | Soft `$7Fxx`, OAM/APU mailboxes, machine EEPROM **512 B**, cart **24C64** I2C, `RDY`, SPI master to S1/S2, **cart-flash bridge** when Adafruit's UPDI Friend is clipped onto the program header |
| **MCU-S1** | OAM apply, **full sprite field in VBlank**, **BG0 next-line fill in HBlank only** (ping-pong), field SRAM via AD mux + HC573 |
| **MCU-S2** | `$7F60`/`$7F61` pads, `$7F40`-`$7F5F` APU mailbox, DPCM samples in S2 flash, **PWM** audio on PF1 |

OAM `$7F20`/`$7F21` latches on M then SPI to S1. APU forwards M to S2. Cart I2C `$7F22`-`$7F24` on M. Machine EE `$7F70`-`$7F72` on M.

**SPI mailbox rules (locked):** exactly one of `/SS_S1` or `/SS_S2` low at a time. Idle both high. OAM blocks go to S1 in **early VBlank** only, or when **`S1_RDY`** says ready. Do **not** blast OAM SPI during HBlank (that window is for BG0 line fill). Separate message IDs / lengths for S1 vs S2 so a mis-select fails closed.

HBlank is short, so S1 only prepares the **next BG0 line** there. Sprites are composited in one VBlank pass into the field buffer (no sprite line ping-pong). See `video-graphics.md`.

**Field AD rules (locked):** S1 uses a state machine with dead cycles between ALE address latch and data `/WE`. AD[7:0] stay **hi-Z** outside an owned write window. PLD field `/OE` (beam read) and S1 `/WE` are mutually exclusive by equation.

Game **entities** live in system RAM / PRG. Drawing goes through OAM + S1 field fill.

## Pin freeze essentials

**GPIO (SPDIP-28):** `PA[7:0]`, `PC[3:0]`, `PD[7:1]`, `PF[6,1,0]` = **22**. **UPDI** = dedicated **pin 19** (not PF6). **VDDIO2** = VDD (5 V). Shared program header for **Adafruit's UPDI Friend**, plus a **4-pos DIP** footprint to select MCU-M / S1 / S2 / cart (default all OFF). Exact pinout TBD.

| Lock | Value |
| --- | --- |
| Cart I2C | M TWI0 DEFAULT **PA2/PA3** |
| Master SPI | SPI1 DEFAULT PC0 MOSI, PC1 MISO, PC2 SCK, PC3 `/SS_S1`. `/SS_S2` = PD5 |
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
| `CPU_D2`..`D5` | PA4-7 Z/in | `SPI_MOSI/MISO/SCK` | PC0-2 |
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
| **Compositor** | Priority, Color PROM index, MAP A14-A18. `LE_7F02`/`03`/`04`, `SEL_VRAM`, `LE_MAP`, three `SEL_SOFT*`, residual `/OE` |

Hard LE and beam stay out of MCU paths. VRAM: PHI2 high = CPU `$7F10`-`$7F12`, PHI2 low = BG fetch (3x HC157). Prefer **AS6C62256-55**. Keep VRAM mux / decode traces short. HC157 **G** must never float (G high forces Y low, not Hi-Z).

**Cart `OE#` (locked):** assert only for PRG `$8000-$FFFF` reads and intentional MAP/CHR fetch windows. Those windows never overlap system RAM or soft `$7Fxx` selects.

Macrocell pressure note: SY(8)+Q(8)+MAP(5) = **21** vs **30** MC on a 22V10. That budget is a hard limit when adding features. A **1-dot** Color PROM index latch in the Compositor is preferred if fit allows.

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

### Console as programmer (locked): Adafruit's UPDI Friend

The console (+ **Adafruit's UPDI Friend**) is a flasher for **only** the **three AVRs** and a **seated cartridge**. It does **not** program the ATF22V10 PLDs, the AT27C256R color PROM, or the pad ATtiny85. Those still need their own tools (PLD programmer, OTP/PROM burner, AVR ISP for the pad) before or beside assembly.

USB-C stays on Adafruit's UPDI Friend (PC side only). The console, cart, and pads have **no USB**. The Friend wires clip onto male header pins on the motherboard.

Adafruit's UPDI Friend is a CH340E USB-serial with the usual 1K RX/TX loopback for SerialUPDI. No USBASP. No separate flasher PCB for AVR/cart work.

**Same header, target select via DIP switch.** One shared program header (PWR / GND / data). A through-hole **4-position DIP switch** footprint on the motherboard routes the Friend data line to exactly one target:

| DIP pos | ON selects |
| --- | --- |
| 1 | **MCU-M** UPDI |
| 2 | **MCU-S1** UPDI |
| 3 | **MCU-S2** UPDI |
| 4 | **Cart** flash path (Friend data into MCU-M bridge / prog pin, then cart `WE#` / bus) |

**Default = all OFF.** Shipping and normal play leave every switch off so the Friend data pin is disconnected from all AVRs and from the cart bridge. That cuts accidental flash risk if someone plugs Adafruit's UPDI Friend in without meaning to program anything.

**One ON at a time.** Two UPDI targets stay off together (would short UPDI pins). Cart mode (pos 4) is alone as well. Silkscreen can say `M / S1 / S2 / CART` and `ALL OFF = SAFE`.

**Firmware (locked):** MCU-M refuses cart-bridge / `WE#` commands unless it reads cart mode from the DIP (or an equivalent strap). Cart mode stays off while a game is running.

Feasibility check (2026-09): AVR128DB28 is UPDI-only on pin 19. DxCore / avrdude **SerialUPDI** talk to it through this adapter. Cart ROM is **SST39SF040** parallel NOR, so cart writes still go through the MCU-M bridge when DIP pos 4 is ON.

| Job | Path |
| --- | --- |
| Reflash an AVR (on board) | Friend on header, matching DIP ON -> that AVR **UPDI** |
| Program cart flash (on board) | Friend on header, DIP pos 4 ON -> **MCU-M** bridge -> cart |
| PLDs / color PROM / pad MCU | **Out of scope** for this header (see below) |

**DIY before soldering.** Someone building a console can program each AVR128DB28 on a breadboard with Adafruit's UPDI Friend first (PWR / GND / UPDI), then solder the flashed chips. On-board header + DIP remain available later for AVR updates and cart programming.

Keep each AVR's UPDI pin configured as **UPDI** (not reset/GPIO) so Adafruit's UPDI Friend works. Adafruit's High Voltage UPDI Friend is only a recovery tool if someone bricks that fuse. Exact header pin numbers and host command protocol stay **TBD**.

### PLDs, color PROM, and pad MCU (not Adafruit's UPDI Friend)

Adafruit's UPDI Friend cannot program these. **Locked:** there is **no** high-voltage programming path on the motherboard (no ~12 V PLD EDIT / no ~13 V PROM VPP injected on-board). Those ICs are programmed **off the console PCB**, then installed (sockets recommended so they can be swapped).

Two practical paths for builders:

1. **Buy them pre-programmed.** Blank stock is the default from distributors. Programming services (distributor / MicrochipDirect-style / kit vendor selling Retr01-ready parts) can ship ATF22V10s with the beam/compositor JEDEC images, an AT27C256R blown with the 64-color table, and pad ATtiny85s with pad firmware. That is the easiest path for non-tinkerers. Note: the color PROM is **OTP** (one-time). A wrong blow means a new chip.
2. **Program them with a separate tool** (off the console PCB):

| Part | DIY options (examples) |
| --- | --- |
| **ATF22V10** | Arduino **Uno/Nano**-based GAL programmers (e.g. Afterburner), or a TL866-class universal programmer that lists ATF22V10 |
| **AT27C256R** | Parallel EPROM/OTP programmer (TL866-class or similar) that supports 27C256 and the required VPP/VCC programming voltages |
| **ATtiny85** (pad) | **ISP**: Arduino as ISP (Nano/Uno), USBasp, USBtinyISP, and so on. Optional ISP header on the pad PCB is fine (5 V only, no HV) |

Prefer programming PLDs and the color PROM **before** they go into the motherboard (or drop pre-programmed parts into sockets). Same for the pad MCU before closing the controller shell.

## Controllers

`$7F60` P1 / `$7F61` P2. Bit set = pressed. MCU-S2 samples.

| Bit | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Button | Start | Coin | Y | X | Up | Down | Left | Right |

**Arcade:** J5/J6 **1x10** (pins 1-8 = bits 0-7, 9-10 GND). J7 **1x4** (`+5V`/`GND`/`RESET_N`/`GND`). Microswitch to GND. Series **47 ohm**. P1 -> PA0-7. P2 bits 0-3 -> PC0-3, 4-6 -> PD1-3, Start -> PF6.

**TRS (home shell):** 2x Switchcraft **35RAPC2BVN4**. Tip=5 V, Ring=DATA, Sleeve=GND. **4.7 kohm** pull-up on DATA (PF0). OD half-duplex UART (pad and host both **open-drain**, never push-pull). Pad MCU = **ATtiny85** (in the controller, not on the 19). Pad PCB is **2-layer**. **115200** 8N1. **< 200 us**/exchange with a hard timeout. Poll `0x55`=P1, `0xAA`=P2 in **VBlank**. Reply = 1 byte bitfield. On timeout, keep last good or clear. Arcade headers and TRS pads are alternate input paths. Pads are optional when the cabinet uses microswitches.

## Video out / sync header

Analog RGB from the PROM DAC always. Sync is flexible on **one** header footprint so cabinets and SCART-style cables can pick a mode without a second connector family.

### J2 sync-capable RGB header (locked with AD724)

| Pin | RGBS / CSYNC mode | RGBHV mode |
| --- | --- | --- |
| 1 | Red | Red |
| 2 | Green | Green |
| 3 | Blue | Blue |
| 4 | CSYNC | HSYNC |
| 5 | GND | VSYNC |
| 6 | GND | GND |

Mode select (solder jumper or 1x3 header next to J2):

| Mode | Beam PLD drives | Encoder / cable notes |
| --- | --- | --- |
| **CSYNC** | Pin 4 = composite sync. Pin 5 tied to GND at the jumper | AD724 CSYNC input |
| **H/V** | Pin 4 = HSYNC, pin 5 = VSYNC | AD724 H+V inputs |

Same pins, same connector body. Cable or jumper chooses the story. CSYNC and H/V meanings never share pin 4 at once.

**Composite:** AD724 -> J9 RCA. An S-video pair can hang off AD724 Y/C later if those pads are needed.

## PCB layout practices

**Stackup (locked for initial design):** Motherboard is **2-layer**. Typical approach: top = signal + local 5 V pours, bottom = mostly unbroken **GND** pour (stitch often). Ground stays continuous under clocks and the CPU/dot buses. Long bottom-side runs stay off the ground pour when a top detour works.

**4-layer later (optional):** Only if hardware bring-up shows real need, or for a commercial **SMD** product spin aimed at lower EMI / RF noise and easier regulatory compliance. Not assumed for the first THT DIY board.

Cart and pad PCBs are **2-layer** as well.

| Ref | Locked |
| --- | --- |
| J1 | CUI **PJ-063AH** barrel 5 V |
| J2 | 1x6 RGB + sync (table above) |
| J3/J4 | Switchcraft **35RAPC2BVN4** |
| J5/J6 | 1x10 arcade |
| J7 | 1x4 power/reset |
| J8 | CUI **RCJ-012** audio RCA |
| J9 | CUI **RCJ-014** composite RCA |
| J36 | EDAC **395-036-520-201** |
| Y1 / Y2 / Y3 | Abracon ACH **8.000** / **5.369318** / FSC for AD724 |

Series **33 ohm** on PHI2 and DOT. Entry bulk **220 uF**. Cart D/OE/WE/SDA/SCL series **33 ohm**. Hold **RESB** until PHI2/DOT are up (RC or supervisor). Prefer sockets for the three ATF22V10s so a bad JEDEC can be swapped.

### Test points

Exposed copper pads (circles or plated holes) for multimeter / scope probes. Prefer one accessible side. Label in silkscreen. Keep clear of tall THT bodies and board-edge keepouts.

Minimum set (expand as layout needs):

| TP | Net / use |
| --- | --- |
| Several **GND** | Probe return (spread around the board) |
| **+5V** | After barrel / regulator entry |
| **PHI2**, **DOT** | Clock sanity |
| **RESET_N** | Bring-up |
| **UPDI_HDR** (Friend data) | Program path alive |
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
| Prog / activity | Optional. Blinks while Adafruit's UPDI Friend / cart-bridge path is busy (if firmware can drive it) |

### Layout rules (bring-up friendly)

These track common practice for a careful **2-layer** digital + video board (same ideas scale to 4-layer if a later revision needs it):

- **Decoupling:** **100 nF** (or similar) at every IC VCC pin, pad as close as practical to the pin, short path into ground (via to the GND pour). Bulk **220 uF** at the 5 V entry. Smallest HF caps closest to the pin.
- **Return paths:** High-frequency return wants a short loop back to ground under the signal. Protect the bottom GND pour. Prefer top-layer crossings. Stitch top ground fills to bottom with vias.
- **Keep clocks short:** PHI2, DOT, AVR clocks, and FSC stays. Crystals and their load caps next to the part. Series **33 ohm** already noted on PHI2/DOT.
- **Board edges:** High-speed and clock traces stay off the PCB perimeter. Edge copper couples into chassis and EMI. Clocks sit toward the middle of the board. Connectors and video out may sit on the edge by nature. Their stub lengths stay short.
- **Spacing / corners:** Prefer 45-degree bends over sharp 90s on faster nets. Give PHI2 / DOT / RGB analog some clearance from noisy switching and from each other where layout allows.
- **Analog video:** AD724 / DAC / RCA area quieter. Local decoupling. Short RGB and sync runs to J2/J9. Keep digital buses from cutting through that island.
- **Power:** Fat 5 V pours on top (or a dedicated pour). Feed from the barrel without daisy-thin power through long skinny traces.
- **Mounting / ESD:** Leave keepout around mounting holes. Tie chassis/mounting strategy deliberately (not accidental floating metal next to edge traces).
- **Silkscreen:** Refdes, polarity, DIP `M/S1/S2/CART` and `ALL OFF = SAFE`, TP names, LED names.

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
