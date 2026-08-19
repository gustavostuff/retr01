# Hardware for Software Engineers: Retr01 Architecture

A software-engineer translation of the discrete logic on **Retr01-A**. Concepts match the locked architecture in this folder (three 32 KB SRAMs, interleaved **VRAM only**, sprite OAM in a **1284**, CHR from cartridge, `$FExx` I/O page). Older Gemini drafts said "Retro2-A" / NES-copied addresses. Those names are retired here.

---

## Part 1: Logic and "the glue"

In software, algorithms run sequentially under a CPU clock. Many logic chips do **not** wait for a clock: change an input voltage and the output follows almost immediately. They are continuous boolean functions in silicon.

### 1. 74-series logic (the standard library)

The 74-series (e.g. 74HC08, 74HC32) is the physical standard library of bitwise operators: AND, OR, NOT, XOR gates in DIP packages.

Example: **74HC08** (quad 2-input AND). Apply 5 V (1) to both inputs -> output is 1. Either input 0 -> output 0.

Software equivalent: `return (A && B);`

1970s/80s arcade boards often wired dozens of these chips into huge physical `if` statements (e.g. "X == 255 **and** Y == 240").

### 2. ATF22V10 (programmable glue)

If 74-series parts are the stdlib, a **22V10 PLD** is a custom compiled function. Lattice **GAL** chips (**GAL16V8**, **GAL20V8**, **GAL22V10**) are discontinued. Retr01 uses Microchip **ATF22V10CQZ-20PU** (same 24-pin 22V10 idea, flash instead of old fuses, still in production as of August 2026). Sheet names like GAL-DEC are roles, not Lattice PNs.

1. Write boolean equations (e.g. CUPL) that decode ranges such as
   "A15 low -> RAM CS", "A15..A8 == $FE -> I/O CS", "else if A15 high -> PRG OE".
   Exact equations belong with the schematic. The ATF is just a compiled router.
2. Compile to a `.jed` file.
3. A programmer burns the chip so it implements that logic.

One 24-pin 22V10 can replace on the order of **~20** discrete gate chips (AND/OR decode). It does **not** replace wide buses: VRAM address mux stays **74HC157**, data isolation stays **74HC245**, beam counters stay **74HC161**, byte latches stay **74HC573**. Retr01-A uses ATFs to cut random 74HC00/08/32-type parts **without** changing timing, interleave, or the CPU map.

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

There are **three** chips (same PN):

| Chip | Role |
|------|------|
| System RAM | CPU-only engine state (`$0000-$7FFF`) |
| VRAM | Live nametables / attrs / scratch, **interleaved** with the PPU |
| Line buffer | Sprite ping-pong (512 bytes used). Beam reads with X. OAM is **not** here — it is RAM inside the 1284 |

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

The 6502 has no USB API, only load/store. Retr01 maps devices into **`$FExx`**. Example: store scroll via a PPU control register in `$FE0x`. The GAL enables a latch instead of system RAM. Same pattern for APU (`$FE4x-$FE5x`), banks (`$FE3x`), pads (`$FE60`/`$FE61`), OAM port (`$FE20`/`$FE21`).

### 3. Binary counters (hardware `for`)

Video timing is two endless loops: X across the line, Y down the frame. **74HC161** counters (chained) count on a pixel clock. High bits of X/Y feed VRAM addressing so the beam automatically walks the nametable. At end-of-line, X resets and Y increments.

Latches + MMIO + counters = the skeleton of an independent video pipeline.

---

## Part 4: The render pipeline

8-bit hardware refuses uncompressed framebuffers. Retr01 resolves **pointers -> pixels** in silicon.

### 1. Nametable (pointer grid)

VRAM holds up to **four** 32x30 nametable slots (2x2 scroll field). Each nametable byte is a tile index 0-255 into the **active BG tile set** on cart (not a bank number, not a VRAM page). Beam counters fetch that index under the current pixel. `scroll_x`/`scroll_y` pick where the 256x240 window sits on those slots.

### 2. CHR banks and tile sets (asset library on cart)

**CHR-ROM** on the cartridge holds 8x8 2bpp patterns. Hardware concatenates tile index + fine Y (row 0-7) into a CHR address. The cart outputs that row's bits, with no CPU math. Banks: 4 per world, each bank = 256 BG patterns + 256 sprite patterns. `$FE30` selects BG bank and sprite bank independently. Mid-frame changes are OK. Scroll does not touch these registers.

Retr01 does **not** expose NES-style **pattern tables/pages** as a separate concept between CHR bytes and screens. A screen's nametable simply names tiles **0-255** in the **currently selected BG bank's BG half**. That is enough indirection already: `world -> BG bank -> tile index -> pixels`. Extra "pattern table/page" terminology would only duplicate what the bank latch already does.

### 3. Attributes and palettes (hardware CSS)

