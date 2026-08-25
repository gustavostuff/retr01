# Retr01 Audio Architecture

Concise software + hardware picture for the dedicated APU path. Channel map and bytecode protocol follow the audio design brief; silicon placement matches the **32-IC BOM** ([`06`](06_hardware_v1_32ic.md)). Register window: **`$FE40`–`$FE5F`** ([`02`](02_graphics_worlds_memory.md)).

## Big picture

Retr01 has **no fixed-function audio ASIC**. Roles split like the rest of the board:

| Role | Who |
|------|-----|
| Sequencer / tracker | **W65C02S** (game PRG), driven by **60 Hz NMI** |
| Synthesizer / mixer | **ATmega328P** @ 16 MHz (island **K**) |
| Analog out | 328P digital mix → **R-2R** (or PWM/RC — board analog TBD) |

The 6502 streams **compressed bytecode** from cart ROM into the APU window. The 328P mixes **8 channels** in software and drives the DAC. The **ATmega1284P** does **not** synthesize audio (sprites, pads, machine EEPROM only).

```text
  Cart PRG / track data
        │
        ▼
  W65C02S  ──NMI 60 Hz──►  decompress / delay / bitmask
        │
        │  STA $FE4x…          The Bus Bridge (below)
        ▼
  ATmega328P APU  ──mix 8 ch──►  R-2R / amp
```

## The Bus Bridge

On Retr01, **“the Bus Bridge”** is not a separate named chip. It is the **CPU → APU write path** that lets an 8 MHz 6502 hand bytes to a 16 MHz AVR without sharing a raw, always-on data bus:

1. **Decode** — PLD (and `$FExx` latches where needed) maps CPU accesses in **`$FE40`–`$FE5F`** to the APU window.
2. **Isolation** — the **CPU-domain 74HC245** (one of three board HC245s) plus PHI2 / `/OE` gating so only one master drives that domain at a time ([`06`](06_hardware_v1_32ic.md) bus split).
3. **Capture** — the 328P sees register-like writes on its port (or a latch clocked on the write); it owns synthesis timing after that.

So: game code does ordinary `STA $FE4x`. The bridge is **decode + bus isolation + APU-side latch/port**, same pattern as other `$FExx` peripherals. Exact GPIO pinout is schematic TBD; the **software contract** is the 32-byte window.

## Eight channels

Mixing is software on the 328P. Channels are split so SFX never steal BGM notes.

**BGM (1–5)** — tracker-style:

| Ch | Wave | Typical use |
|----|------|-------------|
| 1 | Pulse | Lead (duty adjustable) |
| 2 | Pulse | Harmony / arps |
| 3 | Triangle | Bass (no volume envelope — avoids clicks) |
| 4 | Noise | Hats / snares |
| 5 | DPCM | Kick, voice, hits (1-bit delta) |

**SFX (6–8)** — pulse/noise by default; game can trigger freely without muting 1–5.

## DPCM (channel 5)

1-bit delta modulation: 328P keeps a **7-bit** accumulator; each sample bit ±2; result goes into the master mix (e.g. ~16–33 kHz).

The 6502 is **too slow** to stream raw sample bytes at that rate. **Sample payloads live in 328P Flash (PROGMEM).** PRG only sends a **1-byte trigger** (sample ID). The AVR looks up address/length and plays autonomously.

## Note byte (semantic hex)

One byte = one pitch (readable in a hex dump):

| Field | Bits | Meaning |
|-------|------|---------|
| Letter | 7–4 | `0`=G, `A`=A … `F`=F |
| Octave / flat | 3–0 | `0`–`7` = natural octave; `8`–`F` = flat, octave = value−8 |

Example: `C4` = C natural octave 4; `CC` = C-flat octave 4.  
Decode: `is_flat = (b >> 3) & 1`, `octave = b & 7`.

High nibbles **`1`–`9`**, **`E`**, **`F`** are **not** notes — reserved for control codes.

## Control codes

**Sent to the 328P (with channel data):**

| Code | Meaning |
|------|---------|
| `8X` | Volume 0–15 |
| `9X` | Duty / noise type |
| `7X` | DPCM trigger, sample ID `X` (ch 5) |

**Handled on the 6502 (sequencer):**

| Code | Meaning |
|------|---------|
| `FD` | Next byte = **channel bitmask** (bit0=ch1 … bit7=ch8); then that many note/command bytes → push mask + payload toward `$FE4x` |
| `FE` | Next byte = delay in **frames** |
| `FB` | Stop / play-once |
| `FA` | Loop track (reset read pointer) |

## 60 Hz NMI dual tracker

Each NMI:

1. **BGM** — if delay expired, read stream; on `FD`, pull mask + bytes and write through the bus bridge; update pointer / delays / loop.
2. **SFX** — same idea on channels 6–8; inactive SFX exits early; `FB` clears the SFX slot.

Main game logic runs between NMIs; audio work stays bounded per frame.

## Status vs sim

| Layer | Today |
|-------|--------|
| Design (this doc) | 8-ch mixer, DPCM-in-AVR-flash, bytecode protocol |
| HW BOM | 328P + `$FE40`–`$FE5F` + CPU HC245 domain |
| Board sim | Island **K** stub: period/vol **PWM square** smoke — not full mixer/DPCM yet ([`08`](08_simulator.md)) |

## Related

- Memory map / `$FExx`: [`02`](02_graphics_worlds_memory.md)
- 32-IC BOM / APU path: [`06`](06_hardware_v1_32ic.md)
- 328P chip notes: [`hw/md/ATmega328P.md`](../hw/md/ATmega328P.md)
- Overview index: [`01`](01_architecture_overview.md)
