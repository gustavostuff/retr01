# Hardware

Shared motherboard for arcade cabinet wiring and home console shells. Same PCB. Shell and BOM population choose the I/O path (microswitch headers and/or TRS pads). Through-hole DIP where practical. About **14 x 12 cm** minimum. **4-layer**.

Cart image layout: `memory.md`. Physical cart notes: `cartridge.md`. Video rules: `video-graphics.md`.

## Clocks and raster

| Net | Rate / shape |
| --- | --- |
| CPU | **8.000 MHz** (W65C02S) |
| Dot | **5.369318 MHz** |
| Each AVR128DB28 | **24 MHz** internal HFOSC |
| Raster | **341 x 262**, about **60.098 Hz** |
| Composite subcarrier helper | FSC crystal for **AD724** (NTSC **3.579545 MHz** or PAL **4.433618 MHz**) |

Logical playfield **128 x 120**, hardware **2x** to **256 x 240** by default (`SCALE` open). Closed `SCALE_1X` to +5 V selects 1x. Raster size stays the same.

## IC budget

**16** ICs on the motherboard + **2** on the cart = **18** counted parts.

| Scope | Count |
| --- | --- |
| Motherboard | 16 |
| Cart (flash + save EEPROM) | 2 |
| Outside the 18 | AD724 composite encoder, 74HC14, crystals, USB-C flasher MCU and its glue |

Bus discipline: PLD `/OE` + cart `/OE` + MCU 3-state on D[7:0]. Escapes if decode runs out (ordered): soft SEL + A demux on MCU-M, then +1 ATF22V10, then MAP on M GPIO with `RDY` stall, then +1 HC245 on CPU D (that last one pushes past 16 mobo).

## BOM (locked 18)

| Qty | Part | Role |
| --- | --- | --- |
| 1 | W65C02S | Game CPU @ 8 MHz |
| 3 | AVR128DB28-I/SP | MCU-M / MCU-S1 / MCU-S2 @ 24 MHz |
| 3 | AS6C62256 | Sys RAM, interleaved VRAM, sprite/BG0 field |
| 1 | SST39SF040 | 512 KB cart flash |
| 1 | 24C64 | Cart save EEPROM (8 KB I2C) |
| 3 | ATF22V10 | Beam X, Beam Y, Compositor + decode |
| 3 | 74HC157 | VRAM A[11:0] mux (CPU vs beam) |
| 1 | 74HC573 | Field A[7:0] latch (ALE from S1) |
| 1 | 74HC574 | BG1 scroll X `$7F02` |
| 1 | AT27C256R | Color PROM (45 ns OTP, packed R3G3B2) |

**Why AD724 (outside the 18), not AD725:** we want one analog RGB path that can feed **CSYNC or separate H/V** into the same encoder without drama. AD724 accepts HSYNC+VSYNC or CSYNC, and is flexible on FSC / 4FSC / crystal. AD725 is built around 4FSC and a luma-trap pin. Fine chip, worse fit for a dual-sync header story. Composite still lands on its own RCA. RGB analog always comes from the color PROM DAC.

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
| **MCU-M** | Soft `$7Fxx`, OAM/APU mailboxes, machine EEPROM **512 B**, cart **24C64** I2C, `RDY`, SPI master to S1/S2 |
| **MCU-S1** | OAM apply, VBlank sprite field, HBlank BG0 line, field SRAM via AD mux + HC573 |
| **MCU-S2** | `$7F60`/`$7F61` pads, `$7F40`-`$7F5F` APU mailbox, **PWM** audio on PF1 |

OAM `$7F20`/`$7F21` latches on M then SPI to S1. APU forwards M to S2. Cart I2C `$7F22`-`$7F24` on M. Machine EE `$7F70`-`$7F72` on M.

Game **entities** live in system RAM / PRG. Hardware path for drawing them is OAM + S1 field fill, not a fourth MCU role.

## Pin freeze essentials

**GPIO (SPDIP-28):** `PA[7:0]`, `PC[3:0]`, `PD[7:1]`, `PF[6,1,0]` = **22**. **UPDI** = dedicated **pin 19** (not PF6). **VDDIO2** = VDD (5 V). Three 1x3 UPDI headers (UPDI / VCC / GND).

