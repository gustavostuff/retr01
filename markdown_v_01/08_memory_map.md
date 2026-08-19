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
| `$0000-$7FFF` | 32 KB | **System RAM** | Entire AS6C62256, CPU-only, no interleave |
| `$8000-$FDFF` | 32 256 B | **PRG-ROM window** | Banked cart PRG |
| `$FE00-$FEFF` | 256 B | **I/O page** | PPU, VRAM port, OAM, CHR bank latches, APU, MAP port, cabinet, mapper |
| `$FF00-$FFFF` | 256 B | **PRG (high)** | Same mapper window family, holds `$FFFA-$FFFF` vectors |

---

## 2. I/O page layout (`$FE00-$FEFF`)

Grouped in **16-byte blocks** (high nibble of the low byte = device family):

| Range | Block | Device |
|-------|-------|--------|
| `$FE00-$FE0F` | `0` | **PPU control:** mode, status, scroll X/Y, nametable arrangement, NMI, **raster Y / IRQ** |
| `$FE10-$FE1F` | `1` | **VRAM port:** address latch + data R/W into 32 KB VRAM (**interleaved**) |
| `$FE20-$FE2F` | `2` | **OAM port:** address + data into the **1284** (no hardware DMA; `$FE22` unused) |
| `$FE30-$FE3F` | `3` | **CHR bank / world:** BG bank latches, sprite bank, world select |
| `$FE40-$FE5F` | `4-5` | **APU:** NES-style channels |
| `$FE60-$FE6F` | `6` | **Cabinet / controllers** |
| `$FE70-$FE7F` | `7` | **Board EEPROM / DIP** |
| `$FE80-$FE8F` | `8` | **PRG mapper:** only official PRG bank control |
| `$FE90-$FE9F` | `9` | **MAP port:** address latch + data read from cart MAP-ROM |
| `$FEA0-$FEFF` | `A-F` | **Reserved** |

### Scroll (`$FE0x`)

- **`scroll_x`**, **`scroll_y`**: one byte each, values **0-255**, wrap naturally.
- They fine-scroll the 256x240 viewport across the live nametable field (1, 2, or 4 screens arranged/mirrored in the four VRAM slots). Neighbor slots are visible as the camera crosses a seam.
- There is no 16-bit map camera in hardware. Seam refill and empty neighbors: [04_worlds_and_screens.md](04_worlds_and_screens.md). `$FE30` world select stays put unless the game changes chapter.

### Raster (`$FE0x`), not sprite-0

NES sprite-0 hit is **not** the raster API. Gameplay collision is AABB. Beam timing is a compare against the Y counters.

| Field | Role |
|-------|------|
| `raster_y` | Line to match (0-255, visible splits 0-239) |
| `beam_y` | Live Y, read-only |
| `raster_hit` | Sets at start of matching scanline (dot 0). Ack in software |
| `raster_irq_enable` | Match asserts **IRQ**, not NMI |

NMI remains VBlank only. Exact bytes: `B2` in [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md). Mid-frame bank and scroll splits: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) section 8.

Write scroll and `$FE30` during **HBlank** for a clean split (next fetch, up to 8 px delay if you write mid-tile).

### OAM (`$FE20-$FE2F`)

OAM (64 × 4 bytes) lives in the **ATmega1284P**, not in VRAM and not in a dedicated SRAM. The 6502 writes a store loop; there is **no** hardware DMA / `RDY` steal.

| Addr | Role |
|------|------|
| `$FE20` | OAM address (auto-inc after data write) |
| `$FE21` | OAM data |
| `$FE22` | Unused (NC). Do not implement a DMA trigger |

Default entry order: Y, tile, attr, X ([OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) B6). Coprocessor + line buffer: [14_reduced_number_of_chips.md](14_reduced_number_of_chips.md).

### APU (`$FE40-$FE5F`), NES-style

Sound *contract* is NES-like. The 6502 only writes registers. It does not synthesize samples.

| Channels | Role |
|----------|------|
| Pulse 1, Pulse 2 | Square / duty |
| Triangle | Triangle |
| Noise | Noise |
| DMC | Samples / bits |

