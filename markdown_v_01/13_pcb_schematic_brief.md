# Retr01-A — PCB schematic generation brief

> **Superseded** for new work: use [15_schematic_prompt_coprocessor.txt](15_schematic_prompt_coprocessor.txt) (49-chip board, 1284 sprite/input coprocessor). This file is the older discrete-sprite prompt.
>
> **ARCHITECTURE IS STALE (archival reference only):** the remainder of this file may describe the older discrete-sprite era (e.g. `OAM_DMA` at `$FE22`, `6116` SRAM, extra GALs, RDY stalls, larger 74HC counts). Treat it as legacy notes only.
> For the frozen Retr01-A v0 build, use `15_schematic_prompt_coprocessor.txt` plus `[14](14_reduced_number_of_chips.md)` and `[16](16_simulation_and_bringup_plan.md)`.

You are a PCB / schematic designer. This file is archival: use it only for the cabinet/controller connector pinout and any leftover “wiring contract” notes you trust. For the full Retr01-A v0 architecture, use `15_schematic_prompt_coprocessor.txt`.

Do **not** treat this file as a full architecture spec. Ignore the older “draw the whole machine” instructions here; the frozen 49-chip v0 schematic lives in `15_schematic_prompt_coprocessor.txt`.

**Product:** Retr01-A arcade motherboard + plug-in cartridge. Discrete 8-bit computer (W65C02S) with a custom 2bpp tile PPU, NES-style APU on an AVR, analog RGBS video.

**Your output:** only what you still need from this legacy doc—primarily the cabinet/controller connector pinout and the `$FE60/$FE61` pad-bit contract. For the frozen Retr01-A v0 architecture schematic, use `15_schematic_prompt_coprocessor.txt`.

---

## 0. What you must deliver

1. Cabinet/controller connector pinout (20-pin IDC) and `$FE60` / `$FE61` pad-bit contract.
2. Everything else (full schematic, GAL/ATF equations, BOM, bus contention rules) should be generated from `15_schematic_prompt_coprocessor.txt` and verified against `14_reduced_number_of_chips.md` + `16_simulation_and_bringup_plan.md`.

**CAD:** KiCad 8 preferred (`.kicad_sch` + hierarchical sheets). EasyEDA is acceptable if that is all you can emit. Use **hierarchical sheets**, not one 10-meter page.

**If you cannot implement a perfect discrete PPU,** still draw it as real chips: counters, muxes, latches, shift registers, OAM SRAM, resistor DAC. A simplified sprite scanner is OK. An empty “PPU FPGA” box is **not** OK unless you also include the discrete version.

---

## 1. Design philosophy (do not violate)

| Rule | Meaning |
|------|---------|
| Through-hole first | DIP CPU, SRAM, ATF, AVR, 74HC. Socket the big ICs. |
| ATF22V10 for random gates | Decode, timing strobes, raster compare. **Do not** GAL-away 8-bit buses. |
| Wide buses stay 74HC | Address mux = **74HC157**. Data isolation = **74HC245**. Latches = **74HC573**. Beam counters = **74HC161**. |
| Two SRAMs | System RAM CPU-only. VRAM interleaved CPU↔PPU. CHR is **not** in VRAM. |
| Two clocks | CPU **8.000 MHz**. Dot **5.369318 MHz**. They are **independent**. Do not assume a 3:1 NES ratio. |
| No sprite-0 hit | Raster split = Y-counter compare + **IRQ**. NMI = VBlank only. |
| Software collision | No hardware sprite-vs-BG hit for gameplay. |
| No on-board HDMI | RGBS + S-Video + composite **pads**. HDMI is an external box. |
| 5 V only on A | Barrel jack 5.5 × 2.1 mm center-positive. Optional unpopulated USB-C breakout pads (5 V / GND only, not PD). |

---

## 2. Hierarchical sheets (draw all of these)

| Sheet | Contents |
|-------|----------|
| `00_cover` | Title block, rev `A0`, block diagram, clock/reset overview, FIXME legend |
| `01_power` | Barrel, polyfuse, reverse-diode, bulk + decoupling, +5V plane, optional USB-C pads, power LED |
| `02_cpu` | W65C02S, PHI2 clock, reset, RDY pulled up, BE=1, NMIB, IRQB, address/data buffers if needed |
| `03_decode` | ATF22V10 #1 (GAL-DEC): RAM / I/O / PRG / EEPROM chip selects |
| `04_ram_eeprom` | AS6C62256 system RAM, AT28C64B operator EEPROM |
| `05_vram` | AS6C62256 VRAM, 74HC157 address mux, 74HC245 data transceiver, phase select |
| `06_io_latches` | `$FExx` latches: PPU ctrl, scroll, CHR cells, mapper, MAP addr, pads |
| `07_ppu_timing` | Dot clock, 74HC161 X/Y counters (341×262), HBlank/VBlank, NMI, raster IRQ, ATF #2 (GAL-TIM) |
| `08_ppu_bg` | Nametable fetch, attr unpack, CHR address for BG, shift registers, 2bpp combine |
| `09_ppu_spr` | OAM SRAM, HBlank evaluate (max 16), sprite CHR fetch, X counters, compositor mux |
| `10_palette_video` | Palette RAM, 64-color lookup, R/G/B resistor DACs, CSYNC, S-Video/composite stubs |
| `11_cart` | Motherboard cart connector, PRG/CHR/MAP OE, `$FE80` PRG bank, `$FE90` MAP port |
| `12_apu` | ATmega328P (or 1284 if you need space), `$FE40-$FE5F`, analog mix, audio jack |
| `13_cabinet` | 20-pin controller IDC, `$FE60-$FE63` latches, pull-ups, coin/start |
| `14_cart_pcb` | Cartridge: 4× 512 KB flash (or 1× 2 MB SMD + DIP adapter), same connector |

