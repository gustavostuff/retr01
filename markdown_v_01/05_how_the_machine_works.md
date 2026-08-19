# How Retr01-A Hardware Fits Together

A walkthrough of **every kind of part** on the arcade board: chips, clocks, buses, connectors, passives, video, pads. Start here if 14/15 feel like a parts list and 06 feels like metaphors. Exact addresses: [08](08_memory_map.md). Chip counts: [14](14_reduced_number_of_chips.md). Simulator build order: [16](16_simulation_and_bringup_plan.md). Proto-board islands: [17](17_protoboard_test_plan.md).

---

## 1. The machine in one picture

Retr01-A is three computers sharing one 5 V board, plus a pile of 74HC that draws the screen without asking the 6502 to plot pixels.

```text
                    8.000 MHz                  ~5.369 MHz
                        |                           |
                        v                           v
   cart flash -----> W65C02S  ---- $FExx ----->  BG PPU (74HC + VRAM)
   PRG / MAP            |                         161s, 157s, compositor
                        |                           |
                        |         $FE21 OAM         |  CHR /CE (exclusive)
                        +--------> ATmega1284 -----> line-buffer SRAM --> pixels
                        |          pads $FE60/61         |
                        |                               v
                        +--------> ATmega328P -----> speaker
                                   $FE40 audio              RGBS jack
```

| Brain | Clock | Job |
|-------|-------|-----|
| **W65C02S** | 8.000 MHz PHI2 | Game: physics, AABB, streaming screens, poking registers |
| **BG PPU** | independent dot clock | Walk 341×262, fetch nametables from VRAM, fetch BG patterns from cart, emit BG pixels |
| **ATmega1284P** | 20 MHz crystal | OAM, sprite CHR, next-line buffer, pad bytes |
| **ATmega328P** | 16 MHz crystal | NES-style sound from `$FE40` writes |

The 6502 **never** writes a framebuffer. It prepares nametables, OAM, scroll, banks. Video silicon turns that into RGB every dot.

---

## 2. Shared copper (buses)

Everything that is not a dedicated point-to-point net hangs off a few shared buses. A bus is just eight (or sixteen) wires. **Only one chip may drive a given wire at a time.** The rest must sit in High-Z (disconnected) or they short.

| Bus | Width | Typical drivers | Typical listeners |
|-----|-------|-----------------|-------------------|
| CPU **address** | 16 (A0–A15) | W65C02S only | GAL-DEC, RAM, cart, I/O strobes |
| CPU **data** | 8 (D0–D7) | CPU on write; RAM/ROM/I/O on read | whoever `/CS` selected |
| VRAM **address** | 15 | **Mux:** CPU `$FE1x` latch **or** PPU beam | VRAM SRAM only |
| VRAM **data** | 8 | CPU (via 245) **or** PPU | VRAM SRAM; never both |
| Cart **address** | A0–A18 class | CPU (PRG/MAP) **or** PPU/1284 (CHR) | flash `/CE` picks which chip |
| Cart **data** | 8 | flash output | CPU or PPU/1284 |
| Line-buffer **address** | 8+bank | **Mux:** beam X **or** 1284 | SRAM #3 |
| Line-buffer **data** | 8 | 1284 (HBlank write) **or** SRAM (visible read) | compositor |

**Chip select (`/CE`, `/CS`, `/OE`, `/WE`)** is how you pick a listener. GAL-DEC looks at A15–A8 (and PHI2, R/W) and pulls **one** chip’s `/CS` low.

Control pins the CPU always has:

| CPU pin | Meaning |
|---------|---------|
| `RWB` | 1 = read, 0 = write |
| `PHI2` | clock; data is valid around the rising/high phase |
| `NMIB` | VBlank interrupt (edge), pull-up |
| `IRQB` | raster interrupt (level), pull-up |
| `RESB` | reset, via HC14 Schmitt |
| `BE` | bus enable — **strap high** |
| `RDY` | ready — **strap high** (no DMA stall) |

---

## 3. Two clocks, two time bases

They are **not** a 3:1 pair like the NES. Do not cycle-count 6502 instructions to hit a pixel.

| Clock | Source | What it paces |
|-------|--------|----------------|
| **PHI2** 8.000 MHz | canned oscillator | 6502, system RAM, VRAM **ownership mux**, `$FExx` writes |
| **Dot** ~5.369 MHz | 21.477 MHz ÷ 4 | 161 beam counters, nametable fetch sequencer, compositor, analog RGB |

Line = 341 dots (256 visible + 85 HBlank). Frame = 262 lines (240 visible + 22 VBlank). NMI at the start of line 240 (~60.1 Hz).

