# Retr01 Cartridge

Physical cartridge, edge connector, and USB-C programming hardware. Software image layout (`.retr01`) stays in [`memory.md`](memory.md).

**Related:** [`hardware.md`](hardware.md) (motherboard cart path, [runners vs silicon](hardware.md#runners-today-vs-silicon-target)). [`memory.md`](memory.md) (flash map, MAP port). Save EEPROM API: [`memory.md`](memory.md#cart-save-eeprom-24c64-on-cartridge). Flasher is a **bench tool**, not part of the 32-IC motherboard count.

---

## Form factor

Target footprint is **Game Boy-sized** (~**55 mm** PCB width) with a **low-profile** insert so a console shell can stay slim.

| Orientation | Use case |
|-------------|----------|
| **Right-angle (90 deg) edge socket** | **Retr01-C** console: cart and motherboard share a parallel plane (PC Engine / SMS card style, face-up insert). **Primary recommendation.** |
| **Straight (vertical) socket** | **Retr01-A** arcade or top-loading shells. Electrically identical. |

### Connector parts (2x18, 2.54 mm pitch)

**Right-angle (recommended for console)**

| Part | Notes |
|------|-------|
| [EDAC 395-036-559-212](https://www.digikey.com/en/products/detail/edac-inc/395-036-559-212/11138956) | Black, right-angle 2x18 |
| Sullins **EBC18DRAN** | Right-angle 2x18 |

**Straight (arcade / top-load)**

| Part | Notes |
|------|-------|
| [EDAC 395-036-520-201](https://www.digikey.com/en/products/detail/edac-inc/395-036-520-201/1297144) | Black, straight 2x18 |
| Sullins **EBC18DRXN** | Straight 2x18 |

**Optional: TE Standard Edge II receptacle (straight female)**

Industrial card-edge **receptacle** on the **motherboard** (or flasher). The cart is a **PCB card edge** that slides into the slot. No mating pin-header on the cart. Same **36 contacts**, **2.54 mm** pitch, **2x18** rows (**4.85 mm** row spacing). Use when you want a molded housing and gold fingers instead of dual-row pin sockets.

| Part | Notes |
|------|-------|
| [TE Connectivity **5645235-4**](https://www.mouser.mx/en/ProductDetail/TE-Connectivity-AMP/5645235-4?qs=sGAEpiMZZMvlX3nhDDO4AEduDnuFsUToyijPionwouY%3D) (Mouser) | **Standard Edge II**, 36-pos **female**, straight, through-hole solder tail |
| Series | TE **AMP Standard Edge II**, board-to-board card edge, power and signal |

| Spec | Value |
|------|-------|
| Positions | **36** (18 per row) |
| Pitch | **2.54 mm** (0.1 in) |
| Row spacing | **4.85 mm** |
| Orientation | **Vertical** (straight receptacle) |
| Mating | Cart PCB **gold fingers** on **~1.57 mm** (0.062 in) card. Slot depth **~7.5 mm** |
| Contacts | Phosphor bronze, **gold** mating area |
| Current | **3 A** max per contact (far above cart bus needs) |

**Layout notes:** Footprint and finger geometry come from the TE datasheet. Do not assume interchangeability with EDAC/Sullins pin-header patterns. Lock **A1/B1** keying and finger numbering against the [36-pin pinout](#36-pin-edge-pinout) at schematic. Cart PCB thickness and bevel must match **Standard Edge II** card requirements.

Motherboard and flasher use the **same 36-pin electrical pinout**. Pick socket orientation per enclosure. ESD and series-R practice: [`passive_rf_etc.md`](passive_rf_etc.md).

---

## Cartridge electronics

The cart PCB is **passive** (no mapper IC). It exposes:

| IC | Role |
|----|------|
| **SST39SF040** | **512 KB** NOR flash. PRG, CHR, MAP, palettes ([`memory.md`](memory.md)) |
| **24C64** | **8 KB** I2C save EEPROM (per-game saves) |

`CE#` is **tied active** on the cart (or held by cart-local logic). The **motherboard** gates **`OE#`** (and cart programming uses **`WE#`**) so the flash does not fight the system bus during normal play. The 36-pin budget does not route a dedicated `CE#` to the edge. Enable is implicit plus `OE#` from the mobo.

I2C **`SDA` / `SCL`** run to the cart edge. The **ATmega1284P** is the I2C master for saves ([`memory.md`](memory.md#cart-save-eeprom-24c64-on-cartridge)). The 6502 never bit-bangs the cart EEPROM.

---

## 36-pin edge pinout

Pin **A** = top side of cart PCB, **B** = bottom. **18 positions per row**, 2.54 mm pitch.

### Side A (top)

| Pin | Signal | Description | Pin | Signal | Description |
|-----|--------|-------------|-----|--------|-------------|
| A1 | GND | Ground | A10 | A6 | Address 6 |
| A2 | VCC | 5 V | A11 | A7 | Address 7 |
| A3 | SDA | I2C data (save EEPROM) | A12 | A8 | Address 8 |
| A4 | A0 | Address 0 | A13 | A9 | Address 9 |
| A5 | A1 | Address 1 | A14 | A10 | Address 10 |
| A6 | A2 | Address 2 | A15 | A11 | Address 11 |
| A7 | A3 | Address 3 | A16 | A12 | Address 12 |
| A8 | A4 | Address 4 | A17 | A13 | Address 13 |
| A9 | A5 | Address 5 | A18 | GND | Ground |

### Side B (bottom)

| Pin | Signal | Description | Pin | Signal | Description |
|-----|--------|-------------|-----|--------|-------------|
| B1 | GND | Ground | B10 | D6 | Data 6 |
| B2 | VCC | 5 V | B11 | D7 | Data 7 |
| B3 | SCL | I2C clock (save EEPROM) | B12 | OE# | Flash output enable |
| B4 | D0 | Data 0 | B13 | A14 | Address 14 |
| B5 | D1 | Data 1 | B14 | A15 | Address 15 |
| B6 | D2 | Data 2 | B15 | A16 | Address 16 |
| B7 | D3 | Data 3 | B16 | A17 | Address 17 |
| B8 | D4 | Data 4 | B17 | A18 | Address 18 (512 KB) |
| B9 | D5 | Data 5 | B18 | WE# | Flash write enable (programming) |

**Play:** motherboard drives address/data/`OE#`. `WE#` is inactive. **Program:** external flasher asserts `WE#` and runs JEDEC unlock on the SST39SF040 ([`hw/md/SST39SF040.md`](../hw/md/SST39SF040.md)).

---

## USB-C cartridge flasher

Bench tool to program a cart **in the socket** without desoldering the flash IC.

### Why ATmega32U4

`SST39SF040` is **5 V** parallel NOR. A 3.3 V USB MCU would need large level-shifter arrays for address, data, and control. **ATmega32U4-AU** (QFP-44, **16 MHz**, native **5 V**, native **USB 2.0**) bit-bangs the flash with 5 V GPIO.

### Block diagram

```text
  USB-C host (5 V) ----+---- ATmega32U4
                       |         |
                       |    SPI --> 2x 74HC595 --> A0-A15
                       |    GPIO ---------> A16-A18, WE#, OE# (as needed)
                       |    PORTx --------> D0-D7 (8-bit parallel)
                       |
                       +---- 36-pin cart slot (same pinout as console)
```

### Bill of materials (flasher PCB)

| Item | Part / notes |
|------|----------------|
| MCU | **ATmega32U4-AU** (QFP-44) |
| Address shift | **2x 74HC595** (daisy-chained SPI -> **A0-A15**) |
| USB | **USB-C** 16-pin receptacle. **5.1 kohm** on **CC1** and **CC2** (USB-C sink, bus power) |
| Cart interface | **36-pin** 2.54 mm edge slot (straight or right-angle, match your workflow) |
| Passives | Decoupling per IC, USB series resistors as required by layout |

### Operation

| Net | Driven by |
|-----|-----------|
| **A0-A15** | SPI bit-bang into chained **74HC595** |
| **A16-A18** | MCU GPIO (three pins) |
| **D0-D7** | One 8-bit port (e.g. **PORTB**) for fast parallel read/write |
| **WE#** | MCU GPIO. JEDEC command cycles (`$5555`/`$AA`, etc. on SST39SF040) |
| **OE#** | MCU GPIO. Read path during verify |

Firmware presents a USB device (vendor protocol TBD when Studio / CLI tooling lands).

**Runners today:** Emu / Sim load `.retr01` from a host file path (Sim default: `output/test_2.retr01`). On-board flash image and multi-ROM selection are follow-on work ([`hardware.md`](hardware.md#runners-today-vs-silicon-target)).

**Sim (WIP):** Island **F** on the board canvas shows the flasher BOM (**ATmega32U4**, **2x 74HC595**, **USB-C**) for layout and bring-up. The interactive sim does **not** program carts over USB yet (no PC host stream, no cart-in-flasher-socket workflow in the main UI). Lower-level models and `flasher_bench` unit tests (`test_island_flasher`, `test_cart_flash_flow`, `test_usbc_pc_host`) exercise the programming path off-canvas. Treat cart flashing in Sim as **work in progress** until bench UI lands.

### Safety

- Flasher and console **must not** drive the same cart edge at once.
- ESD/TVS at the cart fingers on **both** motherboard and flasher ([`passive_rf_etc.md`](passive_rf_etc.md)).

---

## Open topics (cart hardware)

| Topic | Note |
|-------|------|
| Flasher USB protocol | HID vs CDC vs vendor bulk. Lock when PC tooling ships |
| Sim cart flasher UI | Island **F** on canvas. USB program flow **WIP** (`flasher_bench` tests only) |
| Cart shell / label | Mechanical only |
| Multi-ROM menu | Software + flash layout (post docs) |