---

## 3. Frozen clocks and video timing

| Signal | Value |
|--------|-------|
| `CLK_CPU` | **8.000 MHz** canned oscillator → W65C02S **PHI2** |
| `CLK_DOT` | **5.369318 MHz** — 21.47727 MHz canned osc ÷ 4 (74HC161 or 74HC393). Close enough to NES PPU rate. |
| Dots / line | **341** (256 visible + 85 HBlank) |
| Lines / frame | **262** (240 visible + 22 VBlank) |
| Visible | 256 × 240 |
| H rate | ~15.73 kHz |
| V / NMI | ~60.098 Hz |
| CPU VRAM phase | PHI2 **high** = CPU owns VRAM port. PHI2 **low** = PPU owns VRAM. (If silicon PHI2 polarity fights you, invert with a 74HC04 and document it.) |

**NMI:** assert `NMIB` at the **start of scanline 240** (first VBlank line), pulse or level until acked by the status-read convention below.  
**IRQ:** optional; assert `IRQB` when `beam_y == raster_y` at **dot 0** of that line, if raster IRQ enable is set.

---

## 4. CPU pin wiring (W65C02S6TPG-14 DIP-40)

Use the Western Design Center DIP-40 pinout. Typical bring-up:

| Pin / net | Wiring |
|-----------|--------|
| VDD | +5V, 100 nF + 10 µF nearby |
| VSS | GND |
| PHI2 | `CLK_CPU` 8.000 MHz |
| RESB | RC + 74HC14 Schmitt, also cabinet RESET, also cart `/RESET` if you expose it |
| NMIB | from GAL-TIM VBlank (open-drain or totem via GAL, pull-up 3.3 kΩ) |
| IRQB | from GAL-TIM raster (same) |
| RDY | pull-up 3.3 kΩ to +5V |
| BE | pull-up to +5V (bus enabled) |
| SOB | pull-up |
| RWB | CPU R/W to decode and RAM |
| A0–A15, D0–D7 | system buses |

Do not leave unused outputs floating into the bus. Pull unused inputs to defined levels.

---

## 5. CPU memory map (GAL-DEC must implement this)

```text
0000              7FFF 8000             FDFF FE00  FEFF FF00        FFFF
+---- System RAM ----+-+------ PRG ------+-+- I/O -+-+---- PRG -----+
       32 KB            ~32 KB (gap@$FE)    256 B     256 B+vectors
```

| CPU range | Device | Notes |
|-----------|--------|--------|
| `$0000-$7FFF` | System SRAM CS | Full 32 KB AS6C62256. A0–A14 = CPU A0–A14. |
| `$8000-$FDFF` | Cart PRG OE | Banked. Writes **ignored**. |
| `$FE00-$FEFF` | I/O page | Decode on A15–A8 == `0xFE`. Device = A7–A4. |
| `$FF00-$FFFF` | Cart PRG OE | Same PRG window family. Holds `$FFFA-$FFFF` vectors. |

**PRG banking:** **only** `$FE80`. Writes into `$8000-$FFFF` do not change banks.

**CUPL sketch (GAL-DEC):**

```text
RAM_CS  = !A15;                          /* $0000-$7FFF */
IO_CS   = A15 & A14 & A13 & A12 & A11 & A10 & A9 & !A8;  /* $FE00-$FEFF */
PRG_OE  = A15 & !IO_CS;                  /* $8000-$FDFF and $FF00-$FFFF */
EE_CS   = IO_CS & (A7:A4 == 7);          /* $FE70 block — see I/O table */
```

Qualify SRAM `WE` with `RWB` and PHI2 as needed so you never write on the wrong edge.

---

## 6. I/O page — **use these exact offsets** (B2 is closed for this proto)

Device family = high nibble of the low address byte (`(addr & 0xFF) >> 4`).

### `$FE00-$FE0F` PPU control

