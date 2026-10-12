# Flashing Tier A

Two ATF22V10 PLDs (Beam X and Beam Y) and one AT27C256R color PROM have to be programmed before they go into a socket. The burn files are in `docs/misc/irl_bring_up_log/firmware/a/`. The Adafruit 2045 Si5351 breakout can supply the 5.369318 MHz dot clock, and the 8-channel USB logic analyzer can watch that clock and the sync pulses. The burn itself takes a parallel programmer.

RCA jacks can stay in the bag. Composite is a later tier. This lab is RGB plus sync.

## Already on the bench

| Piece | What it is | Where it fits |
| --- | --- | --- |
| HiLetgo CH340 USB-to-TTL, STC download adapter, Dupont harness | USB serial port. The STC adapter is a UART boot path for STC 8051 parts. | Tier C, as a SerialUPDI lead into an AVR128DB28. Voltage jumper at 5 V, and a resistor between TX and RX. Retr01 has no STC part for the download mode to talk to. |
| USB logic analyzer, 8 channels, 24 M samples | Capture pod for PulseView or sigrok-cli | After power is on. It can see the 5.37 MHz dot clock, CSYNC, HSYNC, and VSYNC. |
| Adafruit 2045 Si5351A (DigiKey 1528-1206-ND) | I2C clock breakout | The 5.369318 MHz dot clock. |

## The programmer

| Chip | Count | File | Hardware |
| --- | --- | --- | --- |
| ATF22V10C, PDIP-24 | 2 | `firmware/a/beam_x.jed` and `firmware/a/beam_y.jed` | XGecu T48 or TL866II Plus, device ATF22V10C. An Arduino Uno or Nano running Afterburner burns these two as well. |
| AT27C256R-45, PDIP-28 | 1 | `firmware/a/at27c256r_kit.bin` (64 bytes, packed R3G3B2) | The same T48 or TL866II Plus, device AT27C256R, VPP in the 12.5 V to 13 V range. Afterburner covers GAL parts only. |

One T48 or TL866II Plus covers both chips. Pre-programmed parts are the other path. The PROM is one-time, so the blank check and the verify are the whole safety net. On the PLDs, leave the security fuse unprogrammed so a later read-back still works.

What each file does, and the pinout, is in `docs/misc/irl_bring_up_log/firmware/a/README.md`. Programmer clicks are in `docs/bringup/programming-tier-abc.md`.

## Clock, next to the burn

The Si5351 keeps 5.369318 MHz in volatile registers, so each power-up needs a fresh I2C write (address 0x60) before a clock pin toggles. That write comes from an I2C master. An Arduino Uno or Nano can do it, and the same board can run Afterburner for the two PLDs. The PROM stays on the T48 or TL866II Plus.

Socket the PLDs. A new equation means they come back out. After a clean verify, the PROM can stay seated.

Tier B is a third ATF22V10 on this same programmer. Tier C is the first time the CH340 comes out.
