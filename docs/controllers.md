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

Sticks and buttons are **microswitch-to-GND** circuits into the **ATmega1284P**. Each switch closes its signal pin to the header **GND**.

| Item | Spec |
|------|------|
| MCU | ATmega1284P samples GPIO and packs `$FE60` / `$FE61` |
| Harness | Series **47 ohm** per signal (in SKiDL). TVS optional at layout |
| TRS jacks | May be **DNP** on pure cabinet builds |

No serial protocol on this path. Debouncing is firmware policy on the 1284.

### Locked headers (schematic freeze)

Three connectors on the motherboard:

| Refdes | Part | Role |
|--------|------|------|
| **J5** | **1×10** pin header, 2.54 mm | Player 1 controls |
| **J6** | **1×10** pin header, 2.54 mm | Player 2 controls |
| **J7** | **1×4** pin header, 2.54 mm | Cabinet **+5 V / GND / RESET** |

Pin 1 is marked on the silkscreen (square pad). Headers face the cabinet harness edge.

#### J5 — Player 1 (1×10)

Pin order matches the `$FE60` bitfield (**LSB = pin 1**):

| Pin | Net | `$FE60` bit | Switch |
|-----|-----|-------------|--------|
| 1 | `P1_RIGHT` | 0 | Right |
| 2 | `P1_LEFT` | 1 | Left |
| 3 | `P1_DOWN` | 2 | Down |
| 4 | `P1_UP` | 3 | Up |
| 5 | `P1_X` | 4 | X |
| 6 | `P1_Y` | 5 | Y |
| 7 | `P1_COIN` | 6 | Coin |
| 8 | `P1_START` | 7 | Start |
| 9 | `GND` | — | Switch common |
| 10 | `GND` | — | Switch common (spare) |

#### J6 — Player 2 (1×10)

Same physical order as J5 for `$FE61`:

| Pin | Net | `$FE61` bit | Switch |
|-----|-----|-------------|--------|
| 1 | `P2_RIGHT` | 0 | Right |
| 2 | `P2_LEFT` | 1 | Left |
| 3 | `P2_DOWN` | 2 | Down |
| 4 | `P2_UP` | 3 | Up |
| 5 | `P2_X` | 4 | X |
| 6 | `P2_Y` | 5 | Y |
| 7 | `P2_COIN` | 6 | Coin |
| 8 | `P2_START` | 7 | Start |
| 9 | `GND` | — | Switch common |
| 10 | `GND` | — | Switch common (spare) |

#### J7 — Power / reset (1×4)

Shared cabinet feed for lamps / coin door / front-panel reset. **Does not** replace the barrel jack (J1); it is a harness tap from the post-ferrite **+5 V** rail.

| Pin | Net | Role |
|-----|-----|------|
| 1 | `+5V` | Fused board 5 V out to cabinet |
| 2 | `GND` | Ground |
| 3 | `RESET_N` | Active-low reset (momentary to GND). Ties into HC14 / `RESB` reset tree |
| 4 | `GND` | Ground (return / keying spare) |

**Why 10 + 10 + 4:** eight bitfield lines need eight pins; dual GND on each player header simplifies harness commons without a third connector; power and reset stay off the control ribbons so a shorted stick wire cannot backfeed the rail through a signal pin.

---

## Retr01-C: Aux pads (3-wire + ATtiny85)

Console builds populate **2x Switchcraft 35RAPC2BVN4** TRS jacks (P1, P2; vertical). Optional **pad boards** use an **ATtiny85** and a **male-male 3.5 mm aux** cable (no proprietary tether).

### Cable (3 conductors)

| Conductor | Signal |
|-----------|--------|
| Tip | **5 V** (jack pad **4**) |
| Ring | **DATA** (jack pad **2**) |
| Sleeve | **GND** (jack pad **1**; shell) |

Pads **3** and **5** are plated for mechanical hold on the VN4 footprint; they are **NC** on **35RAPC2BVN4** (no internal switches). Exact tip/ring assignment is fixed at schematic time. Protection (PPTC on VCC, TVS, series R on DATA): [`passive_rf_etc.md`](passive_rf_etc.md).

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
| Poll (host -> pad) | Host sends **1 byte**: **`0x55`** = P1 poll, **`0xAA`** = P2 poll (**locked**) |
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
| Sim / emu | **Arcade** (default): Host Play drives `$FE60`/`$FE61` directly. **Pads**: Sim ATtiny85 entities + poll/reply into those ports (HUD ARCADE/PADS toggle). No TRS netlist / 1284 USART yet |
| Firmware | 1284 pad firmware + ATtiny85 pad sketch. Post-schematic |

---

## Open topics (controllers)

| Topic | Note |
|-------|------|
| Debounce | Pad-side vs 1284-side threshold |
| Light gun | Identify **`0x02`**, timer read **`0x5A`**. See [`lightgun.md`](lightgun.md) |
| Arcade series-R / TVS footprints | **47 Ω** in SKiDL; optional TVS at layout |
