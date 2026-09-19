# Retr01 Tier B - Beam + Compositor video lab

**Prerequisite:** Tier A working (stable sync, PROM + DAC + AD724, solid color or bars). 
**Scope:** What changes for Tier B only. Clocks, DAC, AD724, color kit, and "omit cart/CPU/AVR" rules stay as in [tier-a-video-lab.md](tier-a-video-lab.md).

**Goal:** Add the third ATF22V10 as **Compositor** so the 6-bit kit index is produced the way the real design intends (priority -> PROM address), still with **no cart, CPU, AVRs, or SRAMs**.

```text
Tier A: index <- DIP or ad-hoc f(X,Y) in Beam X
Tier B: index <- Compositor PLD (priority / mux / optional 1-dot latch)
         Beam X / Beam Y unchanged in role
```

---

## 1. Parts added

| Qty | Part | Role |
| --- | --- | --- |
| **+1** | **ATF22V10** | **Compositor** - color index out, priority between test layers, safe stubs for future decode |

Still **not** required: W65C02, AVRs, SRAMs, HC157/573/574, cart parts, PHI2 oscillator.

**Total PLDs now:** 3 (Beam X, Beam Y, Compositor) - matches the locked motherboard PLD count.

---

## 2. Compositor job (design vs lab)

### Full console (later)

| Owns | Notes |
| --- | --- |
| Priority | Sprite field vs BG1 vs BG0 show-through |
| Color PROM index | 6-bit kit index to AT27C256R |
| MAP A14-A18 | Cart high address from MAP port |
| `LE_7F02` / `03` / `04` | Scroll X latch, scroll Y, raster compare loads |
| `SEL_VRAM`, `LE_MAP`, `SEL_SOFT*` | Decode / residual `/OE` |

Hard LE and beam paths **never** go through an MCU.

Macrocell pressure (full design): scroll-Y + other registered work can approach **~21 of 30** macrocells across the three 22V10s. That budget is a hard limit. A **1-dot registered** color index is preferred if fit allows. See `ic_behavior/ATF22V10.md`.

### Tier B lab (subset)

Required:

1. **Inputs:** Beam X/Y counts or blanking flags. One or two **test layer** pixel/index sources.
2. **Output:** `INDEX[5:0]` -> PROM `A[5:0]` (replace DIP / Beam-X bar equations).
3. **Stubs:** CPU A/D, RWB, PHI2, soft SELs, MAP, `/OE` - **tied to safe constants**, not left floating.

Do **not** implement real MAP, soft `$7Fxx`, or VRAM select until later tiers.

---

## 3. Safe pin tying (no CPU bus)

Until a 6502 and PHI2 exist, treat decode-side inputs as static:

| Logical input | Lab default | Why |
| --- | --- | --- |
| PHI2 | Pull to a defined level (e.g. low) or ignore in equations | No interleave yet |
| RWB, CPU A[15:0], CPU D | GND or unused in JEDEC | No bus cycles |
| `SEL_SOFT*`, `SEL_VRAM`, `LE_MAP` | Deasserted in equations / pulled inactive | Avoid accidental enables |
| MAP A14-A18 outputs | Leave unconnected or drive 0 | No cart |
| RESB | Shared clean reset with Beam PLDs | Same power-on story as Tier A |

Only nets the Compositor JEDEC uses for **index** and **priority** are wired. Unused I/O is documented in JEDEC comments so a later full fuse map can reclaim pins.

---

## 4. Feeding INDEX through the Compositor

### Pattern sources (still software-free)

Keep generating **test pixels** the same way as Tier A, but feed them **into** the Compositor instead of straight to the PROM:

| Source | Example | Wired as |
| --- | --- | --- |
| Beam X MSBs | Color bars | `BG1_TEST[5:0]` or a few bits + fixed low bits |
| Beam Y MSBs | Horizontal bands | `BG0_TEST[5:0]` |
| Fixed DIP | Solid underlay | `BACKDROP[5:0]` |
| XORed X/Y | Checker | `SPR_TEST` (1-bit opaque + fixed color) |

### Priority (lab model of the real stack)

Real order (conceptual): sprites over BG1. BG1 color **0** shows BG0. Shared backdrop.