| Offset | Name | Dir | Reset | Function |
|--------|------|-----|-------|----------|
| `$FE00` | PPUCTRL | R/W | 0 | **bit7** NMI enable. Other bits unused (tie 0). |
| `$FE01` | PPUSTATUS | R | 0 | **bit7** vblank (clears on read). **bit6** raster_hit (sticky; see ack). |
| `$FE02` | SCROLL_X | R/W | 0 | Fine scroll 0–255 |
| `$FE03` | SCROLL_Y | R/W | 0 | Fine scroll 0–255 |
| `$FE04` | NT_ARRANGE | R/W | 0 | bits 1–0: 0=1 screen (slot0), 1=H 2-screen (0\|1), 2=V 2-screen (0\|2), 3=2×2 (slots 0–3). bit2: plane field uses slots 4–5 (software may set this at raster). |
| `$FE05` | RASTER_Y | R/W | 0 | Compare value 0–255 |
| `$FE06` | BEAM_Y | R | — | Live scanline (low 8 bits of Y counter). Read-only. |
| `$FE07` | RASTER_IRQ | R/W | 0 | **bit0** raster IRQ enable. **Write bit7=1** acks `raster_hit`. |

### `$FE10-$FE1F` VRAM port (CPU only on PHI2 high)

| Offset | Name | Function |
|--------|------|----------|
| `$FE10` | VADDR_LO | VRAM address bits 7–0 |
| `$FE11` | VADDR_HI | bits 14–8 (mask to 15 bits) |
| `$FE12` | VDATA | Read/write `vram[addr]`, then addr += VINC |
| `$FE13` | VINC | Increment: 0 treated as 1. Typical 1 or 32. |

VRAM is 32 KB. Address 15-bit. **CHR is not this chip.**

### `$FE20-$FE2F` OAM (dedicated SRAM, not VRAM)

| Offset | Name | Function |
|--------|------|----------|
| `$FE20` | OAM_ADDR | 8-bit index into 256-byte OAM |
| `$FE21` | OAM_DATA | R/W OAM[addr], auto-inc on write |
| `$FE22` | OAM_DMA | Write page `P`: copy 256 bytes from system RAM `$P00–$PFF` into OAM. Stall CPU ~512 cycles (RDY low or GAL wait). |

OAM format (NES-like, **locked for proto**): 64 sprites × 4 bytes: **Y, tile, attr, X**.  
Attr: bits 1–0 palette, bit 5 priority (1 = behind opaque BG), bit 6 H-flip, bit 7 V-flip.

### `$FE30-$FE3F` CHR cells / world

| Offset | Name | Function |
|--------|------|----------|
| `$FE30` | WORLD | bits 2–0 world 0–7 (CHR chapter) |
| `$FE31` | SPR_CELL | bits 1–0 sprite cell 0–3 within world |
| `$FE32` | BG_CELL0 | bits 1–0 for camera slot 0 |
| `$FE33` | BG_CELL1 | bits 1–0 for camera slot 1 |
| `$FE34` | BG_CELL2 | bits 1–0 for camera slot 2 |
| `$FE35` | BG_CELL3 | bits 1–0 for camera slot 3 |
| `$FE36` | BG_CELL4 | bits 1–0 for plane slot 4 |
| `$FE37` | BG_CELL5 | bits 1–0 for plane slot 5 |

CHR byte address (BG):

```text
off = world*0x8000 + bg_cell_for_current_slot*0x1000 + tile*16 + row + plane*8
```

Sprite byte address:

```text
off = world*0x8000 + 0x4000 + spr_cell*0x1000 + tile*16 + row + plane*8
```

### `$FE40-$FE5F` APU (ATmega)

Map this 32-byte window onto the AVR. **NES-like channel contract** (not cycle-accurate RP2A03 required on proto):

| Offset | Channel |
|--------|---------|
| `$FE40-$FE43` | Pulse 1 |
| `$FE44-$FE47` | Pulse 2 |
| `$FE48-$FE4B` | Triangle |
| `$FE4C-$FE4F` | Noise |
| `$FE50-$FE53` | DMC |
| `$FE54-$FE5F` | status / unused |

If you do not know NES bitfields, implement: 8-bit period lo/hi, volume, duty (2 bits), enable. Mix 4 PWM/timer channels + optional DMC 1-bit delta on a 5th pin. **Audio out = summing amp to 3.5 mm jack + arcade pin.**

### `$FE60-$FE6F` cabinet / pads (host-owned)

CPU **reads** these. CPU **writes ignored**. Motherboard latches parallel IDC switches.

| Offset | Byte |
|--------|------|
| `$FE60` | P1: bit0 Dpad Right, 1 Dpad Left, 2 Dpad Down, 3 Dpad Up, 4 X, 5 Y, 6 Coin, 7 Start |
| `$FE61` | P2: same mapping as `$FE60` |
| `$FE62` | Unused |
| `$FE63` | Unused |