**Interleave is on PHI2, not on the dot clock.** The PPU still needs a pixel every ~186 ns. Shift registers / the line buffer hold a row of pixels so the screen does not blink during the CPU’s VRAM half-cycle. Details in §7.

---

## 4. Who may read and write whom

Rows are the **actor**. Columns are the **memory or port**. R = read, W = write, — = never.

| Actor | System RAM | VRAM chip | Line buffer | Cart PRG | Cart CHR | Cart MAP | OAM in 1284 | `$FE60` pads | EEPROM | APU regs |
|-------|------------|-----------|-------------|----------|----------|----------|-------------|--------------|--------|----------|
| **6502** | R/W always | R/W only via `$FE1x` on CPU phase | — | R (`$8000`/`$FF`) | — | R via `$FE90` | W `$FE20/21` | R `$FE60/61` | R/W `$FE7x` | W `$FE4x` |
| **BG PPU** | — | R on PPU phase | R visible (X) | — | R visible | — | — | — | — | — |
| **1284** | — | — | W next bank (HBlank) | — | R HBlank only | — | R/W internal | samples pads | — | — |
| **328P** | — | — | — | — | — | — | — | — | — | owns synthesis |
| **Cart** | — | — | — | driven | driven | driven | — | — | — | — |

Rules that fall out of that table:

- The 6502 **cannot** `LDA` a CHR byte. Patterns are a PPU/1284 fetch path.
- The 6502 **cannot** `LDA` a MAP byte from `$8000`. MAP is a port.
- The PPU **cannot** write VRAM. Only the CPU port can.
- The 1284 **cannot** see system RAM. OAM upload is a 6502 store loop, not DMA.
- CHR `/CE` is **exclusive**: visible line = BG PPU; HBlank = 1284. GAL-PPU enforces that.

---

## 5. Each block, alone and then wired

### 5.1 Power barrel, fuse, caps

**Alone:** 5 V DC in. Polyfuse opens on a short. Reverse diode dies instead of the ICs if you plug the barrel in backwards. 100 nF at each chip kills edge noise. Bulk 10–100 µF at the jack holds the rail when many chips switch at once.

**With the rest:** every DIP’s VCC/GND. No 3.3 V rail on v0 (AVRs and ATF run 5 V).

### 5.2 Oscillators and crystals

**Alone:** a canned oscillator is a clock in a can: power it, get a square wave. A crystal + 22 pF is analog; the AVR oscillates it internally.

**With the rest:** 8 MHz → CPU `PHI2`. 21.477 MHz → ÷4 → 161s. 16 MHz crystal → 328P. 20 MHz crystal → 1284. GAL and 74HC do not generate these clocks.

### 5.3 W65C02S

**Alone:** 8-bit CMOS 6502. 16-bit address, 8-bit data, fully static (PHI2 may stop). Extra instructions vs NMOS 6502. Pins 1 / 5 / 36 are **not** MOS 6502 (`VPB` out, `MLB` out, `BE` in).

**With the rest:** broadcasts A/D every instruction. GAL-DEC decides who answers. Does not talk to VRAM pins directly — only through `$FE1x` and the 157/245 mux. Interrupts: `NMIB` from GAL-TIM (VBlank), `IRQB` from raster compare.

### 5.4 GAL-DEC (ATF22V10 #1)

**Alone:** 10 outputs of compiled boolean. Typical: `RAM_CS = !A15`, `IO_CS = (A15..A8 == $FE)`, `PRG_OE = A15 && !IO_CS`.

**With the rest:** watches CPU A15–A8 (and maybe RWB, PHI2). Strobes EEPROM, `$FExx` nibbles, cart `/CE_PRG`. Does **not** mux 15-bit VRAM addresses (that is 4× 157).

### 5.5 System SRAM (chip 1)

**Alone:** 32 768 bytes. A0–A14, D0–D7, `/CE`, `/OE`, `/WE`.

**With the rest:** A0–A14 = CPU A0–A14. `/CE` = GAL `RAM_CS`. **No mux.** CPU owns it 100% of PHI2 cycles. Game state, stack `$0100`, decompress scratch if you put it here (streaming temps can also sit in VRAM `$2000`).

### 5.6 VRAM SRAM (chip 2) + 4× 74HC157 + 245

**Alone:** same 32 KB part as system RAM.

**With the rest:** this is the only interleaved memory. See §7.

CPU path: write VADDR to `$FE11`/`$FE12`, then `$FE13` data. GAL + 573s hold the 15-bit VRAM address during the CPU phase. 157s select **that** address vs the PPU’s nametable address. 245 connects CPU D to VRAM D only on the CPU phase when the VRAM port is selected.

