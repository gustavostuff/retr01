# Retr01-A Part Prices and Build Cost

Planning estimate for one **Retr01-A** motherboard plus cartridge. Schematic, board size, and exact 74-series counts are **not frozen**. Numbers below are reasonable qty-1 street prices in **USD**, snapshot **August 2026**.

This is not a quote. Distributor stock moves. Shipping, tax, and tariffs are extra.

**Sources (typical):** Mouser, DigiKey, Findchips, Octopart, JLCPCB public pricing.

## How to read this

Scope is **motherboard + cart** only: chips, passives, PCBs, sockets, connectors.

**74-series count** is the big unknown. A discrete PPU with interleaved VRAM, sprites, and RGBS is dozens of glue chips. The table uses a **~50 DIP 74HC** planning count plus **4x 22V10** glue. If the PPU is denser, add chips. If more goes into GALs, subtract 74HC and maybe add a GAL.

Lattice **GAL22V10** DIP is obsolete. The buyable through-hole stand-in is Microchip **ATF22V10CQZ-20PU** (same 24-pin idea, flash, still made).

## 1. Core ICs (motherboard)

Qty-1 authorized-distributor ballpark unless noted.

| Qty | Part (planning) | Role | Unit | Ext. |
|-----|-----------------|------|------|------|
| 1 | W65C02S6TPG-14 (DIP-40) | CPU | $11.00 | $11.00 |
| 2 | AS6C62256-55PCN (DIP-28) | 32 KB system RAM + 32 KB VRAM | $9.30 | $18.60 |
| 1 | Small SRAM (2 KB to 32 KB DIP) | Dedicated OAM (not in VRAM) | $4.00 | $4.00 |
| 4 | ATF22V10CQZ-20PU (DIP-24) | Decode, PPU timing, bus glue | $3.10 | $12.40 |
| 1 | AT28C64B-15PU (DIP-28) | High scores / operator EEPROM (8 KB) | $5.50 | $5.50 |
| 1 | ATmega328P-PU (DIP-28) | NES-style APU. May grow to a larger AVR | $2.90 | $2.90 |
| ~4 | 74HC157 (DIP-16) | VRAM address mux CPU vs PPU | $1.20 | $4.80 |
| ~3 | 74HC245 (DIP-20) | Data bus isolation | $0.90 | $2.70 |
| ~10 | 74HC573 (DIP-20) | Scroll, banks, I/O latches | $1.00 | $10.00 |
| ~6 | 74HC161 (DIP-16) | Beam X/Y counters | $1.00 | $6.00 |
| ~27 | Mix of 74HC00/08/32/86/166/595 | Gates, shift regs, sprite/BG pipeline | $0.80 | $21.60 |
| 2 | Canned oscillators (8.000 MHz, 21.477 MHz class) | CPU clock, NTSC-rate dot (divide to 5.369318 MHz) | $2.50 | $5.00 |
| 1 | 5 V regulator module or LDO | Board-local 5 V | $3.00 | $3.00 |

**Core IC subtotal: about $108**

Notes:

- 74HC DIP from Mouser/DigiKey is often **$0.80 to $1.50** each at qty 1. Hobby shops (e.g. Futurlec) list many of the same parts at **$0.25 to $0.65**, which would cut the 74HC bucket by roughly half if you accept that supply path.
- ATmega328P is a **placeholder**. A full NES-style APU with DMC may want more flash/RAM (ATmega32A / 1284 class, still a few dollars).
- OAM is 256 bytes. A whole extra 32 KB SRAM is wasteful but simple for bring-up. A 2 KB DIP SRAM is enough if you can still buy one.

## 2. Passives, sockets, connectors

Through-hole bring-up should socket the big ICs.

