# Retr01 Memory Map & Address Decoding

Canonical CPU address map, VRAM layout, and how the GAL / emulator bus routes them. Designed to be easy to remember.

## Mnemonic

```text
$0xxx-$7xxx   RAM     ->  "0-7 = work RAM" (full 32 KB)
$8xxx-$FDxx   ROM     ->  "8-FD = program ROM"
$FExx         I/O     ->  "FE = Features"
$FFxx         ROM     ->  "FF = top of PRG + vectors"
```

```text
0000              7FFF 8000             FDFF FE00  FEFF FF00        FFFF
+---- System RAM ----+-+------ PRG ------+-+- I/O -+-+---- PRG -----+
       32 KB            ~32 KB (gap@$FE)    256 B     256 B+vectors
```

Full **32 KB** system SRAM is mapped with no wasted bytes. I/O sits in a 256-byte hole near the top of the cart window so vectors at `$FFFA-$FFFF` stay in PRG.

---

## 1. CPU map (6502 view)

| Range | Size | Region | Notes |
|-------|------|--------|-------|
| `$0000-$7FFF` | 32 KB | **System RAM** | Entire AS6C62256; CPU-only; no interleave |
| `$8000-$FDFF` | 32 256 B | **PRG-ROM window** | Banked cart PRG |
| `$FE00-$FEFF` | 256 B | **I/O page** | PPU, VRAM port, OAM, banks, APU, MAP port, cabinet, mapper |
| `$FF00-$FFFF` | 256 B | **PRG (high)** | Same mapper window family; holds `$FFFA-$FFFF` vectors |

---

## 2. I/O page layout (`$FE00-$FEFF`)

Grouped in **16-byte blocks** (high nibble of the low byte = device family):

| Range | Block | Device |
|-------|-------|--------|
| `$FE00-$FE0F` | `0` | **PPU control** - mode, status, scroll X/Y (one byte each), nametable arrangement, NMI |
| `$FE10-$FE1F` | `1` | **VRAM port** - address latch + data R/W into 32 KB VRAM (**interleaved**) |
| `$FE20-$FE2F` | `2` | **OAM** - address, data, DMA from system RAM (dedicated; not stored in VRAM chip) |
| `$FE30-$FE3F` | `3` | **Bank / world** - BG bank, sprite bank, world select |
| `$FE40-$FE5F` | `4-5` | **APU** - NES-style channels |
| `$FE60-$FE6F` | `6` | **Cabinet / controllers** |
| `$FE70-$FE7F` | `7` | **Board EEPROM / DIP** |
| `$FE80-$FE8F` | `8` | **PRG mapper** - only official PRG bank control |
| `$FE90-$FE9F` | `9` | **MAP port** - address latch + data read from cart MAP-ROM |
| `$FEA0-$FEFF` | `A-F` | **Reserved** |

### Scroll (`$FE0x`)

- **`scroll_x`**, **`scroll_y`**: one byte each, values **0-255**, wrap naturally.
- They fine-scroll the 256x240 viewport across the live nametable field (1, 2, or 4 screens arranged/mirrored in the four VRAM slots).
- You do **not** scroll across an entire world with a larger coordinate; the CPU streams new strips into nametable slots as the camera approaches seams (5-tile margin).

### APU (`$FE40-$FE5F`) - NES-style

| Channels | Role |
|----------|------|
| Pulse 1, Pulse 2 | Square / duty |
| Triangle | Triangle |
| Noise | Noise |
| DMC | Samples / bits |

### Banks (`$FE30`)

Separate **BG bank** and **sprite bank** (0-3 within world) plus world select. Writable mid-frame.

### MAP port (`$FE90`) - cart map reads

Canonical way for the CPU to read compressed MAP-ROM while decompressing into VRAM:

1. Write 24-bit (or lo/hi + bank) MAP address into `$FE90`...
2. Read `$FE92` (data); hardware auto-increments the address.

No MAP window carved out of system RAM or PRG. Decompress into nametable slots via the VRAM data port (`$FE1x`).

### PRG mapper (`$FE80`) - canonical only

