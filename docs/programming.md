# Programming and flashing

Which Retr01 parts need a programmed image, what file each takes, and which bench tools can write them. Cart game images stay in [`cart.md`](cart.md). Chip roles stay in [`hardware.md`](hardware.md) and [`hw/md/`](../hw/md/).

**ASCII only.** No field rewrites for OTP Color PROM once burned.

---

## Quick map

| Part | Where | Image / format | Rewritable? | Typical tool |
|------|-------|----------------|-------------|--------------|
| ATmega1284P | Motherboard | `.hex` / `.elf` (AVR) | Yes (Flash + fuse) | ISP programmer (below) |
| ATmega328P | Motherboard | `.hex` / `.elf` (AVR) | Yes | Same ISP family |
| ATtiny85 | Retr01-C pad PCB | `.hex` (AVR) | Yes | Same ISP family |
| ATF22V10 (x5) | Motherboard | JEDEC `.jed` (CUPL / WinCUPL) | Yes (EE PLD) | ATF / Afterburner-class |
| AT27C256R | Motherboard | Binary / Intel HEX (R3G3B2 table) | **No** (OTP) | Parallel EPROM/OTP programmer |
| SST39SF040 | Cartridge | `.retr01` via flasher protocol | Yes | Retr01 USB-C cart flasher |
| 24C64 | Cartridge | Save image / blank at factory | Yes (I2C) | Cart flasher path or I2C tool |
| ATmega32U4 | Cart **flasher** PCB | `.hex` (USB CDC firmware) | Yes | ISP or DFU (32U4 bootloader) |

HC logic (HC157 / HC245 / HC573), SRAM, oscillators, and AD725 are **not** programmed.

---

## Motherboard: AVRs (ISP)

| Ref (typical) | Part | Clock | Firmware job |
|---------------|------|-------|--------------|
| U1284 | ATmega1284P | 20 MHz | OAM / sprites, pads, machine EEPROM, cart I2C save |
| U328 | ATmega328P | 16 MHz | APU (`$FE40`-`$FE5F`) |

**Interface:** 5 V **ISP** (SPI): `MOSI`, `MISO`, `SCK`, `RESET`, `VCC`, `GND`. Prefer a **6-pin** (2x3) ISP header or pogo pads per MCU so chips can stay in-socket.

