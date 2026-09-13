# Retr01 Hardware

**Status:** Draft v1.2  
**Parent:** [overview.md](overview.md)

## 1. Main board MCUs

| MCU | Role | Primary duties |
|-----|------|----------------|
| AVR128DB28 #1 | Video | Frame render, sprite evaluation, analog video |
| AVR128DB28 #2 | System | Cartridge SPI, input, audio, entity and contract engine, inter-MCU master |

The whole console targets a uniform **5 V** domain (MCUs, cartridge logic levels, controllers) when practical.

## 2. Power

- Connector: female barrel jack, **5.5 mm OD / 2.1 mm ID**
- Nominal input: **5 V DC**
- On-board regulation only as needed for the dual AVR128DB28 and cartridge parts

## 3. Cartridge

### 3.1 Mechanical

- Edge connector with **8 pads per side** (16 contacts total)
- Exact pad assignment is **TBD** (left open for PCB layout)

### 3.2 Contents (data only)

1. **World / CHR chip**  
   Prefer **SPI Flash** (or EEPROM if the size and speed are fine).  
   Holds worlds, nametables, entity visuals, contract attachments and parameters, 4 BG banks, 4 sprite banks.  
   Access patterns that would stall gameplay should be avoided or buffered (load on screen change, cache banks in advance).

2. **Save chip**  
   **SPI FRAM** for game saves. Fast enough that save and load need not interrupt play in a noticeable way.

Wiring from the 16 pads to the two ICs is left to the final PCB design.

## 4. Video and audio outputs

Two families of connectors are always available.

### 4.1 RGB / sync / audio pin header (THT)

Single-row **1x8** male pin header (0.1 inch). Pin 1 is marked on the silkscreen.

| Pin | Signal | Notes |
|-----|--------|-------|
| 1 | R | Analog red |
| 2 | G | Analog green |
| 3 | B | Analog blue |
| 4 | CSYNC | Composite sync (RGBS path) |
| 5 | HSYNC | VGA-style horizontal sync |
| 6 | VSYNC | VGA-style vertical sync |
| 7 | AUDIO | Line-level mono (same mix as the RCA audio) |
| 8 | GND | Ground |

Order rationale: RGB in rainbow order, then CSYNC before separate H/V so an RGBS cable can use pins 1-4 + 8, and an RGBHV cable can use pins 1-3 + 5-6 + 8. Audio sits next to ground at the end of the row.

### 4.2 Classic RCA-style ports

- **Composite video** (analog)
- **Mono audio** (analog)

These are the casual TV-friendly outputs. Levels and encoding details (CVBS amplitude, sync on luma) stay TBD until the Video MCU output stage is locked.

### 4.3 Resolution

- Internal render: **128x120**
- Output scale: **2x** (pixel doubling and controlled scanline or clock adjust)
- Effective output about **256x240**

### 4.4 Colour

The Video MCU firmware holds the stock **8-colour FG palette** defined in [graphics-world.md](graphics-world.md) (from `assets/global_palette.png`).  
Background pixels stay black. The palette is technically changeable in firmware. Shipping games and tools should keep the stock set.

Exact resistor ladder or DAC values that hit those RGB targets remain TBD.

## 5. Input

Both methods are always present on every board.

### 5.1 Direct arcade GPIO

Eight microswitches on the System MCU, brought out on a single-row **1x9** male pin header (0.1 inch). Pin 1 is marked on the silkscreen. Switches are to GND (MCU inputs use internal or board pull-ups).

| Pin | Signal |
|-----|--------|
| 1 | GND (switch common) |
| 2 | Up |
| 3 | Down |
| 4 | Left |
| 5 | Right |
| 6 | X |
| 7 | Y |
| 8 | Start |
| 9 | Select / Coin |

Order rationale: ground first for a shared loom return, then stick UDLR, then face buttons X/Y, then system buttons. Easy to wire a joystick harness without a wiring diagram in hand.

### 5.2 Serial controllers

- Two independent **3.5 mm TRS** jacks (Player 1 and Player 2)
- Controllers are ATtiny-based (or equivalent)
- **5 V** on the ring (matches the console 5 V domain)
- Host pull-up on DATA

#### Physical layer

| Contact | Signal |
|---------|--------|
| Tip | DATA (open-drain / bidirectional) |
| Ring | VCC (5 V) |
| Sleeve | GND |

#### Protocol (poll at start of vblank)

Target: both pads polled in **<= 100 us**.

Physical layer: open-drain DATA, host pull-up. Bit-banged and self-clocked.

**Poll (host to controller):**

- Host pulls DATA low, then releases
  - **~4 us low** = poll Player 1
  - **~8 us low** = poll Player 2
- Host then samples the reply

**Response (controller to host):**

- After its poll pulse, wait ~2 us, then send **9 bits**
  1. Start bit (always 0)
  2-9. Eight status bits, MSB first
- Bit time: **2 us**
- Then release the line

Typical both-pads time is about 50-60 us. Hard ceiling is 100 us.

**Status byte** (1 = pressed):

| Bit | Button |
|-----|--------|
| 0 | Right |
| 1 | Left |
| 2 | Down |
| 3 | Up |
| 4 | Start |
| 5 | Select / Coin |
| 6 | Y |
| 7 | X |

## 6. Inter-MCU link (System to Video)

SPI. System is master. Video is slave. Bulk transfers happen in vertical blank.

### 6.1 Physical layer

- SPI mode 0 preferred (mode 3 acceptable if wiring needs it)
- Clock up to about 8-10 MHz
- Extra lines: Video IRQ / VBlank flag, optional busy or transfer-done

### 6.2 Commands

Every transaction starts with a 1-byte command, then an optional payload.

| Cmd | Name | Payload | Notes |
|-----|------|---------|-------|
| 0x01 | SET_NAMETABLE | 480 bytes (240 indices + 240 attrs) | Full screen upload (no RLE) |
| 0x02 | SET_SPRITES | 1 + N*4 bytes (count + sprites) | N <= 32 |
| 0x03 | SET_SCROLL | 2 bytes (X, Y fine scroll) | Optional |
| 0x04 | SET_BANK_CACHE | variable tile data | Prefetch banks if needed |
| 0x05 | VBLANK_SYNC | none | Optional handshake |
| 0x00 | NOP / STATUS | none | Keep-alive / status |

Screens are small enough that full nametable uploads are fine. **No RLE** (or other codec) in v1. Codec logic would cost more than it saves.

**Sprite entry (4 bytes):**

```
Byte 0 : X
Byte 1 : Y
Byte 2 : Tile index
Byte 3 : Attribute
```

## 7. Open hardware items

- Final 16-pad cartridge edge assignment
- Exact RGB resistor or DAC values
- Composite and RCA level details
- Optional busy line wiring between MCUs
