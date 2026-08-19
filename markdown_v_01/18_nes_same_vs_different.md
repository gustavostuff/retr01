# Retr01 vs NES: Same Mental Model, Different Implementation

Retr01 borrows the NES-era *programming mental model* (nametables + CHR tiles + an OAM-like sprite list + an APU-style sound chip + VBlank/NMI as a frame boundary). But the silicon is discrete and the buses/registers are not the NES PPU.

These tables are for alignment while writing the emulator and authoring tools.

## What is the same as NES

| Topic | NES-style meaning (what you can expect) |
|-------|------------------------------------------|
| 6502 family / game loop | A 6502-class CPU runs game code, and you use frame timing to advance gameplay. |
| NMI as “frame boundary” | VBlank/NMI is used as the “a frame finished” metronome. |
| Tile graphics are 2bpp + transparency | Tiles/pixels are effectively “3 colors + transparent” per 2-bit pixel. |
| Nametable background layout | Background is organized as a 32×30 nametable of 8×8 tile indices. |
| CHR tile data is cartridge-backed | Tile pattern bytes come from the cartridge’s CHR bank (banked). |
| OAM-like sprites | Sprites are defined by a list of bytes (Y, tile index, attributes, X) and compose with background using priority + palette rules. |
| Palettes exist | Pixels reference palette indices which map through a master RGB table. |
| NES-style sound channels | Audio is generated using the NES channel set (pulse / triangle / noise / DMC concept). |

## What is different from NES

| Topic | Retr01 difference (the parts that change how you build/emu) |
|-------|--------------------------------------------------------------|
| CPU/PPU timing contract | Retr01 uses discrete logic and bus interleave (CPU phase vs video phase). The 6502 does not get a NES-style “VBlank-only VRAM update” prison. |
| VRAM is interleaved shared SRAM | CPU and BG PPU take turns on the same 32 KB video SRAM via `PHI2`-based multiplexing. NES has separate PPU VRAM. |
| Different register / memory map | Retr01 exposes the system through `$FE1x`/`$FE30`/`$FE40` and a MAP port `$FE90`, not the NES `$200x` PPU register set. |
| Background palette selection | NES uses attribute bytes that select palettes for 2×2 tile quadrants. Retr01 stores per-tile palette IDs (packed in a 240-byte/nametable attr plane). |
| Sprite throughput + timing | Retr01 targets **16 sprites/scanline** max, evaluated by an **ATmega1284P** during the visible line and written into a next-line line buffer. NES sprite evaluation is in the PPU. |
| No hardware sprite DMA | NES has OAM DMA. Retr01’s OAM upload is a 6502 store loop into the 1284 (`$FE21`), with no DMA steal. |
| Sprite-vs-BG gameplay assumptions | Retr01 is designed for strict software collision (AABB in PRG). Sprite-vs-BG “hits” are not a gameplay trigger. |
| No integrated PPU datapath | Retr01’s BG “PPU” is built from 74HC + counters + muxes (discrete compositor), not a unified console PPU chip. |
| World atlas / MAP streaming | NES loads level data from cartridge/PPU name tables directly. Retr01 adds a `MAP-ROM` + streaming directory (`$FE90`) so “worlds + screens + streaming seams” are first-class. |
| More explicit mid-frame controls | Retr01 supports raster IRQ-driven mid-frame changes (e.g. switching BG/sprite banks) via its own latch set and interrupt sources. |
| No programmer-visible pattern tables/pages | NES discussions often talk about pattern tables or 4 KB pages. Retr01 does not need that extra layer: screens index tiles 0-255 in the currently selected BG bank half, and sprites index tiles 0-255 in the selected sprite bank half. |

