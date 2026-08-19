# Retr01 Pitch Draft

Retr01 is a family of discrete 8-bit machines that share one cartridge, one memory map, and one way of drawing a picture. First board: **Retr01-A**, an arcade motherboard you can socket, probe, and repair. Same core later as **Retr01-C** (console) and **Retr01-H** (handheld).

This is not a Raspberry Pi in a JAMMA shell, and it is not an FPGA pretending to be a PPU. Game code runs on a **W65C02S**, a living CMOS 6502 (which means a real 8-bit CPU you can still buy in a DIP-40, not a core hidden in a bitstream). The background is **74HC** (ordinary 5 V logic chips: counters, muxes, gates). Sprites and pads are an **ATmega1284P**. Audio is a separate **ATmega328P**. If a chip dies, you pull it.

---

## Who it is for

**Cabinet builders** who want RGBS into a 15.7 kHz monitor, a 20-pin IDC for two players, Coin/Start as ordinary bits, and EEPROM for scores — without reverse-engineering a 1988 board.

**Developers** who want a NES-class programming model that is *stricter and kinder*: per-tile palettes, 16 sprites per line, no VBlank lockout on VRAM, raster IRQ instead of sprite-0.

**People who like through-hole.** DIP, 5 V, **ATF22V10** glue that is still in production (small PAL-style chips that compile to AND-OR; not an FPGA, not a discontinued Lattice GAL). Proto qty-1 motherboard + cart is in the **~$200** band ([12](12_part_prices_and_cost.md)).

---

## What you are actually buying (when A ships)

A **motherboard + plug-in cart**. The cart holds PRG (code), CHR (tile art), and MAP (the world atlas). The board holds CPU, RAM, video, sound, and cabinet I/O.

You write 6502. You do not allocate a framebuffer. You fill **nametables** and **OAM**; silicon walks the beam. Nametables are a grid of tile numbers, not pixels. OAM is the sprite table the CPU fills with `STA $FE21` (256 stores in VBlank, no DMA chip).

**Interleaved VRAM** (`PHI2` high = CPU may use the `$FE1x` port, `PHI2` low = the PPU fetches tiles) means you can write graphics *during* the frame. **NMI** at line 240 is a 60 Hz metronome, not a panic window (which means: the NES made you cram all VRAM updates into a tiny blanking gap; Retr01 still shares the video SRAM, but the CPU gets every other cycle, all the time).

---

## Picture

256×240, **2bpp** (two bits per pixel: three colors plus transparency per tile or sprite). **8 palettes** (4 background, 4 sprite), shared backdrop. **Per-tile** background palette select — 240 attr bytes per screen, 2 bits per tile (NES shares one palette across a 2×2 of tiles; we do not).

**64 sprites**, **16 per scanline** (NES had 8). OAM lives in the 1284. A third SRAM is a one-line **ping-pong buffer** (the beam reads last line’s sprites while the 1284 draws the next; objects show up one line late, not a frame late).

**8 worlds** on a sparse grid up to 16×16, at most 64 stored screens each. A screen is 32×30 tiles. Each world has **4 BG banks + 4 sprite banks**, 256 patterns each. A ~2 MB cart can hold on the order of **512** screens if you pack the atlas. **MAP** is a directory the CPU reads through `$FE90` (you do not mmap the whole world; you ask for a room and stream it).

**Raster IRQ** on a scanline compare (an interrupt on a line you pick). Split the screen, swap CHR banks, run a parallax band. No burned sprite-0 pixel.

---

## Sound and sticks

NES-style channels on the 328P: two pulse, triangle, noise, DMC. The 6502 pokes `$FE40–$FE5F` and keeps running physics (the sound chip is a second MCU; the game never bit-bangs a speaker).

Two players, **one byte each** (`$FE60`, `$FE61`): Right, Left, Down, Up, X, Y, Coin, Start. Arcade Coin *is* Select. No 12-button layout, no extra cabinet byte. Retr01-C will use a 3-wire pad with an MCU in the controller; software still reads those two bytes.

---

## Cabinet facts (Retr01-A)

- Through-hole, socket the expensive ICs.
- 5 V barrel (5.5 × 2.1 mm), or the cab PSU. Pads for a USB-C breakout if you want that jack — not USB Power Delivery.
- Analog **RGBS** (red, green, blue, and composite sync on four wires), plus S-Video and composite pads. No HDMI on the board; hang a converter on RGBS if the venue is a TV.
- 20-pin IDC to the control panel. RGBS on its own 5-pin header.

Clocking is honest about 1980s CRTs: CPU **8.000 MHz**, dot **5.369318 MHz**, 341×262, ~60.1 Hz. (That dot rate is NES-class. On a 15.7 kHz arcade/CGA tube, our digital timing *is* the refresh. A modern TV is doing its own 60 Hz; the converter resamples.)

---

## Why this, not the obvious alternatives

| Alternative | Why Retr01 is different |
|-------------|-------------------------|
| MAME / Pi cab | You get a picture. You do not get a machine you can single-step with a logic probe. |
| FPGA “PPU” | Fast to prototype, opaque to repair, and a different product. Our BG path is 74HC you can see. |
| Stock NES clone | 8 sprites/line, VBlank lockout, sprite-0, 1.79 MHz. We kept the *feel* and threw out the prison. |
| Pure AVR game | Fine for a toy. A 6502 + cart + nametables is the whole point of the family. |

Sprites are firmware on a 1284 because a discrete 16-channel shifter farm is a wiring nightmare and saves **zero** features. Background stays discrete. Two AVRs, two jobs — do not merge audio into sprites.

---

## Family

**A** proves the core in a cabinet. **C** is the same map in the living room. **H** is SMD later, same `$0000–$7FFF` / `$FExx` / cart contract. One toolchain. One emulator. Three shells.

---

## Status (honest)

Architecture is locked in `markdown_v_01/`. First silicon is a 49-IC DIP plan ([14](14_reduced_number_of_chips.md)). Bring-up is simulator then proto boards ([16](16_simulation_and_bringup_plan.md), [17](17_protoboard_test_plan.md)), then a hand-drawn schematic — not an AI BOM. A C emulator will match this map before carts go in a cabinet.

If you want the short version: **it is an arcade board you can understand**, with more sprites, better palettes, and VRAM you can touch without waiting for blanking.