An **ATmega** (or similar) on the board runs the timers and mix. Game code pokes `$FE4x-$FE5x` and keeps going. Exact bitfields are still open (`B2` in [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md)). The emulator should treat this page as NES-style channel regs and emit host PCM.

### CHR banks (`$FE30`)

These latches answer **which pictures** the PPU is using. They do **not** scroll, and BG bank selection is tied to the live nametable slots rather than treated as one playfield-wide value.

| Field | Meaning |
|-------|---------|
| World 0-7 | Cart chapter. CHR (and, in software, which MAP set you stream) |
| BG bank latch (per slot) | Which of that world's 4 **BG banks** supplies tile patterns for one **nametable slot** (0–5) |
| Sprite bank | Which of that world's 4 **sprite banks** supplies OAM tile patterns. Global latch within the selected world |

Slots **0-3** (camera) and **4-5** (plane) each carry their own **BG bank latch**. When the BG fetch path resolves which nametable slot the current pixel belongs to, it also selects that slot's BG bank. The **sprite bank** is a **separate latch**: changing any BG bank latch does **not** change the sprite bank, and changing the sprite bank does **not** rewrite any BG bank latch. To show a different sprite bank, or to change slot arrangement / scroll for another scanline band, use raster IRQ as before.

Writable **mid-frame**. The next CHR fetch uses the new value. Shift registers may still show the current tile (up to 8 px). Raster IRQ + HBlank writes: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) section 8. Emulator: on write, set `ppu.world`, `ppu.bg_slot_bank[slot]`, and `ppu.spr_bank` as appropriate.

### Controllers / cabinet (`$FE60-$FE6F`)

CPU view is **one byte per player**. Reads only. Same layout on A, C, and H. Bit numbers are locked; physical IDC pin numbers are still free as long as they match this byte.

| Addr | Byte |
|------|------|
| `$FE60` | Player 1 |
| `$FE61` | Player 2 |
| `$FE62+` | Unused |

| Bit | Name (default) | Arcade | Console / handheld |
|-----|------|--------|--------------------|
| 0 | Dpad Right | stick | d-pad |
| 1 | Dpad Left | stick | d-pad |
| 2 | Dpad Down | stick | d-pad |
| 3 | Dpad Up | stick | d-pad |
| 4 | X | button | button |
| 5 | Y | button | button |
| 6 | Coin (Select on console) | Coin (P1=`Coin1`, P2=`Coin2`) | Select |
| 7 | Start | Start | Start |

**1 = pressed.** No extra cabinet byte. Arcade Coin/Start **are** Select/Start.

**Default names, not restrictions:** the board exposes *bits*, not artwork. On an arcade control panel you can label and wire any physical switch to any bit position (and in Retr01-C, the controller MCU just has to report the same bits). User-facing labeling is therefore free-form; the software only cares that `$FE60`/`$FE61` reflect the intended gameplay inputs.

Example mappings you might see on real cabinets (these are just examples; your wiring can differ):

- **Buttons-only, 8-button panel:**  
  `1, 2, 3, 4` map to `Dpad (U/D/L/R)` in whatever order you want, `A/B` map to `X/Y`, `C` maps to `Coin` (bit 6), and `D` maps to `Start`.
- **Stick + Coin + A/B/C (no dedicated Start button):**  
  stick drives `Dpad (bits 0–3)`, coin drives `Coin (bit 6)`, and you choose *one* of the remaining `A/B/C` buttons to wire to `Start (bit 7)`. (From a player’s point of view: “any of these buttons starts the game once you’ve inserted a coin.”)
- **Keyboard-style panel:**  
  arrow keys map to `Dpad`, `Space` maps to `X`, `Enter` maps to `Y`, `C` maps to `Coin` (bit 6), and `Esc` maps to `Start`.
- **6-button “one action” panel:**  
  keep `Dpad (4) + Coin + Start`, and wire a single extra action button to *both* `X (bit 4)` and `Y (bit 5)` so the cabinet only needs one action switch.

Retr01-A: parallel switches on the 20-pin controller IDC. Retr01-C: 3-wire pad with an MCU **in the controller**; the board 1284 reconstructs the same two bytes. Details: [14_reduced_number_of_chips.md](14_reduced_number_of_chips.md), [03_hardware_variants.md](03_hardware_variants.md).

