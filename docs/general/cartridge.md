# Cartridge

Physical cart and how it ties to the board. **Byte layout of the game image** lives in `memory.md`.

## Hardware on the cart

- 512 KB flash ROM (whole `.retr01` image: PRG, palettes, worlds, other screens, entities, compressed BGM).
- A small EEPROM IC for saves (I2C, **MCU-M** as master).
- EDAC **395-036-520-201** 2x18 (or RA **395-036-559-212**). Full pinout in `hardware.md`.
- Cart PCB: **2-layer** (locked). Motherboard is **4-layer** (signal, GND, GND, signal). See `hardware.md`.

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

The **console is the flasher** for a **seated cart** only. Accessory is **Adafruit's UPDI Friend**, clipped onto motherboard **J10** (2x2). J10 DATA is MCU-M **PC1**. **CART_ARM** is D7 of the compositor MAP latch (`LE_MAP`), cleared by **RESB**.

- **Cart image:** Friend on J10. MCU-M takes a one-wire USART stream on PC1, serves a small 6502 stub on `$8000-$FFFF` reads (with `CPU_RDY` as needed), then the stub in system RAM writes the SST39SF040 (and save EEPROM if needed). MCU-M refuses bridge work unless CART_ARM is high. Cart mode stays off during gameplay. Friend stays off J10 during play.
- **Play vs program:** cart `WE#` stays pulled up and idle in play. The compositor pulses `WE#` only when CART_ARM is high and the CPU is writing cart space. Motherboard gates `OE#` only for PRG / MAP / CHR windows (never with RAM or soft `$7Fxx`).
- **AVRs:** programmed off the motherboard. Friend on a breadboard to pin 19 (SerialUPDI), then the chip is installed. Pin 19 is not on J10.

PLDs (ATF22V10), the color PROM (AT27C256R), the three AVRs, and pad MCUs are **not** flashed through J10. Pre-programmed parts and off-board tools cover those. Prefer programming PLDs, the color PROM, and the AVRs **before** first power-on with the CPU populated. A fuller programming guide will come later. See `hardware.md` and `ic-comms-risks.md`.

J10 pin numbers are locked in `hardware.md`. Host command bytes stay TBD.
