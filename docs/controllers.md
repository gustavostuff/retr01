# Retr01 Controllers

Input paths for the **shared motherboard** (arcade **Retr01-A** and console **Retr01-C**). Software contract is always **`$FE60`** (P1) and **`$FE61`** (P2): bit set = pressed. Layout: [`graphics.md`](graphics.md#graphics-fexx-ports).

**Related:** [`hardware.md`](hardware.md) (pads section, [runners vs silicon](hardware.md#runners-today-vs-silicon-target)). TRS jack passives: [`passive_rf_etc.md`](passive_rf_etc.md). Light gun extension (roadmap): [`lightgun.md`](lightgun.md).

---

## Software bitfield (all paths)

Same mask for P1 and P2:

| Bit | Button |
|-----|--------|
| 7 | Start |
| 6 | Coin |
| 5 | Y |
| 4 | X |
| 3 | Up |
| 2 | Down |
| 1 | Left |
| 0 | Right |

**1 = pressed**, **0 = released**.

The **ATmega1284P** owns pad sampling and presents bytes at `$FE60` / `$FE61`. The W65C02S only reads those ports.

---

## Retr01-A: Arcade (direct GPIO)

Sticks and buttons are **microswitch-to-GND** (or common) circuits wired into **1284** GPIO via headers / IDC.

| Item | Spec |
|------|------|
| MCU | ATmega1284P reads switches directly |
| Harness | Series **22-100 ohm** per line. Optional TVS at connector |
| TRS jacks | May be **DNP** on pure cabinet builds |

No serial protocol on this path. Debouncing is firmware policy on the 1284.

---

## Retr01-C: Aux pads (3-wire + ATtiny85)

Console builds populate **2x Switchcraft 35RAPC** TRS jacks (P1, P2). Optional **pad boards** use an **ATtiny85** and a **male-male 3.5 mm aux** cable (no proprietary tether).

### Cable (3 conductors)

| Conductor | Signal |
|-----------|--------|
| Tip | **5 V** |
| Ring | **DATA** |
| Sleeve | **GND** (shell tied to ground) |

Exact tip/ring assignment is fixed at schematic time. Table above is the locked mapping. Protection (PPTC on VCC, TVS, series R on DATA): [`passive_rf_etc.md`](passive_rf_etc.md).

### Electrical: open-drain UART bus

| Item | Spec |
|------|------|
| Pull-up | **4.7 kohm** on **DATA** at the **motherboard** (1284 side) |
| Host | ATmega1284P UART TX/RX tied together, **open-drain** |
| Pad | ATtiny85 UART TX/RX tied together, **open-drain** |
| Topology | Half-duplex **1-wire UART** on the DATA line |

Both ends release high (idle). Either side pulls low to transmit.

### Protocol

| Parameter | Value |
|-----------|-------|
| Baud | **115200** 8N1 |
| Frame budget | **< 200 us** per exchange (fits in **VBlank**) |
| Poll (host -> pad) | Host sends **1 byte**: **`0x55`** = P1 poll, **`0xAA`** = P2 poll (second port, exact P2 byte lock with schematic) |
| Reply (pad -> host) | Pad samples buttons, then sends **1 byte** state using the bitfield table above |

**Per-frame sequence (once per vertical blank):**

1. 1284 asserts open-drain TX and sends poll byte.
2. Target ATtiny receives poll. If addressed, drives reply byte on DATA.
3. 1284 captures reply into shadow for `$FE60` or `$FE61`.

Pads that miss a poll hold last state until the next good frame.

### Pad MCU

| Item | Spec |
|------|------|
| Part | **ATtiny85** (pad PCB) |
| Power | **5 V** from Tip (through port PPTC on mobo) |
| Buttons | Active-low into GPIO. Firmware packs the bitfield |

### Bring-up status

| Layer | Status |
|-------|--------|
| Silicon / docs | Protocol locked (this file) |
| Sim / emu | Host Play drives `$FE60` / `$FE61` directly. **No** TRS or UART netlist yet |
| Firmware | 1284 pad firmware + ATtiny85 pad sketch. Post-schematic |

---

## Open topics (controllers)

| Topic | Note |
|-------|------|
| P2 poll byte | Confirm `0xAA` vs alternate at 1284 firmware bring-up |
| Debounce | Pad-side vs 1284-side threshold |
| Arcade header pinout | Lock P1/P2 pin order at schematic |
| Light gun | Identify **`0x02`**, timer read **`0x5A`**. See [`lightgun.md`](lightgun.md) |
