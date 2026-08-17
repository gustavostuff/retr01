# Address Decoding — Virtual GAL22V10

## 1. Hardware vs emulator

Physical **GAL22V10** (or equivalent) watches the 16-bit address + control lines and asserts chip selects. The low-level C emulator mirrors that with a single bus router: CPU cores never touch backing arrays directly.

Canonical ranges: [08_memory_map.md](08_memory_map.md).

## 2. Decode summary

| Select | When |
|--------|------|
| System RAM CS | `$0000–$7EFF` |
| I/O / latch enables | `$7F00–$7FFF` (sub-decode by low nibble block) |
| PRG OE | `$8000–$FFFF` |
| VRAM CS + mux | Only for transactions through the VRAM data port, qualified by **clock phase** |
| CHR OE | PPU fetch cycles with mapper bank (not a CPU CS) |

## 3. Pseudocode shape

```c
uint8_t system_bus_read(uint16_t address) {
    if (address >= 0x8000) {
        return mapper_prg_read(address);
    }
    if (address >= 0x7F00) {
        return io_page_read(address & 0xFF);
    }
    /* $0000–$7EFF */
    return system_ram[address];
}

void system_bus_write(uint16_t address, uint8_t data) {
    if (address >= 0x8000) {
        /* normally ignored; mapper may snoop if using cart write-as-command style */
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

`io_page_write` handles PPU regs, **phase-checked** VRAM data port, OAM, bank/world, APU, cabinet I/O, EEPROM, PRG mapper.

## 4. Interleave enforcement

VRAM data-port R/W must tick with the CPU clock phase. On PPU-owned phase, debug builds **assert**; release builds may return open-bus noise if you ever need soft failure.

System RAM ignores interleave.

## 5. Mapper responsibilities

Via `$7F30` / `$7F80` (and optional MAP bank bits):

- PRG slice into `$8000–$FFFF`
- World + BG CHR bank + sprite CHR bank for PPU CHR-ROM fetches
- Which nametable slot is being filled when streaming MAP → VRAM
