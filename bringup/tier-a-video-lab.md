# Retr01 Tier A - Video-only breadboard lab

**Goal:** Stable composite (or RGB) picture on a TV/monitor using only the video path. 
**Out of scope:** Cart, CPU, AVRs, controllers, audio, saves.

Smallest setup that still exercises Retr01's real color path:

```text
DOT clock -> Beam PLDs (raster + sync) -> 6-bit color index
                                              v
                                    AT27C256R color PROM
                                              v
                                    R3G3B2 resistor DAC (~0.7 Vpp RGB)
                                              v
                                    AD724 (RGB -> NTSC/PAL composite)
                                              v
                                    RCA / TV
```

**SoT:** `general_docs/hardware.md`, `general_docs/video-graphics.md`, `general_docs/palette/`, `ic_behavior/AD724.md`, `ic_behavior/ATF22V10.md`, `ic_behavior/AT27C256R.md`.

---

## 1. Parts list

### ICs (required)

| Qty | Part | Package | Role |
| --- | --- | --- | --- |
| 2 | **ATF22V10** (e.g. ATF22V10CQZ-20PU) | PDIP-24 | **Beam X** and **Beam Y** only |
| 1 | **AT27C256R** | PDIP-28 | 64 master colors, packed R3G3B2 |
| 1 | **AD724** | SOIC-16 on **SOIC->DIP-16 adapter** | RGB -> composite |

Optional third ATF22V10 (Compositor) is **not** required for Tier A if the 6-bit index comes from DIP switches or Beam-X equations (see section 5).

### Clocks (required)

| Net | Frequency | Notes |
| --- | --- | --- |
| **DOT** | **5.369318 MHz** | Prefer a **canned CMOS oscillator**, not a raw crystal amp |
| **FSC** | **3.579545 MHz** (NTSC) or **4.433618 MHz** (PAL) | Into AD724 `FIN`. `SELECT` pin low = FSC mode |

### Passives / connectors (minimum)

- **Color DAC resistors** (1% metal film), LSB->MSB:
  - R and G: **4.00 k ohm / 2.00 k ohm / 1.00 k ohm**
  - B: **2.00 k ohm / 1.00 k ohm**
  - Each gun: **75.0 ohm** to GND -> ~**0.7 Vpp** into AD724
- Series **~33 ohm** on DOT (and on FSC if the source is aggressive)
- Decoupling: **100 nF** at every IC VCC pin. Bulk **~220 uF** at 5 V entry
- RCA jack (composite from AD724 `COMP`)
- Optional: header for raw RGB + sync (J2-style) before AD724 is trusted
- 5 V supply capable of a few hundred mA. Clean ground.

### Optional

| Part | When |
| --- | --- |
| **74HC14** | Only if DOT/FSC edges are soft or reset chatters. Skip if canned oscillators are already square. |
| Third **ATF22V10** | Only if Compositor-style index logic is preferred over discrete/DIP index (section 5). |
| Scope / logic analyzer | Strongly recommended for first light |

### Explicitly omit for Tier A

- W65C02S
- Any AVR128DB28 (M / S1 / S2)
- AS6C62256 SRAMs
- 74HC157 / 74HC573 / 74HC574
- SST39SF040, 24C64, ATtiny85
- PHI2 8 MHz oscillator (not used without CPU/VRAM interleave)

---

## 2. What Tier A is testing

| Subsystem | Pass criteria |
| --- | --- |
| DOT clock | Clean ~5.37 MHz square on Beam X CLK |
| Beam X | Stable H counter 0...340, HSYNC/CSYNC at line rate |
| Beam Y | Stable V counter 0...261, VSYNC at ~60.1 Hz |
| Color index path | Known 6-bit value reaches PROM A[5:0] |
| PROM + DAC | RGB levels ~0-0.7 V, colors match kit when index changes |
| AD724 | Lockable composite on a TV (color bars or solid field) |

If any of those fail, fix that layer before adding S1, VRAM, or a CPU.

---

## 3. Rules

### Required

1. **PLDs and the color PROM are programmed before first power-up**, with the CPU-side bus absent. Blank PLDs and a random PROM make debugging impossible.
2. **Unused PROM address lines A[14:6] (and any unused) tie to GND.** Only A[5:0] carry the color index for 64 entries.
3. **CSYNC or H/V stays consistent.** For AD724 CSYNC mode, composite sync drives `HSYNC`, and `VSYNC` stays inactive (> ~2 V) per datasheet recipe. Both meanings never share one pin.
4. **Analog island stays short:** PROM -> DAC resistors -> AD724 R/G/B -> RCA. Digital switching noise stays off that path as much as breadboard allows.
5. **AD724 analog and digital** (`APOS`/`DPOS`, `AGND`/`DGND`) are powered correctly. Encode is on (`ENCD` = H).
6. **`STND` matches the region** (H = NTSC, L = PAL) and FSC frequency matches.
7. **Bring-up order is solid color first** (fixed index), then color bars, then spatial patterns. One change at a time.
8. **DOT, HSYNC, VSYNC/CSYNC, and one RGB gun are scoped** before blaming the TV.

