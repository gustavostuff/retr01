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
| B4-B11 | `D0`-`D7` | CPU `D0`-`D7` |
| B12 | `OE#` | Compositor / MAP decode (33 ohm series) |
| B13-B17 | `A14`-`A18` | Compositor MAP (UPLDV) |
| B18 | `WE#` | Flash path / pull-up idle (33 ohm series) |

Cart series **33 ohm** sit in line between the motherboard buses and J36: **R14** on `OE#`, **R15** on `WE#`, **R16** on `SDA`, **R17** on `SCL`. Cart **D0-D7** have no series parts.

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

## J3 / J4 and J11 / J12, TRS pads (front, bottom-left), Zone 5

3.5 mm jacks are **J3** (P1) and **J4** (P2). The 3.5 mm standard is Switchcraft **35RAPC4BV4**. 6.35 mm (1/4") jacks are **J11** (P1) and **J12** (P2). The 6.35 mm standard is **CLIFF S4** (typical **S4/BMB/PC-C**). Same Tip / Ring / Sleeve nets on both sizes. One size per player.

| Contact | Net |
|---------|-----|
| Tip (T) | `+5V` |
| Ring (R) | `PAD_DATA` (US2 PF0, open-drain) plus **R18** 4.7k pull-up |
| Sleeve (S) | `GND` |

---

## J5 / J6, arcade 2x5 (around US2), Zone 5

J5 is Player 1, right of US2, rotated 180 deg. J6 is Player 2, left of US2, rotated 0 deg. Even column faces the DIP.

| Pins | J5 (P1) | J6 (P2) |
|------|---------|---------|
| Even 2,4,6,8,10 | `PA0`-`PA4` (pin 2 = bit 0) | `PC0`-`PC3`, `PD1` (pin 2 = bit 0) |
| Odd 1,3,5 | `PA5`-`PA7` | `PD2`, `PD3`, `PF6` |
| Odd 7,9 | `GND` | `GND` |

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
| 3 | DATA, UM **PC1** (`SPI_MISO` / USART1) |
| 4 | `GND` |

Layout (top view, pin 1 at top-left):

```text
1 (PWR NC)  2 (GND)
3 (DATA)    4 (GND)
```

**CART_ARM** is compositor MAP latch D7, not a header pin. Silkscreen `CART` at J10.
