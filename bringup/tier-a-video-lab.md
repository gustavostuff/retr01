# Retr01 Tier A — Video-only breadboard lab

**Goal:** Stable composite (or RGB) picture on a TV/monitor using only the video path.  
**Out of scope:** Cart, CPU, AVRs, controllers, audio, saves.

Smallest setup that still exercises Retr01’s real color path:

```text
DOT clock → Beam PLDs (raster + sync) → 6-bit color index
                                              ↓
                                    AT27C256R color PROM
                                              ↓
                                    R3G3B2 resistor DAC (~0.7 Vpp RGB)
                                              ↓
                                    AD724 (RGB → NTSC/PAL composite)
                                              ↓
                                    RCA / TV
```

**SoT:** `general_docs/hardware.md`, `general_docs/video-graphics.md`, `ic_behavior/AD724.md`, `ic_behavior/ATF22V10.md`, `ic_behavior/AT27C256R.md`.

---

## 1. Parts list

### ICs (required)

| Qty | Part | Package | Role |
| --- | --- | --- | --- |
| 2 | **ATF22V10** (e.g. ATF22V10CQZ-20PU) | PDIP-24 | **Beam X** and **Beam Y** only |
| 1 | **AT27C256R** | PDIP-28 | 64 master colors, packed R3G3B2 |
| 1 | **AD724** | SOIC-16 on **SOIC→DIP-16 adapter** | RGB → composite |

Optional third ATF22V10 (Compositor) is **not** required for Tier A if the 6-bit index comes from DIP switches or Beam-X equations (see §5).

### Clocks (required)

| Net | Frequency | Notes |
| --- | --- | --- |
| **DOT** | **5.369318 MHz** | Prefer a **canned CMOS oscillator**, not a raw crystal amp |
| **FSC** | **3.579545 MHz** (NTSC) or **4.433618 MHz** (PAL) | Into AD724 `FIN`; `SELECT` pin low = FSC mode |

### Passives / connectors (minimum)

- **Color DAC resistors** (1% metal film), LSB→MSB:
  - R and G: **4.00 kΩ / 2.00 kΩ / 1.00 kΩ**
  - B: **2.00 kΩ / 1.00 kΩ**
  - Each gun: **75.0 Ω** to GND → ~**0.7 Vpp** into AD724
- Series **~33 Ω** on DOT (and on FSC if the source is aggressive)
- Decoupling: **100 nF** at every IC VCC pin; bulk **~220 µF** at 5 V entry
- RCA jack (composite from AD724 `COMP`)
- Optional: header for raw RGB + sync (J2-style) before AD724 is trusted
- 5 V supply capable of a few hundred mA; clean ground

### Optional

| Part | When |
| --- | --- |
| **74HC14** | Only if DOT/FSC edges are soft or reset chatters. Skip if canned oscillators are already square. |
| Third **ATF22V10** | Only if Compositor-style index logic is preferred over discrete/DIP index (§5). |
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
| Beam X | Stable H counter 0…340, HSYNC/CSYNC at line rate |
| Beam Y | Stable V counter 0…261, VSYNC at ~60.1 Hz |
| Color index path | Known 6-bit value reaches PROM A[5:0] |
| PROM + DAC | RGB levels ~0–0.7 V, colors match kit when index changes |
| AD724 | Lockable composite on a TV (color bars or solid field) |

If any of those fail, fix that layer before adding S1, VRAM, or a CPU.

---

## 3. Do / don’t

### Do

1. **Program PLDs and the color PROM before first power-up** with the CPU-side bus absent. Blank PLDs and a random PROM make debugging impossible.
2. **Tie unused PROM address lines A[14:6] (and any unused) to GND.** Only A[5:0] carry the color index for 64 entries.
3. **Use CSYNC or H/V consistently.** For AD724 CSYNC mode: drive composite sync into `HSYNC`, hold `VSYNC` inactive (> ~2 V) per datasheet recipe. Do not wire both meanings onto the same pin at once.
4. **Keep the analog island short:** PROM → DAC resistors → AD724 R/G/B → RCA. Separate digital switching noise from that path as much as breadboard allows.
5. **Power AD724 analog and digital** (`APOS`/`DPOS`, `AGND`/`DGND`) correctly; enable encode (`ENCD` = H).
6. **Set `STND`** for the region (H = NTSC, L = PAL) and match FSC frequency.
7. **Start with a solid color** (fixed index), then color bars, then spatial patterns. One change at a time.
8. **Scope DOT, HSYNC, VSYNC/CSYNC, and one RGB gun** before blaming the TV.

