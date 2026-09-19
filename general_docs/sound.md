# Sound (APU)

No fixed-function audio ASIC. The **W65C02S** sequences. **MCU-S2** mixes eight voices in software and drives **PWM**. **MCU-M** only forwards the mailbox. **MCU-S1** does not touch audio.

Silicon and pins: `hardware.md`. Soft map: `memory.md`. SPI / VBlank: `ic-comms-risks.md`. S2 chip notes: `../ic_behavior/AVR128DB28.md`.

## Roles

| Role | Who |
| --- | --- |
| Sequencer / tracker | **W65C02S** (cart PRG), **NMI** at ~60 Hz |
| Synthesizer / mixer | **MCU-S2** (AVR128DB28) @ 24 MHz |
| Analog out | MCU-S2 **TCA0 WO1** on **PF1** (`AUDIO_PWM`) into RC / amp / RCA **J8**. On-chip DAC on PD6 is unused (SPI1 ALT1 SCK owns PD6) |

```text
  Cart PRG (BGM / SFX bytecode)
        |
        v
  W65C02S  NMI ~60 Hz
  delay / loop / stop / FD decompress
        |
        |  STA $7F40 + 4*ch   (4 bytes per dirty voice)
        v
  MCU-M  --SPI /SS_S2-->  MCU-S2 mix 8 ch --> PWM PF1 --> RCA
```

## Cart mailbox (`$7F40`-`$7F5F`)

The bus-facing APU is a **32-byte register file**, not a bytecode FIFO. Voice `V` (index **0..7**) occupies `$7F40 + 4*V` .. `$7F43 + 4*V`.

PRG writes ordinary `STA` cycles. PLD `SEL_SOFT2` hits MCU-M. M copies dirty bytes to S2 over SPI (`MOSI + SCK + /SS_S2`, no MISO). S2 owns mix timing after the file lands. PWM is PF1 only.

S2 **does not** parse tracker opcodes. It only mixes the register file.

### Per-voice bytes

Same packing as `apps/common/fw/r01_apu_window.h`.

| Offset | Field |
| --- | --- |
| `[0]` | Bit 0 = enable. Bits 4-7 = volume 0-15. Triangle ignores volume (full when enabled) |
| `[1]` | Period low, or **DPCM sample ID** when wave is DPCM |
| `[2]` | Bits 0-2 = period high (11-bit period with `[1]`). Bits 4-5 = pulse duty 0-3 |
| `[3]` | Wave: 0 pulse, 1 triangle, 2 noise, 3 DPCM |

Pitched voices use the 11-bit period. DPCM v1 uses a fixed decode rate and ignores period.

Mailbox updates belong in **NMI / VBlank**, same discipline as OAM SPI. Long APU work stays off active-display PRG time.

## Eight channels (strict split)

Mixing on S2 keeps **8** concurrent voices. SFX never steal BGM notes.

### BGM, channels 1-5 (indices 0-4)

| Ch | Index | Wave | Role |
| --- | --- | --- | --- |
| 1 | 0 | Pulse | Lead melody (duty adjustable) |
| 2 | 1 | Pulse | Harmony / arpeggio / counter-melody |
| 3 | 2 | Triangle | Bass. Linear steps, **no volume control** (avoids clicks) |
| 4 | 3 | Noise | Hats, cymbals, synth snares |
| 5 | 4 | DPCM | 1-bit delta PCM (kicks, voice, hits) |

### SFX, channels 6-8 (indices 5-7)

| Ch | Index | Default | Role |
| --- | --- | --- | --- |
| 6-8 | 5-7 | Pulse / noise | Jump, shoot, UI blips |

Game logic may fire SFX at any time without muting channels 1-5. BGM FD frames must keep mask bits 0-4. SFX FD frames must keep mask bits 5-7.

## DPCM (channel 5)

**1-bit delta modulation** stores digitized audio cheaper than 8-bit PCM.

### Decode on S2

S2 keeps a **7-bit accumulator** for channel 5. At a fixed **~16 kHz** (v1 mix budget):

1. Read one bit from the current sample.
2. Bit **1**: accumulator **+= 2**.
3. Bit **0**: accumulator **-= 2**.
4. Add the accumulator into the master mix.

### Sample storage

The 6502 is too slow to stream sample bytes at the decode rate without stalling play.

- Payloads live in **MCU-S2 flash (`PROGMEM`)**.
- PRG only sends a **sample ID** (`7X` in the FD payload, then voice 4 `[1]` = `X`).
- S2 looks up start + length in its own flash and plays without further PRG traffic.

Sample IDs are **0..15** (low nibble of `7X`). Examples: `01` kick, `02` snare, `03` a short voice clip.

## Tracker bytecode (PRG)

Cart ROM holds compressed **hex** streams. **One note byte = one pitch.** The 6502 expands a stream into the 8x4 window. This layer never runs on S2.

### Note byte

| Field | Bits | Meaning |
| --- | --- | --- |
| Note letter | 7-4 | `0` = G, `A` = A, `B` = B, `C` = C, `D` = D, `E` = E, `F` = F |
| Octave / accidental | 3-0 | `0`-`7` natural, octave = value. `8`-`F` flat, octave = value - 8 |

```text
is_flat = (byte & 0x08) >> 3
octave  = byte & 0x07
```

### Example letter C (`C0`-`CF`)