Minimal lab priority (combinational in Compositor):

```text
if SPR_OPAQUE:
    INDEX = SPR_COLOR # e.g. fixed kit index 48 or from a small pattern
else if BG1_INDEX != 0:
    INDEX = BG1_INDEX # bars from X
else:
    INDEX = BG0_INDEX # bands from Y, or DIP backdrop
```

- Use **kit index 0** as transparent for the BG1 test layer (matches design: BG1 color 0 -> show-through).
- Force `INDEX = 0` in HBlank/VBlank if a quiet DAC during sync is desired (optional).

### Optional 1-dot index latch

If macrocells allow, register `INDEX` on **DOT** so PROM address is stable for a full pixel. Helps DAC/PROM settling on breadboards. If short on product terms, pure combinational index is acceptable for Tier B.

### Remove Tier A direct drive

- Disconnect DIP->PROM (or leave as a jumpered **override** for debug).
- Delete bar equations from Beam X that drove PROM A[5:0] directly. Beam X should only do raster/sync (+ optional scroll-Y stub).

---

## 5. Rules (Tier B only)

### Required

- All three PLDs are programmed before power-up. Beam X/Y JEDECs that already worked in Tier A stay.
- Index generation moves into the Compositor only. Sync is not rewritten from scratch unless needed.
- Priority is proven with a **visible** test (sprite-colored box over bars, BG1 holes at index 0 showing BG0 bands).
- `INDEX[5:0]` (or PROM A pins) is scoped while patterns change, confirming the Compositor, not only the TV.
- Analog path (PROM -> DAC -> AD724) stays unchanged from Tier A.

### Forbidden

- Partial CPU bus "for later" on Compositor pins without Hi-Z rules (causes fights when silicon is added).
- Full MAP / `SEL_SOFT*` decode on this tier (stub those paths).
- HSYNC/VSYNC/CSYNC routed through the Compositor unless the JEDEC intentionally does that. Design keeps beam timing on Beam X/Y.
- HC157 / VRAM / S1 before the Tier A+B index path is boringly stable.
- Treating Tier B as almost full video. Field SRAM and HBlank BG0 fill are still Tier C.

---

## 6. Suggested checks after wiring

1. **Bypass mode (optional jumper):** DIP -> PROM still works -> analog path intact.
2. **Compositor solid:** equations force `INDEX = 48` (or any bright kit entry) -> full screen that color.
3. **Bars via Compositor:** BG1_TEST from X, priority = BG1 only -> same bars as Tier A, but path is PLD3 -> PROM.
4. **Priority:** opaque test sprite rectangle over bars. Outside rectangle, bars. BG1 index 0 regions show BG0/DIP.
5. **Blanking:** if implemented, index 0 (or hold) during H/V blank - composite should stay stable.

### Failure hints

| Symptom | Likely cause |
| --- | --- |
| Tier A worked, Tier B black | INDEX not reaching PROM. OE/CE. Wrong PLD outputs |
| Wrong layer always wins | Priority equations inverted or SPR_OPAQUE stuck |
| Sparkle on edges | Combinational index glitch - try 1-dot latch or simplify terms |
| Sync lost after adding PLD | DOT loading, shared reset, power dip - not compositor logic |

---

## 7. Wiring delta (from Tier A)

```text
Beam X -- raster / CSYNC ---> (as Tier A) ---> AD724
Beam Y -- VSYNC -----------> (as Tier A) ---> AD724

Beam X,Y counts / blank ---> Compositor
Test patterns (X bars, Y bands, DIP, box) ---> Compositor

Compositor INDEX[5:0] ---> PROM A[5:0]
         (CPU/MAP/SEL stubs tied safe)

PROM -> DAC -> AD724 -> RCA (unchanged)
```

---

## 8. Exit criteria -> Tier C

Tier B is done when:

- Sync still locks (no regression from Tier A).
- PROM index is **only** from the Compositor (or documented debug mux).
- At least one **priority** demo is visible and matches the equation intent.
- No floating decode inputs. JEDEC stubs are documented.

**Next:** [tier-c-video-lab.md](tier-c-video-lab.md) - MCU-S1 + field SRAM + HC573, VBlank pattern fill, optional BG0 line regions on the **same** field chip.
