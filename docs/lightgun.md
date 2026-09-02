# Retr01 Light Gun

Potential **Retr01-C** accessory: a CRT light gun on the same **3-wire TRS** bus as aux gamepads ([`controllers.md`](controllers.md)). The gun carries its own **ATtiny85**, photodiode front end, and a **buffered 16-bit timer** so the console does not need per-target flashing frames.

**Related:** [`controllers.md`](controllers.md) (TRS cable, UART poll, button bitfield). [`graphics.md`](graphics.md) (`$FExx` map). [`hardware.md`](hardware.md) ([runners vs silicon](hardware.md#runners-today-vs-silicon-target)). [`passive_rf_etc.md`](passive_rf_etc.md) (TRS protection).

**Status:** **Roadmap / design spec only.** No Emu, Sim, or 1284 pad firmware implements this path today.

---

## Why a buffered timer

Classic NES-style Zapper games often need **one blanking frame plus one white-box frame per on-screen target** to learn what was aimed at. Many enemies means visible flicker, extra lag, and tight coupling between sprite count and input latency.

Retr01 targets a **fixed short sequence** instead:

| Approach | Frames to resolve aim (typical) |
|----------|----------------------------------|
| NES-style per-target flash | **2 + N** (N = candidate targets) |
| Retr01 buffered timer (this spec) | **2** flash frames + **1** result read (independent of target count) |

The gun measures **when** the CRT beam lit the photodiode (microsecond timer after a VBlank-locked sync). The game flashes **all** hitboxes at once on one frame. One timer sample still identifies the scanline (and coarse X) of the hit.

---

## Gun hardware

### Optical front end

| Block | Part / role |
|-------|-------------|
| Sensor | Photodiode aimed through the barrel |
| Condition | **LM393** (or similar) comparator. Brightness spike -> digital edge |
| MCU | **ATtiny85** @ **8 MHz** (same family as the pad board) |
| Power | **5 V** from TRS Tip (through mobo PPTC, same as pads) |

Shell, trigger, and side buttons are mechanical. The electronics fit a **DIY resin-printed** or molded grip. No extra pins beyond the standard **3-wire** port.

### Timer math (NTSC-class 15 kHz CRT)

| Quantity | Typical value |
|----------|----------------|
| Horizontal period | **~63.5 us** per scanline |
| Frame time | **~16.6 ms** top to bottom |

**ATtiny85 timer:** hardware timer prescaler **8** at **8 MHz** -> **1 us** per tick. Frame-length counts need **~16 600** ticks, so firmware uses an **8-bit timer plus overflow ISR** to build a **16-bit** free-running counter.

| Axis | Resolution (design intent) |
|------|---------------------------|
| **Y** (scanline) | **~1 us** ticks over the frame. Divide elapsed time by **~63.5** to map to a scanline index |
| **X** (within line) | **~63** distinct ticks across one scanline (**256** logical pixels / **~63** ticks -> **~4** pixels per tick). Enough for gameplay hitboxes on a **128x120** field |

---

## Cable and bus

Same **Retr01-C** aux port as gamepads ([`controllers.md`](controllers.md#retr01-c-aux-pads-3-wire--attiny85)):

| Conductor | Signal |
|-----------|--------|
| Tip | **5 V** |
| Ring | **DATA** (half-duplex open-drain UART) |
| Sleeve | **GND** |

**4.7 kohm** pull-up on **DATA** at the motherboard. Gun and pad MCUs use **open-drain** UART TX/RX tied together.

**Coexistence:** Port 1 can host a **gun** while port 2 stays a **standard pad** (or vice versa). Discovery distinguishes device type. Poll timing stays inside **VBlank** (**< 200 us** per exchange, same budget as pads).

---

## Serial protocol (extension)

Base link: **115200** 8N1 on the **DATA** line. Poll and reply bytes below extend the pad protocol in [`controllers.md`](controllers.md#protocol).

### Device discovery

On boot (or hot-plug), the **ATmega1284P** host sends **`0xFF`** (identify).

| Device | Reply byte |
|--------|------------|
| Standard gamepad | **`0x01`** |
| Light gun | **`0x02`** |

### Standard poll (every VBlank)

Host sends the normal port poll byte (**`0x55`** for P1, P2 byte TBD at schematic lock).

1. Gun UART RX ISR fires on the poll byte. The **stop bit edge** **resets** the 16-bit beam timer to **0** (VBlank sync lock).
2. Gun replies with **one byte** button state (gun layout, not identical to the stick bit names):

| Bit | Control |
|-----|---------|
| 7 | Start |
| 6 | Coin |
| 5 | **Trigger** |
| 4 | Aux (side button) |
| 3 | Up |
| 2 | Down |
| 1 | Left |
| 0 | Right |

D-pad bits can map to extra shell buttons. Firmware debounce policy matches pads (open topic).

### Timer read (after a hit)

When the host sees **Trigger** pressed, the game runs the flash sequence (below). On the **next** poll after the flash frame, the host requests stored timer data:

| Direction | Byte |
|-----------|------|
| Host -> gun | **`0x5A`** (gun data request) |
| Gun -> host | **Timer high**, **Timer low** (16-bit big-endian style on the wire) |

If the photodiode never saw the CRT (**aimed away**, room light only), the gun returns **`0xFFFF`** (miss).

Comparator fire **freezes** the running timer into a holding register at the hit instant. The host forwards that value to the 6502 during VBlank.

---

## CPU I/O (proposed)

**Not implemented in runners today.** `$FE30`-`$FE38` are already **world / bank helpers** ([`graphics.md`](graphics.md)). Proposed gun timer readback (1284 shadow, 6502 read):

| Addr | Name | Role |
|------|------|------|
| `$FE80` | `GUN_HI` | Beam timer bits **15:8** (last completed shot) |
| `$FE81` | `GUN_LO` | Beam timer bits **7:0** |

`$FE80` is marked unused in the current port map. Lock or move at schematic + PRG header freeze.

**6502 math (sketch):** `scanline = timer / 63` (integer). Remainder maps to coarse X. Game code matches **(X, Y)** against enemy hitboxes for that frame. No per-enemy serial flash loop.

---

## Recommended 2-frame game sequence

From the game programmer's view (frames are numbered from trigger commit):

| Frame | Video | Bus / CPU |
|-------|-------|-----------|
| **0** | Normal play | Read `$FE60` / `$FE61`. Trigger goes high |
| **1** | **Black** screen (anti-spoof) | VBlank poll resets gun timer |
| **2** | **White hitboxes** on **all** targets at once | Photodiode hit **latches** timer in the gun |
| **3** | Resume normal draw | Host sends **`0x5A`**, fills `$FE80`/`$FE81`. Game resolves hit from timer |

Blackout rejects false triggers from room lights. The simultaneous flash is what removes the **N-target** penalty.

---

## Bring-up status

| Layer | Status |
|-------|--------|
| This document | Design target |
| 1284 pad firmware | Standard pads only ([`controllers.md`](controllers.md)) |
| ATtiny85 gun sketch | Not started |
| `$FE80` / `$FE81` in Emu / Sim | Not allocated |
| Sim TRS / UART netlist | Not modeled ([`controllers.md`](controllers.md#bring-up-status)) |

---

## Open topics (light gun)

| Topic | Note |
|-------|------|
| Product name | **Retr01-U** vs generic accessory SKU |
| `$FE80` / `$FE81` | Confirm vs other future `$FE80` uses |
| P2 poll byte | Same **`0xAA`** convention as pads |
| Hitbox coordinate space | Logical **128x120** vs beam counters |
| LCD / flat panel | Out of scope. CRT timing assumed |
| Emu / Sim | Host Play could fake timer bytes before silicon |
