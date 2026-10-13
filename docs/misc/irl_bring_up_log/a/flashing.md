# Flashing Tier A

Two ATF22V10 PLDs (Beam X and Beam Y) and one AT27C256R color PROM are programmed before they go into a socket. The burn files are in `docs/misc/irl_bring_up_log/firmware/a/`. After that burn, a breadboard on Manjaro carries the dot clock, the two PLDs, and an 8-channel logic analyzer. The HiLetgo CH340 lead is the SerialUPDI path for an AVR128DB28. That write starts at Tier C, and the same machine setup covers the lead.

RCA jacks stay in the bag. Composite is a later tier. This lab is RGB plus sync.

## Software

These packages cover the clock write, the capture pod, and the later AVR write.

```bash
pamac install arduino-cli pulseview sigrok-cli sigrok-firmware-fx2lafw
pamac install python-pymcuprog
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit Si5351 Library"
```

`python-pymcuprog` is the package that provides `pymcuprog`. `sigrok-firmware-fx2lafw` is the firmware the 24 MHz pod loads, so PulseView lists the pod as `fx2lafw`.

Serial ports belong to the `uucp` group.

```bash
sudo usermod -aG uucp "$USER"
```

A new login picks up the group. One CH340 enumerates as `/dev/ttyUSB0` on the `ch341` module. An official Uno enumerates as `/dev/ttyACM0`. A second CH340, including many Nano clones, takes the next `/dev/ttyUSB` number.

## The burn

| Chip | Count | File | Hardware |
| --- | --- | --- | --- |
| ATF22V10C, PDIP-24 | 2 | `firmware/a/beam_x.jed` and `firmware/a/beam_y.jed` | XGecu T48 or TL866II Plus, device ATF22V10C. An Arduino Uno or Nano running Afterburner burns these two as well. |
| AT27C256R-45, PDIP-28 | 1 | `firmware/a/at27c256r_kit.bin` (64 bytes, packed R3G3B2) | The same T48 or TL866II Plus, device AT27C256R, VPP in the 12.5 V to 13 V range. Afterburner covers GAL parts only. |

One T48 or TL866II Plus covers both chips, with Xgpro. Programmer clicks are in `docs/bringup/programming-tier-abc.md`. On this bench the Arduino can run Afterburner for the two PLDs. The PROM stays on the T48 or TL866II Plus.

Pre-programmed parts are the other path. The PROM is one-time, so the blank check and the verify are the whole safety net. On the PLDs, leave the security fuse unprogrammed so a later read-back still works.

What each file does, and the full pinout, is in `docs/misc/irl_bring_up_log/firmware/a/README.md`.

## Breadboard

One 5 V source and one ground. The Arduino 5 V pin can be that source. The Si5351, both PLDs, and the analyzer ground share it.

### Clock

The Adafruit 2045 (DigiKey 1528-1206-ND) is the 5.369318 MHz dot clock on CLK0. The chip keeps that setting in volatile registers, so the write happens again after every power-up. I2C address is 0x60.

| Breakout | Goes to |
| --- | --- |
| VIN | Arduino 5 V, and the breadboard 5 V rail |
| GND | Arduino GND, and the breadboard ground rail |
| SDA | Arduino A4 (Uno or Nano) |
| SCL | Arduino A5 (Uno or Nano) |
| CLK0 | Beam X pin 1, through 33 ohm |

CLK1 and CLK2 stay quiet. CLK0 swings to about 3.3 V. The ATF22V10 clock pin treats that level as high.

The sketch directory is `si5351_dot`, and the file inside it is `si5351_dot.ino`. `setFrequency` takes whole hertz, so 5.369318 MHz is `5369318`. Output `0` is CLK0. `begin()` writes the 10 pF crystal load this breakout uses.

```cpp
#include <Wire.h>
#include <Adafruit_SI5351.h>

Adafruit_SI5351 clk;

void setup() {
  clk.begin();
  clk.setFrequency(0, SI5351_PLL_A, 5369318UL);
  clk.enableOutputs(true);
}

void loop() {}
```

An official Uno takes this upload.

```bash
arduino-cli compile --fqbn arduino:avr:uno si5351_dot
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:uno si5351_dot
```

A Nano takes `arduino:avr:nano` and whichever `/dev/ttyUSB` node it enumerated, in place of those two arguments.

`setup()` runs when the Arduino resets. A later power cycle of the breadboard needs that reset again before Beam X has a dot clock.

### PLDs

Socket both ATF22V10C parts. Pin 24 is VCC. Pin 12 is GND. Pins 3 to 11 and pin 13 tie to GND. Pin 2 (RES) ties high so the counter runs.

Beam X pin 1 is the dot clock. Beam Y pin 1 is Beam X pin 14 (HSYNC), one rising edge per line. Beam X pin 14 is the line-sync probe. Beam Y pin 14 is the frame-sync probe.

A new equation means the PLDs come back out. After a clean verify, the PROM can stay seated. Color-index jumpers are in the firmware README.

### Logic analyzer

The pod labeled around 24M samples and 8 channels is a capture tool. It does not burn a chip. Analyzer GND ties to breadboard GND. Each channel input takes a 5 V lab signal.

| Channel | Net | A good trace |
| --- | --- | --- |
| D0 | DOT (Si5351 CLK0, or Beam X pin 1) | About 5.37 MHz |
| D1 | Beam X HSYNC (pin 14) | About 15.7 kHz, high for about 4.7 us |
| D2 | Beam Y VSYNC (pin 14) | About 60.1 Hz, high for 3 lines |
| D3 | Spare | A counter bit, or left open |

Sample rate is 24 MHz. That shows the dot clock is present. Line sync and frame sync are the clean measurements, because those edges are far apart.

In PulseView the device is `fx2lafw`, the sample rate is 24 MHz, and D0 through D3 are enabled. A 20 ms capture holds a full frame and change. The frequency and duty decoders sit on each channel.

```bash
sigrok-cli -d fx2lafw --config samplerate=24M --channels D0,D1,D2,D3 --time 20ms -o tier-a.sr
```

`tier-a.sr` opens in PulseView.

### CH340

Tier C is the first time this lead programs a chip. The HiLetgo board is a USB serial port on a CH340, with a Dupont harness and a 5 V / 3.3 V jumper. The jumper sits on 5 V. The STC legend is a UART boot path for STC 8051 parts. Retr01 has no STC chip, so that extra pin stays open.

A resistor from 1 k ohm to 4.7 k ohm ties adapter TX to adapter RX. The wire to the AVR leaves from the RX side of that resistor.

| Adapter | AVR128DB28 |
| --- | --- |
| RX, on the UPDI side of the resistor | Pin 19 UPDI |
| GND | Breadboard GND, and pins 15 and 21 |
| 5 V | Pins 6 (VDDIO2), 14 (AVDD), and 20 (VDD) |

The chip takes 5 V from one source. That source is either the adapter 5 V pin or the breadboard rail.

```bash
pymcuprog write -d avr128db28 -t uart -u /dev/ttyUSB0 -f firmware.hex
```

DxCore in the Arduino IDE uses the same port, board AVR128DB28, programmer SerialUPDI. The click path is in `docs/bringup/programming-tier-abc.md`.

Tier B is a third ATF22V10 on this same parallel programmer.
