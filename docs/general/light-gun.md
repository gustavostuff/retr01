# Light gun

Roadmap. Same TRS jacks as the pads. The gun reports a trigger every frame, and a beam time after a two-frame flash. The console turns that time into a point on the raster and tests it against the white boxes it just drew.

This works on a display that lights one spot at a time while the 341 by 262 raster scans. A panel that lights the whole frame at once turns every box on together, and one timer cannot tell the boxes apart.

Summary table and the `$7F80` / `$7F81` proposal: `hardware.md`. Pad poll rules: `../ic_behavior/ATtiny85.md`.

## Hardware

The shell is an ATtiny85, a photodiode, an LM393, and a trigger switch. Tip is 5 V, ring is DATA, sleeve is GND. DATA is open-drain 115200 8N1, with the motherboard 4.7 kohm pull-up on MCU-S2 PF0. The gun is outside the counted 21, same as a pad.

The photodiode watches a narrow patch of the screen. The LM393 turns that current into a logic edge. The edge captures a 16-bit timer. Ticks are 1 us. A steady room lamp sits below the comparator threshold. The CRT flash, while the gun is on a bright patch, sits above it.

The timer is wide enough for one frame. A frame is about 16.64 ms. The counter overflows at 65.535 ms. No pulse leaves the capture at `0xFFFF`.

## Bytes

MCU-S2 polls in VBlank, one jack at a time. J3 is `0x55`. J4 is `0xAA`. Each exchange stays under 200 us.

| Byte | Direction | Meaning |
| --- | --- | --- |
| `0xFF` | Host | Identify. Pad replies `0x01`. Gun replies `0x02` |
| `0x55` or `0xAA` | Host | Frame poll. Stop bit clears the timer and arms capture |
| Reply to the poll | Gun | Bit 5 is the trigger. Other bits stay clear |
| `0x5A` | Host | Read the capture. Gun sends HI, then LO |
| `0xFFFF` | Gun | No pulse, or the black frame already saw light |

`$7F60` / `$7F61` still hold the poll reply, so the trigger bit lands where a pad's Y bit lands. The two timer bytes are the proposed soft ports `$7F80` (HI) and `$7F81` (LO). S2 holds them. M serves the read.

A pad ignores `0x5A`. Only a gun answers it.

## Timer and the scan

The beam walks left to right, then down one line. DOT is 5.369318 MHz. A line is 341 dots, about 63.51 us. A frame is 262 of those lines. The active picture inside that raster is 256 by 240. The rest is blanking. Exact blanking edges live in the Beam X and Beam Y PLDs.

The zero of the count is the stop bit of that jack's poll, in the VBlank before the frame the poll arms. J3 is polled, then J4. The two stop bits are not the same dot.

`GAP_DOTS` is the integer dot distance from that stop bit to the first active dot. Each jack has its own value, measured against the beam once and stored. It stays valid while both polls keep the same delay from the VBlank edge. It is not a rounded microsecond subtracted from T.

Both stop bits land before the first active dot, so both gaps are positive and the whole active picture is still ahead of the timer. The two exchanges take under 400 us, about 2148 dots, about 6.3 lines. Blanking in front of the picture has to cover that.

T is the captured tick count in microseconds. DOT is 5369318 Hz. The conversion is integer division of one product:

```text
dots = (T * 5369318 + 2684659) / 1000000
beam = dots - GAP_DOTS
line = beam / 341
dot  = beam mod 341
```

The product is about 37 bits near the bottom of the frame. A 32-bit multiply drops the top bits and the line comes out wrong. The added 2684659 is half a tick (half of 5369318). It puts the result in the middle of the microsecond T names.

What is left is at most 3 dots, at the top of the picture and at the bottom. It does not grow with the line number. A flash within 3 dots of a line edge can be reported on the next line. Hit boxes stay clear of that edge, or the test allows a 3-dot pad. At 2x scale a logical pixel is 2 dots by 2 lines, so the horizontal grain is about two logical pixels. `SCALE_1X` skips that divide.

A capture that converts to `GAP_DOTS` is line 0, dot 0. One line later is 341 dots past the gap.

Dividing T by 64, or subtracting a rounded microsecond gap and then dividing by 64, uses a 64 us line. The real line is 63.51 us. At line 239 that shortcut is about 2 lines low. That path is not used.

## Which white box

A shot is two flashed frames and one timer read.

1. The poll that reports the trigger is the start of the black frame. The upcoming active scan is black. Any capture in that frame is outside light (a lamp, a window, a stuck comparator). The gun remembers that as a spoof.
2. The next poll starts the white frame and clears the timer again. Every live target is a white rectangle on that black field. The first LM393 edge latches the timer. Later edges in the same frame are ignored.
3. The following VBlank sends `0x5A`. Spoof, or no edge, replies `0xFFFF`. Otherwise the reply is the white-frame capture. The live counter may reset on this poll. The capture register is what `0x5A` returns.

The host already knows the rectangles it drew. The formula above turns the capture into a line and a dot, then into logical x,y (divide by 2 at 2x scale). The rectangle that contains that point is the target. A point in a gap is a miss.

Only the first flash is kept. Two boxes inside the lens at the same time collapse into whichever the beam reaches first (higher on the screen, then further left). Boxes stay apart by more than the lens spot, and by more than the horizontal grain.

The black frame is what makes a lamp fail closed. A lamp that passes the comparator trips during the black scan, the spoof flag sticks, and `0x5A` is `0xFFFF` even if the white frame also flashed. The black frame also clears the previous picture so gameplay pixels are not extra targets.