### Forbidden

1. **Cart flash, RAM, or MCU data pins on any shared bus.** Tier A has no bus fights by design.
2. **PHI2 or a fake 6502.** Interleaved VRAM is a later problem.
3. **Digital 5 V levels on AD724 R/G/B.** The encoder expects ~0.7 Vpp video from the resistor DAC.
4. **Floating HC-style enables** if any 74xx is added later. Pure Tier A simply omits them.
5. **Push-pull fights on sync.** One driver for HSYNC/CSYNC, one for VSYNC.
6. **Studio/Emu Play expectations.** This lab only proves the analog/raster path.
7. **Skipping decoupling** on a breadboard at 5 MHz. Noise looks like bad colors or unlocked chroma.
8. **Random PROM data.** The locked 64-color kit (or a simple ramp) keeps index N a known color.

---

## 4. Raster targets (from design)

| Parameter | Value |
| --- | --- |
| Dot clock | 5.369318 MHz |
| Line length | **341** dots |
| Frame | **262** lines |
| Frame rate | ~**60.098 Hz** |
| Logical playfield | 128x120 (2x scaled to 256x240 on the full system) |

Tier A does **not** need correct active/blanking pixel art yet - it needs **stable** H and V timing that a TV will lock to. Approximate NTSC-ish blanking is enough for first light:

- Rough guide: active ~256 dots, rest of 341 = blanking/sync region (tune while watching the scope and TV).
- Vertical: active ~240 lines, rest of 262 = blanking/VSYNC region.

Exact Retr01 HBlank/VBlank equations live in the Beam X/Y JEDEC files for the full design. For the lab, implement a **minimal** counter + sync pulse generator in the two PLDs.

### Beam PLD split (design intent)

| PLD | Owns |
| --- | --- |
| **Beam X** | Dot counter inside the line (0...340), HBlank, HSYNC/CSYNC |
| **Beam Y** | Line counter (0...261), VBlank, VSYNC |

Clock Beam X from **DOT**. Clock Beam Y from a line tick (e.g. end-of-line pulse from Beam X).

---

## 5. Feeding the 6-bit color index

The PROM is addressed by a **6-bit kit index** (0...63). Video never pokes RGB at runtime. It only selects which of the 64 burned colors is shown.

```text
Index[5:0] -> PROM A[5:0]
PROM Q[7:0] -> {R2 R1 R0 G2 G1 G0 B1 B0} (R3G3B2 pack)
            -> resistor DAC -> R, G, B analog
```

Unused PROM address pins -> **GND**. 
`CE#` / `OE#` held active (or hard-wired active for the lab).

### Method A - DIP switches (simplest first light)

1. Wire six switches (or jumpers) to PROM A[5:0] with pull-downs (or pull-ups - pick one and stay consistent).
2. Power up with index `000000` (kit black / index 0).
3. Flip to index `110000` (48) or `111111` (63) and confirm brightness/color changes on RGB and composite.

**Pros:** Zero PLD equation risk for color. 
**Cons:** Full-screen solid color only.

### Method B - Color bars from Beam X (recommended Tier A demo)

Derive index from horizontal position so one line shows a bar pattern:

| Region of active X (example) | Index | Intended look |
| --- | --- | --- |
| 0-31 | 48 | White-ish (kit 48) |
| 32-63 | 52 | Yellow-ish |
| 64-95 | 55 | Green-ish |
| 96-127 | 58 | Cyan-ish |
| 128-159 | 44 | Blue-ish |
| 160-191 | 33 | Magenta-ish |
| 192-223 | 34 | Red-ish |
| 224-255 | 16 | Dark gray |

Implementation options:

1. **Inside Beam X PLD:** combinatorial equations from the X counter MSBs -> 6 index bits (macrocell budget permitting).
2. **External 74HC logic:** e.g. take X[7:5] through a small PROM/GAL or hardwired map into 6 bits (lab-only, not part of the final BOM).
3. **External tiny MCU (not Retr01 S1):** temporary pattern generator driving A[5:0] only. Remove before calling the lab PLD-pure. Prefer A or B.1 for faithfulness.

During blanking, force index **0** (or hold last) so the DAC is quiet in sync regions if cleaner composite is desired.

### Method C - Vertical stripes / checkerboard

- **Vertical bars:** index = `Y[5:0]` or `Y[7:2]` (from Beam Y).
- **Checkerboard:** index = `{X[4] xor Y[4], fixed bits...}` or two-level (e.g. 0 vs 48).

