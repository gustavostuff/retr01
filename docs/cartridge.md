# Cartridge

Physical cart and how it ties to the board. **Byte layout of the game image** lives in `memory.md`.

## Hardware on the cart

- 512 KB flash ROM (whole `.retr01` image: PRG, palettes, worlds, other screens).
- A small EEPROM IC for saves (I2C, helper MCU as master).
- Aim for about 18 pins/pads per side. Candidate connector:
  [EDAC 395-036-520-201](https://www.digikey.com/en/products/detail/edac-inc/395-036-520-201/1297144)
  or similar.

## Logical contents (pointer)

See `memory.md` for the full map. Short version:

- Flat **32 KB PRG** (no banking). See `selling-points.md`.
- Global palette index planes (256 B total).
- Up to **8** world blobs (each with 32 KB CHR, sparse BG1/BG0 screens, entities).
- Global **other screens** (title, interstitial, credits).
- Max fill of every world cap leaves ~**8.4 KB** free for entities / extra globals.

## Desired workflow

Flash the cartridge through the console itself, possibly with USBasp or an Adafruit UPDI Friend in the loop. Exact path is open. See `open-questions.md`.
