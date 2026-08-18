# Retr01-A Core IC Layout (Coprocessor Architecture)

Motherboard IC plan after offloading **sprites and player input** to a dedicated AVR. Through-hole only. Planning total is the table sum at the bottom (~**53** ICs vs the old ~63 discrete-sprite estimate).

This file is the coprocessor / input / chip-count source of truth until `03` / `08` / `12` are revised to match.

---

## 1. Locked decisions

| Topic | Decision |
|-------|----------|
| Sprite + input MCU | **ATmega1284P-PU** (DIP-40), 20 MHz crystal. Pins + SRAM the 328P does not have. |
| Audio MCU | **ATmega328P-PU** (DIP-28). Separate chip. Do not merge APU and sprites. |
| OAM storage | **Inside the 1284** (256 bytes). Not in the extra SRAM. |
| Extra SRAM | **Sprite line buffer only** (ping-pong, video-rate readout). Beam indexes it with X. |
| OAM upload | 6502 **store loop** to `$FE21` (auto-inc). **No** hardware DMA / `RDY` steal. `$FE22` unused. |
| Sprite timing | **One-line pipeline** (already in the docs). See §4. |
| Player input | **One byte per player**, all form factors. See §3. |
| Retr01-C pads | **3-wire** cable, MCU **in the controller** serializes 8 bits. Board 1284 reconstructs the same byte as arcade GPIO. |

---

## 2. Core ICs (motherboard)

| Subsystem | Qty | Part (through-hole DIP) | Role |
|-----------|-----|-------------------------|------|
| **CPU** | 1 | W65C02S6TPG-14 (DIP-40) | Game logic, MAP stream, BG VRAM via `$FE1x`. 8.000 MHz PHI2. |
| **APU** | 1 | ATmega328P-PU (DIP-28) | NES-style pulse / triangle / noise / DMC. `$FE40–$FE5F`. |
| **Sprite + input** | 1 | **ATmega1284P-PU (DIP-40)** | OAM in internal RAM; eval + CHR fetch; writes next line-buffer bank; latches P1/P2 bytes. |
| **System + VRAM** | 2 | AS6C62256-55PCN (DIP-28) | 32 KB CPU RAM + 32 KB interleaved VRAM. Unchanged. |
| **Line buffer** | 1 | 2 KB-class SRAM (6116 DIP-24 if buyable, else another 62256) | Two 256-byte banks. Visible: PPU reads `{bank, X}`. HBlank: 1284 writes the other bank. **Not OAM.** |
| **EEPROM** | 1 | AT28C64B-15PU (DIP-28) | High scores / operator settings. |
| **GAL** | 3 | ATF22V10CQZ-20PU (DIP-24) | Decode, beam/NMI/IRQ, CHR/VRAM `/CE`. (Was 4; sprite-eval GAL is gone.) |
| **Beam counters** | 4 | 74HC161 (DIP-16) | Dot X and line Y (341 × 262). No sprite-X 161s. |
| **Latches** | 7 | 74HC573 (DIP-20) | Scroll, banks, I/O, 6502→1284 OAM write capture. |
| **VRAM mux** | 4 | 74HC157 (DIP-16) | 15-bit VRAM address CPU vs PPU. **Do not cut this to 2** — one 157 is only 4 bits. |
| **Line-buffer mux** | 2 | 74HC157 (DIP-16) | Buffer address: beam X vs 1284. |
| **Transceivers** | 3 | 74HC245 (DIP-20) | CPU / VRAM / cart isolation. |
| **Glue** | 16 | 74HC00/04/08/32/86/688 mix | PHI2 interleave, HBlank, CHR BG vs sprite `/CE`, compositor (BG vs line-buffer pixel). |
| **Clocks** | 2 | Canned oscillators | 8.000 MHz CPU; 21.477 MHz ÷ 4 → ~5.369 MHz dot. |
| **Power** | 1 | 5 V regulator module | Optional if the barrel is already a clean 5 V. Counted for cabinet-tolerant builds. |
| **Total** | **53** | | Planning motherboard ICs (not sockets, not cart flash). |

Band if a proto grows muxes or glue: **48–56**. Cart flash chips sit on the **cartridge**, not in this total.

---

## 3. Input — one byte per player (A / C / H)

Eight bits including directions. **No extra action buttons. No separate cabinet byte.** Select/Start **are** Coin/Start on arcade.

| Bit | Name | Arcade | Console (Retr01-C) | Handheld (Retr01-H) |
|-----|------|--------|--------------------|---------------------|
| 0 | Right | stick | d-pad | d-pad |
| 1 | Left | stick | d-pad | d-pad |
| 2 | Down | stick | d-pad | d-pad |
| 3 | Up | stick | d-pad | d-pad |
| 4 | A | button | button | button |
| 5 | B | button | button | button |
| 6 | Select / Coin | **Coin** (P1=`Coin1`, P2=`Coin2`) | Select | Select |
| 7 | Start | **Start** | Start | Start |