Useful to verify both counters are alive and not swapped.

### Method D - Raster HUD block

Force a bright index only when `X` and `Y` are in a small rectangle (e.g. corner box). Everything else index 0. Confirms coordinate compares in the PLD.

### Method E - Manual override + PLD (debug mux)

Jumper:

- Position **LAB**: index from DIP switches.
- Position **BEAM**: index from PLD bar generator.

Proves PROM/DAC/AD724 independently of beam equations.

---

## 6. Color PROM contents (locked kit)

Burn **64 bytes** at addresses 0...63. Each byte is packed **R3G3B2** = `{R2 R1 R0 G2 G1 G0 B1 B0}`.

Preview RGB (8-bit, not PROM bytes), GIMP/Aseprite palettes, and the C SoT live under [`../general_docs/palette/`](../general_docs/palette/README.md). Convert each preview RGB to R3G3B2 for the burner, or use Studio's `<stem>_prom.bin` (64 packed bytes) when a project export exists. Indices **0...15** darkest -> **48...63** brightest.

**OTP note:** AT27C256R is one-time programmable in normal use. Verify the image on a burner that supports 27C256 VPP/VCC before committing, or use a UV-erasable equivalent only if the process allows it. Adafruit's UPDI Friend cannot program this part.

---

## 7. AD724 wiring checklist

| Pin | Name | Tier A connection |
| --- | --- | --- |
| 1 | STND | H = NTSC, L = PAL |
| 2 | AGND | Analog ground |
| 3 | FIN | FSC clock / crystal |
| 4 | APOS | +5 V analog |
| 5 | ENCD | High (encode on) |
| 6 | RIN | From red DAC node |
| 7 | GIN | From green DAC node |
| 8 | BIN | From blue DAC node |
| 9 | CRMA | Optional S-video C later |
| 10 | COMP | -> RCA center (75 ohm load world) |
| 11 | LUMA | Optional S-video Y later |
| 12 | SELECT | **Low** for FSC mode |
| 13 | DGND | Digital ground |
| 14 | DPOS | +5 V digital |
| 15 | VSYNC | H/V mode: from Beam Y. CSYNC mode: held inactive per datasheet |
| 16 | HSYNC | H/V mode: HSYNC. CSYNC mode: composite sync from Beam X |

**CSYNC mode (often easiest on one wire):** Beam X outputs CSYNC -> AD724 pin 16. Pin 15 held high enough to select CSYNC recipe.

---

## 8. Suggested bring-up order

1. **Power + DOT only** - scope oscillator. No other ICs if needed.
2. **Beam X alone** - program minimal H counter + HSYNC. Scope line rate.
3. **Add Beam Y** - V counter + VSYNC. Scope ~60 Hz. Confirm TV syncs to black (PROM still optional).
4. **PROM + DAC + fixed index (DIP)** - solid color on RGB pins. Measure ~0.7 Vpp.
5. **AD724** - composite solid color on TV.
6. **Index from beam (bars)** - full Tier A demo.
7. **Only then** plan Tier B (Compositor), not before.

### Failure quick map

| Symptom | Check |
| --- | --- |
| No sync / rolling | DOT present? H/V pulse widths? AD724 STND/FSC match? |
| Sync but black | PROM OE/CE, index stuck, DAC wiring, ENCD low |
| Wrong colors | PROM image, R vs G vs B resistor order, R3G3B2 bit order |
| Chroma unlock | FSC frequency/amplitude, SELECT pin, analog grounding |
| Noise / sparkle | Decoupling, breadboard inductance, DOT series resistor |

---

## 9. Out of scope (next tiers)

| Later | Adds |
| --- | --- |
| **Tier B** | Third ATF22V10 as Compositor (priority / index formalization) |
| **Tier C** | MCU-S1 + field SRAM + HC573 for VBlank pattern fill |
| **Full video** | Interleaved VRAM, HC157s, PHI2, MCU-M, 6502, cart |

Those stay out of the first breadboard until Tier A is boringly reliable.

---

## 10. One-page wiring summary

```text
[ 5.369318 MHz osc ] --33 ohm--> Beam X CLK
                              Beam X -> HSYNC/CSYNC
                              Beam X -> line tick -> Beam Y CLK
                              Beam Y -> VSYNC

Index[5:0] <- DIP switches -or- f(X,Y) from PLDs
        |
        v
 AT27C256R (A[5:0], other A grounded, CE/OE active)
        | Q[7:0] R3G3B2
        v
 Resistor DAC -> R, G, B (~0.7 Vpp)
        |
        v
     AD724 RIN/GIN/BIN + HSYNC/VSYNC + FSC
        |
        v
     COMP -> RCA -> TV
```