Active **low** switches on the harness, inverted on the board so CPU sees **1 = pressed**. 10 kΩ pull-ups on IDC inputs.

### `$FE70-$FE7F` board EEPROM

AT28C64B (8 KB). Map `$FE70` = addr lo, `$FE71` = addr hi (mask 13 bits), `$FE72` = data. Or simpler: map EEPROM at a 256-byte window with latching address. **Simplest proto:** `$FE70` data, `$FE71` addr-lo, `$FE72` addr-hi, `$FE73` control (`WE` pulse). High scores / operator settings.

### `$FE80-$FE8F` PRG mapper

| Offset | Name | Function |
|--------|------|----------|
| `$FE80` | PRG_BANK | 8-bit bank number. PRG CPU address = `bank * 0x8000 + (cpu_addr & 0x7FFF)` into cart PRG flash. |

### `$FE90-$FE9F` MAP port (24-bit, auto-inc on data read)

| Offset | Name | Function |
|--------|------|----------|
| `$FE90` | MAP_LO | address bits 7–0 |
| `$FE91` | MAP_MID | bits 15–8 |
| `$FE92` | MAP_HI | bits 23–16 |
| `$FE93` | MAP_DATA | **read** cart MAP-ROM at addr, then addr = (addr+1) & 0xFFFFFF. Writes ignored. |

MAP-ROM is **not** in CPU space. No MAP window over RAM.

### `$FEA0-$FEFF`

Reserved. Decode as open bus (`0xFF`) or pull-ups. Do not alias to RAM.

---

## 7. VRAM chip map (32 KB)

CPU touches VRAM **only** through `$FE1x`. PPU fetches nametable/attr from this chip on PHI2 low.

| Offset | Size | Contents |
|--------|------|----------|
| `$0000-$07FF` | 2 KB | Slot 0: 960 tiles + 240 packed attrs at `+0x3C0` |
| `$0800-$0FFF` | 2 KB | Slot 1 |
| `$1000-$17FF` | 2 KB | Slot 2 |
| `$1800-$1FFF` | 2 KB | Slot 3 |
| `$2000-$2FFF` | 4 KB | Streaming scratch |
| `$3000-$37FF` | 2 KB | Plane slot 4 (parallax) |
| `$3800-$3FFF` | 2 KB | Plane slot 5 |
| `$4000-$7FFF` | 16 KB | Reserved (leave wired, unused) |

**Attr packing (not NES):** one byte = 2×2 tiles, 2 bits per tile:

- bits 0–1 top-left, 2–3 top-right, 4–5 bottom-left, 6–7 bottom-right  
- index `attrs[(ty/2)*16 + (tx/2)]`

**Interleave hardware:**

```text
vram_addr_pins = PHI2 ? cpu_vram_addr[14:0] : ppu_fetch_addr[14:0]
vram_data      = PHI2 ? cpu_data_via_245   : ppu_internal_bus
vram_WE        = PHI2 & CPU_write_VDATA
vram_OE        = always-read except during WE
```

Use **~4× 74HC157** for 15 address bits + WE/OE if needed, **1–2× 74HC245** for D0–D7. Never let CPU and PPU drive D0–D7 together.

**Wrong-phase CPU VRAM access:** hardware cannot easily abort; still qualify VDATA with PHI2 so a mistimed store is a no-op. Emulator treats it as a hard error; silicon just ignores.

---

## 8. PPU — what to actually build

This is the hard sheet. Build a **NES-class 2bpp tile renderer**, not a framebuffer.

### 8.1 Beam

Chain **74HC161** (or 74HC4040) on `CLK_DOT`:

- **X / dot:** 0…340, then wrap; HBlank when X ≥ 256.
- **Y / scanline:** 0…261; visible 0–239; VBlank 240–261.

GAL-TIM derives: `HBLANK`, `VBLANK`, `VISIBLE`, `DOT0`, `NMI` (Y==240 && X==0), `RASTER_HIT` (Y==RASTER_Y && X==0).

### 8.2 Background fetch (visible dots)

For pixel (x, y):

```text
sx = (x + SCROLL_X) & 0xFF
sy = (y + SCROLL_Y) & 0xFF   /* plus nametable slot from NT_ARRANGE when sx/sy wrap across 256 */
tx = sx / 8;  ty = sy / 8
fine_x = sx % 8;  fine_y = sy % 8
tile = vram[slot_base + ty*32 + tx]
attr = unpack 2 bits from vram[slot_base + 0x3C0 + (ty/2)*16 + (tx/2)]
chr  = cart_chr[ world*0x8000 + bg_cell_for_slot*0x1000 + tile*16 + fine_y ]     /* plane 0 */
chr2 = cart_chr[ world*0x8000 + bg_cell_for_slot*0x1000 + tile*16 + 8 + fine_y ] /* plane 1 */
ci   = 2bpp pixel from chr/chr2 at fine_x (bit 7 is left)
```