### MAP port (`$FE90`): cart map reads

MAP-ROM holds the world atlas and compressed screens. It is **not** in the 6502 address space. You cannot `LDA` a nametable off the cart. Atlas, directory, empty template, and `load_screen`: [04_worlds_and_screens.md](04_worlds_and_screens.md).

Typical MAP access:

1. Write a 24-bit MAP address into `$FE90` (lo / mid / hi, exact regs TBD).
2. Read `$FE92` (data). Hardware auto-increments the address.

No MAP window over RAM or PRG. PRG banking stays `$FE80` only.

### PRG mapper (`$FE80`): canonical only

**Only** `$FE80` block selects which PRG slice appears at `$8000-$FDFF` / `$FF00-$FFFF`.  
Writes into `$8000-$FFFF` do **not** change banks (ignored / open bus). Keeps GAL decode simple.

---

## 3. VRAM chip map (32 KB, not in CPU space)

CPU touches VRAM only through **`$FE10-$FE1F`**. CHR comes from cartridge CHR-ROM. **OAM is not in this chip** (see `$FE2x`).

| VRAM offset | Size | Contents |
|-------------|------|----------|
| `$0000-$07FF` | 2 KB | Nametable slot 0: 960 tiles + 240 packed attrs (+ pad) |
| `$0800-$0FFF` | 2 KB | Nametable slot 1 |
| `$1000-$17FF` | 2 KB | Nametable slot 2 |
| `$1800-$1FFF` | 2 KB | Nametable slot 3 |
| `$2000-$2FFF` | 4 KB | Streaming scratch (decompress temps) |
| `$3000-$37FF` | 2 KB | Plane slot 4 (repeating parallax, optional) |
| `$3800-$3FFF` | 2 KB | Plane slot 5 (second band / hills, optional) |
| `$4000-$7FFF` | 16 KB | Reserved |

Each slot (2 KB): tiles at `+0x000` (960 bytes), packed attributes at `+0x3C0` (240 bytes). One attr byte is a **2×2 attr quadrant** with **four** 2-bit palette IDs (one per tile), not NES's shared 2x2. Remaining bytes in the slot are pad. Slots **0–3** form the live 1/2/4-screen **camera**. Slots **4–5** are **parallax planes** only (not part of the 4-screen camera). Raster split: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) section 8.

---

## 4. Cartridge (outside CPU map)

| Region | Budget | Access |
|--------|--------|--------|
| PRG | <=512 KB | `$8000-$FDFF` + `$FF00-$FFFF` via `$FE80` |
| CHR | <=256 KB | PPU fetches; world + BG/sprite **banks** via `$FE30` latches |
| MAP | <=~1.17 MB | CPU reads via **`$FE90` MAP port** only |

---

## 5. Address decoding (GAL / virtual bus)

| Select | When |
|--------|------|
| System RAM CS | `$0000-$7FFF` |
| I/O / latch enables | `$FE00-$FEFF` |
| PRG OE | `$8000-$FDFF` and `$FF00-$FFFF` |
| VRAM CS + mux | VRAM data-port cycles, qualified by **clock phase** |
| CHR OE | PPU fetch cycles; CHR address from world + slot BG bank or global sprite bank |
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
| Line-buffer SRAM | Beam vs 1284 (ping-pong; not in CPU space) |
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

Interleave muxes toggle on the **CPU** clock phases. The beam and fetch sequencer advance on the **dot** clock. Line buffers / shift registers sit between those domains (same idea as a real PPU).

On analog **RGBS** into a 15.7 kHz arcade/CGA CRT, this timing **is** the monitor refresh (~15.7 kHz H, ~60.1 Hz V). NMI is that same vertical rate. An off-board analog-to-HDMI converter would resample into a TV's own 60 Hz. The PPU still counts 341x262. Full RGBS sync polarity can still be tuned on the bench. The numbers above are the locked digital timing.

Graphics overview: [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md).  
Emulator: [07_emulator_specification.md](07_emulator_specification.md).
