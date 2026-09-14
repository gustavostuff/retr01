# Cartridge

Physical cart and how it ties to the board. **Byte layout of the game image** lives in `memory.md`.

## Hardware on the cart

- 512 KB flash ROM (whole `.retr01` image: PRG, palettes, worlds, other screens, entities).
- A small EEPROM IC for saves (I2C, **MCU-M** as master).
- EDAC **395-036-520-201** 2x18 (or RA **395-036-559-212**). Full pinout in `hardware.md`.
- Cart PCB: **2-layer** (locked). Motherboard stays **4-layer**.

## Logical contents (pointer)

See `memory.md` for the full map. Short version:

- Flat **32 KB PRG** (no banking). See `selling-points.md`.
- Global palette index planes (256 B total).
- Up to **8** world blobs (each with 32 KB CHR, up to **32** BG1 + **8** BG0 screens).
- Up to **128** entity types in a **global** catalog, shared across worlds. See `software-api.md`.
- Global **other screens** (title, interstitial, credits).

## Programming workflow

The **console is the flasher**. Accessory is **Adafruit's UPDI Friend**, clipped onto **one** shared motherboard header. A **4-pos DIP** selects MCU-M, MCU-S1, MCU-S2, or cart (default **all OFF** = safe / nothing connected).

- **AVRs (on board):** turn on the matching DIP, SerialUPDI into that chip's UPDI pin.
- **Cart image:** turn on the cart DIP, MCU-M bridges onto the cart bus and programs the SST39SF040 (and save EEPROM if needed).

Builders can also flash each AVR on a **breadboard** with Adafruit's UPDI Friend before soldering, then use the on-board header later for cart programming and AVR updates.

Exact header pin numbers / protocol TBD. See `hardware.md`.
