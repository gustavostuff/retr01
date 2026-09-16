# Retr01 Tier C — S1 Field Fill Video Lab

**Prerequisite:** Tier B working (Beam X/Y + Compositor → PROM → DAC → AD724, priority demo OK).  
**This doc only covers Tier C deltas.** Clocks, PLD roles, color kit, AD724, and “no cart / no 6502 / no MCU-M/S2” stay as in A/B.

**Goal:** MCU-S1 writes the **sprite field** in VBlank (and optionally **BG0** lines in HBlank) so the picture includes simple backgrounds plus a couple of small sprites—still a video-only lab.

```text
Tier B:  patterns from beam math / DIP into Compositor
Tier C:  S1 firmware fills field SRAM (sprites) + optional BG0 line buffers
         Compositor composites those buffers with test BG1 (or simplified BG1 source)
```

---

## 1. What you add

| Qty | Part | Role |
| --- | --- | --- |
| 1 | **AVR128DB28** | **MCU-S1 only** @ internal 24 MHz HFOSC |
| 1 | **AS6C62256-55** | Sprite **field** SRAM (VBlank fill, beam read in active display) |
| 0–1 | **AS6C62256-55** | Optional second SRAM for BG0 ping-pong line buffers / simple BG store |
| 0–1 | **74HC573** | Field A[7:0] latch if using S1 multiplexed **AD[7:0]** + **ALE** as designed |
| 0–3 | **74HC157** | Only if you implement PHI2-style VRAM mux; **skip** for pure S1+beam lab |
| 0–1 | **74HC574** | Scroll X; optional — hardwire scroll = 0 |

**Programmer:** UPDI (e.g. Adafruit UPDI Friend) for the AVR only.

**Still omit:** W65C02, MCU-M, MCU-S2, cart flash, 24C64, pads, audio, PHI2 (unless you deliberately add interleaved VRAM).

---

## 2. Timing rules (locked design — keep them)

| Work | When | Who |
| --- | --- | --- |
| **Sprite field** | Entire overlay, **one pass in VBlank** | S1 → field SRAM |
| **BG0 next line** | **HBlank only** (ping-pong) | S1 |
| BG1 / compose | Active display | Beam + Compositor (+ whatever BG1 source you still use) |

- Sprites are **not** rebuilt per scanline in HBlank.  
- HBlank budget is small: **next BG0 line only**.  
- No MCU-M yet → **no OAM SPI**; S1 synthesizes its own “OAM” or writes the field directly from firmware tables.  
- Design caps (for orientation, not hard lab limits): up to **64** sprites, **16 per scanline**, **8×8**, **2bpp** on the full system. Lab art can be simpler (direct field pixels / few metasprites).

### S1 bus discipline

- **AD[7:0]** hi-Z whenever S1 is not in an owned write window.  
- Sequence: present address → **ALE** → present data → **`/WE`**, with dead time so beam **`/OE`** never overlaps **`/WE`**.  
- Idle-safe: **ALE** low, **`/WE`** high when not writing.  
- Optional **`S1_RDY`**: useful later for M; for Tier C, still good as a “field fill done” flag for debug.

---

## 3. Simple pixel art (what Tier C is for)

You do **not** need a full game engine. Aim for something readable and fun:

| Layer | Lab idea | Where it lives |
| --- | --- | --- |
| **BG1** | Sky + ground bands, or a short tile strip | Still beam/Compositor test pattern, **or** a second buffer if you add SRAM #2 and teach the Compositor to fetch it |
| **BG0** | Distant hills / clouds (parallax later) | Optional: S1 fills **one line per HBlank** into ping-pong buffers |
| **Sprites** | 1–2 Mario-like figures (e.g. 2×2 or 2×3 of 8×8 tiles): stand + simple walk frames | **Field SRAM**, rebuilt (or page-flipped) in **VBlank** |

Practical approach:

1. Store a few **8×8** 2bpp tiles (or even 1-bit masks + fixed kit colors) in S1 flash/`.rodata`.  
2. In VBlank, blit 1–2 metasprites into known field coordinates (transparent = kit index **0** or your chosen key).  
3. Animate by swapping frame tables every N frames (VBlank counter).  
4. Keep colors as **kit indices 0…63** (PROM), not free RGB.