| Lock | Value |
| --- | --- |
| Cart I2C | M TWI0 DEFAULT **PA2/PA3** |
| Master SPI | SPI1 DEFAULT PC0 MOSI, PC1 MISO, PC2 SCK, PC3 `/SS_S1`. `/SS_S2` = PD5 |
| Slave SPI | SPI1 ALT1 PD4 MOSI, PD5 MISO, PD6 SCK, PD7 `/SS` |
| DAC / audio | On-chip DAC on PD6 unused. S2 audio = TCA0 WO1 **PF1** |
| Pad UART | USART2 OD on **PF0** |
| Field | AD[7:0] + ALE -> HC573. A[14:8] direct. S1 drives ALE + `/WE` only. `/OE`/`/CE` from PLD |

**Soft SEL** (I/O page `$7F00-$7FFF`, low bytes match the old `$FExx` map):

| SEL | Covers (examples) |
| --- | --- |
| `SEL_SOFT0` | `$7F00` / `$7F06` / `$7F07` |
| `SEL_SOFT1` | `$7F05` / `$7F08` |
| `SEL_SOFT2` | MAP/OAM/APU/EE family (`$7F90`-`$7F92`, `$7F20`/`$7F21`, `$7F40`-`$7F5F`, `$7F22`-`$7F24`, `$7F70`-`$7F72`) |

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

Never route hard LE or beam through an MCU. VRAM: PHI2 high = CPU `$7F10`-`$7F12`, PHI2 low = BG fetch (3x HC157).

Macrocell pressure note from the prior bring-up: SY(8)+Q(8)+MAP(5) = **21** vs **30** MC on a 22V10. Stay honest about that budget when adding features.

## On-board memory (chips)

| Chip | Use |
| --- | --- |
| AS6C62256 #1 | System RAM behind `$0000-$7EFF` |
| AS6C62256 #2 | Interleaved VRAM (CPU PHI2 high, beam PHI2 low) |
| AS6C62256 #3 | Sprite field + BG0 ping-pong lines (S1) |
| AT27C256R | **64** master colors, packed **R3G3B2** `{RRRGGGBB}`. Video reads by 6-bit index. No CPU runtime access |

**Color DAC** (1% metal film). LSB->MSB: R/G **4.00 / 2.00 / 1.00 kohm**. B **2.00 / 1.00 kohm**. **75.0 ohm** to GND each gun -> **~0.7 Vpp**. Unused PROM address pins to GND.

Cart palettes are **indices only** into this PROM. See `memory.md` and `video-graphics.md`.

## Cartridge

Game Boy-sized (~**55 mm** width). Passive cart: **SST39SF040** + **24C64**. No mapper. `CE#` tied active. Mobo gates `OE#`. `WE#` for program only. Socket: EDAC **395-036-520-201** straight 2x18 (right-angle option **395-036-559-212** for tight shells). Pitch **2.54 mm**. Cart **1.6 mm**, 4-layer F/GND/GND/B. A0-A13 from CPU. A14-A18 from Compositor MAP.

### Side A / Side B (36-pin)

| A | Sig | A | Sig | B | Sig | B | Sig |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A1 | GND | A10 | A6 | B1 | GND | B10 | D6 |
| A2 | VCC | A11 | A7 | B2 | VCC | B11 | D7 |
| A3 | SDA | A12 | A8 | B3 | SCL | B12 | OE# |
| A4 | A0 | A13 | A9 | B4 | D0 | B13 | A14 |
| A5 | A1 | A14 | A10 | B5 | D1 | B14 | A15 |
| A6 | A2 | A15 | A11 | B6 | D2 | B15 | A16 |
| A7 | A3 | A16 | A12 | B7 | D3 | B16 | A17 |
| A8 | A4 | A17 | A13 | B8 | D4 | B17 | A18 |
| A9 | A5 | A18 | GND | B9 | D5 | B18 | WE# |

### USB-C flasher (bench, outside the 18)