**NT_ARRANGE** selects which of slots 0–3 (or 4–5 for plane) supply that tile. Implement 1 / 2H / 2V / 4-screen like NES nametable mirroring, but 2 KB slots instead of NES 1 KB.

Shift registers hold 8 pixels so CHR fetch can happen during HBlank/early dots. **Cell/scroll writes take effect on the next tile fetch** (up to 8 px delay). That is correct.

### 8.3 Sprites (simplify if needed, but draw chips)

**Full intent:** during HBlank, scan 64 OAM Y values; copy up to **16** that hit the next scanline into secondary OAM; fetch 16 sprite rows from the active **sprite cell**; during visible dots, X-match and mux.

**Allowed proto shortcut:** scan OAM every line with a small state machine in GAL-TIM + 74HC163; drop sprites after 16; no MMC3-style A12 clock. 8×8 sprites only (no 8×16).

**Compositor (per pixel):**

1. Sprite pattern color 0 → skip sprite (transparent).
2. Else if sprite priority=1 **and** BG `ci != 0` → BG wins.
3. Else sprite wins.
4. BG `ci == 0` → shared **backdrop** (master index from palette RAM slot BG0[0]).

OAM sprite #0 is a **normal sprite**. No hit flag.

### 8.4 Palettes and DAC

- **Palette RAM:** 32 bytes (8 palettes × 4 indices). Each byte is a **6-bit master color index** 0–63.  
  - BG palettes 0–3 at `$FE08-$FE0F` **or** (if those bytes are taken) decode extra writes: **`$FE08` pal_addr, `$FE09` pal_data`**. Use this pair so we do not collide with raster regs:  
    **LOCKED for proto:** `$FE08` PAL_ADDR (0–31), `$FE09` PAL_DATA.  
  - Indices 0,4,8,12 (BG color 0) should hardware-mirror to the same backdrop byte, or software copies them. **Hardware-mirror BG color 0** if easy (write to any BG palette[0] writes all).
- **Master LUT:** 64 entries × 18-bit RGB (6-6-6) in a 27C256 / AT28C64 leftover / or three 64×8 PROMs. Program with `retr01_palette_v_01` (table in §16).
- **DAC:** 6-bit R, 6-bit G, 6-bit B **R-2R or weighted resistors** (~0.7 Vpp into 75 Ω). Also emit **CSYNC** (XOR or NOR of HSYNC+VSYNC; **negative** sync for arcade RGBS unless you annotate FIXME).  
- **S-Video / composite:** pads + analog RC / AD725-class encoder **optional**. If analog encoder is too much, **RGBS first**, leave unpopulated footprints and 75 Ω pads for Y/C and CVBS.

HSYNC: ~4.7 µs near the start of HBlank (dot ~260–276 region — pick a GAL range and note it).  
VSYNC: ~3 lines in the VBlank (e.g. Y=244–246). Polarity: **negative** pulses for RGBS proto.

---

## 9. Cartridge

### 9.1 Budget

| Region | Ceiling | Proto mapping |
|--------|---------|----------------|
| PRG | 512 KB | flash 0 |
| CHR | 256 KB | flash 1 (use 512 KB part, ignore upper half) |
| MAP | ~1.17 MB | flash 2 + 3 (1 MB) |
| **Total** | ~2 MB | 4 × SST39SF040-class 512 KB 32-pin DIP **or** 1 × 16 Mbit TSOP + adapter |

True 2 MB DIP NOR is basically gone. **Default proto cart:** 4 × **SST39SF040** (512 KB, 32-pin). If obsolete, use **SST39SF020A** / Winbond W49F002 equivalents, or one **S29GL** TSOP on the cart.

### 9.2 Motherboard cart connector (freeze this)

**2×20 = 40-pin 0.1" header**, keyed, cart plugs onto the motherboard (not a gold-finger JAMMA cart unless you also draw that). Pin 1 marked.

| Pin | Name | Pin | Name |
|-----|------|-----|------|
| 1 | +5V | 2 | +5V |
| 3 | GND | 4 | GND |
| 5 | A0 | 6 | A1 |
| 7 | A2 | 8 | A3 |
| 9 | A4 | 10 | A5 |
| 11 | A6 | 12 | A7 |
| 13 | A8 | 14 | A9 |
| 15 | A10 | 16 | A11 |
| 17 | A12 | 18 | A13 |
| 19 | A14 | 20 | A15 |
| 21 | A16 | 22 | A17 |
| 23 | A18 | 24 | D0 |
| 25 | D1 | 26 | D2 |
| 27 | D3 | 28 | D4 |
| 29 | D5 | 30 | D6 |
| 31 | D7 | 32 | /OE |
| 33 | /WE | 34 | /CE_PRG |
| 35 | /CE_CHR | 36 | /CE_MAP |
| 37 | PHI2 | 38 | /RESET |
| 39 | /IRQ (unused, NC) | 40 | GND |

**Address mux on cart / GAL-CART:**

- **PRG:** CPU A0–A14 + `PRG_BANK` as A15–A18 into PRG flash. `/CE_PRG` from GAL-DEC when PRG window. `/OE` = RWB (read). `/WE` = 1 (ROM).
- **CHR:** PPU-built 18-bit CHR address (world, cell, tile, row, plane). `/CE_CHR` during PPU CHR fetch only.
- **MAP:** 24-bit MAP latch A0–A18 into MAP flashes (two 512 KB chips: A19 selects). `/CE_MAP` on `$FE93` read.

Do **not** bus-fight PRG vs CHR vs MAP: only one `/CE` at a time. CHR fetch is PPU-phase; MAP/PRG are CPU-phase. If both could overlap, gate CHR `/CE` with `!PHI2`.

### 9.3 `.retr01` image vs silicon

File format for the emulator is header `0x30` + PRG + CHR + MAP blobs. **Hardware flash** is just those three regions. Programming is off-board (TL866). You do not need to parse the header on silicon.

---

## 10. System RAM and OAM SRAM

| Chip | PN (planning) | Use |
|------|----------------|-----|
| U-RAM | **AS6C62256-55PCN** DIP-28 | `$0000-$7FFF` |
| U-VRAM | **AS6C62256-55PCN** DIP-28 | 32 KB video |
| U-OAM | **6116** 2 KB DIP-24 if you can still buy it; else **another AS6C62256** with only 256 bytes used | `$FE2x` |

AS6C62256 pinout is the usual 32K×8 DIP-28 (A0–A14, /CE /OE /WE, I/O0–7).

---

## 11. GAL assignment (4 × ATF22V10CQZ-20PU)

Lattice GAL22V10 DIP is EOL. Use **Microchip ATF22V10CQZ-20PU**.

| GAL | Name | Job |
|-----|------|-----|
| U-GAL1 | **GAL-DEC** | RAM_CS, IO_CS, PRG_OE, EE_CS, I/O nibble strobes `/IO0`…`/IO9`, PHI2-qualified WE |
| U-GAL2 | **GAL-TIM** | 341/262 terminal counts if not using dedicated 161s for wrap, HBLANK, VBLANK, NMI, raster compare vs latched RASTER_Y, IRQB, sprite-eval window |
| U-GAL3 | **GAL-PPU** | VRAM vs CHR /CE, plane/slot address bits, fetch sequencer strobes, OAM DMA RDY |
| U-GAL4 | **GAL-IO** | Latch clocks for `$FExx` (decode A3–A0 inside each 16-byte block), MAP auto-inc, pad latch /LE |

If equations do not fit, **U-GAL5** is allowed. Publish `.pld` / `.jed` sources next to the schematic.

**22V10 reminder:** 12 inputs dedicated + 10 I/O, 10 product terms per output (varies). Keep wide compares on 74HC688 if a raster Y compare burns the GAL.

Raster compare **allowed discrete:** 74HC688 8-bit identity compare of `BEAM_Y` vs `RASTER_Y` latch, AND with `DOT0` and `IRQ_EN`.

---

## 12. 74HC planning counts (start here, add as needed)

From the cost doc, plus this brief:

| Qty | Part | Role |
|-----|------|------|
| 4–6 | 74HC157 | VRAM (and maybe CHR) address mux |
| 2–4 | 74HC245 | CPU data isolation, cart data, VRAM data |
| 10–16 | 74HC573 | Scroll, CHR cell latches, MAP addr, pads, palette addr, PPUCTRL |
| 6–10 | 74HC161 | Dot X, line Y, sprite X, DMA count |
| 2–4 | 74HC166 / 74HC595 / 74HC299 | BG + sprite pixel shift |
| 4–8 | 74HC00/04/08/32/86 | leftover gates if GAL full |
| 1–2 | 74HC688 | raster compare, 341 detect |
| 1–2 | 74HC393 | clock divide 21.477 → 5.369 |

**Do not** try to stay under a chip count if it makes the PPU fictional. 50–80 DIP 74HC is in-family.

---

## 13. APU (ATmega)

**Default IC:** ATmega328P-PU 16 MHz crystal (independent of 8 MHz CPU). If flash is tight, **ATmega1284P-PU**.

- CPU writes `$FE4x` → GAL pulses `/APU_WR` + puts data/addr on a small 74HC573 captured by AVR (external interrupt or poll).  
- Simpler: map the 32-byte page onto AVR **external SRAM-style** (ALE + /RD /WR) if you use a 1284.  
- **Simplest that still works:** 8-bit addr+data latches; AVR interrupt on write; AVR synthesizes pulse/triangle/noise in firmware; PWM on OC1A through a 1 kΩ + 100 nF LPF to an LM386 or just a summing resistor to a 3.5 mm jack.

DMC can be a stub (silence) on proto. Label it FIXME. Do not block the board.

Audio jack: stereo OK (same mono on both). Also route to IDC for cabinet speaker.

---

## 14. Cabinet IDC (20-pin, freeze pinout)

Standard 0.1" 2×10 IDC (controller header). Active-low inputs.

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| 1 | +5V | 2 | GND |
| 3 | P1 Right (bit0) | 4 | P1 Left (bit1) |
| 5 | P1 Down (bit2) | 6 | P1 Up (bit3) |
| 7 | P1 X (bit4) | 8 | P1 Y (bit5) |
| 9 | P1 Coin (bit6) | 10 | P1 Start (bit7) |
| 11 | P2 Right (bit0) | 12 | P2 Left (bit1) |
| 13 | P2 Down (bit2) | 14 | P2 Up (bit3) |
| 15 | P2 X (bit4) | 16 | P2 Y (bit5) |
| 17 | P2 Coin (bit6) | 18 | P2 Start (bit7) |
| 19 | /RESET in (optional) | 20 | Service/Tilt (optional / unused in v1) |

RGBS video is **not** on this IDC (use a separate 5-pin 0.1" header: R, G, B, CSYNC, GND, each 75 Ω series).

---

## 15. Power

- Barrel: **female 5.5 mm × 2.1 mm**, **center positive**, **5 V**.  
- 2 A polyfuse. Reverse Schottky. Bulk 470 µF + 100 nF per IC.  
- Linear is fine; no switching required.  
- **Unpopulated** 4 pads: USB-C breakout `VBUS, GND, GND, GND` (no CC resistors on our PCB).  
- Target draw guess: 800 mA–1.5 A. Size fuse 2 A.

Reset: 10 kΩ + 10 µF on RESB, 74HC14, momentary to GND (and optionally via cabinet IDC pin 19 `/RESET`).

---

## 16. Master palette v0.1 (program the LUT)

64 colors, index 0 = backdrop `#000000`. Rows dark→light. Pack as 6-6-6 by taking high 6 bits of each 8-bit channel, or use 8-8-8 DACs if you prefer (then this table is exact).