### Don’t

1. **Don’t hang cart flash, RAM, or MCU data pins on any shared bus.** Tier A has no bus fights by design.
2. **Don’t run PHI2 or fake a 6502** yet. Interleaved VRAM is a later problem.
3. **Don’t feed AD724 digital 5 V levels on R/G/B.** It expects ~0.7 Vpp video from the resistor DAC.
4. **Don’t leave HC-style enables floating** if any 74xx is added later; for pure Tier A simply omit them.
5. **Don’t use push-pull fights on sync.** One driver for HSYNC/CSYNC, one for VSYNC.
6. **Don’t expect Studio/Emu Play behavior.** This lab only proves the analog/raster path.
7. **Don’t skip decoupling** on a breadboard at 5 MHz — noise looks like bad colors or unlocked chroma.
8. **Don’t program the PROM with random data.** Use the locked 64-color kit (or a simple ramp) so index N always means a known color.

---

## 4. Raster targets (from design)

| Parameter | Value |
| --- | --- |
| Dot clock | 5.369318 MHz |
| Line length | **341** dots |
| Frame | **262** lines |
| Frame rate | ~**60.098 Hz** |
| Logical playfield | 128×120 (2× scaled to 256×240 on the full system) |

Tier A does **not** need correct active/blanking pixel art yet — it needs **stable** H and V timing that a TV will lock to. Approximate NTSC-ish blanking is enough for first light:

- Rough guide: active ~256 dots, rest of 341 = blanking/sync region (tune while watching the scope and TV).
- Vertical: active ~240 lines, rest of 262 = blanking/VSYNC region.

Exact Retr01 HBlank/VBlank equations live in the Beam X/Y JEDEC files for the full design. For the lab, implement a **minimal** counter + sync pulse generator in the two PLDs.

### Beam PLD split (design intent)

| PLD | Owns |
| --- | --- |
| **Beam X** | Dot counter inside the line (0…340), HBlank, HSYNC/CSYNC |
| **Beam Y** | Line counter (0…261), VBlank, VSYNC |

Clock Beam X from **DOT**. Clock Beam Y from a line tick (e.g. end-of-line pulse from Beam X).

---

## 5. Feeding the 6-bit color index

The PROM is addressed by a **6-bit kit index** (0…63). Video never pokes RGB at runtime; it only selects which of the 64 burned colors is shown.

```text
Index[5:0]  →  PROM A[5:0]
PROM Q[7:0] →  {R2 R1 R0 G2 G1 G0 B1 B0}  (R3G3B2 pack)
            →  resistor DAC → R, G, B analog
```

Unused PROM address pins → **GND**.  
`CE#` / `OE#` held active (or hard-wired active for the lab).

### Method A — DIP switches (simplest first light)

1. Wire six switches (or jumpers) to PROM A[5:0] with pull-downs (or pull-ups — pick one and stay consistent).
2. Power up with index `000000` (kit black / index 0).
3. Flip to index `110000` (48) or `111111` (63) and confirm brightness/color changes on RGB and composite.

**Pros:** Zero PLD equation risk for color.  
**Cons:** Full-screen solid color only.

### Method B — Color bars from Beam X (recommended Tier A demo)

Derive index from horizontal position so one line shows a bar pattern:

| Region of active X (example) | Index | Intended look |
| --- | --- | --- |
| 0–31 | 48 | White-ish (kit 48) |
| 32–63 | 52 | Yellow-ish |
| 64–95 | 55 | Green-ish |
| 96–127 | 58 | Cyan-ish |
| 128–159 | 44 | Blue-ish |
| 160–191 | 33 | Magenta-ish |
| 192–223 | 34 | Red-ish |
| 224–255 | 16 | Dark gray |

Implementation options:

1. **Inside Beam X PLD:** combinatorial equations from the X counter MSBs → 6 index bits (macrocell budget permitting).
2. **External 74HC logic:** e.g. take X[7:5] through a small PROM/GAL or hardwired map into 6 bits (lab-only; not part of the final BOM).
3. **External tiny MCU (not Retr01 S1):** temporary pattern generator driving A[5:0] only; remove before calling the lab PLD-pure. Prefer A or B.1 for faithfulness.

During blanking, force index **0** (or hold last) so the DAC is quiet in sync regions if cleaner composite is desired.

### Method C — Vertical stripes / checkerboard

