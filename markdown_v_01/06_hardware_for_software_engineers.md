# Hardware for Software Engineers - Retr01 Architecture

A software-engineer translation of the discrete logic on **Retr01-A**. Concepts match the locked architecture in this folder (two 32 KB SRAMs, interleaved **VRAM only**, CHR from cartridge, `$7Fxx` I/O page). Older Gemini drafts said "Retro2-A" / NES-copied addresses - those names are retired here.

---

## Part 1: Logic and "the glue"

In software, algorithms run sequentially under a CPU clock. Many logic chips do **not** wait for a clock: change an input voltage and the output follows almost immediately - continuous boolean functions in silicon.

### 1. 74-series logic (the standard library)

The 74-series (e.g. 74HC08, 74HC32) is the physical standard library of bitwise operators: AND, OR, NOT, XOR gates in DIP packages.

Example: **74HC08** (quad 2-input AND). Apply 5 V (1) to both inputs -> output is 1; either input 0 -> output 0.

Software equivalent: `return (A && B);`

1970s/80s arcade boards often wired dozens of these chips into huge physical `if` statements (e.g. "X == 255 **and** Y == 240").

### 2. GAL22V10 (programmable glue)

If 74-series parts are the stdlib, a **GAL** (Generic Array Logic) is a custom compiled function. A GAL22V10 has up to 10 outputs among 22 logic-capable pins, with an internal AND/OR fabric connected by fuses.

1. Write boolean equations (e.g. CUPL):  
   `ROM_ENABLE = ADDRESS_15 & !ADDRESS_14;`
2. Compile to a `.jed` file.
3. A programmer burns fuses so the chip permanently implements that logic.

One 24-pin GAL can replace on the order of **~20** discrete 74-series chips. Retr01 uses GALs to glue CPU <-> memory <-> video timing without a wire jungle.

### 3. Address decoding (6502 + GAL as router)

The W65C02S has 16 address pins (A0-A15). It broadcasts a 16-bit address; it does **not** know whether that address is RAM, cart ROM, or the APU. A GAL watches the high address bits and drives **Chip Select** pins so only the right device wakes up.

Canonical Retr01 ranges (see [08_memory_map.md](08_memory_map.md)):

| CPU sees | GAL wakes |
|----------|-----------|
| `$0000-$7EFF` | System RAM |
| `$7F00-$7FFF` | I/O latches / ports (PPU, VRAM port, APU, ...) |
| `$8000-$FFFF` | Cartridge PRG |

### Recommended learning

- Ben Eater - *Build an 8-bit computer from scratch* (YouTube)
- Ben Eater - *Build a 6502 computer* (YouTube)
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
| System RAM | CPU-only engine state (`$0000-$7EFF`) |
| VRAM | Live nametables / attrs / scratch - **interleaved** with the PPU |

### 2. Bus contention (hardware race)

If the CPU drives a data line to 1 while the PPU drives it to 0, you get **bus contention**: corruption and possible IC damage. Hardware needs a mutex.

### 3. 74HC157 multiplexers (ternary operator)

```text
active_address = clock_is_high ? cpu_address : ppu_address;
```

A 74HC157 selects between two input sets with a Select pin. A wall of muxes in front of **VRAM** address pins toggles whether CPU or video owns the index.

### 4. 74HC245 transceivers (mutex / High-Z)

Address muxing is not enough; data lines need isolation. A 74HC245 is an 8-bit bidirectional buffer. Pins have a third state: **High-Z** (disconnected). When the PPU owns VRAM, the CPU side goes High-Z so it cannot fight the bus.

### 5. Memory interleaving (VRAM only, ~8 MHz class)

Wire the CPU clock phase to mux Select / transceiver Enable on the **VRAM** path (system RAM stays CPU-exclusive):

| Clock phase | VRAM owner |
|-------------|------------|
| Phase A | PPU fetches nametable/attr for the beam |
| Phase B | CPU may R/W VRAM via the `$7F1x` data port |

This "context switch" can happen millions of times per second. To game code it feels like continuous VRAM access; the hardware is trading ownership every tick. **CHR patterns are not in this SRAM** - the PPU reads them from **cartridge CHR-ROM**.

