# retr01 Audio Architecture

Software + hardware specification for the dedicated APU path. Channel map, DPCM, and bytecode protocol follow the audio design brief (NES-style tracker on 6502 + software mixer on AVR). Silicon placement matches the **32-IC BOM** ([`06`](06_hardware_v1_32ic.md)). Register window: **`$FE40`-`$FE5F`** ([`02`](02_graphics_worlds_memory.md)).

---

## 1. Hardware architecture overview

retr01 has **no fixed-function audio ASIC**. Roles split like the rest of the board:

| Role | Who |
|------|-----|
| Sequencer / tracker | **W65C02S** (game PRG), driven by **60 Hz NMI** |
| Synthesizer / mixer (APU) | **ATmega328P** @ 16 MHz (island **K**) |
| Analog out | 328P **8-bit digital mix** -> discrete **R-2R** resistor ladder DAC (PWM/RC alternate TBD) |

Flow:

1. The **W65C02S** streams **compressed bytecode** from cart ROM.
2. Custom **bus-bridge** logic (below) hands bytes from the 8 MHz 6502 domain to the 16 MHz AVR.
3. The **328P** mixes waveforms in software and drives the DAC.
4. The **ATmega1284P** does **not** synthesize audio (sprites, pads, machine EEPROM only).

```text
  Cart PRG / track data
        |
        v
  W65C02S  -> NMI 60 Hz ->  decompress / delay / bitmask
        |
        |  STA $FE4x...          Bus Bridge
        v
  ATmega328P APU  -> mix 8 ch ->  R-2R DAC / amp
```

### The Bus Bridge

On retr01, **"the Bus Bridge"** is **not** a separate named IC. It is the **CPU -> APU write path** that lets an 8 MHz 6502 hand bytes to a 16 MHz AVR without sharing a raw, always-on data bus:

1. **Decode**: PLD (and `$FExx` latches where needed) maps CPU accesses in **`$FE40`-`$FE5F`** to the APU window.
2. **Isolation**: the **CPU-domain 74HC245** (one of three board HC245s) plus PHI2 / `/OE` gating so only one master drives that domain at a time ([`06`](06_hardware_v1_32ic.md) bus split).
3. **Capture**: the 328P sees register-like writes on its port (or a latch clocked on the write). It owns synthesis timing after that.

Game code does ordinary `STA $FE4x`. The bridge is **decode + bus isolation + APU-side latch/port**, same pattern as other `$FExx` peripherals. Exact GPIO pinout is schematic TBD. The **software contract** is the 32-byte window.

---

## 2. Eight-channel software mixer & allocation

Mixing runs in software on the 328P so the system can keep **8 concurrent channels**. Channels are **strictly divided** so SFX never steal BGM notes. BGM channels are typed for a classic 8-bit tracker feel.

### 2.1 Background music (BGM), channels 1-5

| Ch | Wave | Role |
|----|------|------|
| 1 | Pulse | Primary lead melody (duty cycle adjustable) |
| 2 | Pulse | Harmony / arpeggios / counter-melody |
| 3 | Triangle | Dedicated bassline. **Linear quantization, no volume control** (avoids clicks) |
| 4 | Noise | Cymbals, hi-hats, synthesized snares |
| 5 | DPCM | 1-bit delta-encoded PCM (kicks, voice clips, orchestral hits) |

### 2.2 Sound effects (SFX), channels 6-8

| Ch | Default | Role |
|----|---------|------|
| 6-8 | Pulse / noise | Dynamic SFX: jump, shoot, UI blips, etc. |

Game logic may trigger SFX **arbitrarily** without interrupting channels 1-5.

---

## 3. DPCM (channel 5)

**1-bit delta modulation** stores digitized audio at a fraction of the cost of 8-bit PCM.

### 3.1 Decode on the ATmega

The 328P keeps a **7-bit accumulator** for channel 5. At a fixed sample rate (e.g. **~16 kHz or ~33 kHz**):

1. Read **1 bit** from the current DPCM sample byte.
2. If the bit is **1**, accumulator **+= 2**.
3. If the bit is **0**, accumulator **-= 2**.
4. Add the accumulator into the **master mix**.

### 3.2 Sample storage (memory challenge)

The 6502 is **too slow** to stream raw sample bytes at ~33 kHz without stalling game logic. Therefore:

- **Sample payloads live in 328P Flash (`PROGMEM`).**
- PRG only sends a **1-byte trigger** (sample ID), e.g. `0x01` kick, `0x02` snare, `0x03` "Start" voice.
- The AVR looks up **start address + length** in its own flash and plays **autonomously** at the sample rate.

---

## 4. Semantic hex protocol (note data)

Custom bitfield: highly compressed for the CPU, yet readable in a hex dump. **One byte = one pitch.**

### 4.1 Byte structure

| Field | Bits | Meaning |
|-------|------|---------|
| Note letter | 7-4 (high nibble) | `0`=G, `A`=A, `B`=B, `C`=C, `D`=D, `E`=E, `F`=F |
| Octave / accidental | 3-0 (low nibble) | `0`-`7` = **natural**, octave = value. `8`-`F` = **flat**, octave = value - 8 |

