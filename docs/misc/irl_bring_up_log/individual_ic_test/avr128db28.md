# AVR128DB28 MCU-S1

Used in Tier C.

The socket holds the Tier C field-fill image. This page is MCU-S1 only. MCU-M and MCU-S2 are later tiers.

## Power

| Pin | Tie |
| --- | --- |
| 6 VDDIO2, 14 AVDD, 20 VDD | 5 V, one source |
| 15 and 21 | GND |

Pin 19 stays the UPDI pin. The CH340 lead from the flashing page is off the pin during this check.

Field pins, from [`docs/general/hardware.md`](../../../general/hardware.md):

| Net | Pin |
| --- | --- |
| AD0 to AD7 | 22, 23, 24, 25, 26, 27, 28, 1 |
| A8 to A14 | 2, 3, 4, 5, 7, 8, 9 |
| ALE | 16 (PF0) |
| /WE | 17 (PF1) |
| S1_RDY | 18 (PF6) |

## Pass

The SRAM and the latch can stay off the board. A resistor from /WE to 5 V holds that net high if the pin is still an input.

After reset, and with no vertical timing pulse, ALE sits low and /WE sits high. AD pins are hi-Z. A resistor from one AD pin to ground sits low in that state.

The vertical timing wire is the input the Tier C image reads. On this bench the pulse that exists is Beam Y pin 14, high for three lines at about 60.1 Hz. While that pulse is high, ALE and /WE both toggle in a burst, and the AD pins leave hi-Z. Between pulses, ALE is low, /WE is high, and AD is hi-Z again.

/WE bursts that run for the whole frame are a failed image. The fill belongs in the vertical window. S1_RDY may rise when a fill finishes. The ALE and /WE check stands on its own.
