# Open questions and TBDs

Items left open in the rough specs, plus a practical way to close each one. Update this file when a decision lands, then fold the answer into the matching doc.

## 1. AVR duty split

**Open:** Two AVR128DB28 chips help the 6502. What exactly does each one own beyond "sprites in vblank" and "entities"?

**Suggestion:** Write a one-page ownership table: bus mastership per phase, IRQ sources, shared memory windows, and max work per frame (cycles and bytes). Prototype the sprite-composite path first on one AVR. Put entity update and DMA-ish sprite list build on the other. Revisit only if timing fails in a cycle budget spreadsheet or a cheap FPGA/CPLD stand-in for the video glue.

**Touches:** `hardware.md`, `software-api.md`, `video-graphics.md`

## 2. Hardware video scope

**Open:** "Most video logic in hardware" needs a concrete list (scroll regs, nametable fetch, BG compose, sprite limits, palette apply, and so on).

**Suggestion:** Draw a block diagram with boxes that must be discrete logic or a small CPLD versus boxes that may stay in AVR firmware. Anything that must run every pixel or every cycle on a scanline should not depend on AVR code. Anything that can finish in vblank may stay on an AVR.

**Touches:** `hardware.md`, `video-graphics.md`

## 3. Cartridge binary map

**Resolved:** Adopt the prior-project `.retr01` layout in `memory.md` (header, u24 pointer table, global pals, PRG, other screens, world table, world blobs with sparse 12 B dirs, MAP at `$7F90`-`$7F93`). I/O is **`$7F00-$7FFF`**, not a hole inside PRG.

**Still soft:** entity/PA packed record sizes inside the world blob, and exact free-byte split between other screens and entities at max fill.

**Touches:** `memory.md`, `cartridge.md`, `world-scrolling.md`

## 4. Animated tiles (BG attr bit 7)

**Open:** Software iterates a set of tile patterns. Timing and where the set lives are undefined.

**Suggestion:** v1 can be CPU-side only: a small table of (tile slot, frame count, period, bank/pattern list) updated in NMI. Hardware bit 7 is just a hint for tools and for your updater. Hardware auto-animate can wait.

**Touches:** `video-graphics.md`, `software-api.md`

## 5. Sprite attr bits 6 and 7

**Open:** Reserved. No use yet.

**Suggestion:** Leave them zero in tools. Do not document game meaning until a real need appears (priority, collision class, or entity link). Avoid burning them on vanity flags.

**Touches:** `video-graphics.md`

## 6. Attribute index ranges (0-3 vs 0-4)

**Open:** Draft text said bank and palette fields as 0-4 while those fields are two bits and there are four banks.

**Suggestion:** Lock to **0-3** for bank and palette nibble halves. Fix docs and any sample data. If a fifth bank is ever required, that is a format bump, not a silent stretch of two bits.

**Touches:** `video-graphics.md`, `cartridge.md`

## 7. BG0 screen budget

**Resolved:** Cap is **0..8** BG0 screens per world (sparse), matching `memory.md`.

**Touches:** `memory.md`, `world-scrolling.md`

## 8. Console-side cart flashing

**Open:** Want to program carts from the console, maybe with USBasp or Adafruit UPDI Friend.

**Suggestion:** Split into (A) in-system flash of the cart ROM while seated, and (B) a dock or pass-through that only needs the AVR programmer for cart MCU/EEPROM if any. For v1, a dedicated flash header on the cart plus a PC tool may ship faster than full console-mediated flashing. Design the edge connector with the flash pins reserved either way.

**Touches:** `cartridge.md`, `hardware.md`

## 9. Interleaved VRAM timing

**Open:** CPU and video take turns by CPU phase. Exact arbitration, wait states, and safe write windows are unspecified.

**Suggestion:** Document phi2 (or equivalent) ownership on paper first. State when PRG may write nametables and when sprite AVR may write the overlay buffer. Add a hard rule: no mid-scanline PRG VRAM writes in v1 if that simplifies glue.

**Touches:** `hardware.md`, `world-scrolling.md`

## 10. Palette buffer and row select

**Open:** How PRG selects the 8-palette buffer and keeps BG/sprite row pairing (shared BG color) is not fully specified.

**Suggestion:** Memory-map a small set of registers: current row index, optional force of shared BG index, and a DMA or copy path from cart palette rows into the active buffer during vblank only.

**Touches:** `video-graphics.md`

## 11. Entity and sprite budget coupling

**Open:** 64 sprites, 16 per line, entities up to 4x4x4 sprites. How spawning fails under pressure is unclear.

**Suggestion:** Define soft caps (for example max active entities, max sprites claimed by entities) and a clear error or drop policy. Add a scanline overflow behavior (flicker, priority drop, or hard clip) and test it in the platformer sample.

**Touches:** `software-api.md`, `video-graphics.md`

## 12. Platformer physics scope

**Open:** "Simple physics" needs bounds (gravity, jump, one-way, slopes, moving platforms).

**Suggestion:** v1: axis-separated movement, solid tiles via attr bit 6, AABB vs entity hitboxes, no slopes. Add slopes only if a vertical slice demo needs them.

**Touches:** `software-api.md`

## 13. 16 IC budget

**Open:** Max 16 ICs on the main board. No draft BOM yet.

**Suggestion:** Start a living BOM with CPU, two AVRs, RAM, VRAM, AD724, glue (or one CPLD counted as one IC), and I/O. Anything past 16 forces function into an AVR or a larger programmable device. Track count in `hardware.md` when the first schematic pass exists.

**Touches:** `hardware.md`

## 14. Scroll edge cases

**Open:** Sparse maps, missing neighbor screens, and simultaneous BG0/BG1 window shifts need rules.

**Suggestion:** Define empty slots as solid color or wrap/clamp. Sequence cart DMA so a corner move (three screens) cannot miss a frame. Prefer double-buffering the 2x2 window descriptors if glue allows.

**Touches:** `world-scrolling.md`

## Decision log (fill as you go)

| Date | Item | Decision |
| --- | --- | --- |
| 2026-09-13 | PRG banking | No PRG banks. Single flat 32 KB PRG region. Documented in `selling-points.md` and `cartridge.md`. |
| 2026-09-13 | Cart image | Adopt prior `.retr01` map into `memory.md` (format_ver 2 layout, sparse dirs, other screens, MAP port). |
| 2026-09-13 | BG0 screens | Firm cap 0..8 present BG0 screens per world. |
| 2026-09-13 | PRG vs I/O | Contiguous PRG `$8000-$FFFF`. I/O page `$7F00-$7FFF`. No low/high PRG split and no `$FExx` hole in ROM. |
