# Hardware for Software Engineers: Retr01 Architecture

A software-engineer translation of the discrete logic on **Retr01-A**. Concepts match the locked architecture in this folder (two 32 KB SRAMs, interleaved **VRAM only**, CHR from cartridge, `$FExx` I/O page). Older Gemini drafts said "Retro2-A" / NES-copied addresses. Those names are retired here.

---

## Part 1: Logic and "the glue"

In software, algorithms run sequentially under a CPU clock. Many logic chips do **not** wait for a clock: change an input voltage and the output follows almost immediately. They are continuous boolean functions in silicon.

### 1. 74-series logic (the standard library)

The 74-series (e.g. 74HC08, 74HC32) is the physical standard library of bitwise operators: AND, OR, NOT, XOR gates in DIP packages.

Example: **74HC08** (quad 2-input AND). Apply 5 V (1) to both inputs -> output is 1. Either input 0 -> output 0.

Software equivalent: `return (A && B);`

1970s/80s arcade boards often wired dozens of these chips into huge physical `if` statements (e.g. "X == 255 **and** Y == 240").

### 2. GAL22V10 (programmable glue)

If 74-series parts are the stdlib, a **GAL** (Generic Array Logic) is a custom compiled function. A GAL22V10 has up to 10 outputs among 22 logic-capable pins, with an internal AND/OR fabric connected by fuses.

1. Write boolean equations (e.g. CUPL) that decode ranges such as
   "A15 low -> RAM CS", "A15..A8 == $FE -> I/O CS", "else if A15 high -> PRG OE".
   Exact fuse equations belong with the schematic. The GAL is just a compiled router.
2. Compile to a `.jed` file.
3. A programmer burns fuses so the chip permanently implements that logic.

One 24-pin GAL can replace on the order of **~20** discrete 74-series chips. Retr01 uses GALs to glue CPU <-> memory <-> video timing without a wire jungle.

### 3. Address decoding (6502 + GAL as router)

The W65C02S has 16 address pins (A0-A15). It broadcasts a 16-bit address. It does **not** know whether that address is RAM, cart ROM, or the APU. A GAL watches the high address bits and drives **Chip Select** pins so only the right device wakes up.

Canonical Retr01 ranges (see [08_memory_map.md](08_memory_map.md)):

| CPU sees | GAL wakes |
|----------|-----------|
| `$0000-$7FFF` | System RAM (full 32 KB) |
| `$FE00-$FEFF` | I/O latches / ports (PPU, VRAM port, APU, ...) |
| `$8000-$FDFF`, `$FF00-$FFFF` | Cartridge PRG (I/O hole at `$FExx`) |

### Recommended learning

- Ben Eater, *Build an 8-bit computer from scratch* (YouTube)
- Ben Eater, *Build a 6502 computer* (YouTube)
- *Nand2Tetris* (course/book)

---

## Part 2: Memory, multiplexers, and race conditions

Software memory is an abstraction (`uint8_t vram[32768]; vram[500] = 0xFF;`). Hardware memory is shared copper. Two drivers fighting on one wire is a physical short.

### 1. SRAM (the physical array)

Retr01 uses **AS6C62256-class 32 KB** SRAMs:

| Pin group | Software idea |
|-----------|----------------|
| A0-A14 | Array index (0...32767) |
| D0-D7 | Byte value |
| WE | The `=` (write) vs read |

There are **two** chips:

| Chip | Role |
|------|------|
| System RAM | CPU-only engine state (`$0000-$7FFF`) |
| VRAM | Live nametables / attrs / scratch, **interleaved** with the PPU |

### 2. Bus contention (hardware race)

If the CPU drives a data line to 1 while the PPU drives it to 0, you get **bus contention**: corruption and possible IC damage. Hardware needs a mutex.

### 3. 74HC157 multiplexers (ternary operator)

```text
active_address = clock_is_high ? cpu_address : ppu_address;
```

A 74HC157 selects between two input sets with a Select pin. A wall of muxes in front of **VRAM** address pins toggles whether CPU or video owns the index.

### 4. 74HC245 transceivers (mutex / High-Z)

Address muxing is not enough. Data lines need isolation. A 74HC245 is an 8-bit bidirectional buffer. Pins have a third state: **High-Z** (disconnected). When the PPU owns VRAM, the CPU side goes High-Z so it cannot fight the bus.

### 5. Memory interleaving (VRAM only, ~8 MHz class)

Wire the CPU clock phase to mux Select / transceiver Enable on the **VRAM** path (system RAM stays CPU-exclusive):

| Clock phase | VRAM owner |
|-------------|------------|
| Phase A | PPU fetches nametable/attr for the beam |
| Phase B | CPU may R/W VRAM via the `$FE1x` data port |

