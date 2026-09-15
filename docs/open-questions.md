# Open questions and TBDs

Items still open, plus how to close them. Update this file when a decision lands, then fold the answer into the matching doc.

## Still open

### 5. Sprite attr bits 6 and 7

**Open:** Reserved. Leave **0** in tools until a real need appears.

**Touches:** `video-graphics.md`

### Instance + PA byte schemas

**Partly sized:** Live instance records in system RAM and optional `PA` blobs still need a frozen byte layout. Spawn locations are **PRG-side** (not cart).

**Entity cart pack (decision):** Studio writes the locked offset-table **EntityDef** from `software-api.md` (variable length, max **356 B**). World catalog = `u16` type directory + defs. The old fixed **20 B** snapshot is retired.

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

**Resolved:** **32** BG1 / **0..8** BG0 per world. Entity types: **16 per world** (catalog in the world blob). Maxed def **356 B**. See `memory.md`, `software-api.md`.

### 8. Cart flashing

**Resolved:** Console + **Adafruit's UPDI Friend**. Shared header. **4-pos DIP** selects M / S1 / S2 / cart (default all OFF). Flashes **AVRs + cart only**. PLDs / color PROM / pad MCU: pre-programmed buy option or DIY external tools. Header pin numbers / protocol TBD.

### 9. Interleaved VRAM timing

**Resolved (baseline):** PHI2 high = CPU `$7F10`-`$7F12`. PHI2 low = BG fetch.

### 10. Palette buffer and row select

**Resolved:** `$7F08` / `$7F09`, vblank loads, paired rows. See `video-graphics.md`.

### 11. Entity and sprite budget coupling

**Resolved:** Fail spawn/frame-change on OAM shortfall. Drop overflow sprites per scanline. Catalog cap **16** types per world.

### 12. Platformer physics scope

**Resolved (v1):** Axis-separated, bit-6 solids, AABB, gravity/jump. No slopes/movers/one-ways.

### 13. IC budget + composite IC

**Resolved:** 17+2=19. **AD724** on the motherboard BOM (dual-sync composite). See `hardware.md`.

### 14. Scroll edge cases

**Resolved (baseline):** Empty slots = backdrop color index 0. Default clamp at playfield edges. **BG0 and BG1** may each programmatically autoscroll and/or wrap repeating strips. See `world-scrolling.md`. API TBD.

### 15. Entity CHR home world

**Superseded:** Dropped global catalog + `chr_world`. Entities are **16**/world. Sprite banks use the current world's SPR CHR. See item 16.

### 16. Entity caps (per world)

**Resolved:** **16** entity types per world, catalog inside each world blob. No global shared catalog. `format_ver` **3** (pointer table 5 slots). Sprite banks = current world SPR CHR. See `memory.md`, `software-api.md`.

### 17. IC comms mitigations folded into design docs

**Resolved:** Idle-safe enable pulls, soft-port / `RDY` rules, OAM SPI in early VBlank only, scroll writes in NMI/VBlank, cart `OE#`/`WE#` play-vs-program, save RDY+timeout. Catalog in `ic-comms-risks.md`. Normative copies in `hardware.md`, `video-graphics.md`, `world-scrolling.md`, `software-api.md`, `memory.md`, `cartridge.md`.

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
| 2026-09-14 | IC budget | AD724 counted on motherboard. 17 mobo + 2 cart = 19. |
| 2026-09-14 | AD724 mount | SOIC-16 on SOIC-to-DIP adapter. Main PCB 100% THT. |
| 2026-09-14 | 74HC14 | Optional. Skip if canned PHI2/DOT + simple reset. Add if soft edges or reset chatter. |
| 2026-09-14 | Other screens | Max 16 total shared pool (title/interstitial/credits). Dropped 46 credits cap. |
| 2026-09-14 | Entity spawns | Spawn locations in PRG, not cart world blobs. |
| 2026-09-13 | Sync out | One header, CSYNC or H/V mode. |
| 2026-09-13 | Branding | One product: Retr01. |
| 2026-09-14 | Flasher | Console + Adafruit's UPDI Friend. Shared header. 4-pos DIP: M/S1/S2/cart, default all OFF. Scope: AVRs + cart only. |
| 2026-09-14 | Entity caps | 16 types per world (catalog in world blob). Dropped global 128 + `chr_world`. format_ver 3. |
| 2026-09-14 | Entity pack | Offset-table format, max 356 B. |
| 2026-09-14 | Anim tiles | base..base+3 wrap in bank, default delay 6. |
| 2026-09-14 | Video timing | Sprites VBlank pass. BG0 HBlank ping-pong only. |
| 2026-09-14 | PCB layers | Initial: motherboard, cart, and pads all 2-layer. 4-layer mobo only later if bring-up / commercial SMD needs it. |
| 2026-09-14 | Mobo size | 170 x 120 mm. |
| 2026-09-14 | BG autoscroll | BG0 and BG1 may each autoscroll and/or wrap strips. Default is clamp. |
| 2026-09-14 | Player vs camera | Separate systems. Dead zone (e.g. 32x30). Axis lock, follow, or auto camera. |
| 2026-09-15 | IC comms | Idle-safe pulls, RDY OD, OAM SPI early VBlank, scroll in NMI/VBlank, cart OE/WE rules. See `ic-comms-risks.md`. |
| 2026-09-15 | Cart save UX | Multi-frame OK. Chunk I2C, short RDY, keep spinner/UI alive. No full-save picture freeze. |
| 2026-09-15 | Entity cart bytes | Studio packs locked EntityDef + u16 type directory. Retired 20 B snapshot. |
