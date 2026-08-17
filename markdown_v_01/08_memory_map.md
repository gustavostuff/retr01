# Retr01 Memory Map

Canonical CPU address map and VRAM layout. Designed to be easy to remember.

## Mnemonic

```text
$0xxx–$7Exx   RAM     →  “0–7 = work RAM”
$7Fxx         I/O     →  “7F = Features / Fast I/O”
$8xxx–$Fxxx   ROM     →  “8–F = program ROM”
```

Half the 64 KB space is system RAM, one page is peripherals, the upper half is cartridge PRG.

```text
0000                                7F00      8000                    FFFF
├──────────── System RAM ───────────┼─ I/O ───┼────── PRG window ──────┤
         ~31.5 KB                     256 B              32 KB
```

---

## 1. CPU map (6502 view)

| Range | Size | Region | Notes |
|-------|------|--------|-------|
| `$0000–$7EFF` | 32 512 B | **System RAM** | Full 32 KB chip minus I/O page; CPU-only; no interleave |
| `$7F00–$7FFF` | 256 B | **I/O page** | PPU ports, VRAM port, OAM, banks, APU, cabinet, mapper |
| `$8000–$FFFF` | 32 KB | **PRG-ROM window** | Banked into ~512 KB cart PRG; vectors at `$FFFA–$FFFF` |

Reset / NMI / IRQ vectors live in the PRG window at `$FFFA–$FFFF` (standard 6502).

### Why steal `$7Fxx` from the RAM chip?

Keeps the rule “below `$8000` is onboard, `$8000+` is cart” while giving every peripheral a single memorable page. Usable work RAM is still **>31 KB**.

---

## 2. I/O page layout (`$7F00–$7FFF`)

Grouped in **16-byte blocks** so the high nibble of the low byte is the device family:

| Range | Block | Device |
|-------|-------|--------|
| `$7F00–$7F0F` | `0` | **PPU control** — mode, status, scroll X/Y, nametable base, NMI enables |
| `$7F10–$7F1F` | `1` | **VRAM port** — address latch (16-bit via two writes) + data R/W into 32 KB VRAM (**interleaved**) |
| `$7F20–$7F2F` | `2` | **OAM** — address, data, DMA trigger from system RAM |
| `$7F30–$7F3F` | `3` | **Bank / world** — BG pattern bank, sprite pattern bank, world select, CHR mapper assists |
| `$7F40–$7F5F` | `4–5` | **APU** — NES-style channels (see below) |
| `$7F60–$7F6F` | `6` | **Cabinet / controllers** — sticks, buttons, coin, start (Retr01-A IDC) |
| `$7F70–$7F7F` | `7` | **Board EEPROM / DIP** — high scores, operator settings |
| `$7F80–$7F8F` | `8` | **PRG mapper** — which 32 KB (or 16+16) slice appears at `$8000–$FFFF` |
| `$7F90–$7FFF` | `9–F` | **Reserved** — keep `$7FF0–$7FFF` free for debug/ID if needed |

### APU block (`$7F40–$7F5F`) — NES-style

Mirror the familiar NES channel set on the ATmega coprocessor (register packing can match NES `$4000–$4017` layout inside this window for author muscle memory):

| Channels | Role |
|----------|------|
| Pulse 1, Pulse 2 | Square / duty |
| Triangle | Triangle |
| Noise | Noise |
| DMC | Samples / bits |

Exact byte-for-byte NES identity is optional; **channel set and behavior** are NES-style.

### Bank registers (`$7F30`)

- Separate **BG bank** and **sprite bank** (0–3 within current world, plus world select).
- Writable **mid-frame** / mid-scanline; emulator and GAL must allow it (glitches if mis-timed are the author’s problem).

---

## 3. VRAM chip map (32 KB, not in CPU space)

CPU touches VRAM only through **`$7F10–$7F1F`** (address + data). PPU fetches nametable/attr on its interleaved phases; CHR bits come from **cartridge CHR-ROM**, not this chip.

| VRAM offset | Size | Contents |
|-------------|------|----------|
| `$0000–$07FF` | 2 KiB | Nametable slot 0: 960 tiles + 960 attrs (+ pad to 2 KiB) |
| `$0800–$0FFF` | 2 KiB | Nametable slot 1 |
| `$1000–$17FF` | 2 KiB | Nametable slot 2 |
| `$1800–$1FFF` | 2 KiB | Nametable slot 3 |
| `$2000–$23FF` | 1 KiB | OAM working copy / eval buffers *(or keep OAM in regs only)* |
| `$2400–$2FFF` | ~3 KiB | Streaming scratch (5-tile perimeter strips) |
| `$3000–$7FFF` | 20 KiB | Reserved / future (status, widescreen experiments, etc.) |

Slots 0–3 form the **2×2** scroll field for the “four screens visible” case.

---

## 4. Cartridge (outside CPU map)

| Region | Budget | Access |
|--------|--------|--------|
| PRG | ≤512 KiB | Window `$8000–$FFFF` via `$7F80` mapper |
| CHR | ≤256 KiB | PPU-only fetches; banked by `$7F30` world/bank |
| MAP | ≤~1.17 MiB | CPU reads via mapper window or dedicated MAP bank bit *(implementation detail)* into VRAM through `$7F11` data port |

---

## 5. Interleave rule (emulator + GAL)

| Memory | Ownership |
|--------|-----------|
| System RAM `$0000–$7EFF` | CPU always |
| VRAM chip | **PPU phase** vs **CPU phase** (via `$7F1x` data port) |
| CHR-ROM | PPU fetch path (cart); CPU does not need CHR in address space for rendering |

Wrong-phase CPU VRAM access: **hard error in emulator debug builds** (recommended).