This "context switch" happens at the CPU clock (planning **8 MHz**, vs ~1.79 MHz on a stock NES). To game code there is **no VBlank lockout**. You may touch VRAM any time the CPU owns its phase. The CPU does **not** own every bus cycle. Ownership still alternates. **CHR patterns are not in this SRAM.** The PPU reads them from **cartridge CHR-ROM**.

---

## Part 3: Variables, APIs, and the infinite loop

### 1. Latches (hardware variables)

A **74HC573** (octal latch) holds a byte after you stop writing, for example scroll X/Y and bank selects. Software equivalent: a variable that survives after the store instruction finishes.

### 2. Memory-mapped I/O (hardware API)

The 6502 has no USB API, only load/store. Retr01 maps devices into **`$FExx`**. Example: store scroll via a PPU control register in `$FE0x`. The GAL enables a latch instead of system RAM. Same pattern for APU (`$FE4x-$FE5x`), banks (`$FE3x`), controllers (`$FE6x`).

### 3. Binary counters (hardware `for`)

Video timing is two endless loops: X across the line, Y down the frame. **74HC161** counters (chained) count on a pixel clock. High bits of X/Y feed VRAM addressing so the beam automatically walks the nametable. At end-of-line, X resets and Y increments.

Latches + MMIO + counters = the skeleton of an independent video pipeline.

---

## Part 4: The render pipeline

8-bit hardware refuses uncompressed framebuffers. Retr01 resolves **pointers -> pixels** in silicon.

### 1. Nametable (pointer grid)

VRAM holds up to **four** 32x30 nametable slots (2x2 scroll field). Each nametable byte is a tile index 0-255 into the **active BG pattern set** on cart (not a bank number, not a VRAM page). Beam counters fetch that index under the current pixel. `scroll_x`/`scroll_y` pick where the 256x240 window sits on those slots.

### 2. Pattern tables (asset library on cart)

**CHR-ROM** on the cartridge holds 8x8 2bpp patterns. Hardware concatenates tile index + fine Y (row 0-7) into a CHR address. The cart outputs that row's bits, with no CPU math. Banks: 4 per world, each bank = 256 BG patterns + 256 sprite patterns. `$FE30` selects BG bank and sprite bank independently. Mid-frame changes are OK. Scroll does not touch these registers.

### 3. Attributes and palettes (hardware CSS)

2bpp patterns pick among 3 colors + transparency. **8 palettes** (4 BG + 4 sprite). **Per-tile BG attributes:** 240 bytes at `+0x3C0` in each VRAM slot. One byte is a 2x2 cell with four 2-bit palette selects (one per tile). NES instead uses one select for a whole 2x2. Shared universal **BG color 0 / backdrop**. Sprite OAM attr byte is NES-like. Mux logic combines pattern bits + palette -> DAC color index.

### 4. Sprite compositor (hardware z-index)

OAM holds 64 sprites (Y, tile, attr, X, NES-like grouping). During HBlank, hardware scans Y values, fills a line buffer for at most **16** sprites on the next scanline, fetches rows from the **sprite** CHR page on cart.

### 5. Final multiplexer (pixel priority)

For each pixel: if the sprite sample is transparent (typically index 0), output BG. Otherwise output sprite. Entire pipeline runs without the CPU. The CPU only updates nametables/OAM/banks through interleaved access and `$FExx`.

---

## Part 5: Timing, audio, and the Retr01-A loop

### 1. The "calm" NMI

On a stock NES, VBlank NMI is a panic window to shove graphics before lockout. With interleaved VRAM, the CPU is never forced to wait for VBlank to use the VRAM port (it still shares the chip on alternating phases). NMI becomes a **60 Hz metronome** (`frame_ready = true`) for pacing.

### 2. ATmega APU (audio microservice)

Waveform math would eat the 6502. An **ATmega** runs its own loop and timers, synthesizing **NES-style** channels: 2 pulse + triangle + noise + DMC. The 6502 writes command/status bytes in `$FE40-$FE5F` and continues physics/AABB.

### 3. Main loop shape

```c
volatile bool frame_ready = false;

void nmi_handler(void) {
    frame_ready = true;
}

void main(void) {
    init_system();
    for (;;) {
        if (!frame_ready)
            continue;
        frame_ready = false;

        uint8_t p1 = read_controllers();   /* $FE6x */
        calculate_aabb_collisions();
        update_player_state(p1);

        update_oam();                      /* via $FE2x / DMA */
        update_nametable_seams();          /* VRAM port $FE1x, interleaved */

        if (player_jumped)
            apu_write(CMD_PLAY_JUMP);      /* $FE4x */
    }
}
```

Collision stays in software. The electrical timing stays under the GAL, muxes, and ATmega. The engine stays readable C (or later, whatever compiles to 6502).

---

## See also

- [08_memory_map.md](08_memory_map.md): addresses to memorize
- [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md): worlds / banks / screens
- [07_emulator_specification.md](07_emulator_specification.md): what the C emulator must enforce