| Item | Planning | Ext. |
|------|----------|------|
| DIP sockets (40 / 28 / 24 / 20 / 16 pin mix, ~50 pcs) | $0.20 to $1.00 each | $25 |
| Caps, resistors, resistor-DAC for RGB, decoupling, reset | bulk | $15 |
| 40-pin IDC header + ribbon (cabinet I/O) | | $6 |
| Power barrel / screw terminal, headers, crystal load caps | | $8 |
| Optional JAMMA edge (if you go that route later) | | $8 |

**Board sundries subtotal: about $50 to $60** (skip JAMMA if you stay on IDC).

RGBS can be a cheap resistor ladder (**~$2**). A video DAC IC is optional and maybe **$8 to $15** if analog quality needs it.

## 3. PCB manufacturing

Retr01-A is DIP-heavy. Plan a **large** board, JAMMA-ish area, **4-layer** so 8 MHz buses have a ground plane. Guess: **~250 x 160 mm**. 2-layer is cheaper and meaner to route.

JLCPCB (and peers) advertise small-board promos. A board this size is **not** the $2 deal. Public 4-layer area rates plus engineering fees, for **5 pcs**:

| Build | Guess (5 pcs, board only) | Per board, lot split | Plus typical intl. shipping |
|------|---------------------------|----------------------|-----------------------------|
| 2-layer, ~250 x 160 mm | $15 to $35 | $3 to $7 | $20 to $40 per order |
| 4-layer, ~250 x 160 mm | $40 to $90 | $8 to $18 | $20 to $40 per order |

**Use $20 per motherboard** as a 4-layer proto share (PCB + a slice of shipping). First order is worse because you pay the 5-pc minimum and shipping once.

Cart PCB is small 2-layer: **~$2 to $5** for a pack of 5, call **$2** each.

## 4. Cartridge (~2 MB PRG + CHR + MAP)

True **2 MB parallel NOR in DIP** is basically gone. Planning options:

| Approach | Notes | Guess |
|----------|-------|-------|
| 4x SST39SF040-class 512 KB DIP | 4 Mbit each, 32-pin DIP. Family is aging / often obsolete at majors. Historical DigiKey ~$2.70 | $12 to $20 if you can still buy them |
| 1x 16 Mbit (2 MB) PLCC/TSOP + socket | More available in SMD. Fine on a cart, not pure DIP | $8 to $18 |
| Smaller first carts (512 KB) | Fine for bring-up ROMs | $3 to $6 |

Plus cart shell / edge / header: **$3 to $8**.

**Planning cart: $20** (2 MB, mixed package). Bring-up cart: **$8**.

## 5. Motherboard + cart rollup

| Bucket | Low | Planning | High |
|--------|-----|----------|------|
| Core ICs | $90 | $108 | $140 |
| 74HC path (hobby vs Mouser) | already in ICs | | +$0 to +$20 |
| Sundries + sockets | $40 | $55 | $75 |
| 4-layer PCB share | $10 | $20 | $35 |
| **Motherboard (no cart)** | **$140** | **$185** | **$250** |
| Cartridge | $8 | $20 | $30 |
| **Board + cart** | **$150** | **$205** | **$280** |

First unit also wants a programmer (TL866-class, often programs ATF22V10 too): **~$50 to $80**, once, not per board.

## 6. What actually moves the number

1. **Discrete PPU chip count.** 30 vs 80 of 74HC is the motherboard swing. GALs exist to keep this from becoming a 1985 wiring nightmare.
2. **Authorized DIP 74HC vs hobby stock.** Same silicon, 2x price difference at qty 1.
3. **2 MB cart in 2026.** Budget $20 and accept a PLCC/TSOP flash. Do not assume a single DIP 2 MB part stays in the catalog.
4. **Video output on the board.** A resistor DAC is cheap. A dedicated video DAC IC is optional.

## 7. Suggested planning figure (round)

Until the schematic exists, pitch **Retr01-A motherboard + cart, proto qty 1** as **$200** (band $150 to $280).

Revisit this file when the first real BOM (exact 74HC list and board outline) lands.
