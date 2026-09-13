# Hardware

Main board and fixed I/O. Cart edge details that are physical live here. Cart content layout is in `cartridge.md`.

## CPU and memory

- W65C02 at 8 MHz.
- 32 KB system RAM.
- 32 KB interleaved VRAM. CPU and video bus take turns on VRAM by CPU phase.

## Coprocessors

A pair of AVR128DB28 at 24 MHz helps the 6502 with heavy work.

Known intents so far:

- One AVR draws the sprite overlay during vblank.
- One AVR manages entities.

Exact split of duties is still open. See `open-questions.md`.

## Video outputs

- RGBS and RGBHV.
- Composite via an AD724.

## Board constraints

- At most 16 ICs on the main board to orchestrate everything.
- Screen scroll is hardware based.
- MAP/nametable streaming from cart is hardware based.
- Most video logic should be in hardware so PRG stays on game logic.

## Player and arcade I/O

- GPIO for arcade microswitches.
- Two-player controllers via 3.5mm jack connectors (three cables per pad).

## Cartridge connector

Target about 18 pins/pads per side so a connector like
[EDAC 395-036-520-201](https://www.digikey.com/en/products/detail/edac-inc/395-036-520-201/1297144)
(or similar) can be used.

Cart contents and flashing are covered in `cartridge.md`.
