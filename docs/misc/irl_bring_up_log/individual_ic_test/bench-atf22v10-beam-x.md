# ATF22V10 Beam X

The socket holds `beam_x.jed`. The package is 300 mil, so it sits across the breadboard center. The notch is pin 1. Power is off while the chip goes in or comes out.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| ATF22V10C, PDIP-24, `beam_x.jed` | 1 | Dot counter and HSYNC |
| 74HCU04, PDIP-14 | 1 | Pierce oscillator and buffer |
| 74HC74, PDIP-14 | 1 | Divides 21.47727 MHz by 4 |
| 21.47727 MHz crystal, HC-49 | 1 | Master clock |
| 1 Mohm | 1 | Pierce feedback |
| 18 pF | 2 | Crystal load |
| 100 nF | 3 | One across each IC |
| 33 ohm | 1 | Series resistor on the dot clock |
| LED, red | 10 | Nine count bits and HSYNC |
| 2.2 kohm | 10 | LED series resistor |
| 1 kohm | 1 | RTS series resistor for the count row |

The oscillator chip is a 74HCU04. A 74HC04 is a different part. The same stage is the console dot clock.

## Clock

The crystal, the 1 Mohm resistor, the two 18 pF capacitors, and 74HCU04 pins 1 and 2 sit on adjacent rows. Leads on that loop stay short. The crystal has no polarity. 18 pF is the starting load. 15 pF is the next value when 74HCU04 pin 4 stays quiet.

| What | Where |
| --- | --- |
| 74HCU04 pin 14 | +5 V |
| 74HCU04 pin 7 | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 14 |
| 74HCU04 pins 5, 9, 11, and 13 | Ground |
| 74HCU04 pins 6, 8, 10, and 12 | Open |
| Crystal | 74HCU04 pin 1 and pin 2 |
| 1 Mohm | 74HCU04 pin 1 to pin 2 |
| 18 pF | 74HCU04 pin 1 to ground |
| 18 pF | 74HCU04 pin 2 to ground |
| 74HCU04 pin 2 | 74HCU04 pin 3 |
| 74HCU04 pin 4 | 74HC74 pin 3 |
| 74HC74 pin 14 | +5 V |
| 74HC74 pin 7 | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 14 |
| 74HC74 pins 1, 4, 10, and 13 | +5 V |
| 74HC74 pin 6 | 74HC74 pin 2 |
| 74HC74 pin 5 | 74HC74 pin 11 |
| 74HC74 pin 8 | 74HC74 pin 12 |
| 74HC74 pin 9 | One end of the 33 ohm resistor |

74HC74 pin 9 is the 5.369318 MHz dot clock.

## Place

| What | Where |
| --- | --- |
| Pin 24 (VCC) | +5 V |
| Pin 12 (GND) | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 24 |
| Pins 3 through 11, and pin 13 | Ground |
| Pin 2 (RES) | +5 V |
| Pin 1 (CLK) | 74HC74 pin 9, through 33 ohm. Off during the step check |
| Pins 22, 18, 19, 20, 17, 15, 16, 23, 21 | Q8 through Q0, left to right. Pin, then 2.2 kohm, then the LED anode. Cathode to ground |
| Pin 14 | HSYNC. Same LED wiring. This lamp is not a count bit |

A lit LED is a 1. A dark LED is a 0. The nine count lamps read as one binary word, Q8 on the left.

Count runs 0 through 340, then returns to 0. HSYNC is high on counts 264 through 288. The color index is not on this chip. Q pins are the count.

| Pin | Name |
| --- | --- |
| 14 | HSYNC |
| 15 | Q3 |
| 16 | Q2 |
| 17 | Q4 |
| 18 | Q7 |
| 19 | Q6 |
| 20 | Q5 |
| 21 | Q0 (least significant bit) |
| 22 | Q8 (most significant bit) |
| 23 | Q1 |

## Step

The 33 ohm resistor stays off pin 1. DTR is the only clock. A mechanical switch on pin 1 bounces, so one click is many counts.

The HiLetgo CH340 jumper sits on 5 V. Adapter ground ties to breadboard ground. The adapter 5 V pin stays open while the breadboard rail is powered. DTR ties to pin 1. On this adapter DTR rests low while `dtr` is true.

`python-pyserial` provides `serial`. The port is `/dev/ttyUSB0`.

```python
import time
import serial

ser = serial.Serial("/dev/ttyUSB0", 9600)
ser.dtr = True
time.sleep(0.05)

def steps(n):
    for _ in range(n):
        ser.dtr = False
        time.sleep(0.001)
        ser.dtr = True
        time.sleep(0.001)

input("reset, then enter")
while True:
    line = input("steps: ")
    if line.strip():
        steps(int(line))
```

The number on the `steps:` line is how many rising edges follow. The count advances on each rising edge.

Ground pin 2, then return pin 2 to +5 V, before the first edge. That is count 0. Every count LED is dark. The HSYNC LED is dark.

| Steps from 0 | Q8 to Q0 | HSYNC |
| --- | --- | --- |
| 0 | 000000000 | dark |
| 1 | 000000001 | dark |
| 2 | 000000010 | dark |
| 3 | 000000011 | dark |
| 4 | 000000100 | dark |
| 264 | 100001000 | lit |
| 289 | 100100001 | dark |
| 341 | 000000000 | dark |

Q0 changes on every edge. Enter 1 for one count. Enter 264 from count 0 to light HSYNC. HSYNC stays lit through count 288. Enter 25 more to land on 289, and HSYNC goes dark. Enter 52 more to land on 0.

## Count

[`apps/flasher_tool/verify.py`](../../../../apps/flasher_tool/verify.py) adds the row `Beam X count`. The row stays at `no programmer` until a CH340 port is present. Enter on that row does not talk to the programmer socket.

| Adapter | Beam X |
| --- | --- |
| GND | Breadboard ground |
| DTR | Pin 1 |
| RTS | Pin 2, through 1 kohm |
| CTS | Pin 14 |

Pin 2 stays off the +5 V rail for this row. The 33 ohm resistor stays off pin 1. The adapter 5 V pin stays open. The jumper sits on 5 V.

Enter drives RTS low, which clears the count, then clocks DTR. A pass prints `count ok`. HSYNC is low at count 0, high at 264, low at 289, and low again at 0. CTS is the only pin the lead reads. The lamps still show the other bits.

## Check

Pin 1 returns to 74HC74 pin 9 through the 33 ohm resistor. DTR stays off pin 1.

Analyzer ground ties to breadboard ground. D0 is Beam X pin 1. D1 is Beam X pin 14. D2 is Beam X pin 21. Sample rate is 24 MHz.

| Probe | A good trace |
| --- | --- |
| 74HCU04 pin 4 | About 21.48 MHz |
| 74HC74 pin 5 | About 10.74 MHz |
| 74HC74 pin 9, and Beam X pin 1 | About 5.37 MHz |
| Beam X pin 14 | About 15.7 kHz, high for about 4.7 us |
| Beam X pin 21 | About 2.68 MHz across one line |

RES low clears the counter and holds it there. Beam X pin 14 stays low while pin 2 is low, because count 0 is outside the sync window. Pin 2 returns high before the next check.

The full pin story is in [`../firmware/a/README.md`](../firmware/a/README.md).