AVR decode:

```text
is_flat = (byte & 0x08) >> 3;
octave  = byte & 0x07;
```

### 4.2 Complete `C0`-`CF` reference (example letter C)

| Hex (natural) | Pitch | Octave | Hex (flat) | Pitch | Octave |
|---------------|-------|--------|------------|-------|--------|
| `C0` | C natural | 0 | `C8` | C flat | 0 |
| `C1` | C natural | 1 | `C9` | C flat | 1 |
| `C2` | C natural | 2 | `CA` | C flat | 2 |
| `C3` | C natural | 3 | `CB` | C flat | 3 |
| `C4` | C natural | 4 | `CC` | C flat | 4 |
| `C5` | C natural | 5 | `CD` | C flat | 5 |
| `C6` | C natural | 6 | `CE` | C flat | 6 |
| `C7` | C natural | 7 | `CF` | C flat | 7 |

Same pattern for letters `0` (G) and `A`-`F`.

### 4.3 Reserved high nibbles

Note data only uses high nibbles **`0`** and **`A`-`F`**. Ranges **`1X`-`9X`**, plus **`EX`** / **`FX`**, are reserved for **control codes**.

---

## 5. Control codes & decompression

### 5.1 APU state commands (sent to the 328P)

These ride with channel data through the bus bridge:

| Code | Meaning |
|------|---------|
| `8X` | Set volume of the target channel to `X` (0-15) |
| `9X` | Duty cycle (pulse) or noise type (noise) |
| `7X` | **DPCM trigger**: play PROGMEM sample ID `X` (**channel 5 only**) |

### 5.2 Playback control (handled on the 6502)

| Code | Meaning |
|------|---------|
| `FD` | **Channel bitmask**: core decompression bridge (below) |
| `FE` | Next byte = delay in **frames** |
| `FB` | Stop / play-once. Mute or mark track not to loop |
| `FA` | Loop track. Reset CPU read pointer to start of track block |

#### `FD` bitmask (decompression bridge)

1. Next byte is an **8-bit mask** (bit 0 = ch1 ... bit 7 = ch8).
2. The 6502 counts set bits -> that many **note / command bytes** follow in ROM.
3. If **bit 4** is set, one of those bytes is the **DPCM trigger ID** for channel 5.
4. CPU pushes **`FD` + mask + payload bytes** through `$FE4x` to the 328P latch.

---

## 6. 60 Hz NMI dual-streaming loop

The sequencer is driven entirely by the W65C02S **NMI** (~60x/s). **BGM** and **SFX** are two independent state machines. Main game logic runs between NMIs. Audio work stays bounded per frame.

### Step 1: Evaluate BGM (channels 1-5)

1. Decrement `BGM_Delay_Counter`. If still **> 0**, skip to Step 2.
2. Read the byte at `BGM_Read_Pointer`.
3. If **playback control** (`FA` / `FB` / `FE` ...): execute loop, stop, or wait.
4. If **`FD`**:
   - Read the mask byte.
   - Pull exactly as many note/command bytes as the mask requires (including DPCM ID when bit 4 is set).
   - Push `FD`, mask, and data bytes to the ATmega latch via the bus bridge.
5. Advance `BGM_Read_Pointer` as needed.

### Step 2: Evaluate SFX (channels 6-8)

1. If no SFX active -> **exit** audio routine.
2. Decrement `SFX_Delay_Counter`. If still **> 0** -> exit.
3. Read the byte at `SFX_Read_Pointer`.
4. If control code: execute. On **`FB`**, mark SFX inactive and mute SFX channels.
5. If **`FD`**: read mask, pull data, push to ATmega latch.
6. Advance `SFX_Read_Pointer`.
7. End NMI -> return to main game logic.

```text
  NMI entry
    |
    +-> BGM SM (ch 1-5), delay / FD / FA*FB*FE -> $FE4x
    |
    +-> SFX SM (ch 6-8), same, early-out if idle -> $FE4x
          |
          v
       RTI -> game loop
```

---

## 7. Status vs sim

| Layer | Today |
|-------|--------|
| Design (this doc) | 8-ch mixer, DPCM-in-AVR-flash, semantic hex + NMI dual tracker |
| HW BOM | 328P + `$FE40`-`$FE5F` + CPU HC245 domain |
| Board sim | Island **K** stub: period/vol **PWM square** smoke. Not full mixer/DPCM yet ([`retr01_sim/README.md`](../retr01_sim/README.md)) |

---

## Related

- Memory map / `$FExx`: [`02`](02_graphics_worlds_memory.md)
- 32-IC BOM / APU path: [`06`](06_hardware_v1_32ic.md)
- Game module budgets: [`08`](08_game_modules.md)
- 328P chip notes: [`hw/md/ATmega328P.md`](../hw/md/ATmega328P.md)
- Overview index: [`01`](01_architecture_overview.md)
