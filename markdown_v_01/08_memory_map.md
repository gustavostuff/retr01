# Retr01 Memory Map & Address Decoding

Canonical CPU address map, VRAM layout, and how the GAL / emulator bus routes them. Designed to be easy to remember.

## Mnemonic

```text
$0xxx-$7Exx   RAM     ->  "0-7 = work RAM"
$7Fxx         I/O     ->  "7F = Features / Fast I/O"
$8xxx-$Fxxx   ROM     ->  "8-F = program ROM"
```

```text
0000                                7F00      8000                    FFFF
+------------ System RAM -----------+- I/O ---+------ PRG window ------|
         ~31.5 KB                     256 B              32 KB
```

---

## 1. CPU map (6502 view)

| Range | Size | Region | Notes |
|-------|------|--------|-------|
| `$0000-$7EFF` | 32 512 B | **System RAM** | Full 32 KB chip minus I/O page; CPU-only; no interleave |
| `$7F00-$7FFF` | 256 B | **I/O page** | PPU ports, VRAM port, OAM, banks, APU, cabinet, mapper |
| `$8000-$FFFF` | 32 KB | **PRG-ROM window** | Banked into ~512 KB cart PRG; vectors at `$FFFA-$FFFF` |

Reset / NMI / IRQ vectors live at `$FFFA-$FFFF` (standard 6502).

Stealing `$7Fxx` from the RAM chip keeps "below `$8000` = onboard, `$8000+` = cart" while giving every peripheral one memorable page. Usable work RAM is still **>31 KB**.

---

## 2. I/O page layout (`$7F00-$7FFF`)

Grouped in **16-byte blocks** (high nibble of the low byte = device family):

| Range | Block | Device |
|-------|-------|--------|
| `$7F00-$7F0F` | `0` | **PPU control** - mode, status, scroll X/Y, nametable base, NMI enables |
| `$7F10-$7F1F` | `1` | **VRAM port** - address latch + data R/W into 32 KB VRAM (**interleaved**) |
| `$7F20-$7F2F` | `2` | **OAM** - address, data, DMA trigger from system RAM |
| `$7F30-$7F3F` | `3` | **Bank / world** - BG bank, sprite bank, world select |
| `$7F40-$7F5F` | `4-5` | **APU** - NES-style channels |
| `$7F60-$7F6F` | `6` | **Cabinet / controllers** |
| `$7F70-$7F7F` | `7` | **Board EEPROM / DIP** |
| `$7F80-$7F8F` | `8` | **PRG mapper** |
| `$7F90-$7FFF` | `9-F` | **Reserved** |

### APU (`$7F40-$7F5F`) - NES-style

| Channels | Role |
|----------|------|
| Pulse 1, Pulse 2 | Square / duty |
| Triangle | Triangle |
| Noise | Noise |
| DMC | Samples / bits |

Register packing may mirror NES `$4000-$4017` for familiarity; behavior is NES-style either way.

### Banks (`$7F30`)

Separate **BG bank** and **sprite bank** (0-3 within world) plus world select. Writable **mid-frame**; mistimed changes are the author's problem.

---

## 3. VRAM chip map (32 KB, not in CPU space)

CPU touches VRAM only through **`$7F10-$7F1F`**. PPU fetches nametable/attr on its phases; CHR comes from **cartridge CHR-ROM**.

| VRAM offset | Size | Contents |
|-------------|------|----------|
| `$0000-$07FF` | 2 KiB | Nametable slot 0 (960 tiles + 960 attrs + pad) |
| `$0800-$0FFF` | 2 KiB | Nametable slot 1 |
| `$1000-$17FF` | 2 KiB | Nametable slot 2 |
| `$1800-$1FFF` | 2 KiB | Nametable slot 3 |
| `$2000-$23FF` | 1 KiB | OAM working / eval buffers *(optional)* |
| `$2400-$2FFF` | ~3 KiB | Streaming scratch (5-tile perimeter) |
| `$3000-$7FFF` | 20 KiB | Reserved |

Slots 0-3 = **2x2** scroll field (up to four screens visible).

---

## 4. Cartridge (outside CPU map)

| Region | Budget | Access |
|--------|--------|--------|
| PRG | <=512 KiB | `$8000-$FFFF` via `$7F80` |
| CHR | <=256 KiB | PPU fetches; banked by `$7F30` |
| MAP | <=~1.17 MiB | Stream into VRAM via data port *(path TBD)* |

---

## 5. Address decoding (GAL / virtual bus)

Physical **GAL22V10** (or equivalent) watches address + control lines and asserts chip selects. The C emulator mirrors this with one bus router - CPU code never touches `system_ram[]` / `vram[]` directly.

| Select | When |
|--------|------|
| System RAM CS | `$0000-$7EFF` |
| I/O / latch enables | `$7F00-$7FFF` (sub-decode by 16-byte block) |
| PRG OE | `$8000-$FFFF` |
| VRAM CS + mux | VRAM data-port cycles, qualified by **clock phase** |
| CHR OE | PPU fetch cycles with mapper bank (not a CPU CS) |

```c
uint8_t system_bus_read(uint16_t address) {
    if (address >= 0x8000)
        return mapper_prg_read(address);
    if (address >= 0x7F00)
        return io_page_read(address & 0xFF);
    return system_ram[address];
}

void system_bus_write(uint16_t address, uint8_t data) {
    if (address >= 0x8000) {
        mapper_maybe_write(address, data);
        return;
    }
    if (address >= 0x7F00) {
        io_page_write(address & 0xFF, data);
        return;
    }
    system_ram[address] = data;
}
```

`io_page_write` handles PPU regs, **phase-checked** VRAM data port, OAM, banks, APU, cabinet I/O, EEPROM, and PRG mapper.

### Interleave

| Memory | Ownership |
|--------|-----------|
| System RAM `$0000-$7EFF` | CPU always |
| VRAM chip | PPU phase vs CPU phase (via `$7F1x`) |
| CHR-ROM | PPU fetch path only |

Wrong-phase CPU VRAM access: **hard error in emulator debug builds** (recommended).

### Mapper responsibilities (`$7F30` / `$7F80`)

- PRG slice into `$8000-$FFFF`
- World + BG CHR bank + sprite CHR bank for PPU CHR-ROM fetches
- Which nametable slot is being filled when streaming MAP -> VRAM

Emulator implementation detail for the same router lives in [07_emulator_specification.md](07_emulator_specification.md).
