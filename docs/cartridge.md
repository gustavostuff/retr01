# Cartridge

Physical cart and how it ties to the board. **Byte layout of the game image** lives in `memory.md`.

## Hardware on the cart

- 512 KB flash ROM (whole `.retr01` image: PRG, palettes, worlds, other screens).
- A small EEPROM IC for saves (I2C, **MCU-M** as master).
- EDAC **395-036-520-201** 2x18 (or RA **395-036-559-212**). Full pinout in `hardware.md`.

## Logical contents (pointer)

See `memory.md` for the full map. Short version:

- Flat **32 KB PRG** (no banking). See `selling-points.md`.
- Global palette index planes (256 B total).
- Up to **8** world blobs (each with 32 KB CHR, sparse BG1/BG0 screens, entities).
- Global **other screens** (title, interstitial, credits).
- Max fill of every world cap leaves ~**8.4 KB** free for entities / extra globals.

## Desired workflow

**v1:** program carts on the bench with the USB-C flasher (same 36-pin edge). Protocol and cmds are in `hardware.md`.

Console-seated flashing can come later. Edge already has `WE#` / `OE#` / data for that path.