| Idx | Hex | Idx | Hex | Idx | Hex | Idx | Hex |
|-----|-----|-----|-----|-----|-----|-----|-----|
| 0 | `#000000` | 16 | `#363636` | 32 | `#949494` | 48 | `#FFFFFF` |
| 1 | `#290514` | 17 | `#740A40` | 33 | `#C04A7A` | 49 | `#F1A2BB` |
| 2 | `#2A0507` | 18 | `#77091A` | 34 | `#C54A4D` | 50 | `#F1A6A1` |
| 3 | `#230F06` | 19 | `#693512` | 35 | `#B8601B` | 51 | `#F1A983` |
| 4 | `#1E1306` | 20 | `#5D3F0E` | 36 | `#A27326` | 52 | `#EEAC44` |
| 5 | `#1A1605` | 21 | `#514617` | 37 | `#8F7E2F` | 53 | `#D4BA33` |
| 6 | `#141807` | 22 | `#424C19` | 38 | `#77872D` | 54 | `#B0C841` |
| 7 | `#061A07` | 23 | `#13511A` | 39 | `#209030` | 55 | `#73D275` |
| 8 | `#051A13` | 24 | `#16503F` | 40 | `#2E8E72` | 56 | `#22D0A6` |
| 9 | `#071918` | 25 | `#114E4D` | 41 | `#318B89` | 57 | `#3BCDC9` |
| 10 | `#08181C` | 26 | `#164D58` | 42 | `#1F889C` | 58 | `#48C9E4` |
| 11 | `#071722` | 27 | `#164A66` | 43 | `#2483B5` | 59 | `#88C4ED` |
| 12 | `#030B3D` | 28 | `#163794` | 44 | `#4D77D7` | 60 | `#A4BDEF` |
| 13 | `#16033A` | 29 | `#472990` | 45 | `#7E6AD3` | 61 | `#BBB5F1` |
| 14 | `#20052D` | 30 | `#5F167D` | 46 | `#9D5DBF` | 62 | `#D5A9EF` |
| 15 | `#260420` | 31 | `#6C115F` | 47 | `#B352A0` | 63 | `#F09BDD` |

