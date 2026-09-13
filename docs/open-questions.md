# Open questions and TBDs

Items still open, plus how to close them. Update this file when a decision lands, then fold the answer into the matching doc.

## Still open

### 4. Animated tiles (BG attr bit 7)

**Open:** Software iterates a set of tile patterns. Timing and where the set lives are undefined.

**Suggestion:** v1 can be CPU-side only: a small table of (tile slot, frame count, period, bank/pattern list) updated in NMI. Hardware bit 7 is just a hint for tools and for your updater. Hardware auto-animate can wait.

**Touches:** `video-graphics.md`, `software-api.md`

### 5. Sprite attr bits 6 and 7

**Open:** Reserved. No use yet.

**Suggestion:** Leave them zero in tools. Do not document game meaning until a real need appears (priority, collision class, or entity link). Avoid burning them on vanity flags.

**Touches:** `video-graphics.md`

### Entity / PA packed sizes (from cart map)

**Partly sized:** Maxed entity def = **288 B**. Canonical capacity write-up is in `memory.md`. Instance + `PA` byte schemas still open.

**Suggestion:** Freeze instance + `PA` next.

**Touches:** `memory.md`, `software-api.md`

## Resolved (kept for history)

### 1. AVR duty split

**Resolved:** Three AVR128DB28. **MCU-M** soft `$7Fxx` / SPI / cart I2C / RDY. **MCU-S1** OAM + sprite field + HBlank BG0. **MCU-S2** pads + APU PWM. Entity logic stays on the 6502. See `hardware.md`.

### 2. Hardware video scope

**Resolved (baseline):** Beam X / Beam Y / Compositor on 3x ATF22V10. Color PROM index in compositor. VRAM mux on PHI2. Sprite/BG0 field on S1. Details in `hardware.md`.

### 3. Cartridge binary map

**Resolved:** `.retr01` layout in `memory.md`. I/O page `$7F00-$7FFF`.

### 6. Attribute index ranges

**Resolved:** Bank and palette attr fields are **0-3**. Documented in `video-graphics.md`.

### 7. Screen budgets (BG1 / BG0)

**Resolved:** **32** present BG1 screens per world. **0..8** BG0 screens per world. Max-fill free space ~**69.6 KB**. See `memory.md`.

### 8. Console-side cart flashing

**Resolved for v1:** USB-C bench flasher on the 36-pin edge (`hardware.md`). Console-seated flash later.

### 9. Interleaved VRAM timing

**Resolved (baseline):** PHI2 high = CPU `$7F10`-`$7F12`. PHI2 low = BG fetch. 3x HC157. Tear rule in `hardware.md`.

### 10. Palette buffer and row select

**Resolved:** `$7F08` = `PAL_ROW` (0-7, BG+sprite paired, shared backdrop). `$7F09` = `PAL_DATA` auto-inc. Load active buffer from cart in **vblank only**. See `video-graphics.md`.

### 11. Entity and sprite budget coupling

**Resolved:** Spawn / frame-change **fails** if OAM cannot fit. Scanline overflow drops later OAM entries on that line (no flicker). See `software-api.md` and `video-graphics.md`.

### 12. Platformer physics scope

**Resolved (v1):** Axis-separated movement, solids via attr bit 6, AABB hitboxes, simple gravity/jump. No slopes, moving platforms, or one-ways yet. See `software-api.md`.

### 13. IC budget

**Resolved:** **16** motherboard + **2** cart = **18**. AD724 and flasher outside that count. BOM in `hardware.md`.

### 14. Scroll edge cases

**Resolved (baseline):** Empty / missing screen slots fill with the **current backdrop color** (shared BG color index 0 of the active palette row). No map wrap in v1. Camera clamps. Corner DMA may take more than one frame. See `world-scrolling.md`.

## Decision log

| Date | Item | Decision |
| --- | --- | --- |
| 2026-09-13 | PRG banking | No PRG banks. Single flat 32 KB PRG region. |
| 2026-09-13 | Cart image | Adopt prior `.retr01` map into `memory.md`. |
| 2026-09-13 | BG0 screens | Firm cap 0..8 present BG0 screens per world. |
| 2026-09-13 | BG1 screens | Cap cut from 48 to **32** present BG1 screens per world (~69.6 KB free at max fill). |
| 2026-09-13 | Entity catalog | No hard cart limit on entity types. Signal **100+** distinct defs with headroom. CHR unique-maxed ~16/world without tile reuse. |
| 2026-09-13 | PRG vs I/O | Contiguous PRG `$8000-$FFFF`. I/O `$7F00-$7FFF`. |
| 2026-09-13 | MCU set | 3x AVR128DB28 (M / S1 / S2). Old role split. |
| 2026-09-13 | Color path | AT27C256R master colors. Cart holds indices only. |
| 2026-09-13 | Composite | AD724 outside IC-18 (better CSYNC or H/V fit than AD725). |
| 2026-09-13 | Sync out | One header supports CSYNC or H/V via jumper/cable mode. |
| 2026-09-13 | Branding | One product name: Retr01 (no A/C SKU split in docs). |
| 2026-09-13 | Attr fields | Bank/palette nibbles are 0-3. |
| 2026-09-13 | Flash v1 | Bench USB-C flasher first. |
| 2026-09-13 | Palette ports | `$7F08` row, `$7F09` data, vblank loads, paired BG/sprite rows. |
| 2026-09-13 | OAM pressure | Fail spawn on OAM shortfall. Drop overflow sprites per scanline. |
| 2026-09-13 | Platformer v1 | Axis-separated, bit-6 solids, AABB, gravity/jump. No slopes/movers/one-ways. |
| 2026-09-13 | Empty screens | Fill with active backdrop (BG color index 0). Clamp, no wrap. |
