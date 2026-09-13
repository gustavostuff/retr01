# Cartridge

Physical cart and what it stores. Connector pin target is also noted in `hardware.md`.

## Hardware on the cart

- 512 KB flash ROM for the game.
- A small EEPROM IC for saves.
- Aim for about 18 pins/pads per side. Candidate connector:
  [EDAC 395-036-520-201](https://www.digikey.com/en/products/detail/edac-inc/395-036-520-201/1297144)
  or similar.

## Desired workflow

Flash the cartridge through the console itself, possibly with USBasp or an Adafruit UPDI Friend in the loop. Exact path is open. See `open-questions.md`.

## Content the cart holds (logical)

Exact binary map is TBD. Expected pieces:

- PRG / game code.
- World and screen maps (BG1 and optional BG0), including sparse slot placement.
- Tile and sprite pattern banks (per world: 4 BG + 4 sprite banks, 256 tiles each, 2bpp).
- 32 BG palettes and 32 sprite palettes (user defined), as rows of individual palettes. Each individual palette is four indexes into the board RGB table (0..63). The 64 RGB values themselves live on a main-board AVR EEPROM, not as the cart palette payload.

Screen size reminder: 16x15 tiles, 240 tile bytes + 240 attribute bytes per screen.

Limits reminder: up to 8 worlds, up to 48 BG1 screens per world, up to about 12 BG0 screens per world (soft number).