**Host software:** [`avrdude`](https://github.com/avrdudes/avrdude) (CLI). Arduino IDE / PlatformIO also work if the board definition matches the MCU and F_CPU.

### Recommended programmers (pick one)

| Tool | Notes |
|------|-------|
| **USBasp** (or USBasp-compatible) | Cheap, widely stocked, works with `avrdude -c usbasp`. Good default for DIP AVRs at 5 V. |
| **Atmel-ICE** (Microchip) | Official debug/program path. ISP + debugWIRE/JTAG where the part supports it. Better if you want stepping later. |
| **Arduino as ISP** | Spare Uno/Nano running the ArduinoISP sketch. Fine for bring-up. Slower and fussier than a dedicated USBasp. |

**Fuses:** set crystal / full-swing oscillator, clock divide, and BOD to match the board oscillators (20 MHz on 1284, 16 MHz on 328). Wrong CKSEL bricks ISP until HV rescue. Document fuse bytes next to each firmware release.

**ATtiny85 (pad boards):** same USBasp / Atmel-ICE chain. Use a SOIC-8 clip or a 6-pin ISP breakout on the pad PCB. See [`controllers.md`](controllers.md).

---

## Motherboard: ATF22V10 SPLDs

Five **ATF22V10** (or ATF22V10CQZ-class) packages: decode, VRAM/timing glue, beam X, beam Y / raster, compositor. Equations -> **JEDEC** fuse file. See [`hw/md/ATF22V10.md`](../hw/md/ATF22V10.md).

**Flow:**

1. Write equations in **WinCUPL** / CUPL (or equivalent).
2. Fit -> `.jed`.
3. Erase / program / verify in a **ZIF** or DIP-24 socket on the programmer (or in-circuit if the board wires Vpp/OE correctly. Socket-out is safer on first spin).

### Recommended programmers (pick one)

| Tool | Notes |
|------|-------|
| **Afterburner** (open ATF programmer) | Community favorite for Microchip/Atmel ATF15xx and **ATF22V10** families. USB, socket adapters, open firmware. Strong fit for Retr01 DIP-24 parts. |
| **Microchip ATDH1150USB** (+ ATMISP / vendor flow) | Official Atmel/Microchip ATF path. Heavier and costlier. Use if you want vendor-supported production tooling. |

Generic "universal" programmers sometimes list ATF22V10. Prefer a tool whose **ATF** algorithm is known-good (Afterburner or Microchip). Do not assume every TL866 profile programs CQZ grades correctly without a verify pass on a known JEDEC.

**Blank / unprogrammed PLDs:** treat outputs as undefined. Always load JEDEC before expecting decode or beam behavior.

---

## Motherboard: Color PROM (OTP)

| Part | Role | Format |
|------|------|--------|
| **AT27C256R-45** | 64-entry master palette | Packed **R3G3B2** bytes. Unused address pins tied **GND**. See [`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md) |

**One-time programmable.** Needs a parallel EPROM/OTP programmer that supports the **AT27C256** family and the **~12.5 V** program algorithm.

### Recommended programmers (pick one)

| Tool | Notes |
|------|-------|
| **XGecu T48** (or current XGecu successor) | Modern universal programmer. DIP-28 / adapter support for AT27C256R. Verify against Microchip algo notes. |
| **TL866II Plus** (with AT27C256 support in software) | Common bench unit. Confirm the exact AT27C256R device ID in the device list before buying adapters. |

Studio exports the Color PROM table. Burn once per board revision / palette kit. Wrong burn means a new OTP part.

Sim may still model **AT28C16** until renamed. Same 64-entry table semantics ([`hardware.md`](hardware.md)).

---

## Cartridge: flash and save EEPROM

| Part | Interface | Tool |
|------|-----------|------|
| **SST39SF040** | Parallel NOR on 36-pin edge | **Retr01 USB-C cart flasher** (ATmega32U4 + HC595s). Protocol in [`cart.md`](cart.md#usb-c-cartridge-flasher) |
| **24C64** | I2C on cart | Prefer flasher firmware path when implemented. Else any 5 V I2C EEPROM tool for factory blanking |

Do **not** program cart flash through the motherboard during play. `WE#` is for the external flasher.

**Flasher MCU itself:** program the **32U4** once with CDC firmware (ISP header on the flasher PCB, or DFU if the Caterina/DFU bootloader is present).

---

## Suggested bench kit (minimal)

For one person building motherboards + carts:

1. **USBasp** (or Atmel-ICE) for all AVRs: 1284, 328, Tiny85, flasher 32U4.
2. **Afterburner** (or ATDH1150USB) for the five ATF22V10s.
3. **XGecu T48** or **TL866II Plus** for AT27C256R Color PROM burns.
4. **Retr01 USB-C cart flasher** for SST39SF040 (and later 24C64) in-cart.

That covers every programmable part in the BOM without overlapping the game-image path with ISP.

---

## Related docs

| Doc | Covers |
|-----|--------|
| [`hardware.md`](hardware.md) | 32-IC BOM, islands, PLD roles |
| [`cart.md`](cart.md) | Edge pinout, USB-C flasher protocol |
| [`memory.md`](memory.md) | `.retr01` layout, EEPROM mailboxes |
| [`sound.md`](sound.md) | 328P APU firmware expectations |
| [`controllers.md`](controllers.md) | ATtiny85 pad firmware |
| [`hw/md/ATF22V10.md`](../hw/md/ATF22V10.md) | PLD resources / JEDEC |
| [`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md) | Color PROM packing |
| [`hw/md/SST39SF040.md`](../hw/md/SST39SF040.md) | Flash command set |
