# AVR128DB28 MCU-S1

The socket holds the Tier C field-fill image. The package is 300 mil, so it sits across the breadboard center. The notch is pin 1. Power is off while the chip goes in or comes out. This page is MCU-S1 only. MCU-M and MCU-S2 are later tiers.

## Parts

| Part | Qty | Role |
| --- | --- | --- |
| AVR128DB28, SPDIP-28, Tier C field-fill image | 1 | MCU-S1 |
| 100 nF | 1 | Bypass from VDD to ground |
| 10 kohm | 1 | Holds /WE high while that pin is still an input |

## Place

| What | Where |
| --- | --- |
| Pins 6 (VDDIO2), 14 (AVDD), and 20 (VDD) | +5 V, one source |
| Pins 15 and 21 | Ground |
| 100 nF | +5 V rail to the ground rail, beside pin 20 |
| Pin 19 (UPDI) | Open during this check. The CH340 lead from the flashing page is off this pin |
| 10 kohm on /WE | Pin 17 to +5 V |

Field pins, from [`docs/general/hardware.md`](../../../general/hardware.md):

| Net | Pin |
| --- | --- |
| AD0 to AD7 | 22, 23, 24, 25, 26, 27, 28, 1 |
| A8 to A14 | 2, 3, 4, 5, 7, 8, 9 |
| ALE | 16 (PF0) |
| /WE | 17 (PF1) |
| S1_RDY | 18 (PF6) |

The SRAM and the latch can stay off the board.

## Check

After reset, and with no vertical timing pulse, ALE sits low and /WE sits high. AD pins are hi-Z. A resistor from one AD pin to ground sits low in that state.

The vertical timing wire is the input the Tier C image reads. On this bench the pulse that exists is Beam Y pin 14, high for three lines at about 60.1 Hz. While that pulse is high, ALE and /WE both toggle in a burst, and the AD pins leave hi-Z. Between pulses, ALE is low, /WE is high, and AD is hi-Z again.

/WE bursts that run for the whole frame are a failed image. The fill belongs in the vertical window. S1_RDY may rise when a fill finishes. The ALE and /WE check stands on its own.
