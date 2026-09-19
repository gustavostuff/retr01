# ATtiny85 behavior

Microchip **ATtiny85**. Retr01 **TRS pad** MCU (inside the controller shell). **Outside** the counted **19**.

**Package:** 8-pin **DIP** (or SOIC-8). **VCC:** 5 V from the TRS tip. Programmed with **ISP** off the console (Arduino-as-ISP or USBasp). Adafruit's UPDI Friend does **not** program this part.

**Board:** `../general_docs/hardware.md` (controllers). Host on the motherboard is **MCU-S2** (`PAD_DATA` on PF0).

---

## 1. Inputs (what pins can sense)

Logical pad-board nets (DIP-8 physical pins depend on firmware pin map):

| Net | Dir | What the net must provide |
| --- | --- | --- |
| **VCC** / **GND** | PWR | Tip = 5 V, Sleeve = GND on Switchcraft TRS |
| **DATA** | IO | Ring. Open-drain UART. Sense host polls |
| **RESET#** | IN | ISP / reset (keep recoverable) |
| **Button GPIOs** | IN | Local pad switches to GND (firmware-defined pins) |

Arcade cabinets do **not** need this chip. They wire microswitches straight into MCU-S2 GPIO headers.

### Stimulus checklist

| Stimulus | Expect |
| --- | --- |
| Host poll `0x55` (P1 pad) | Reply 1 byte button bitfield within **< 200 us** |
| Host poll `0xAA` (P2 pad) | Same for player 2 |
| Idle | DATA released (4.7 kohm pull-up on mobo PF0) |
| Unknown poll byte | No reply / ignore |

---

## 2. Process (what happens inside)

AVR tiny core with flash firmware. Pad job:

1. Debounce / sample local buttons into an 8-bit field (same bit order as `$7F60`/`$7F61`).
2. Speak **115200 8N1** open-drain half-duplex UART on DATA.
3. On matching poll, drive the reply byte then release DATA.

| Bit | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Button | Start | Coin | Y | X | Up | Down | Left | Right |

### Light-gun roadmap (same TRS bus)

Future firmware: photodiode + LM393 + 16-bit beam timer. Identify `0xFF` -> `0x02` (gun) vs pad `0x01`. Not required for baseline pads.

---

## 3. Outputs (what pins can drive)

| Net | Dir | What the chip drives |
| --- | --- | --- |
| **DATA** | IO | Open-drain UART reply. Idle = Hi-Z (pull-up high) |

### 8-pin DIP (baseline pad sim shell)

Physical GPIO assignment is firmware-owned. A minimal sim pin shell used historically:

```text
         +-----\/-----+
 RESET#  | 1        8 | VCC
   (IO)  | 2        7 | (IO)
   (IO)  | 3        6 | (IO)
   GND   | 4        5 | DATA
         +------------+
```

Wire button inputs and ISP to the pins the pad firmware actually uses. Keep **DATA** on the TRS ring.

| Logical name | Dir | Notes |
| --- | --- | --- |
| RESET# | IN | ISP |
| DATA | IO | OD UART to MCU-S2 PF0 |
| VCC / GND | PWR | From TRS |
| Button IOs | IN | Pad switches |

### Where it sits on Retr01

```text
Pad switches --> ATtiny85 --OD UART DATA--> TRS ring --> MCU-S2 PF0
MCU-S2 polls 0x55 / 0xAA each frame and mirrors into $7F60 / $7F61
```

---

## Sim notes

- Phase 1: poll/reply stub (`0x55`/`0xAA` -> button byte). Full AVR core optional.
- Register one instance per pad (`"PAD1"`, `"PAD2"`) with different poll addresses.
- Pin names at least `"DATA"`, `"VCC"`, `"GND"`, `"RESET#"`.
