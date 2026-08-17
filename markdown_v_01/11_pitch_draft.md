# Retr01 Pitch Draft

## Retr01: Arcade Motherboard

Discrete-logic 8-bit arcade hardware for developers and cabinet builders. First product: **Retr01-A**. Same core later: **Retr01-C** (console), **Retr01-H** (handheld).

### Graphics

- **16 sprites per scanline** (64 OAM). NES had 8.
- **2bpp** (3 colors + transparency). **8 palettes** (4 BG + 4 sprite) with shared BG backdrop. **Per-tile** background palette select, 4 tiles packed in each attr byte (NES shares one select across a 2x2).
- **8 worlds x 64 screens**. Each screen is a **32x30** nametable. **4 pattern banks per world**, each bank **512** patterns (256 BG + 256 sprites). Up to **512** screens per ~**2 MB** cart.

### Interleaved VRAM

32 KB video SRAM shared on clock phases between CPU and PPU, so there is no VBlank-only graphics prison. Separate 32 KB system RAM for game state. CHR streams from the cartridge.

### Cabinet (Retr01-A)

- 40-pin IDC controls, analog RGBS / S-Video / composite pads (external analog-to-HDMI converter if you want a TV), EEPROM scores/settings, through-hole bring-up.

### Prove it in software first

A hardware-faithful **low-level C emulator** enforces the memory map, interleave, and sprite rules before flash.
