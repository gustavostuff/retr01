# BitRow Console

A small 5V DIY console built around the idea that **every row of the display is a binary number**.

You feed it 1-bit pixel art (or game frames). Each horizontal line becomes a binary value that is loaded into hardware registers/counters. The bits themselves are the pixels. An optional count-up mode lets the image “materialize” by counting from 0 to the target value on every row.

The system is designed as a self-contained console: plug in 5V, connect a simple arcade-style controller, and play.

---

## 1. Concept

- Display: 16×16 LED matrix (recommended) or 32×32
- Each row = one binary word (16-bit or 32-bit)
- Pixel ON = 1, Pixel OFF = 0
- The hardware holds (and can count to) those binary values
- Simple games and binary art both use the same pipeline

This creates a unique aesthetic: the screen is not just driven by a microcontroller — it is literally made of binary numbers.

---

## 2. System Overview

```
5V Power
   │
   ▼
┌─────────────────────────────┐
│  AVR128DB28 (or similar)    │  ← Main brain
│  - Game logic               │
│  - Binary row calculation   │
│  - Input reading            │
│  - Optional sound           │
└────────────┬────────────────┘
             │ parallel / SPI load
             ▼
┌─────────────────────────────┐
│  Row Registers / Counters   │  ← 74HC574 + optional 74HC163
│  (one 16-bit word per row)  │
└────────────┬────────────────┘
             │
             ▼
┌─────────────────────────────┐
│  16×16 LED Matrix + Drivers │
└─────────────────────────────┘

Controller (arcade style)
├── D-Pad (Up / Down / Left / Right)
└── 1 Action Button
```

---

## 3. Power

- Single 5V supply (USB power bank, wall adapter, or bench supply)
- Recommended: 5V / 2A or higher (LED matrix can draw significant current at full brightness)
- Separate logic and LED power rails if possible, joined at the power entry with proper decoupling
- On-board 100 nF + 10 µF decoupling per major IC

---

## 4. Display Section

### Recommended: 16×16

- Easier to drive and still very playable for simple games
- 16 rows × 16-bit binary words

### Row Storage Options

**A. Simple & practical (recommended for games)**  
- 74HC574 (or 74HC595) octal registers  
- AVR loads the final binary value for each row every frame  
- Fast, clean, easy to update at 30–60 fps

**B. Pure binary / educational (keeps the original soul)**  
- Cascaded 74HC163 binary counters (4 chips = 16 bits per row)  
- Target value stored in 74HC574 registers  
- 74HC688 or cascaded 74HC85 comparators detect match  
- Optional count-up animation from 0 → target

You can implement both: normal games use direct load, while special effects or art mode use the count-up path.

### LED Driving

- Column drivers: 74HC595 + transistors, TPIC6B595, or constant-current drivers
- Row sinking: ULN2803, discrete NPN, or MOSFET array
- Current-limiting resistors (or constant-current chips) required
- Multiplexing is acceptable and reduces chip count

---

## 5. Controller – Arcade Style

Simple and robust:

- **D-Pad**: 4 directional switches (Up, Down, Left, Right)
- **1 Action Button**

### Suggested implementation

- All 5 inputs pulled high with 10 kΩ resistors
- Switches to ground (active low)
- Debounced in software (or with simple RC + Schmitt if desired)
- Connected directly to AVR GPIO pins

Optional extras:
- Second action button
- Start / Select
- Piezo speaker on one pin for beeps

The controller can be built into the case or connected via a short cable (even a TRS or simple header).

---

## 6. Main Microcontroller

**Recommended: AVR128DB28** (same family used in Retr01)

Reasons:
- Plenty of flash and RAM for simple games + binary art
- Flexible I/O and timers
- You already know the chip
- Can generate simple sound via timer or DAC

Alternatives: ATmega328PB, RP2040, or any 5V-tolerant MCU with enough pins.

### Main tasks of the MCU
1. Read D-pad + button
2. Run game logic or art player
3. Convert the current frame into 16 binary row values
4. Load those values into the row registers (or start count-up)
5. Handle timing / frame rate
6. Optional: generate sound

---

## 7. Operating Modes

1. **Binary Art Player**  
   Load a sequence of frames. Can use instant load or dramatic count-up materialization.

2. **Game Mode**  
   Classic simple games that fit a 16×16 grid:
   - Snake
   - Pong / Breakout
   - Tiny Tetris
   - Shooter
   - Maze / roguelike
   - Reaction games

3. **Hybrid**  
   Games can trigger count-up effects on death, level clear, pause, etc.

---

## 8. Suggested Build Path

1. Start with 8×8 matrix + AVR + direct register load (prove the concept)
2. Expand to 16×16
3. Add the D-pad + button
4. Implement one game (Snake is ideal)
5. Optionally add the 74HC163 + comparator count-up path for the signature effect
6. Design a simple enclosure (3D printed or laser-cut) that looks like a tiny console

---

## 9. Project Names

- BitRow Console
- Binary Canvas Console
- CountFrame
- RowBit Arcade
- PixelCount
- Binary Pocket
- BitArcade

---

## 10. Spirit of the Project

This is not trying to be a powerful handheld.  
It is a small, understandable, slightly weird console where the display itself is made of binary numbers. The combination of pure digital logic (counters / registers) and a friendly AVR for games creates a unique educational and playful device.

Power it with 5V, plug in the simple arcade controls, and play.

Happy building!