CPU view (6502):

| Addr | Byte |
|------|------|
| `$FE60` | Player 1 |
| `$FE61` | Player 2 |

Reads only. Writes ignored. 1284 updates the latches each frame (or continuously). **1 = pressed** after onboard invert.

**Retr01-A:** 16 GPIO on the 1284 (8+8) from the IDC. Pull-ups, active-low switches.

**Retr01-C:** 3-wire pad (clock / data / latch or equivalent). A small MCU **inside each controller** samples 8 buttons and shifts one byte. The 1284 on the motherboard deserializes into `$FE60` / `$FE61`. Same software contract as arcade.

**Retr01-H:** same 8 bits, buttons on the shell into the 1284 (or a later SMD sibling). Same `$FE6x` bytes.

Service / tilt are **not** in the CPU map for v1. Leave unconnected IDC pins if a cabinet wants them later.

---

## 4. Sprite pipeline (matches existing docs)

Already specified:

- [06_hardware_for_software_engineers.md](06_hardware_for_software_engineers.md): during **HBlank**, scan OAM Y, fill a line buffer for at most **16** sprites on the **next** scanline, fetch sprite CHR.
- [07_emulator_specification.md](07_emulator_specification.md): same secondary-OAM / next-line model.

That **is** a one-line pipeline: pixels on line *N* were chosen at the end of line *N−1*. Not an extra frame of lag. BG bank/scroll still have the documented **up to 8 px** fetch delay; that is separate.

The 1284 cannot finish a 64-sprite scan in 85 HBlank dots (~16 µs). It uses the **whole current line** (~63.5 µs visible + HBlank) to build the **next** line’s buffer:

1. **Visible dots:** compositor clocks the **current** SRAM bank with beam X. 1284 scans internal OAM, keeps ≤16 hits for line *N+1*.
2. **HBlank:** 1284 owns cart **CHR** (`/CE_CHR` exclusive vs BG), reads 16 × 2 bitplane bytes, expands 2bpp, writes the **next** SRAM bank (only sprite pixels; rest transparent / cleared).
3. **Start of next line:** banks swap.

Guest 6502 code does not see a new delay beyond what 06/07 already described. The emulator (when rewritten) should keep **next-line sprite eval**, not same-line 74HC magic.

**6502 OAM port** (`$FE20–$FE21`): GAL clocks a 573 on write; 1284 copies into internal OAM. Upload with a 256-byte loop in VBlank. 64 sprites × 4 bytes: Y, tile, attr, X (NES-like, [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) B6).

---

## 5. Why the 1284 (not a second 328P)

| Need | 328P (DIP-28) | 1284P (DIP-40) |
|------|----------------|----------------|
| GPIO | 23 | 32 |
| P1+P2 (16) + OAM port + CHR data + SRAM `/WE` | does not fit | fits |
| Internal RAM for OAM + working set | 2 KB, tight | 16 KB, easy |
| Line-time budget at 20 MHz | ~1.0k cycles/line | ~1.3k cycles/line |

The **328P stays the APU**. Two AVRs, two jobs.

---

## 6. What this removes / adds

| Change | Approx. chips |
|--------|----------------|
| Removed discrete sprite scanner (161 X-units, eval comparators, sprite shift farm) | −12 to −15 |
| Removed input shift registers (`74HC165`) | −2 |
| Added 1284P | +1 |
| Kept VRAM 157s at 4 (was wrongly cut to 2 in the first draft of this file) | 0 vs old VRAM mux |
| Added 2× 157 for line-buffer address mux | +2 |
| Line-buffer SRAM instead of “OAM SRAM” | same 1 SRAM, different job |

Net vs original ~63: about **−10**, landing at **53** in the table. Still a smaller board than a full discrete sprite PPU.

**Not removed:** BG nametable fetch, attr unpack, VRAM interleave, beam counters, RGB DAC, cart PRG/CHR/MAP decode. Those stay 74HC + GAL.

---

## 7. GAL split (3 × ATF22V10)

| GAL | Job |
|-----|-----|
| GAL-DEC | `RAM_CS`, `IO_CS`, `PRG_OE`, nibble strobes for `$FExx` |
| GAL-TIM | 341/262 wrap assists, `HBLANK`, `VBLANK`, `NMI`, raster compare → `IRQB` |
| GAL-PPU | VRAM phase `/CE`, CHR `/CE` BG vs 1284, line-buffer `/OE` `/WE`, OAM-write strobe to 1284 |

If equations overflow, add a 4th ATF rather than stuffing sprite eval back into 74HC.

---

## 8. Out of scope for this file

- Cart flash count (on the cartridge PCB).
- Exact RGBS analog levels (`B3`).
- APU NES bitfields (`B2` for `$FE4x` only).
- Retr01-C 3-wire bit-level protocol (`B5`) — only the contract: **8 bits in, one `$FE6x` byte out**.
