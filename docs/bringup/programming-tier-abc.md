# Programming guide for Tier A, B, and C ICs

Summary of programming tools and flashing procedures for the programmable devices in Retr01 Tiers A, B, and C.

Volatile parts (such as AS6C62256 SRAM) and standard logic (such as 74HC573 and 74HC74) require no programming device.

---

## 1. Programming devices summary

| Device | Target ICs | Interface | Primary tool |
| --- | --- | --- | --- |
| **Universal programmer** | ATF22V10 (PLDs), AT27C256R (Color PROM) | Parallel ZIF socket (high voltage VPP) | XGecu T48 (or TL866II Plus) with Xgpro |
| **SerialUPDI adapter** | AVR128DB28 (MCU-S1) | 1-wire UPDI (pin 19) | USB-to-UART module (CH340/CP2102) with resistor, or Adafruit UPDI Friend |

---

## 2. ATF22V10 PLD programming (Beam X, Beam Y, Compositor)

Retr01 uses three ATF22V10C parts in 24-pin DIP packages.

### Hardware setup
- Seat the ATF22V10 directly into the 40-pin ZIF socket of the programmer. Align pin 1 toward the lever per the socket diagram.
- No external adapter board is required for standard DIP-24 parts.

### Software workflow (Xgpro)
1. Select the device: search for **ATF22V10C** or **ATF22V10C(DIP24)**.
2. Load the target JEDEC fuse file (`.jed`) produced by CUPL or WinCUPL for the specific role (Beam X, Beam Y, or Compositor).
3. Verify programming options: keep default VPP and programming algorithms.
4. Execute blank check, write, and verify operations.
5. Keep security fuse unprogrammed during lab bring-up to allow verification and re-flashing.

---

## 3. AT27C256R Color PROM programming (Master palette)

Retr01 uses one AT27C256R OTP parallel EPROM in a 28-pin DIP package (600 mil row width).

### Hardware setup
- Seat the AT27C256R directly into the 40-pin ZIF socket of the programmer.
- Ensure the socket lever locks the pins securely.

### Software workflow (Xgpro)
1. Select the device: search for **AT27C256R** or **AT27C256R@DIP28**.
2. Run a blank check to verify all bits read unprogrammed (0xFF).
3. Load the 64-byte master palette binary file (packed R3G3B2 format, bytes 0 to 63).
4. Verify the programming algorithm specifies standard 12.5 V to 13.0 V VPP per the Microchip datasheet.
5. Execute write and verify operations.
6. Because the AT27C256R is one-time programmable (OTP), verification is permanent.

---

## 4. AVR128DB28 MCU-S1 programming (UPDI)

Retr01 uses the AVR128DB28-I/SP in a 28-pin SPDIP package (300 mil row width). Programming uses the single-wire UPDI interface on pin 19.

### SerialUPDI hardware adapter setup
A standard USB-to-UART adapter (such as a CH340 or CP2102 module) operates as a SerialUPDI programmer:
- Set the module supply voltage jumper to 5 V.
- Place a resistor (1.0 k ohm to 4.7 k ohm) between the module TX pin and RX pin.
- Connect the module RX pin to AVR128DB28 pin 19 (UPDI).
- Connect module GND to breadboard GND.
- Connect 5 V power to AVR128DB28 VDD and VDDIO2 pins (pins 6 and 14 to +5 V, pins 15 and 21 to GND).

### Flashing via pymcuprog (command line)
Microchip's `pymcuprog` tool flashes hex files over standard serial ports:

```bash
pymcuprog write -d avr128db28 -t uart -u /dev/ttyUSB0 -f firmware.hex
```

On Windows, replace `/dev/ttyUSB0` with the corresponding COM port (such as `COM3`).

### Flashing via Arduino IDE (GUI)
1. Install the `DxCore` board package.
2. Select Board: **AVR128DB28**.
3. Select Programmer: **SerialUPDI** (or SerialUPDI with 4.7k resistor).
4. Select the matching serial port and upload.