Default BG palettes (master indices): `[0,20,36,52]`, `[0,23,39,55]`, `[0,27,43,59]`, `[0,18,34,50]`.  
Default sprite palettes: `[0,32,48,49]`, `[0,35,51,50]`, `[0,39,55,54]`, `[0,43,59,58]`. Hardware does not need these baked in; software loads palette RAM.

---

## 17. Test / bring-up hooks (put on the schematic)

- 2×5 debug header: A0–A7 + GND or a 6502 bus breakout.  
- LEDs: PHI2 (buffered), VBLANK, IRQ, HALT/stopped unused.  
- Jumpers: `/NMI` disable, `/IRQ` disable.  
- UART from ATmega (TX/RX pads) for APU debug.  
- Cart `/OE` LED.  
- Probe points: `CLK_CPU`, `CLK_DOT`, `HSYNC`, `VSYNC`, `PHI2`, `RAM_CS`, `VRAM_CS`.

A first ROM can be a 32 KB PRG with reset vector `$FFFC` = `$8000`, `JMP $8000`, NOP fill — still map it on cart flash.

---

## 18. Allowed shortcuts (use these rather than stopping)

| If stuck on… | Do this |
|--------------|---------|
| Cycle-perfect 341 counter | 9-bit counter + GAL equal-to-341 reset. Close enough if visible is 256 and V=262. |
| Perfect sprite overflow | Hard cap 16, drop rest. |
| 8×16 sprites | Skip. 8×8 only. |
| DMC | Silence. |
| Composite / S-Video | Footprints only. RGBS works. |
| Palette color 0 hardware mirror | Software copies; skip mirror. |
| OAM DMA cycle steal | Pulse RDY for ~512 PHI2 cycles **or** pause PHI2 with a 74HC02 gate (ugly but proto-legal). Document which. |
| Fine X scroll at 8 MHz vs 5.37 MHz | Independent clocks; use line buffers. Do **not** couple the oscillators. |
| GAL overflow | Add ATF #5. |
| 2 MB DIP flash | 4 × 512 KB or one TSOP. |
| Exact analog IRE / sync | Negative CSYNC, 75 Ω, 0.7 V RGB. Tune later. |
| Parallax planes | Slots 4–5 are just more nametable SRAM. Software + raster IRQ does the rest. No extra compositor layer in v0. |