- **Vertical bars:** index = `Y[5:0]` or `Y[7:2]` (from Beam Y).
- **Checkerboard:** index = `{X[4] xor Y[4], fixed bits…}` or two-level (e.g. 0 vs 48).

Useful to verify both counters are alive and not swapped.

### Method D — Raster HUD block

Force a bright index only when `X` and `Y` are in a small rectangle (e.g. corner box). Everything else index 0. Confirms coordinate compares in the PLD.

### Method E — Manual override + PLD (debug mux)

Jumper:

- Position **LAB**: index from DIP switches.
- Position **BEAM**: index from PLD bar generator.

Proves PROM/DAC/AD724 independently of beam equations.

---

## 6. Color PROM contents (locked kit)

Burn **64 bytes** at addresses 0…63. Each byte is packed **R3G3B2** = `{R2 R1 R0 G2 G1 G0 B1 B0}`.

Studio/Emu kit preview RGB (8-bit) for reference — **not** the PROM bytes themselves:

```text
# idx   preview RGB hex
 0 000000  1 290514  2 2A0507  3 230F06  4 1E1306  5 1A1605  6 141807  7 061A07
 8 051A13  9 071918 10 08181C 11 071722 12 030B3D 13 16033A 14 20052D 15 260420
16 363636 17 740A40 18 77091A 19 693512 20 5D3F0E 21 514617 22 424C19 23 13511A
24 16503F 25 114E4D 26 164D58 27 164A66 28 163794 29 472990 30 5F167D 31 6C115F
32 949494 33 C04A7A 34 C54A4D 35 B8601B 36 A27326 37 8F7E2F 38 77872D 39 209030
40 2E8E72 41 318B89 42 1F889C 43 2483B5 44 4D77D7 45 7E6AD3 46 9D5DBF 47 B352A0
48 FFFFFF 49 F1A2BB 50 F1A6A1 51 F1A983 52 EEAC44 53 D4BA33 54 B0C841 55 73D275
56 22D0A6 57 3BCDC9 58 48C9E4 59 88C4ED 60 A4BDEF 61 BBB5F1 62 D5A9EF 63 F09BDD
```

Convert each preview RGB to R3G3B2 for the burner, or use Studio’s `<stem>_prom.bin` (64 packed bytes) when a project export exists. Indices **0…15** darkest → **48…63** brightest.

**OTP note:** AT27C256R is one-time programmable in normal use. Verify the image on a burner that supports 27C256 VPP/VCC before committing, or use a UV-erasable equivalent only if the process allows it. Adafruit’s UPDI Friend cannot program this part.

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
| 10 | COMP | → RCA center (75 Ω load world) |
| 11 | LUMA | Optional S-video Y later |
| 12 | SELECT | **Low** for FSC mode |
| 13 | DGND | Digital ground |
| 14 | DPOS | +5 V digital |
| 15 | VSYNC | H/V mode: from Beam Y; CSYNC mode: held inactive per datasheet |
| 16 | HSYNC | H/V mode: HSYNC; CSYNC mode: composite sync from Beam X |

**CSYNC mode (often easiest on one wire):** Beam X outputs CSYNC → AD724 pin 16; pin 15 held high enough to select CSYNC recipe.

---

## 8. Suggested bring-up order

1. **Power + DOT only** — scope oscillator; no other ICs if needed.
2. **Beam X alone** — program minimal H counter + HSYNC; scope line rate.
3. **Add Beam Y** — V counter + VSYNC; scope ~60 Hz; confirm TV syncs to black (PROM still optional).
4. **PROM + DAC + fixed index (DIP)** — solid color on RGB pins; measure ~0.7 Vpp.
5. **AD724** — composite solid color on TV.
6. **Index from beam (bars)** — full Tier A demo.
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

Do not mix those into the first breadboard until Tier A is boringly reliable.

---

## 10. One-page wiring summary

```text
[ 5.369318 MHz osc ] --33Ω--→ Beam X CLK
                              Beam X → HSYNC/CSYNC ──┐
                              Beam X → line tick ──→ Beam Y CLK
                                                     Beam Y → VSYNC

Index[5:0] ← DIP switches  -or-  f(X,Y) from PLDs
        │
        ▼
 AT27C256R (A[5:0], other A grounded, CE/OE active)
        │ Q[7:0] R3G3B2
        ▼
 Resistor DAC → R, G, B (~0.7 Vpp)
        │
        ▼
     AD724 RIN/GIN/BIN + HSYNC/VSYNC + FSC
        │
        ▼
     COMP → RCA → TV
```
