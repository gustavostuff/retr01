# Cartridge

Physical cart and how it ties to the board. **Byte layout of the game image** lives in `memory.md`.

## Hardware on the cart

- 512 KB flash ROM (whole `.retr01` image: PRG, palettes, worlds, other screens, entities, compressed BGM).
- A small EEPROM IC for saves (I2C, **MCU-M** as master).
- EDAC **395-036-520-201** 2x18 (or RA **395-036-559-212**). Full pinout in `hardware.md`.
- Cart PCB: **2-layer** (locked). Motherboard initial design is **2-layer** too (see `hardware.md`).

## Logical contents (pointer)

See `memory.md` for the full map. Short version:

- Flat **32 KB PRG** (no banking). See `selling-points.md`.
- Global palette index planes (256 B total).
- Global CHR: **16** banks (**64 KB**). Playfields, other screens, sprites, and the marked player share this pool. Nametable attrs and OAM attrs use the same 4-bit bank field. See `memory.md`.
- Up to **8** world blobs (maps only: up to **64** BG1 + **16** BG0 screens each).
- Compressed **BGM** bytecode (MAP region, outside the 32 KB PRG window). At max fill that leftover is ~**66.7 KB**, on the order of **24 minutes** of busy 5-channel tracker BGM. See `memory.md`. AKWF / DPCM samples stay in MCU-S2 flash.
- Global entity catalog: up to **32** types.
- Global **other screens**: max **16** total (title / interstitial / credits share the pool).
- Marked **player** patterns: global CHR (any of the 16 banks).

## Programming workflow

The **console is the flasher** for the **three AVRs** and a **seated cart** only. Accessory is **Adafruit's UPDI Friend**, clipped onto **one** shared motherboard header. A **4-pos DIP** selects MCU-M, MCU-S1, MCU-S2, or cart (default **all OFF** = safe / nothing connected).

- **AVRs (on board):** turn on the matching DIP, SerialUPDI into that chip's UPDI pin.
- **Cart image:** turn on the cart DIP, MCU-M bridges onto the cart bus and fills the SST39SF040 (and save EEPROM if needed). MCU-M must **refuse** bridge / `WE#` work unless cart mode is selected. Cart mode stays off during gameplay.
- **Play vs program:** cart `WE#` stays pulled up and idle in play. Motherboard gates `OE#` only for PRG / MAP / CHR windows (never with RAM or soft `$7Fxx`).

PLDs (ATF22V10), the color PROM (AT27C256R), and pad MCUs are **not** flashed through this console header. Builders can **buy them pre-programmed** or program them themselves (Arduino Nano/Uno-based GAL tools such as Afterburner, a TL866-class PROM/PLD programmer, Arduino-as-ISP for the ATtiny85). Prefer programming PLDs and the color PROM **before** first power-on with the CPU populated. A fuller programming guide will come later. See `hardware.md` and `ic-comms-risks.md`.

Builders can also flash each AVR on a **breadboard** with Adafruit's UPDI Friend before soldering, then use the on-board header later for cart programming and AVR updates.

Exact header pin numbers / protocol TBD. See `hardware.md`.