**Forbidden shortcuts:** FPGA-as-entire-PPU with no 74HC; dropping VRAM interleave; mapping CHR into VRAM; banking PRG from writes to `$8000`; HDMI PHY; 3.3 V-only CPU; omitting cart MAP port.

---

## 19. Reference parts (buyable, Aug 2026 planning)

| Qty | PN | Package | Role |
|-----|----|---------|------|
| 1 | W65C02S6TPG-14 | DIP-40 | CPU |
| 2–3 | AS6C62256-55PCN | DIP-28 | RAM / VRAM / optional OAM |
| 4–5 | ATF22V10CQZ-20PU | DIP-24 | Glue |
| 1 | AT28C64B-15PU | DIP-28 | EEPROM |
| 1 | ATmega328P-PU | DIP-28 | APU |
| 1 | OSC 8.000 MHz DIP-8/14 | — | CPU |
| 1 | OSC 21.47727 MHz | — | Dot source |
| 4 | SST39SF040 or equiv. | DIP-32 on **cart** | Flash |
| ~50–80 | 74HC00/04/08/32/86/157/161/166/245/573/688/393 | DIP-14/16/20 | Glue |
| 1 | LM1117-5.0 **or omit if barrel is already 5 V** | — | Only if you accept 7–9 V in (default: **barrel is 5 V, no regulator**) |
| 1 | 3.5 mm jack | — | Audio |
| 1 | Barrel PJ-102A class | — | Power |
| 1 | IDC 40 | — | Cabinet |
| 1 | Header 40 | — | Cart |

Sockets for CPU, SRAM, ATF, AVR, EEPROM, flash.

---

## 20. ERC / DRC expectations

- Every IC: 100 nF at VCC.  
- No unconnected 74HC inputs.  
- Data bus: one driver (245 dir bit from GAL).  
- Cart `/CE_*` mutually exclusive.  
- RESB, NMIB, IRQB have pull-ups.  
- Video 75 Ω series on R,G,B,CSYNC.  
- ESD: nothing fancy required; mention TVS on IDC later.

---

## 21. How to title the project in CAD

```text
Retr01-A Arcade Motherboard + Cart
Rev A0  — first full schematic, known-imperfect PPU
License: same as Retr01 repo (ask owner if unsure)
```

Sheet 00 must say in a text box:

> Raster IRQ replaces NES sprite-0. CHR is on the cartridge. VRAM is interleaved on PHI2. I/O map is `$FExx` as in sheet 06. Do not “fix” this to NES `$2000`.

---

## 22. Success criteria for *your* output

You succeeded if a human can:

1. Open the schematic and see a W65C02S talking to 32 KB RAM and `$FExx` latches.  
2. See VRAM behind 157/245 with a PHI2 select.  
3. See X/Y counters making 256×240 + NMI.  
4. See a cart connector with PRG, CHR, MAP chip enables.  
5. See RGBS pads and an AVR APU.  
6. Flash *something* into cart PRG and get the CPU out of reset (even if video is wrong).

Video not matching the emulator on day one is acceptable. **A blank PPU sheet is not.**

**Start drawing now. Freeze every TBD with the tables in this file. Generate the complete schematic, cart, GAL sources, and BOM.**
