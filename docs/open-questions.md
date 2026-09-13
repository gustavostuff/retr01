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

### 10. Palette buffer and row select

**Open:** How PRG selects the active palette row / buffer and keeps BG/sprite row pairing (shared BG color) is not fully specified beyond soft ports on MCU-M.

**Suggestion:** Lock register meanings under `$7F08`/`$7F09` (and friends) in a short register map. Copy from cart palette planes into the active buffer in vblank only.

**Touches:** `video-graphics.md`, `hardware.md`, `memory.md`

### 11. Entity and sprite budget coupling

**Open:** 64 sprites, 16 per line, entities up to 4x4x4 sprites. How spawning fails under pressure is unclear.

**Suggestion:** Define soft caps (for example max active entities, max sprites claimed by entities) and a clear error or drop policy. Add a scanline overflow behavior (flicker, priority drop, or hard clip) and test it in the platformer sample.

**Touches:** `software-api.md`, `video-graphics.md`

### 12. Platformer physics scope

**Open:** "Simple physics" needs bounds (gravity, jump, one-way, slopes, moving platforms).

**Suggestion:** v1: axis-separated movement, solid tiles via attr bit 6, AABB vs entity hitboxes, no slopes. Add slopes only if a vertical slice demo needs them.

**Touches:** `software-api.md`

### 14. Scroll edge cases

**Open:** Sparse maps, missing neighbor screens, and simultaneous BG0/BG1 window shifts need rules.

**Suggestion:** Define empty slots as solid color or wrap/clamp. Sequence cart DMA so a corner move (three screens) cannot miss a frame. Prefer double-buffering the 2x2 window descriptors if glue allows.

**Touches:** `world-scrolling.md`

### Entity / PA packed sizes (from cart map)

**Partly sized:** A maxed entity **definition** is **288 B** (or **290 B** with a 2 B header). About **3** fit in **1 KB**. See `software-api.md`. Instance records and optional `PA` blobs are still undefined. With **32 BG1 + 8 BG0** caps, max-fill flash headroom is ~**69.6 KB** for entities / other screens.

**Suggestion:** Freeze instance + `PA` next, then document worst-case bytes per world in `memory.md`.

**Touches:** `memory.md`, `software-api.md`

## Resolved (kept for history)

### 1. AVR duty split

**Resolved:** Three AVR128DB28. **MCU-M** soft `$7Fxx` / SPI / cart I2C / RDY. **MCU-S1** OAM + sprite field + HBlank BG0. **MCU-S2** pads + APU PWM. Entity logic stays on the 6502. See `hardware.md`.

### 2. Hardware video scope

**Resolved (baseline):** Beam X / Beam Y / Compositor on 3x ATF22V10. Color PROM index in compositor. VRAM mux on PHI2. Sprite/BG0 field on S1. Details in `hardware.md`. Fine pixel rules can still grow in `video-graphics.md`.

### 3. Cartridge binary map

**Resolved:** `.retr01` layout in `memory.md`. I/O page `$7F00-$7FFF`.

### 6. Attribute index ranges

**Resolved:** Bank and palette attr fields are **0-3**. Documented in `video-graphics.md`.

### 7. Screen budgets (BG1 / BG0)

**Resolved:** **32** present BG1 screens per world. **0..8** BG0 screens per world. Max-fill free space ~**69.6 KB**. See `memory.md`.

### 8. Console-side cart flashing

**Resolved for v1:** USB-C bench flasher on the 36-pin edge (`hardware.md`). Console-seated flash later. `WE#` already on the edge.

### 9. Interleaved VRAM timing

**Resolved (baseline):** PHI2 high = CPU `$7F10`-`$7F12`. PHI2 low = BG fetch. 3x HC157. Tear rule: do not poke a cell the beam is fetching. Off-screen slots and VBlank are safe. See `hardware.md` / prior memory notes.

### 13. IC budget

**Resolved:** **16** motherboard + **2** cart = **18**. AD724 and flasher sit outside that count. BOM in `hardware.md`.

## Decision log

| Date | Item | Decision |
| --- | --- | --- |
| 2026-09-13 | PRG banking | No PRG banks. Single flat 32 KB PRG region. |
| 2026-09-13 | Cart image | Adopt prior `.retr01` map into `memory.md`. |
| 2026-09-13 | BG0 screens | Firm cap 0..8 present BG0 screens per world. |
| 2026-09-13 | BG1 screens | Cap cut from 48 to **32** present BG1 screens per world (~69.6 KB free at max fill). |
| 2026-09-13 | PRG vs I/O | Contiguous PRG `$8000-$FFFF`. I/O `$7F00-$7FFF`. |
| 2026-09-13 | MCU set | 3x AVR128DB28 (M / S1 / S2). Old role split. |
| 2026-09-13 | Color path | AT27C256R master colors. Cart holds indices only. |
| 2026-09-13 | Composite | AD724 outside IC-18 (better CSYNC or H/V fit than AD725). |
| 2026-09-13 | Sync out | One header supports CSYNC or H/V via jumper/cable mode. |
| 2026-09-13 | Branding | One product name: Retr01 (no A/C SKU split in docs). |
| 2026-09-13 | Attr fields | Bank/palette nibbles are 0-3. |
| 2026-09-13 | Flash v1 | Bench USB-C flasher first. |
