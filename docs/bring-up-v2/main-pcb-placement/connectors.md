# Main PCB: connectors

Board-edge and cart I/O. Zones and sequence: [README.md](README.md). Series resistors on cart and pads: [resistors.md](resistors.md).

---

## J1, DC barrel (rear, top-left)

| Pin | Destination |
|-----|-------------|
| Center (+) | `+5V` rail, then E1(+) |
| Sleeve / shunt | `GND` |

Bulk **E1** sits next to J1 ([capacitors.md](capacitors.md)).

---

## J36, cart edge 2x18 (board center, under CPU)

View into the socket: **A** is one face, **B** is the opposite face.

| Edge | Net | Source |
|------|-----|--------|
| A1, B1, A18 | `GND` | Plane |
| A2, B2 | `+5V` | Rail |
| A3 | `SDA` | UM TWI (33 ohm series + 4.7k pull-up) |
| B3 | `SCL` | UM TWI (33 ohm series + 4.7k pull-up) |
| A4-A17 | `A0`-`A13` | CPU `A0`-`A13` |
| B4-B11 | `D0`-`D7` | CPU `D0`-`D7` (33 ohm series) |
| B12 | `OE#` | Compositor / MAP decode (33 ohm series) |
| B13-B17 | `A14`-`A18` | Compositor MAP (UPLDV) |
| B18 | `WE#` | Flash path / pull-up idle (33 ohm series) |

Cart series **33 ohm** sit in line between the motherboard buses and J36: **R14-R21** on `D0`-`D7`, **R22** on `OE#`, **R23** on `WE#`, **R24** on `SDA`, **R25** on `SCL`.

---

## J2, RGBS header 2x4 (rear, top-right), Zone 2

| Pin | Net |
|-----|-----|
| 1 | Red (DAC) |
| 2 | Green (DAC) |
| 3 | Blue (DAC) |
| 4 | CSYNC |
| 5 | HSYNC |
| 6 | VSYNC |
| 7 | `GND` |
| 8 | `GND` |

Layout (top view, pin 1 at top-left):

```text
1 (R)       2 (G)
3 (B)       4 (CSYNC)
5 (HSYNC)   6 (VSYNC)
7 (GND)     8 (GND)
```

DAC guns are [resistors.md](resistors.md) **R1-R11**. Encoder is [ics.md](ics.md) **U725**.

---

## J8, audio RCA (rear), driven from Zone 5

| Pin | Destination |
|-----|-------------|
| Center | US2 `AUDIO_PWM` (PF1) |
| Shell | `GND` |

---

## J9, composite RCA (rear), Zone 2

| Pin | Destination |
|-----|-------------|
| Center | U725 `COMP` |
| Shell | `GND` |

---

## J3 / J4, TRS pads (front, bottom-left), Zone 5

| Contact | Net |
|---------|-----|
| Tip (T) | `+5V` |
| Ring (R) | `PAD_DATA` (US2 PF0, open-drain) plus **R26** 4.7k pull-up |
| Sleeve (S) | `GND` |

---

## J5, arcade 2x10 (bottom-left), Zone 5

| Pins | Destination |
|------|-------------|
| Odd 1-15 | P1 bits to US2 `PA0`-`PA7` |
| Even 2-16 | P2 bits to US2 `PC0`-`PC3`, `PD1`-`PD3`, `PF6` |
| 17-20 | `GND` |

Cabinet microswitch series **47 ohm** lives on the harness ([`hardware.md`](../../general/hardware.md), [`passive_bom.md`](../../passive_bom.md)).

---

## J7, cab power/reset 2x2 (near J5)

Cab harness brings `+5V`, `GND`, and reset.

| Pin | Net |
|-----|-----|
| 1 | `+5V` |
| 2 | `GND` |
| 3 | `RESET_N` / RESB path |
| 4 | `GND` |

Layout (top view, pin 1 at top-left):

```text
1 (+5V)   2 (GND)
3 (RESB)  4 (GND)
```

U130 holds the supervisor path on RESB ([ics.md](ics.md)).

---

## J10, cart program header 2x2 (near UM)

Adafruit's UPDI Friend clips onto this header. Cart flash only. Pin numbers follow [`hardware.md`](../../general/hardware.md).

| Pin | Net |
|-----|-----|
| 1 | PWR, **NC** (Friend 5 V does not feed the rail) |
| 2 | `GND` |
| 3 | DATA, through **SW10** pos 1 to UM **PC1** |
| 4 | `GND` |

Layout (top view, pin 1 at top-left):

```text
1 (PWR NC)  2 (GND)
3 (DATA)    4 (GND)
```

**SW10** is a 2-pos DIP next to J10. Default both OFF. Pos 1 series-connects DATA. Pos 2 pulls **CART_ARM** high into the compositor. Silkscreen `CART DATA / ARM` and `ALL OFF = SAFE`.