PPU path: 161 X/Y + scroll + NT_ARRANGE → nametable index → VRAM A. PPU reads tile bytes and packed attrs. PPU never writes this chip.

### 5.7 Cartridge (flash, not on the motherboard count)

**Alone:** parallel NOR. `/CE` `/OE` `/WE`. Byte at an 19-bit-class address.

**Three regions, three `/CE`s** (GAL never asserts two at once):

| `/CE` | Who addresses it | When |
|-------|------------------|------|
| PRG | CPU A0–A14 + `$FE80` bank bits | CPU reads `$8000–$FDFF` / `$FF00–$FFFF` |
| CHR | PPU or 1284 (tile, row, plane, `$FE30` banks) | Visible = BG; HBlank = sprites |
| MAP | `$FE90` 24-bit latch auto-inc | CPU read of MAP data |

`/WE` tied high on ROM carts (program off-board with a TL866).

Motherboard cart connector also carries +5, GND, PHI2, `/RESET`.

### 5.8 74HC161 beam counters + GAL-TIM

**Alone:** a 161 is a 4-bit binary counter. Four of them make ~9-bit X and ~9-bit Y.

**With the rest:** clocked by **dot**. Wrap X at 341, Y at 262 (GAL-TIM + maybe a 688). Outputs:

- `HBLANK` when X ≥ 256  
- `VBLANK` when Y ≥ 240  
- `NMI` pulse at Y = 240, X = 0  
- `DOT0` / raster: 688 compares Y to latched `RASTER_Y` → `IRQB` if enabled  

The 6502 does not increment these. It only reads `BEAM_Y` if you expose it on `$FE06`.

### 5.9 74HC573 latches

**Alone:** 8 D-flip-flops with a transparent latch enable. Capture a byte, hold it forever after `/LE` goes inactive.

**With the rest:** scroll X/Y, NT arrange, `$FE30` banks, MAP address, OAM addr/data capture toward the 1284, pad bytes toward the CPU. Software “variable.” Hardware “register.”

### 5.10 BG fetch, attrs, 2bpp shifters (74HC + GAL-PPU)

**Alone:** gates and shift registers. Not a named CPU.

**With the rest:** each visible tile:

1. Compute `sx = x + scroll_x`, `sy = y + scroll_y` (8-bit wrap).  
2. Pick nametable slot from `NT_ARRANGE`.  
3. Read tile index from VRAM (PPU phase).  
4. Read 2-bit palette from packed attr (`+0x3C0` in the slot).  
5. Address cart CHR: `(world*4+bg_bank)*0x2000 + tile*16 + fineY + plane*8`.  
6. Shift out 8 pixels. Color 0 = shared backdrop.

GAL-PPU owns VRAM `/OE` `/WE` vs PHI2, and CHR `/CE` vs HBlank.

### 5.11 ATmega1284P (sprites + pads)

**Alone:** 8-bit AVR, 16 KB SRAM, 32 GPIO, 20 MHz. Firmware, not 6502 code.

**With the rest:**

- **OAM:** 256 bytes inside the 1284. CPU `STA $FE21` → GAL `OAM_WR` → 1284 copies into OAM[addr].  
- **Eval:** during the **visible** line (~63.5 µs), scan 64 Y values, keep ≤16 for line *N+1*.  
- **Blit:** during **HBlank**, take CHR bus, 16×2 plane reads, write SRAM #3 next bank.  
- **Pads:** IDC switches (active low, 10 kΩ pull-ups) into 573s; 1284 and/or GAL present inverted bytes on `$FE60`/`$FE61`.

It does **not** drive RGB. It only fills the line buffer the compositor already knows how to read.

### 5.12 Line-buffer SRAM (chip 3) + 2× 157

**Alone:** yet another 32 KB chip. Only **512 bytes** used: `$0000–$00FF` and `$0100–$01FF`.

**With the rest:**

| Time | Address source | Data |
|------|----------------|------|
| Visible | `{display_bank, BEAM_X[7:0]}` | SRAM `/OE` → compositor. `/WE` high |
| HBlank | 1284 A0–A7 | 1284 `/WE` pulses. Rest of the line left transparent (color 0) |

Swap banks at start of HBlank / next line. Byte packing (planning): bits 1–0 color, 3–2 sprite pal, 4 priority.

### 5.13 Compositor + palette + RGBS

**Alone:** gates that pick BG vs sprite per pixel, then a 6-bit index through a LUT, then resistor DAC.

**With the rest:** every visible dot:

1. BG `ci` from shifters.  
2. Sprite byte from line buffer at X.  
3. Sprite color 0 → BG. Else if sprite priority and BG opaque → BG. Else sprite.  
4. 6-bit master color → R, G, B R-2R → 75 Ω → jack.  
5. CSYNC (negative) from HBlank/VBlank windows.

This is analog. Digital sim stops at the 6-bit index.

### 5.14 ATmega328P (APU)

**Alone:** smaller AVR. Timers + PWM mix NES-style channels.

**With the rest:** CPU writes `$FE40–$FE5F` into a 573; 328P IRQs or polls. Output → mix → 3.5 mm jack. **No** CHR, **no** OAM, **no** pads.

### 5.15 Board EEPROM

**Alone:** 8 KB parallel AT28C64B.

**With the rest:** `$FE7x` port. High scores / operator. Slow. Not video.

### 5.16 Cabinet IDC and RGBS header

**Alone:** connectors. Not logic.

**With the rest:** 40-pin IDC = 16 player switches + power/GND/reset/speaker. RGBS is a **separate** 5-pin header (R G B CSYNC GND). Coin/Start **are** bits 6–7 of the two pad bytes, not extra CPU registers.

### 5.17 Glue 74HC00/04/14/08/32/86/688

**Alone:** NAND, invert, Schmitt, AND, OR, XOR, 8-bit identity compare.

**With the rest:** invert PHI2 for mux select, qualify `/WE` with phase, make CHR BG vs 1284 exclusive, compositor XOR/mux, reset Schmitt, 688 for `Y == RASTER_Y` (and maybe 341 wrap if GAL-TIM is full). Frozen qty: [14](14_reduced_number_of_chips.md).

---

## 6. Interleaved VRAM (the mutex)

System RAM is **not** interleaved. Only **VRAM chip 2** is.

PHI2 **high** (planning): CPU may use the VRAM **port**. 157s present the latched `$FE1x` address. 245 connects CPU D. PPU must not drive VRAM D.

PHI2 **low**: PPU presents nametable/attr address. CPU 245 is High-Z. If guest code “touches VRAM” on this phase, that is a **hardware bug** (emulator: hard error in debug).

```text
          CPU A/D                 PPU fetch addr
              |                         |
              +---- 4x 157 A mux -------+
                         |
                    VRAM A0-A14
                         |
                    VRAM D0-D7 ---- 245 ---- CPU D  (only CPU phase)
                         |
                    PPU tile/attr latch     (only PPU phase)
```

Why the screen does not flicker at 50% VRAM duty: the PPU copies a tile row into **shift registers** on its phase, then shifts pixels out on every **dot** while the CPU is using VRAM on the other phase. Two time domains, a small FIFO of bits between them.

CHR is a **second** bus on the cart, not this SRAM. Interleave does not apply to CHR except that `/CE_CHR` is time-sliced BG vs 1284.

Line-buffer SRAM is a **third** mutex: beam vs 1284, switched on HBlank, not on PHI2.

---

## 7. One scanline, all actors

Visible dots 0–255:

- 161s count X.  
- BG PPU: VRAM on PPU phases, shift pixels, compositor reads line-buffer[X] (the bank filled **last** HBlank).  
- 1284: **not** on CHR. Scanning OAM for **next** line.  
- 6502: running game code; may hit VRAM port on CPU phases; may write scroll in HBlank if you wait.

HBlank dots 256–340:

- Compositor idle (or border).  
- 1284 owns CHR, writes next line-buffer bank.  
- Banks swap so the buffer just filled becomes the display bank on the next line.

VBlank lines 240–261:

- NMI. 6502 uploads OAM (256 `STA $FE21`), refills nametable seams, APU pokes.  
- 1284 can copy OAM and sample pads.  
- No visible pixels.

---

## 8. What the 6502 actually touches

Think of `$FExx` as the only API:

| You store here | Hardware that moves |
|----------------|---------------------|
| `$FE02/03` scroll | 573s → BG fetch address math |
| `$FE1x` VRAM | 157/245/SRAM #2 |
| `$FE21` OAM | 573 → 1284 internal RAM |
| `$FE30` banks | 573s → CHR address (BG and sprite halves) |
| `$FE4x` | 573 → 328P |
| `$FE80` | 573 → cart PRG A15+ |
| `$FE90` | 573 → cart MAP address, then read data |

You **load** `$FE60`/`$FE61` (pads) and status (`vblank`, `raster_hit`, `beam_y`).

That is the whole contract between “game” and “picture.” Everything else in this file is the picture keeping itself alive at 5.369 MHz without the game’s help.
