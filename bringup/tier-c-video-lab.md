# Retr01 Tier C — S1 field fill video lab

**Prerequisite:** Tier B working (Beam X/Y + Compositor → PROM → DAC → AD724, priority demo OK).  
**Scope:** Tier C deltas only. Clocks, PLD roles, color kit, AD724, and “no cart / no 6502 / no MCU-M/S2” stay as in A/B.

**Goal:** MCU-S1 writes the **sprite field** in VBlank (and optionally **BG0** lines in HBlank) so the picture includes simple backgrounds plus a couple of small sprites — still a video-only lab.

```text
Tier B:  patterns from beam math / DIP into Compositor
Tier C:  S1 firmware fills field SRAM (sprites) + optional BG0 line buffers
         Compositor composites those buffers with test BG1 (or simplified BG1 source)
```

**SoT:** `general_docs/hardware.md`, `general_docs/video-graphics.md`, `general_docs/ic-comms-risks.md`, `ic_behavior/AVR128DB28.md`, `ic_behavior/AS6C62256.md`, `ic_behavior/74HC573.md`.

---

## 1. What to add

| Qty | Part | Role |
| --- | --- | --- |
| 1 | **AVR128DB28** | **MCU-S1 only** @ internal 24 MHz HFOSC |
| 1 | **AS6C62256-55** | **Field** SRAM (#3 role): sprite field **and** BG0 ping-pong line regions |
| 1 | **74HC573** | Field A[7:0] latch (S1 multiplexed **AD[7:0]** + **ALE**) — matches locked design |
| 0–1 | **74HC574** | Scroll X; optional — hardwire scroll = 0 |

**Programmer:** UPDI (e.g. Adafruit UPDI Friend) for the AVR only.

**Still omit:** W65C02, MCU-M, MCU-S2, cart flash, 24C64, pads, audio, PHI2, HC157s (those belong to Tier E).

### Field memory model (locked design)

The design uses **one** field AS6C62256 for:

- Full sprite field buffer (VBlank fill)
- BG0 next-line **ping-pong** regions (HBlank fill)

A second SRAM for BG0 is **not** the product path. If a temporary second chip is used for an early experiment, treat it as disposable and migrate to single-chip regions before calling Tier C done.

---

## 2. Timing rules (locked design — keep them)

| Work | When | Who |
| --- | --- | --- |
| **Sprite field** | Entire overlay, **one pass in VBlank** | S1 → field SRAM |
| **BG0 next line** | **HBlank only** (ping-pong) | S1 |
| BG1 / compose | Active display | Beam + Compositor (+ whatever BG1 source is still used) |

- Sprites are **not** rebuilt per scanline in HBlank.
- HBlank budget is small: **next BG0 line only**.
- No MCU-M yet → **no OAM SPI**; S1 synthesizes its own OAM-like tables or writes the field directly from firmware.
- Design caps (orientation, not hard lab limits): up to **64** sprites, **16 per scanline**, **8×8**, **2bpp** on the full system. Lab art can be simpler (direct field pixels / few metasprites).

### S1 bus discipline

- **AD[7:0]** hi-Z whenever S1 is not in an owned write window.
- Sequence: present address → **ALE** → present data → **`/WE`**, with dead time so beam **`/OE`** never overlaps **`/WE`**.
- Idle-safe: **ALE** low, **`/WE`** high when not writing.
- Optional **`S1_RDY`**: useful later for M; for Tier C, still good as a “field fill done” debug flag.

---

## 3. Simple pixel art (what Tier C is for)

A full game engine is not required. Aim for something readable:

| Layer | Lab idea | Where it lives |
| --- | --- | --- |
| **BG1** | Sky + ground bands, or a short tile strip | Still beam/Compositor test pattern until Tier E VRAM exists |
| **BG0** | Distant hills / clouds (parallax later) | Optional: S1 fills **one line per HBlank** into ping-pong regions on the **field** chip |
| **Sprites** | 1–2 small figures (e.g. 2×2 or 2×3 of 8×8 tiles): stand + simple walk frames | **Field SRAM**, rebuilt (or page-flipped) in **VBlank** |

Practical approach:

1. Store a few **8×8** 2bpp tiles (or even 1-bit masks + fixed kit colors) in S1 flash/`.rodata`.
2. In VBlank, blit 1–2 metasprites into known field coordinates (transparent = kit index **0** or a chosen key).
3. Animate by swapping frame tables every N frames (VBlank counter).
4. Keep colors as **kit indices 0…63** (PROM), not free RGB.

That proves: S1 fill timing, field read during active display, Compositor priority (sprites over BG, BG1 index 0 → BG0).

**Avoid for first firmware:** full 128×120 software framebuffer every frame, per-scanline sprite eval in HBlank, or streaming from a cart (no cart yet).

---

## 4. Firmware sketch (S1)

```text
on reset:
  AD / A high-Z, ALE=0, /WE=1
  wait until VBL edge stable (from Beam PLD)

main:
  if rising VBlank:
    build_sprite_field()   // blit sprites from tile tables
    // optional: prepare BG0 line 0 for first HBlank
  if HBlank and BG0 enabled:
    fill_next_bg0_line()   // only the upcoming line
  // idle / animate frame counters
```

- **build_sprite_field:** clear transparent, then draw 1–2 sprites (and maybe a static prop).
- Finish **before** active display; glitches at the top of the frame mean the fill is too late — reduce work or double-buffer field regions if the hardware map allows.
- Scope `/WE` bursts: they should cluster in VBlank (and short HBlank for BG0), not during the whole frame.

---

## 5. Compositor integration

- **Sprite layer** input = field SRAM data path wired for the beam.
- **BG1** = keep Tier B bar/band generator until Tier E.
- **BG0** = ping-pong line the beam reads when BG1 index is 0.
- Priority stays the Tier B model: opaque sprite pixel wins; else BG1 if non-zero; else BG0/backdrop.

If field wiring is not finished, blitter code can still be developed with a logic analyzer on AD/ALE/`/WE`, then hang the SRAM.

---

## 6. Do / don’t

### Do

- Hi-Z AD outside write windows; mutual exclusion of field `/OE` (beam) vs `/WE` (S1).
- Prefer **55 ns** SRAM; keep field traces short on a breadboard as much as possible.
- Start with **one static sprite**, then animation, then a second sprite, then BG0 lines.
- Decouple the AVR heavily; UPDI only when a perfect picture is not required.

### Don’t

- Don’t fill the field in HBlank or active display.
- Don’t run long SPI/I2C on S1 during the video windows that matter (nothing to talk to yet).
- Don’t add MCU-M “just for OAM SPI” until field-direct blits work (that is Tier D).
- Don’t expect Host Play / Studio art pipelines; this is hand-authored tiles in firmware.
- Don’t invent a second field SRAM as the long-term BG0 home.

---

## 7. Bring-up order

1. S1 alone: blink LED / GPIO; UPDI reliable.
2. ALE + `/WE` state machine with AD hi-Z defaults — scope before SRAM.
3. Write/read **field SRAM** with beam OE disabled — data integrity.
4. Enable beam read of field + Compositor sprite path — static solid sprite block.
5. Tile blits → one metasprite → 2-frame walk.
6. Optional BG0 HBlank line fill + show-through where BG1 index is 0.

### Pass criteria

- Stable sync (no regression from B).
- At least **one** clear sprite over BG from **S1-written** field memory.
- Optional: second sprite and/or BG0 line show-through.
- `/WE` activity confined to VBlank (and HBlank only for BG0).

---

## 8. Wiring delta (from Tier B)

```text
MCU-S1  AD[7:0] ──┬── HC573 ── field A[7:0]
        A[14:8] ──┤
        ALE, /WE ─┘
                  └── AS6C62256 field (D, A, CE/OE/WE as per PLD)
                      (sprite field + BG0 ping-pong regions)

Beam / Compositor ── read field during active display (PLD /OE)
S1 ── fill field during VBlank only
S1 ── optional BG0 next-line fill during HBlank only

PROM / DAC / AD724 / Beam X,Y  — unchanged
```

---

## 9. After Tier C

**Next:** [tier-d-oam-spi.md](tier-d-oam-spi.md) — MCU-M owns OAM over SPI; S1 stops synthesizing the motion table itself.

Do not start PHI2/VRAM (E), 6502 soft I/O (F), cart (G), or S2 (H) until a static + simple animated sprite from S1 is reliable on composite.