| Hex (natural) | Pitch | Octave | Hex (flat) | Pitch | Octave |
| --- | --- | --- | --- | --- | --- |
| `C0` | C natural | 0 | `C8` | C flat | 0 |
| `C1` | C natural | 1 | `C9` | C flat | 1 |
| `C2` | C natural | 2 | `CA` | C flat | 2 |
| `C3` | C natural | 3 | `CB` | C flat | 3 |
| `C4` | C natural | 4 | `CC` | C flat | 4 |
| `C5` | C natural | 5 | `CD` | C flat | 5 |
| `C6` | C natural | 6 | `CE` | C flat | 6 |
| `C7` | C natural | 7 | `CF` | C flat | 7 |

Same pattern for letters `0` (G) and `A`-`F`.

### Two layers (no collision)

**FD payload** (one byte per set mask bit) may contain:

| Code | Meaning |
| --- | --- |
| Note `0x` / `Ax`-`Fx` | Pitch for that channel (including `F0`-`FF` as F natural / F flat) |
| `8X` | Volume `X` (0-15). Triangle still ignores volume |
| `9X` | Pulse duty or noise type (`X` 0-3 used) |
| `7X` | DPCM sample ID `X` (**channel 5 / index 4 only**) |
| `1X`-`6X` | Reserved |

**6502 stream opcodes** are never inside an FD payload:

| Code | Meaning |
| --- | --- |
| `FD` | Next byte = **8-bit channel mask** (bit 0 = ch1 ... bit 7 = ch8), then one payload byte per set bit |
| `FE` | Next byte = delay in **frames** |
| `FB` | Stop / play-once. Mute that family (BGM 1-5 or SFX 6-8) |
| `FA` | Loop. Reset the CPU read pointer to the start of that track block |

`FA` / `FB` / `FD` / `FE` parsed as stream opcodes would collide with F-flat note bytes if they appeared as payload. They do not. Notes in `Fx` only ride **after** `FD` + mask.

### `FD` expand (CPU)

1. Read the mask byte.
2. Count set bits. That many payload bytes follow in ROM.
3. For each set bit, apply that payload byte to the matching voice (volume, duty, note period, or DPCM ID).
4. `STA` the **4-byte** slot at `$7F40 + 4*ch` for each dirty channel. M SPI-forwards to S2.

If mask bit 4 is set, that payload byte is the DPCM command (usually `7X`) for channel 5.

## NMI dual-stream (~60 Hz)

Two independent state machines. Main game logic runs between NMIs. Audio work stays bounded per frame.

**BGM (channels 1-5)**

1. Decrement `BGM_Delay_Counter`. If still **> 0**, skip to SFX.
2. Read the byte at `BGM_Read_Pointer`.
3. Stream opcode `FA` / `FB` / `FE`: loop, stop, or wait.
4. `FD`: expand mask + payload into voices 0-4, write `$7F40` slots.
5. Advance the pointer.

**SFX (channels 6-8)**

1. If no SFX active, return.
2. Decrement `SFX_Delay_Counter`. If still **> 0**, return.
3. Same opcode / `FD` path, mask bits 5-7 only.
4. On `FB`, mark SFX inactive and mute voices 5-7.

```text
  NMI
    |
    +-> BGM SM (ch 1-5)  delay / FD / FA / FB / FE -> $7F4x
    |
    +-> SFX SM (ch 6-8)  same, no-op if idle
          |
          v
        RTI
```

Host Play ticks this same tracker in C (`apps/common/r01_apu_tracker.c`) and applies FD frames into the 8x4 window (`r01_apu_fd.c`). That is the cart protocol. The PC speaker mixes that window (`r01_apu_mix.c`). It is not MCU-S2 PWM.

## Status

| Layer | State |
| --- | --- |
| Design (this doc) | Locked |
| HW BOM | MCU-S2 + `$7F40`-`$7F5F` + MCU-M SPI + PWM PF1 |
| MCU-S2 FW | 8x4 mix to PWM. DPCM PROGMEM decode still filling in |
| Host mix | PC speaker mixes the `$7F40` window (`r01_apu_mix`). DPCM IDs use short host stand-in streams |
| 6502 PRG tracker | NMI dual-stream. Host C MVP exists. Cart ASM still filling in |
| Studio Audio tab | BGM grid editor in `.r01proj`. Export packs bytecode at `$B000`. Timeline Play/Stop encodes FD/FE/FA and mixes the window |
| Host Play | Boot track from PRG `$80FE`. Bytecode at `$B000`. Tracker fills `$7F40`. P1 **G** / **H** fire short SFX on voices 6-8 |

Bring-up Tier **H** only needs a real `$7F40` beep through S2 PWM. Full tracker depth can wait on hardware.

## Related

- Soft `$7Fxx` / APU range: `memory.md`
- MCU-S2 pins, RCA **J8**, SPI `/SS_S2`: `hardware.md`
- Mailbox SPI vs OAM VBlank: `ic-comms-risks.md`
- S2 process / PWM pin: `../ic_behavior/AVR128DB28.md`
- Bring-up: `../bringup/tier-h-pads-audio.md`
- Window packing: `../apps/common/fw/r01_apu_window.h`
- PRG BGM blob: `../apps/common/r01_apu_cart.h`
- FD expand / NMI tracker (host): `../apps/common/r01_apu_fd.h`, `../apps/common/r01_apu_tracker.h`
- Host mix of the window: `../apps/common/r01_apu_mix.h`