2bpp patterns pick among 4 indices (00, 01, 10, 11). **8 palettes** (4 BG + 4 sprite). **Per-tile BG attributes:** 240 bytes at `+0x3C0` in each VRAM slot. One byte is a 2x2 cell with four 2-bit palette selects (one per tile). NES instead uses one select for a whole 2x2.

**Color 0 is NES-like, two jobs:**
- On **BG**, index 0 is the shared **backdrop** color (opaque playfield). All BG palettes share that one color 0.
- On **sprites**, index 0 is **transparent**. The mux does not draw that sprite pixel.

Sprite OAM attr byte is NES-like (which of the 4 sprite palettes, flips, priority). Mux logic combines pattern bits + palette -> DAC color index.

### 4. Sprite compositor (hardware z-index)

OAM holds 64 sprites (Y, tile, attr, X, NES-like grouping) **inside the ATmega1284P**. The 6502 uploads with a store loop to `$FE21` (auto-inc). There is **no** hardware DMA. During the current scanline the 1284 evaluates the **next** line (Y match, cap **16**), fetches sprite CHR from cart, and writes the other ping-pong bank of the line-buffer SRAM. The beam indexes that buffer with X. Same one-line delay as the old discrete HBlank eval — not an extra frame.

### 5. Final multiplexer (pixel priority)

For each pixel the PPU already has a BG sample and (maybe) a sprite sample:

1. If the sprite pixel is pattern color 0, it is transparent. Output the BG pixel.
2. Else if the OAM **priority** bit is set and the BG pixel is not backdrop (BG color 0), output the BG pixel (sprite behind).
3. Otherwise output the sprite pixel.

Pattern color 0 is not OAM sprite #0. OAM entry 0 is a normal sprite. There is **no** NES sprite-0 hit flag. Raster splits use `raster_y` + IRQ ([02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) section 8).

The 6502 does **not** plot pixels. Counters, nametable fetch, CHR from cart, attr unpack, the 1284 + line buffer, compositor mux, and RGBS all run every dot. The CPU prepares nametables through the interleaved VRAM port, OAM via `$FE20`/`$FE21`, and `$FExx` latches. It may also write those latches **mid-frame** (bank, scroll) from a raster IRQ. NMI means "a frame finished." IRQ (optional) means "the beam hit `raster_y`."

---

## Part 5: Timing, audio, and the Retr01-A loop

### 1. The "calm" NMI

On a stock NES, VBlank NMI is a panic window to shove graphics before lockout. With interleaved VRAM, the CPU is never forced to wait for VBlank to use the VRAM port (it still shares the chip on alternating phases). NMI becomes a **60 Hz metronome** (`frame_ready = true`) for pacing.

Mid-frame bank and scroll tricks do **not** use NMI and do **not** use sprite-0. They use a **raster IRQ** when the beam reaches `raster_y`. The NMI handler stays dumb. The IRQ handler writes `$FE30` / scroll and re-arms the next line.

### 2. ATmega APU (audio microservice)

Waveform math would eat the 6502. A separate **ATmega328P** runs its own loop and timers, synthesizing **NES-style** channels: 2 pulse + triangle + noise + DMC. Do not merge this with the **1284** (sprites + pads). The 6502 writes command/status bytes in `$FE40-$FE5F` (the sound contract, bitfields TBD) and continues physics/AABB.

### 3. Main loop shape

```c
volatile bool frame_ready = false;

void nmi_handler(void) {
    frame_ready = true;
}

void irq_handler(void) {
    /* set_parallax arms this: restore playfield camera, ack raster_hit */
}

void main(void) {
    init_system();
    for (;;) {
        if (!frame_ready)
            continue;
        frame_ready = false;

        uint8_t p1 = read_controllers();   /* $FE60 */
        calculate_aabb_collisions();
        update_player_state(p1);

        update_oam();                      /* store loop to $FE21, no DMA */
        update_nametable_seams();          /* VRAM port $FE1x, interleaved */

        if (player_jumped)
            apu_write(CMD_PLAY_JUMP);      /* $FE4x */
    }
}
```

Collision stays in software. The electrical timing stays under the GAL, muxes, 1284 (sprites + pads), and 328P (APU). The engine stays readable C (or later, whatever compiles to 6502).

---

## See also

- [05_how_the_machine_works.md](05_how_the_machine_works.md): buses, who may R/W whom, interleave
- [08_memory_map.md](08_memory_map.md): addresses to memorize
- [14_reduced_number_of_chips.md](14_reduced_number_of_chips.md): 1284 coprocessor, pads, 49-chip v0
- [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md): banks / patterns / palettes
- [04_worlds_and_screens.md](04_worlds_and_screens.md): worlds / screens / MAP atlas
- [07_emulator_specification.md](07_emulator_specification.md): what the C emulator must enforce