---

## Part 3: Variables, APIs, and the infinite loop

### 1. Latches (hardware variables)

A **74HC573** (octal latch) holds a byte after you stop writing - e.g. scroll X/Y, bank selects. Software equivalent: a variable that survives after the store instruction finishes.

### 2. Memory-mapped I/O (hardware API)

The 6502 has no USB API - only load/store. Retr01 maps devices into **`$7Fxx`**. Example: store scroll via a PPU control register in `$7F0x`; the GAL enables a latch instead of system RAM. Same pattern for APU (`$7F4x-$7F5x`), banks (`$7F3x`), controllers (`$7F6x`).

### 3. Binary counters (hardware `for`)

Video timing is two endless loops: X across the line, Y down the frame. **74HC161** counters (chained) count on a pixel clock. High bits of X/Y feed VRAM addressing so the beam automatically walks the nametable. At end-of-line, X resets and Y increments.

Latches + MMIO + counters = the skeleton of an independent video pipeline.

---

## Part 4: The render pipeline

8-bit hardware refuses uncompressed framebuffers. Retr01 resolves **pointers -> pixels** in silicon.

### 1. Nametable (pointer grid)

VRAM holds up to **four** 32x30 nametable slots (2x2 scroll field). Each byte is a tile index (0-255 into the active **BG page**). Beam counters fetch the index under the current pixel.

### 2. Pattern tables (asset library on cart)

**CHR-ROM** on the cartridge holds 8x8 2bpp patterns. Hardware concatenates tile index + fine Y (row 0-7) into a CHR address. The cart outputs that row's bits - no CPU math. Banks: 4 per world; each bank = BG page (256) + sprite page (256). BG bank and sprite bank may differ and may change mid-frame.

### 3. Attributes and palettes (hardware CSS)

2bpp patterns pick among 3 colors + transparency. **Retr01 selects BG palette per tile** (not NES 2x2 attribute blocks). Attribute bytes live beside nametable data in VRAM; mux logic combines pattern bits + palette -> DAC color index.

### 4. Sprite compositor (hardware z-index)

OAM holds 64 sprites (Y, tile, attr, X - NES-like grouping). During HBlank, hardware scans Y values, fills a line buffer for at most **16** sprites on the next scanline, fetches rows from the **sprite** CHR page on cart.

### 5. Final multiplexer (pixel priority)

For each pixel: if the sprite sample is transparent (typically index 0), output BG; else output sprite. Entire pipeline runs without the CPU; the CPU only updates nametables/OAM/banks through interleaved access and `$7Fxx`.

---

## Part 5: Timing, audio, and the Retr01-A loop

### 1. The "calm" NMI

On a stock NES, VBlank NMI is a panic window to shove graphics before lockout. With interleaved VRAM, the CPU is not locked out of video memory for most of the frame. NMI becomes a **60 Hz metronome** (`frame_ready = true`) for pacing - not the only legal time to touch graphics.

### 2. ATmega APU (audio microservice)

Waveform math would eat the 6502. An **ATmega** runs its own loop and timers, synthesizing **NES-style** channels: 2 pulse + triangle + noise + DMC. The 6502 writes command/status bytes in `$7F40-$7F5F` and continues physics/AABB.

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

        uint8_t p1 = read_controllers();   /* $7F6x */
        calculate_aabb_collisions();
        update_player_state(p1);

        update_oam();                      /* via $7F2x / DMA */
        update_nametable_seams();          /* VRAM port $7F1x, interleaved */

        if (player_jumped)
            apu_write(CMD_PLAY_JUMP);      /* $7F4x */
    }
}
```

Collision stays in software. The electrical timing stays under the GAL, muxes, and ATmega - the engine stays readable C (or later, whatever compiles to 6502).

---

## See also

- [08_memory_map.md](08_memory_map.md) - addresses to memorize  
- [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) - worlds / banks / screens  
- [07_emulator_specification.md](07_emulator_specification.md) - what the C emulator must enforce  
