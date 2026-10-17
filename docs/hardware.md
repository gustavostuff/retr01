# Hardware

One shared motherboard for home console shells and arcade cabinets. Same PCB. Populate arcade microswitch headers, TRS pad jacks, or both. The board outline is **170 x 120 mm**. The motherboard is **4-layer**: signal, GND, GND, signal. Cart and pad PCBs are **2-layer**.

Counted motherboard ICs use DIP, SPDIP, or PDIP footprints, except the composite encoder. That part is a Sony **CXA2075M** in a 24-pin SOP (`SOP-24_7.5x15.4mm_P1.27mm`, refdes **U2075**). Datasheet: [CXA2075M](https://www.alldatasheet.com/datasheet-pdf/pdf/146350/SONY/CXA2075M.html). **J11** is the 2x2 shunt that selects NTSC or PAL. Cart and pad stay through-hole.

Cart image layout: `memory.md`. Physical cart notes: `cartridge.md`. Video rules: `video-graphics.md`.

## Clocks and raster

The W65C02S runs at **8.000 MHz**. Each AVR128DB28 runs its **24 MHz** internal oscillator. The raster is **341 x 262** at about **60.098 Hz**.

Discrete crystal clock generation:
- **PHI2 (8.000 MHz):** An 8.000 MHz crystal with an unbuffered **74HCU04** inverter gate and 1 M ohm feedback resistor forms the Pierce oscillator, buffered by a second gate to feed the CPU clock.
- **DOT (5.369318 MHz):** A 21.47727 MHz master crystal with a second 74HCU04 gate and 1 M ohm feedback resistor feeds an **SN74HC74** dual flip-flop. The flip-flop divides the frequency by 4 to produce a symmetrical 50% duty-cycle 5.369318 MHz square wave.
- **FSC:** Y3 is the subcarrier into the CXA2075M **SCIN** pin. Stuff **3.579545 MHz** when J11 is on NTSC, or **4.433618 MHz** when J11 is on PAL.

Logical playfield **128 x 120**, hardware-scaled **2x** to **256 x 240** by default (`SCALE` open). Closing `SCALE_1X` to +5 V selects 1x. Raster size stays the same.

## IC budget

**19** ICs on the motherboard and **2** on the cart make **21** counted parts. Crystals and the MCP130 reset supervisor sit outside that count. **Adafruit's UPDI Friend** is a programming accessory, not a BOM IC.

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

Shared-net failure rules: `ic-comms-risks.md`.

## BOM (locked 21)

| Qty | Part | Role | Package |
| --- | --- | --- | --- |
| 1 | W65C02S | Game CPU at 8 MHz | PDIP-40 |
| 3 | AVR128DB28-I/SP | MCU-M, MCU-S1, MCU-S2 at 24 MHz | SPDIP-28 |
| 3 | AS6C62256 | System RAM, interleaved VRAM, sprite field plus BG0 ping-pong. Prefer **-55** (55 ns) | PDIP-28 |
| 1 | SST39SF040-55-4C-NHE | 512 KB cart NOR flash, 55 ns, 5 V, in a PLCC-32 socket | PLCC-32 |
| 1 | 24C64 | Cart save EEPROM, 8 KB I2C | DIP-8 |
| 3 | ATF22V10 | Beam X, Beam Y, Compositor and decode | PDIP-24 |
| 3 | 74HC157 | VRAM address mux, CPU versus beam | DIP-16 |
| 1 | 74HC573 | Field address latch, ALE from S1 | DIP-20 |
| 1 | 74HC574 | BG1 scroll X at `$7F02` | DIP-20 |
| 1 | AT27C256R | Color PROM, 45 ns, one-time, packed R3G3B2 | PDIP-28 |
| 1 | CXA2075M | RGB to NTSC/PAL composite encoder | SOP-24 |
| 1 | 74HCU04 | Crystal oscillator and clock buffer | DIP-14 |
| 1 | 74HC74 | Divide-by-4 for the 5.369318 MHz dot clock | DIP-14 |

**Still outside the count:** crystals (common THT HC-49/US), reset supervisor (MCP130 in TO-92). Pad **ATtiny85**: DIP-8 and SOIC-8 both exist. **Adafruit's UPDI Friend** is an accessory, not a BOM IC.

Cart flash listing: [DigiKey](https://www.digikey.com.mx/es/products/detail/microchip-technology/SST39SF040-55-4C-NHE-T/2615114).

### Reset generation: MCP130 supervisor

A dedicated 3-pin supervisor IC (**Microchip MCP130** in TO-92) controls system reset. It monitors the 5 V rail, provides brown-out detection, enforces a 350 ms power-on delay, and debounces manual reset switches directly on the **RESB** net with a sharp rising edge.

### Composite encoder: CXA2075M

The **CXA2075M** is on the motherboard BOM (one of the **19**). Package is SOP-24.

RGB from the color PROM DAC goes to **RIN**, **GIN**, and **BIN**. CSYNC from the beam PLDs goes to **SYNCIN**. That same CSYNC is **J2** pin 5. **Y3** is a Pierce crystal on **U04** gates 5 and 6, with **R25** (1 Mohm) across the crystal and **C23**/**C24** to GND. Gate 6 drives **SCIN**. **SCIN** is a clock input, so the crystal does not sit on that pin. **J11** drives **NPIN**: shunt on the left column (pins 1 and 3) for NTSC (NPIN at +5 V, Y3 at 3.579545 MHz), or on the right column (pins 2 and 4) for PAL (NPIN at GND, Y3 at 4.433618 MHz). The raster stays **341 x 262** at about **60 Hz** either way. HSYNC and VSYNC stay on the beam PLDs. Composite is **CVOUT** on **J2** pin 4. **YTRAP** is open, so the encoder uses its internal trap.

## MCU roles (3x AVR128DB28)

**MCU-M** owns soft `$7Fxx`, the OAM and APU mailboxes, a **512 B** machine EEPROM, cart **24C64** I2C, `RDY`, and SPI as master to S1 and S2. It is also the cart-flash bridge when Adafruit's UPDI Friend is on **J10** and **CART_ARM** is set.

**MCU-S1** applies OAM, fills the sprite field in one VBlank pass, and fills the next BG0 line in HBlank only. Field SRAM uses an AD mux and a 74HC573.

**MCU-S2** serves `$7F60` and `$7F61` for the pads, the `$7F40`-`$7F5F` APU mailbox, DPCM samples in S2 flash, and **PWM** audio on PF1.

OAM `$7F20`/`$7F21` latches on M then SPI to S1. APU forwards M to S2. Cart I2C `$7F22`-`$7F24` on M. Machine EE `$7F70`-`$7F72` on M.

**SPI mailbox rules (locked):** exactly one of `/SS_S1` or `/SS_S2` low at a time. Idle both high. OAM blocks go to S1 in **early VBlank** only, or when **`S1_RDY`** says ready. OAM SPI stays out of HBlank (that window is for BG0 line fill). Separate message IDs / lengths for S1 vs S2 so a mis-select fails closed.

HBlank is short, so S1 only prepares the **next BG0 line** there. Sprites are composited in one VBlank pass into the field buffer (no sprite line ping-pong). See `video-graphics.md`.

**Field AD rules (locked):** S1 uses a state machine with dead cycles between ALE address latch and data `/WE`. AD[7:0] stay **hi-Z** outside an owned write window. PLD field `/OE` (beam read) and S1 `/WE` are mutually exclusive by equation.

Game **entities** live in system RAM / PRG. Drawing goes through OAM + S1 field fill.

## PLD roles (3x ATF22V10)

**Beam X** counts the dot and the horizontal position inside 341. It holds scroll Y at `$7F03`, and it produces HBlank, VBlank, and NMI.

**Beam Y** counts the line and the vertical position. Raster Y at `$7F04` plus a cascaded compare produces IRQB.

**Compositor** owns priority, the color PROM index, and MAP bits A14-A18. It produces `LE_7F02`, `LE_7F03`, `LE_7F04`, `SEL_VRAM`, `LE_MAP`, the three `SEL_SOFT*` lines, and the residual `/OE`. Cart `WE#` is gated by **CART_ARM**.

Hard LE and beam stay out of MCU paths. VRAM: PHI2 high = CPU `$7F10`-`$7F12`, PHI2 low = BG fetch (3x HC157). Prefer **AS6C62256-55**. Keep VRAM mux / decode traces short. HC157 **G** must never float (G high forces Y low, not Hi-Z).

**Cart `OE#` (locked):** assert only for PRG `$8000-$FFFF` reads and intentional MAP/CHR fetch windows. Those windows never overlap system RAM or soft `$7Fxx` selects.

Macrocell pressure note: SY(8)+Q(8)+MAP(5)+CART_ARM(1) = **22** vs **30** MC on a 22V10. That budget is a hard limit when adding features. A **1-dot** Color PROM index latch in the Compositor is preferred if fit allows.

## On-board memory

Three **AS6C62256** parts. One is system RAM at `$0000-$7EFF`. One is interleaved VRAM (CPU on PHI2 high, beam on PHI2 low). One is the sprite field, filled in VBlank, plus the BG0 ping-pong line buffers, filled in HBlank.

The **AT27C256R** holds **64** master colors, packed **R3G3B2** `{RRRGGGBB}`. Video reads them by a 6-bit index. The CPU has no runtime path to the PROM. Kit RGB is in `video-graphics.md`.

**Color DAC** (1% metal film). LSB->MSB: R/G **4.00 / 2.00 / 1.00 kohm**. B **2.00 / 1.00 kohm**. **75.0 ohm** to GND each gun -> **~0.7 Vpp**. Unused PROM address pins to GND.

Cart palettes are **indices only** into this PROM. See `memory.md` and `video-graphics.md`.

## Cartridge

Game Boy-sized (~**55 mm** width). Passive cart: **SST39SF040-55-4C-NHE** + **24C64**. No mapper. `CE#` tied active. Mobo gates `OE#`. `WE#` for program (console flash path) and is idle in normal play (**board pull-up**). The flash is a PLCC-32 in a socket, so the cart PCB stays through-hole. Edge connector: EDAC **395-036-520-201** straight 2x18 (right-angle option **395-036-559-212** for tight shells). Pitch **2.54 mm**. Cart **1.6 mm**, **2-layer** PCB. A0-A13 from CPU. A14-A18 from Compositor MAP.

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

**CHR fetch:** CHR is **16** banks on the same flash (**64 KB**). During a CHR window the cell or sprite attr bank field (4 bits) is part of the flash address: bits **0-1** on **A12-A13** (inside a 16 KB page), bits **2-3** on **A14-A15**, CHR region base on MAP **A16-A18**. MCU-S1 can form that address in firmware. If BG1 CHR is built in the Compositor, folding bits 2-3 onto A14/A15 is a 22V10 product-term question. See `video-graphics.md`.

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

Host command bytes on the Friend serial link stay TBD. Pin numbers and this bridge contract are locked.

J10 programs a seated cart only. An AVR is reflashed off the motherboard, with the Friend on a breadboard to that chip's **UPDI** pin 19, then installed. PLDs, the color PROM, and the pad MCU are programmed off the motherboard.

Keep each AVR's UPDI pin configured as **UPDI** (not reset/GPIO) so Adafruit's UPDI Friend works on the bench. Adafruit's High Voltage UPDI Friend is only a recovery tool if that fuse is bricked. Pin 19 is not brought to J10.

### PLDs, color PROM, AVRs, and pad MCU (not J10)

J10 does not program these. AVR128DB28 parts use Adafruit's UPDI Friend on a breadboard (SerialUPDI to pin 19), then get installed. There is no high-voltage programming path on the motherboard. PLDs, the color PROM, and the pad MCU are programmed off the console PCB, then installed. The color PROM is one-time. A wrong blow means a new chip.

## Controllers

`$7F60` is player 1 and `$7F61` is player 2. A set bit means pressed. MCU-S2 samples. From bit 7 down to bit 0 the buttons are Start, Coin, Y, X, Up, Down, Left, and Right.

**Arcade:** J5 and J6 **2x5**, one per player. US2 sits between them. Even pins 2-10 are bits 0-4 (pin 2 = bit 0). J5 odd pins 7 and 9 are GND. J5 odd pin 1 = PA7 (US2 1), pin 3 = PA6 (US2 28), pin 5 = PA5 (US2 27). J6 odd pins 1 and 3 are GND. J6 odd pin 5 = PD2 Y (US2 8), pin 7 = PF6 Start (US2 18), pin 9 = PD3 Coin (US2 9). J5 is Player 1 to the right of US2 (180 deg, even column faces PA0-4). J6 is Player 2 to the left (0 deg, even column faces PC0-3 / PD1). J7 **2x2** (`+5V`/`GND` / `RESET_N`/`GND`). Microswitch to GND. Series **47 ohm**. P1 -> PA0-7. P2 bits 0-3 -> PC0-3, 4-6 -> PD1-3, Start -> PF6. The JAMMA finger edge shares the J5 and J6 button nets, and the +5 V and GND rails. Picture and sound stay on J2. Footprint `Retr01_Lib:JAMMA-56`. Pitch 3.96 mm. Numbered pads are the parts side. Letter pads are the solder side, skipping G, I, O, and Q. Pin 7 is the key.

| Control | P1 pad | P2 pad |
| --- | --- | --- |
| Coin | 16 | T |
| Start | 17 | U |
| Up | 18 | V |
| Down | 19 | W |
| Left | 20 | X |
| Right | 21 | Y |
| Button 1 (X) | 22 | Z |
| Button 2 (Y) | 23 | a |

GND pads are 1, 2, 28, A, B, and f. +5 V pads are 3, 4, C, and D. The other fingers stay off the netlist. There is no -5 V rail and no +12 V rail, so those fingers stay open.

**TRS (home shell):** 3.5 mm or 6.35 mm (1/4") jacks. Same nets on both sizes. Tip=5 V, Ring=DATA, Sleeve=GND. Populate one size per player. **J3** is player 1 and **J4** is player 2. Each footprint carries the Switchcraft **35RAPC2BVN4** 3.5 mm outline and the **CLIFF S4** 6.35 mm outline (typical **S4/BMB/PC-C**). **4.7 kohm** pull-up on DATA (PF0). OD half-duplex UART (pad and host both **open-drain**, never push-pull). Pad MCU = **ATtiny85** (in the controller, not on the 21). Pad PCB is **2-layer**. **115200** 8N1. **< 200 us**/exchange with a hard timeout. Poll `0x55`=P1, `0xAA`=P2 in **VBlank**. Reply = 1 byte bitfield. On timeout, keep last good or clear. Arcade headers and TRS pads are alternate input paths. Pads are optional when the cabinet uses microswitches.

## Video out

Analog RGB from the PROM DAC. One Kycon **KMDGX-9S-AS** female 9-pin mini-DIN (**J2**) carries RGB, CSYNC, composite, mono audio, and +5 V. Pin numbers are the official recommended PCB layout. That drawing is the bottom view. The footprint is the same layout with the connector facing up, so each pad number is the drawing's pin number.

| Pin | Net |
| --- | --- |
| 1 | Blue |
| 2 | +5 V |
| 3 | Green |
| 4 | Composite (CXA2075M CVOUT) |
| 5 | CSYNC |
| 6 | Audio (`AUDIO_PWM` after the RC) |
| 7 | Red |
| 8 | Audio (same net as pin 6) |
| 9 | Audio (same net as pin 6) |
| S | GND (three shield holes) |

HSYNC and VSYNC stay on the beam PLDs. They have no pin on this socket. **J11** chooses NTSC or PAL for composite on pin 4.

## Connectors

**J1** is a GCT **DCJ200-10-A** barrel jack for 5 V. **J2** is the mini-DIN above. **J3** and **J4** are the TRS jacks. **J5** and **J6** are the 2x5 arcade headers. **J7** is the 2x2 reset header. **J10** is the cart program header. **J11** is the NTSC/PAL shunt. **J36** is the EDAC **395-036-520-201** cart socket. **Y1**, **Y2**, and **Y3** are the HC-49/US crystals in the clock section.

Series **33 ohm** on PHI2 and DOT. Entry bulk **220 uF**. Cart OE#/WE#/SDA/SCL series **33 ohm**. Cart **D[7:0]** ties straight to J36. Hold **RESB** until PHI2/DOT are up (RC or supervisor). Prefer sockets for the three ATF22V10s so a bad JEDEC can be swapped.

Layer assignment, keepout, and trace widths: [`pcb.md`](pcb.md). Shared-net failure rules: [`ic-comms-risks.md`](ic-comms-risks.md).

## Light gun

The gun uses the same TRS bus as the pads, with an ATtiny85, a photodiode, an LM393, and a 16-bit beam timer at 1 us ticks. Host `0xFF` identifies a pad (`0x01`) or a gun (`0x02`). The VBlank poll is `0x55` and `0xAA`. Host `0x5A` reads the timer. Proposed ports are `$7F80` and `$7F81`. The rest of the scan is in `light-gun.md`.

## Related

`memory.md` | `cartridge.md` | `video-graphics.md` | `world-scrolling.md` | `software-api.md` | `sound.md` | `light-gun.md` | `ic-comms-risks.md` | [`pcb.md`](pcb.md)