**ATmega32U4-AU** 16 MHz / 5 V USB. **2x 74HC595** -> A0-A15. GPIO -> A16-A18, WE#, OE#. Port -> D0-D7. USB-C **5.1 kohm** on CC1/CC2. Same 36-pin edge. Never drive cart from flasher and console together.

**Frame:** `0xAA 0x55` | cmd | len u16 LE | payload | xor(cmd..payload). CDC **115200 8N1**.

| cmd | Name | Payload |
| --- | --- | --- |
| `0x01` | PING | empty |
| `0x02` | INFO | empty |
| `0x10` | ERASE_CHIP | empty |
| `0x11` | ERASE_SECTOR | addr u24 LE |
| `0x20` | WRITE | addr u24 LE + 1-256 B |
| `0x21` | READ | addr u24 LE + n u16 LE |
| `0x22` | VERIFY | addr u24 LE + data |

Replies: `0x81` PONG, `0x82` INFO_R, `0x90` OK, `0x91` DATA, `0x9E` NAK(+errno). Flow: PING -> INFO -> ERASE_CHIP -> WRITE -> optional VERIFY.

Console-mediated flash (cart seated in a live Retr01) can come later. Bench flasher is the v1 path.

## Controllers

`$7F60` P1 / `$7F61` P2. Bit set = pressed. MCU-S2 samples.

| Bit | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Button | Start | Coin | Y | X | Up | Down | Left | Right |

**Arcade:** J5/J6 **1x10** (pins 1-8 = bits 0-7, 9-10 GND). J7 **1x4** (`+5V`/`GND`/`RESET_N`/`GND`). Microswitch to GND. Series **47 ohm**. P1 -> PA0-7. P2 bits 0-3 -> PC0-3, 4-6 -> PD1-3, Start -> PF6.

**TRS (home shell):** 2x Switchcraft **35RAPC2BVN4**. Tip=5 V, Ring=DATA, Sleeve=GND. **4.7 kohm** pull-up on DATA (PF0). OD half-duplex UART. Pad MCU = **ATtiny85** (in the controller, not on the 18). **115200** 8N1. **< 200 us**/exchange. Poll `0x55`=P1, `0xAA`=P2. Reply = 1 byte bitfield.

## Video out / sync header

Analog RGB from the PROM DAC always. Sync is flexible on **one** header footprint so cabinets and SCART-style cables can pick a mode without a second connector family.

### J2 sync-capable RGB header (proposal)

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
| **CSYNC** | Pin 4 = composite sync. Pin 5 tied to GND at the jumper | AD724 CSYNC input. Simple TV / RGBS boxes |
| **H/V** | Pin 4 = HSYNC, pin 5 = VSYNC | AD724 H+V inputs. RGBHV monitors and some upscalers |

Same pins, same connector body. Cable or jumper chooses the story. Do not drive CSYNC and H/V meanings onto pin 4 at once.

**Composite:** AD724 -> J9 RCA (outside IC-18). S-video pair can hang off AD724 Y/C later if we want the pads.

**Stackup:** Top signal+5V / Inner GND / Inner GND / Bottom signal+5V.

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

Series **33 ohm** on PHI2 and DOT. Entry bulk **220 uF**. **100 nF** per IC VCC. Cart D/OE/WE/SDA/SCL series **33 ohm**.

## Light gun (roadmap)

Same TRS bus as pads. ATtiny85 + photodiode + LM393 + 16-bit beam timer (1 us ticks).

| Byte | Role |
| --- | --- |
| Host `0xFF` | Identify. Pad `0x01`. Gun `0x02` |
| `0x55`/`0xAA` | VBlank poll. Stop bit resets timer. Reply bit5 = Trigger |
| Host `0x5A` | Timer HI, LO. Miss = `0xFFFF` |

Black anti-spoof then white hitboxes on all targets, then `0x5A`. Two flash frames + one read. Proposed `$7F80`/`$7F81` (GUN_HI/LO). `$7F80` was reserved unused in the I/O page map.

## Related

`memory.md` | `cartridge.md` | `video-graphics.md` | `world-scrolling.md` | `software-api.md` | `open-questions.md`
