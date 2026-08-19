# Proto-board tests (from the machine walkthrough)

Physical tests for the blocks in [05_how_the_machine_works.md](05_how_the_machine_works.md). Simulator order stays in [16](16_simulation_and_bringup_plan.md). Do **Digital first** for a block when you can; this file is what you solder when you want a scope trace.

You will not fit a 49-chip 8 MHz bus on one solderless breadboard. Use **one island per protoboard** (or a small 2-layer proto PCB), sockets, short wires, **100 nF at every VCC**. Clock the CPU island at 1–2 MHz for the first bring-up if 8 MHz rings. Passives: [16 §1](16_simulation_and_bringup_plan.md).

Instruments: 5 V bench supply with current limit (~200 mA to start), LED, logic probe or cheap analyzer, scope (2 ch is enough), optional 15.7 kHz RGBS monitor. A TL866 (or similar) to burn ATF22V10 and cart flash.

**Pass** means the check in that section, not “the whole game runs.”

---

## Ground rules

- 5 V only. No 3.3 V LV parts.
- W65C02S: `BE` and `RDY` high, `VPB`/`MLB` NC, clock = **PHI2**.
- One `/CE` at a time on any shared data bus.
- If two chips fight D0–D7, stop. That is a short. Fix mux/`/OE` before adding more ICs.
- Do not hang the 1284 and the BG PPU on CHR until each side works alone.

---

## Island A — Power (05 §5.1)

**Board:** barrel or bench 5 V, polyfuse, reverse diode, bulk cap, LED + 1 kΩ.

**Test:** rail is 5.0 V ±0.25 V under a 100 Ω dummy load. Reverse the barrel (on a sacrificial fuse) and nothing else dies. Current with no logic: milliamps, not amps.

---

## Island B — Clocks and reset (05 §5.2, §5.3 straps)

**Board:** 8 MHz can → PHI2 (or a 1 MHz can for early CPU tests). HC14 + 10 kΩ + 10 µF reset. Optional 21.477 MHz + HC ÷4 as a **separate** oscillator island.

**Test:** PHI2 is a clean square on the scope. `RESB` sits low ~100 ms at power-on, then high. Button pulls reset. Dot clock (if built) is ~5.37 MHz.

---

## Island C — CPU + system RAM + tiny ROM (05 §5.3, §5.5, GAL-DEC stub)

**Board:** W65C02S, AS6C62256 #1, 32 KB (or smaller) flash/EPROM at `$8000` with `$FFFC` → `$8000`, `JMP $8000` / NOP fill. Decode can be 74HC for this island: `RAM_CS = !A15`, ROM when A15 and not `$FE`. No VRAM yet.

**Test:** after reset, A15 toggles (code in ROM). Data bus shows the NOP opcode if you trigger on PHI2. Current stays sane. `NMIB`/`IRQB` pulled up, unused.

---

## Island D — `$FExx` decode + one latch (05 §5.4, §5.9)

**Board:** ATF22V10 GAL-DEC **or** 74HC138/688 stub. One 573 at `$FE02` (scroll X). Guest: `LDA #$55 / STA $FE02 / LDA $FE02` if you wire it readable, or probe the 573 Q pins.

**Test:** stores to `$0000` still hit RAM. Stores to `$FE02` clock that 573 only. Stores to `$8000` do not change RAM or the 573.

---

## Island E — Pads (05 §5.16, `$FE60`)

**Board:** 8 switches, 10 kΩ pull-ups, invert, 573 or 245 onto CPU D when GAL selects `$FE60`. Need island C+D.

**Test:** guest `LDA $FE60`. Right pressed → bit 0 set (1 = pressed). No switch → `$00`. P2 (`$FE61`) later; same circuit.

---

## Island F — EEPROM (05 §5.15)

**Board:** AT28C64B on `$FE7x`. Need C+D.

**Test:** write a byte, power-cycle (or `/WE` protocol from 08/15), read it back. Slow is fine. Do not put this on the video path.

---

## Island G — VRAM port, no beam yet (05 §5.6, §6)