**Only** `$FE80` block selects which PRG slice appears at `$8000-$FDFF` / `$FF00-$FFFF`.  
Writes into `$8000-$FFFF` do **not** change banks (ignored / open bus). Keeps GAL decode simple.

---

## 3. VRAM chip map (32 KB, not in CPU space)

CPU touches VRAM only through **`$FE10-$FE1F`**. CHR comes from cartridge CHR-ROM. **OAM is not in this chip** (see `$FE2x`).

| VRAM offset | Size | Contents |
|-------------|------|----------|
| `$0000-$07FF` | 2 KB | Nametable slot 0: 960 tiles + 960 per-tile attrs (+ pad) |
| `$0800-$0FFF` | 2 KB | Nametable slot 1 |
| `$1000-$17FF` | 2 KB | Nametable slot 2 |
| `$1800-$1FFF` | 2 KB | Nametable slot 3 |
| `$2000-$2FFF` | 4 KB | Streaming scratch (5-tile perimeter, decompress temps) |
| `$3000-$7FFF` | 20 KB | Reserved |

Each slot (2 KB): tiles at `+0x000` (960 bytes), per-tile attributes at `+0x3C0` (960 bytes). Offset `+0x3C0` is where NES puts attrs, but Retr01 keeps **960** attribute bytes (one palette select per tile), not 64. Slots 0-3 form the live 1/2/4-screen field.

---

## 4. Cartridge (outside CPU map)

| Region | Budget | Access |
|--------|--------|--------|
| PRG | <=512 KB | `$8000-$FDFF` + `$FF00-$FFFF` via `$FE80` |
| CHR | <=256 KB | PPU fetches; banked by `$FE30` |
| MAP | <=~1.17 MB | CPU reads via **`$FE90` MAP port** only |

---

## 5. Address decoding (GAL / virtual bus)

| Select | When |
|--------|------|
| System RAM CS | `$0000-$7FFF` |
| I/O / latch enables | `$FE00-$FEFF` |
| PRG OE | `$8000-$FDFF` and `$FF00-$FFFF` |
| VRAM CS + mux | VRAM data-port cycles, qualified by **clock phase** |
| CHR OE | PPU fetch cycles with mapper bank |
| MAP OE | MAP data-port reads |

```c
uint8_t system_bus_read(uint16_t address) {
    if (address < 0x8000)
        return system_ram[address];
    if (address >= 0xFE00 && address <= 0xFEFF)
        return io_page_read(address & 0xFF);
    return mapper_prg_read(address); /* $8000-$FDFF and $FF00-$FFFF */
}

void system_bus_write(uint16_t address, uint8_t data) {
    if (address < 0x8000) {
        system_ram[address] = data;
        return;
    }
    if (address >= 0xFE00 && address <= 0xFEFF) {
        io_page_write(address & 0xFF, data);
        return;
    }
    /* PRG window: ignore writes (mapper is $FE80 only) */
}
```

### Interleave

| Memory | Ownership |
|--------|-----------|
| System RAM `$0000-$7FFF` | CPU always |
| VRAM chip | PPU phase vs CPU phase (via `$FE1x`) |
| CHR-ROM | PPU fetch path only |

Wrong-phase CPU VRAM access: **hard error in emulator debug builds**.

### Clocks and frame timing (locked)

| Clock / measure | Value |
|-----------------|--------|
| CPU (W65C02S) | **8.000 MHz** |
| Dot (pixel) clock | **5.369318 MHz** (NTSC PPU-rate) |
| Dots / scanline | **341** (256 active + 85 HBlank) |
| Scanlines / frame | **262** (240 active + 22 VBlank) |
| Frame / NMI rate | **~60.098 Hz** |

Interleave muxes toggle on the **CPU** clock phases. The beam and fetch sequencer advance on the **dot** clock. Line buffers / shift registers sit between those domains (same idea as a real PPU). Full RGBS sync polarity can still be tuned on the bench; the numbers above are the locked digital timing.

Graphics overview: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md).  
Emulator: [07_emulator_specification.md](07_emulator_specification.md).
