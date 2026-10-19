# BitRow Console

A small 5 V DIY console. Every row of the display is a binary number.

Input is 1-bit pixel art or game frames. Each horizontal line is a binary value in hardware registers or counters. The bits are the pixels. An optional count-up mode fills the image by counting from 0 to the target value on every row.

The console is self-contained. Power is 5 V. The control is a simple arcade-style pad.

## 1. Concept

- Display: 16x16 LED matrix, or 32x32
- Each row is one binary word, 16-bit or 32-bit
- Pixel on is 1. Pixel off is 0
- The hardware holds those binary values, and can count up to them
- Simple games and binary art share the same pipeline

The screen is made of binary numbers, not only driven by a microcontroller.

## 2. System overview

```text
5V power
   |
   v
+-----------------------------+
| AVR128DB28 (or similar)     |  main brain
| Game logic                  |
| Binary row calculation      |
| Input reading               |
| Optional sound              |
+-------------+---------------+
              | parallel or SPI load
              v
+-----------------------------+
| Row registers / counters    |  74HC574, optional 74HC163
| One 16-bit word per row     |
+-------------+---------------+
              |
              v
+-----------------------------+
| 16x16 LED matrix + drivers  |
+-----------------------------+

Controller (arcade style)
+-- D-pad (up, down, left, right)
+-- 1 action button
```

## 3. Power

- One 5 V supply (USB power bank, wall adapter, or bench supply)
- 5 V at 2 A or higher. A full-bright LED matrix draws a lot of current
- Logic and LED rails stay separate when the layout allows it. They join at the power entry, with decoupling
- 100 nF and 10 uF on each major IC

## 4. Display

### 16x16

A 16x16 matrix is the practical size. Simple games stay playable. The display is 16 rows of 16-bit words.

### Row storage

**A. Direct load, for games**

- 74HC574 or 74HC595 octal registers
- The AVR loads the final binary value of each row every frame
- Updates land cleanly at 30 to 60 fps

**B. Count-up, for binary art**

- Cascaded 74HC163 counters. Four chips make 16 bits per row
- The target value sits in 74HC574 registers
- A 74HC688, or cascaded 74HC85 comparators, detects a match
- The row can count from 0 to the target

Normal games use direct load. Art mode uses the count-up path.

### LED driving

- Column drivers: 74HC595 plus transistors, a TPIC6B595, or a constant-current driver
- Row sinking: ULN2803, discrete NPN, or a MOSFET array
- Current-limiting resistors, or constant-current chips
- Multiplexing is acceptable and cuts the chip count

## 5. Controller

Arcade style:

- **D-pad**: four switches (up, down, left, right)
- **One action button**

Wiring:

- All five inputs pull high through 10 kohm
- Switches go to ground (active low)
- Debounce is in software, or a small RC plus a Schmitt trigger
- The switches land on AVR GPIO pins

Optional parts: a second action button, Start and Select, and a piezo on one pin for beeps.

The pad can sit in the case, or on a short cable (TRS or a plain header).

## 6. Main microcontroller

**AVR128DB28**, the Retr01 MCU.

- Flash and RAM cover simple games and binary art
- I/O and timers are flexible
- Simple sound comes from a timer or a DAC

Other parts that fit: ATmega328PB, RP2040, or another 5 V-tolerant MCU with enough pins.

The MCU does this work:

1. Read the D-pad and the button
2. Run game logic or the art player
3. Turn the current frame into 16 binary row values
4. Load those values into the row registers, or start a count-up
5. Hold the frame timing
6. Generate sound when a game uses it

## 7. Operating modes

1. **Binary art player.** A sequence of frames. Load is instant, or a count-up that fills the picture.
2. **Game mode.** Simple games on a 16x16 grid: Snake, Pong, Breakout, a tiny Tetris, a shooter, a maze, a reaction game.
3. **Hybrid.** A game can start a count-up on death, level clear, or pause.

## 8. Build path

1. An 8x8 matrix, an AVR, and direct register load
2. A 16x16 matrix
3. The D-pad and the button
4. One game. Snake is the first
5. The 74HC163 and comparator count-up path
6. A small enclosure, 3D printed or laser-cut

## 9. Names

- BitRow Console
- Binary Canvas Console
- CountFrame
- RowBit Arcade
- PixelCount
- Binary Pocket
- BitArcade

## 10. Scope

This is a small console, not a powerful handheld. The display is made of binary numbers. Registers and counters hold the picture. The AVR runs the games.