**Board:** SRAM #2, 4× 157, 245, VADDR/VDATA 573s, PHI2 as mux select. PPU side of the 157s tied to a **fixed** address (e.g. all 0) or a DIP switch. CPU `$FE1x` as in 08.

**Test:** CPU writes `$AA` to VRAM `$0000` on CPU phase, reads it back. Scope `/OE` of the 245: High-Z on the opposite phase. If you can halt PHI2, confirm no two drivers on D.

This is the **interleave mutex** without graphics. If this island is dirty, do not build a PPU on top.

---

## Island H — Beam + dummy RGB (05 §5.8, §5.13 analog only)

**Board:** 4× 161, GAL-TIM or 74HC wrap at 341/262, R-2R from X[5:0] or a palette DIP. CSYNC from HBlank/VBlank. **No** nametable fetch required.

**Test:** HSYNC ~15.7 kHz, VSYNC ~60 Hz on the scope. RGBS monitor shows a stable raster (color bars / gradient). NMI pin pulses at ~60 Hz.

If the monitor will not lock, fix sync **before** connecting VRAM.

---

## Island I — Background tiles (05 §5.10)

**Board:** H + G + CHR ROM (or a second flash) + attr unpack + 2bpp shifters + `$FE30` bank 573s. GAL-PPU CHR `/CE` = BG only (1284 not present).

**Test:** bake a nametable in VRAM (via `$FE1x` or a programmer). Known tile 0 in CHR. Screen shows an 8×8 pattern, scroll `$FE02/03` moves it. Wrong-phase CPU VRAM access should glitch; right-phase should not.

---

## Island J — 1284 + line buffer, no BG (05 §5.11–5.12)

**Board:** 1284, SRAM #3, 2× 157, HBLANK from island H (or an AVR timer approximating 63.5 µs / 16 µs). ISP header. CHR ROM on the 1284 data bus **only** during a fake HBlank.

**Test:** firmware fills bank 1 with a solid color at X=32–40. Beam (or a 161 from H) reads `{bank, X}`. Scope `/WE` of SRAM #3 during HBlank only. CPU `$FE21` path: 573 + `OAM_WR` ISR stores 256 bytes; dump OAM over UART if you want.

Do **not** share CHR with island I until GAL-PPU exclusive `/CE` is tested on a **third** small board (two `/CE` probes: never both low).

---

## Island K — Compositor stitch (05 §5.13 digital)

**Board:** I output (BG `ci`) and J output (line-buffer byte) into the priority mux, then the same R-2R as H.

**Test:** BG tiles plus a sprite bar one line late. Sprite color 0 shows BG. Priority bit puts sprite behind opaque BG.

---

## Island L — APU (05 §5.14)

**Board:** 328P, 16 MHz crystal, `$FE40` 573 from a **second** 6502 island or a PC serial fake. 3.5 mm jack, PWM RC filter.

**Test:** poke pulse regs (or a hard-coded firmware beep). No CHR, no VRAM on this board.

---

## Island M — Cart connector (05 §5.7)

**Board:** 40-pin header, one SST39SF040 as PRG, `/CE_PRG` from GAL-DEC. CPU island C talks to it instead of the proto ROM.

**Test:** `$FFFC` vectors from flash. Bank `$FE80` if you wire A15+. MAP and CHR chips after PRG is boringly reliable.

---

## Suggested order

```text
A power  →  B clocks  →  C CPU/RAM  →  D decode  →  E pads
                ↓
         G VRAM mutex    H beam/RGBS
                ↓              ↓
                I BG tiles     J 1284 + line buffer
                         \    /
                          K compositor
L audio and M cart can run in parallel with I/J.
```

When A–E, G, H, I, J, K, L, M all pass on **separate** protoboards, that is the signal to draw one KiCad motherboard — from these islands, not from Celus. Simulator path: [16](16_simulation_and_bringup_plan.md).

---

## What not to test on a solderless breadboard

- Full 8 MHz 16-bit address bus with 20+ DIP packages (too much capacitance). Move that island to proto PCB.
- Analog S-Video / composite encoder (pads only until RGBS works).
- Retr01-C 3-wire pad protocol (software still `$FE60`; leave B5 for later).