That is enough to prove: S1 fill timing, field read during active display, Compositor priority (sprites over BG, BG1 index 0 → BG0).

**Avoid for first firmware:** full 128×120 software framebuffer every frame, per-scanline sprite eval in HBlank, or streaming from a cart (no cart yet).

---

## 4. Firmware sketch (S1)

```text
on reset:
  AD / A high-Z, ALE=0, /WE=1
  wait until VBL edge stable (from Beam PLD)

main:
  if rising VBlank:
    build_sprite_field()   // blit Mario-like sprites from tile tables
    // optional: prepare BG0 line 0 for first HBlank
  if HBlank and BG0 enabled:
    fill_next_bg0_line()   // only the upcoming line
  // idle / animate frame counters
```

- **build_sprite_field:** clear transparent, then draw 1–2 sprites (and maybe a static coin/block).  
- Finish **before** active display; if the TV glitches at the top, your fill is too late — reduce work or double-buffer field regions if the hardware map allows.  
- Use a scope on `/WE` bursts: they should cluster in VBlank (and short HBlank for BG0), not during the whole frame.

---

## 5. Compositor integration

- **Sprite layer** input = field SRAM data path you wired for the beam.  
- **BG1** = keep Tier B bar/band generator, **or** replace with a static BG1 buffer once available.  
- **BG0** = ping-pong line the beam reads when BG1 index is 0.  
- Priority stays the Tier B model: opaque sprite pixel wins; else BG1 if non-zero; else BG0/backdrop.

If field wiring is not finished, you can still develop blitter code on the bench with a logic analyzer on AD/ALE/`/WE`, then hang the SRAM.

---

## 6. Do / don’t

### Do

- Hi-Z AD outside write windows; mutual exclusion of field `/OE` (beam) vs `/WE` (S1).  
- Prefer **55 ns** SRAM; keep field traces short on a breadboard as much as possible.  
- Start with **one static sprite**, then animation, then a second sprite, then BG0 lines.  
- Decouple the AVR heavily; UPDI only when not needing a perfect picture.

### Don’t

- Don’t fill the field in HBlank or active display.  
- Don’t run long SPI/I2C on S1 during the video windows you care about (nothing to talk to yet—keep it that way).  
- Don’t add MCU-M “just for OAM SPI” until field-direct blits work.  
- Don’t expect Host Play / Studio art pipelines; this is hand-authored tiles in firmware.

---

## 7. Bring-up order

1. S1 alone: blink LED / GPIO; UPDI reliable.  
2. ALE + `/WE` state machine with AD hi-Z defaults — scope before SRAM.  
3. Write/read **field SRAM** with CPU halted on beam (or beam OE disabled) — data integrity.  
4. Enable beam read of field + Compositor sprite path — static solid sprite block.  
5. Tile blits → one Mario-like metasprite → 2-frame walk.  
6. Optional BG0 HBlank line fill + show-through where BG1 index is 0.

### Pass criteria

- Stable sync (no regression from B).  
- At least **one** clear sprite over BG from **S1-written** field memory.  
- Optional: second sprite and/or BG0 line show-through.  
- `/WE` activity confined to VBlank (and HBlank only for BG0).

---

## 8. Wiring delta (from Tier B)

```text
MCU-S1  AD[7:0] ──┬── HC573 (optional) ── field A[7:0]
        A[14:8] ──┤
        ALE, /WE ─┘
                  └── AS6C62256 field (D, A, CE/OE/WE as per PLD)

Beam / Compositor ── read field during active display (PLD /OE)
S1 ── fill field during VBlank only

Optional: 2nd SRAM + HBlank line fill → BG0 input to Compositor

PROM / DAC / AD724 / Beam X,Y  — unchanged
```

---

## 9. After Tier C

Real console path: **MCU-M** (soft `$7Fxx`, OAM SPI), **6502 + PHI2**, **cart**, interleaved VRAM (HC157s), pads/audio on **S2**.  
Do not start that until a static + simple animated sprite from S1 is reliable on composite.

---

*Retr01 Tier C video lab — S1 field fill + simple BG/sprite pixel art.*
