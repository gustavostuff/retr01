# Open questions and TBDs

Items still open, plus how to close them. Update this file when a decision lands, then fold the answer into the matching doc.

## Still open

### 5. Sprite attr bits 6 and 7

**Open:** Reserved. Leave **0** in tools until a real need appears.

**Touches:** `video-graphics.md`

### Instance + PA byte schemas

**Partly sized:** Entity **defs** are locked (offset-table pack, max **356 B**). Spawn instance records and optional `PA` blobs still need a frozen byte layout.

**Touches:** `memory.md`, `software-api.md`

### Console program header / flash protocol

**Device locked:** Adafruit's UPDI Friend on one shared motherboard header. **4-pos DIP** selects MCU-M / S1 / S2 / cart. Default **all OFF** (safe). Flasher scope: **AVRs + cart only**. PLDs / color PROM / pad MCU: pre-programmed buy option or DIY external tools. Still **TBD:** header pin numbers and host commands.

**Touches:** `hardware.md`, `cartridge.md`

## Resolved (kept for history)

### 1. AVR duty split

**Resolved:** Three AVR128DB28. M / S1 / S2. Entity behavior on 6502. See `hardware.md`.

### 2. Hardware video scope

**Resolved:** Beam X / Y / Compositor PLDs. Sprites in **VBlank**. BG0 line ping-pong in **HBlank only**. See `hardware.md`, `video-graphics.md`.

### 3. Cartridge binary map

**Resolved:** `.retr01` layout in `memory.md`. I/O `$7F00-$7FFF`.

### 4. Animated tiles (BG attr bit 7)

**Resolved:** Bit 7 = animate through `base`, `base+1`, `base+2`, `base+3` with wrap in-bank (`& 0xFF`). Default step delay **6** frames, configurable in PRG/sys RAM. See `video-graphics.md`.

### 6. Attribute index ranges

**Resolved:** Bank and palette fields **0-3**.

### 7. Screen + entity budgets

**Resolved:** **32** BG1 / **0..8** BG0 per world. Entity types: **128** global (shared across worlds). Maxed def **356 B**. See `memory.md`, `software-api.md`.

### 8. Cart flashing

**Resolved:** Console + **Adafruit's UPDI Friend**. Shared header. **4-pos DIP** selects M / S1 / S2 / cart (default all OFF). Flashes **AVRs + cart only**. PLDs / color PROM / pad MCU: pre-programmed buy option or DIY external tools. Header pin numbers / protocol TBD.

### 9. Interleaved VRAM timing

**Resolved (baseline):** PHI2 high = CPU `$7F10`-`$7F12`. PHI2 low = BG fetch.

### 10. Palette buffer and row select

**Resolved:** `$7F08` / `$7F09`, vblank loads, paired rows. See `video-graphics.md`.

### 11. Entity and sprite budget coupling

**Resolved:** Fail spawn/frame-change on OAM shortfall. Drop overflow sprites per scanline. Catalog caps 16/128.

### 12. Platformer physics scope

**Resolved (v1):** Axis-separated, bit-6 solids, AABB, gravity/jump. No slopes/movers/one-ways.

### 13. IC budget + composite IC

**Resolved:** 16+2=18. **AD724** frozen for dual-sync composite. See `hardware.md`.

### 14. Scroll edge cases

**Resolved (baseline):** Empty slots = backdrop color index 0. Clamp, no wrap.

## Decision log

| Date | Item | Decision |
| --- | --- | --- |
| 2026-09-13 | PRG banking | No PRG banks. Single flat 32 KB PRG. |
| 2026-09-13 | Cart image | `.retr01` map in `memory.md`. |
| 2026-09-13 | Screens | 32 BG1 / 0..8 BG0 per world. |
| 2026-09-13 | PRG vs I/O | Contiguous PRG `$8000-$FFFF`. I/O `$7F00-$7FFF`. |
| 2026-09-13 | MCU set | 3x AVR128DB28 (M / S1 / S2). |
| 2026-09-13 | Color path | AT27C256R master colors. |
| 2026-09-13 | Composite | **AD724** frozen (CSYNC or H/V). |
| 2026-09-13 | Sync out | One header, CSYNC or H/V mode. |
| 2026-09-13 | Branding | One product: Retr01. |
| 2026-09-14 | Flasher | Console + Adafruit's UPDI Friend. Shared header. 4-pos DIP: M/S1/S2/cart, default all OFF. Scope: AVRs + cart only. |
| 2026-09-14 | Entity caps | 128 types global catalog (shared across worlds). Dropped 16/world. |
| 2026-09-14 | Entity pack | Offset-table format, max 356 B. |
| 2026-09-14 | Anim tiles | base..base+3 wrap in bank, default delay 6. |
| 2026-09-14 | Video timing | Sprites VBlank pass. BG0 HBlank ping-pong only. |
| 2026-09-14 | PCB layers | Motherboard 4-layer. Cart and pad PCBs 2-layer. |
