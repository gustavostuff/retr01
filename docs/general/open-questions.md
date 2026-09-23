# Open questions and TBDs

Open items and close criteria. A landed decision folds into the matching doc.

## Still open

### CHR A14/A15 mix (Compositor)

**Open:** 16-bank CHR puts attr bits **2-3** on cart **A14-A15** during a fetch. MCU-S1 can do that in firmware (sprites / BG0). BG1 during active display: if the Compositor builds the CHR address, it must fold those two bits. Product-term fit on the existing 22V10 is unproven. If BG1 CHR is the MAP helper (MCU-M 24-bit seek), the PLD never sees them.

**Touches:** `hardware.md`, `video-graphics.md`

### Console program header / flash protocol

**Device locked:** Adafruit's UPDI Friend on one shared motherboard header. **4-pos DIP** selects MCU-M / S1 / S2 / cart. Default **all OFF** (safe). Flasher scope: **AVRs + cart only**. PLDs / color PROM / pad MCU: pre-programmed buy option or DIY external tools. Still **TBD:** header pin numbers and host commands.

**Touches:** `hardware.md`, `cartridge.md`

### Export tree (C vs ASM vs cart)

**Resolved:** **Ctrl+E** writes `data/` blobs, compiles `game_logic.c` with llvm-mos into 32 KB PRG, and packs `.retr01`. Studio Play and `./scripts/emu.sh` boot that image. `game_logic.c` is created once. See `software-api.md`.

**Touches:** `software-api.md`, `apps/studio/README.md`

## Resolved

### 1. AVR duty split

**Resolved:** Three AVR128DB28. M / S1 / S2. Entity behavior on 6502. See `hardware.md`.

### 2. Hardware video scope

**Resolved:** Beam X / Y / Compositor PLDs. Sprites in **VBlank**. BG0 line ping-pong in **HBlank only**. See `hardware.md`, `video-graphics.md`.

### 3. Cartridge binary map

**Resolved:** `.retr01` layout in `memory.md`. I/O `$7F00-$7FFF`.

### 4. Animated tiles (BG attr bit 7)

**Resolved:** Tile animation is PRG. Attr bit 7 is V flip. See `video-graphics.md`.

### 5. Sprite attr bits 6 and 7

**Resolved:** H flip and V flip (same pack as BG). See `video-graphics.md`.

### 6. Attribute index ranges

**Resolved:** Bank field is **0-15** (4 bits). Palette is **0-3**. See `video-graphics.md`.

### 7. Screen + entity budgets

**Resolved:** See item 18.

### 8. Cart flashing

**Resolved:** Console + **Adafruit's UPDI Friend**. Shared header. **4-pos DIP** selects M / S1 / S2 / cart (default all OFF). Flashes **AVRs + cart only**. PLDs / color PROM / pad MCU: pre-programmed buy option or DIY external tools. Header pin numbers / protocol TBD.

### 9. Interleaved VRAM timing

**Resolved (baseline):** PHI2 high = CPU `$7F10`-`$7F12`. PHI2 low = BG fetch.

### 10. Palette buffer and row select

**Resolved:** `$7F08` / `$7F09`, vblank loads, paired rows. See `video-graphics.md`.

### 11. Entity and sprite budget coupling

**Resolved:** Fail spawn/frame-change on OAM shortfall. Drop overflow sprites per scanline. Catalog cap **32** types cart-wide. On-screen instance count is soft (fits in **64** OAM sprites). See `software-api.md`.

### 12. Platformer physics scope

**Resolved (v1):** Axis-separated, AABB, gravity/jump (gravity in 1/16 px), 16 px meter, short hop on jump release, Down crouch. No slopes/movers/one-ways. Solids are a bank+tile list in system RAM. See `software-api.md`.

### 13. IC budget + composite IC

**Resolved:** 17+2=19. **AD724** on the motherboard BOM (dual-sync composite). See `hardware.md`.

### 14. Scroll edge cases

**Resolved (baseline):** Missing BG1 slots show **BG0** by default (then backdrop under BG0). Optional **BG0 clip to BG1** flag forces backdrop outside present BG1 slots. Camera follow uses the player and dead zone only. Empty slots and the present-screen bbox do not stop the camera. **BG0 and BG1** may each programmatically autoscroll and/or wrap repeating strips. See `world-scrolling.md`.

### 15. Entity CHR catalog

**Resolved:** One global entity catalog (**32** types). Player and other entities use global SPR banks. See item 18.

### 16. Entity caps

**Resolved:** See item 18.

### 17. IC comms mitigations folded into design docs

**Resolved:** Idle-safe enable pulls, soft-port / `RDY` rules, OAM SPI in early VBlank only, scroll writes in NMI/VBlank, cart `OE#`/`WE#` play-vs-program, save RDY+timeout. Catalog in `ic-comms-risks.md`. Normative copies in `hardware.md`, `video-graphics.md`, `world-scrolling.md`, `software-api.md`, `memory.md`, `cartridge.md`.

### 18. Global CHR + catalogs (2026-09-20)

**Resolved:** **7** worlds. **64** BG1 / **16** BG0 present screens per world. CHR is **16 BG + 16 SPR** banks cart-wide (**128 KB**). One global entity catalog, **32** types, **4 x 8 x 6**, maxed def **1044 B**. Attr bits 0-3 = bank 0-15, 4-5 = pal, 6-7 = H/V flip. Collision solids are a bank+tile list in RAM. Tile anim is PRG. Passive cart, no mapper. Compressed BGM lives in cart flash outside PRG. See `memory.md`, `video-graphics.md`, `software-api.md`, `sound.md`, `selling-points.md`.

### 19. Collision solids packing

**Resolved:** Collision marks BG1 patterns by bank index and tile index. Palette and H/V flip do not affect solidity. Author `game_logic.c` calls `r01_solid_pattern_add(ctx, bank, tile)`. Studio also stores `solid_patterns` as `[bank, tile]` pairs. Packed carts put the list at PRG `$8700` and copy it into system RAM `$0200` at boot. PRG probes the BG1 nametable against that list. See `memory.md`.

### 20. Instance + PA byte schemas

**Resolved:** An **instance** is a placed copy of a catalog type (spawn row in PRG, live record in RAM). **`PA`** is a Play dump of the marked player's drawable frames. Spawn records are **6 B** in PRG (`$81C0` count, `$81C1` table). Live RAM instances are **12 B**. One cart-wide **`PA`** blob (max **1031 B**) after world-0 maps. Type directory is **64 B**. See `software-api.md`, `memory.md`.

### 21. Cart BGM region

**Resolved:** Compressed BGM (FD/FE/FA plus wavetable ids) lives in the cart image as its own MAP region. PRG `$80FE` is the boot track index only. AKWF cycles and DPCM samples stay in MCU-S2 flash. World cap is **7**. Max-fill leftover ~**41 KB** is about **15 minutes** of busy 5-channel BGM. See `memory.md`, `sound.md`.

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
| 2026-09-14 | Other screens | Max 16 total shared pool (title/interstitial/credits). |
| 2026-09-17 | Other screens | Cap **8** (ids 0..7). |
| 2026-09-17 | Cart layout | **7** worlds. **16** other screens. **4** BG + **4** SPR other CHR. `format_ver` **4**. |
| 2026-09-14 | Entity spawns | Spawn locations in PRG, not cart world blobs. |
| 2026-09-13 | Sync out | One header, CSYNC or H/V mode. |
| 2026-09-13 | Branding | One product: Retr01. |
| 2026-09-14 | Flasher | Console + Adafruit's UPDI Friend. Shared header. 4-pos DIP: M/S1/S2/cart, default all OFF. Scope: AVRs + cart only. |
| 2026-09-14 | Entity caps | 16 types per world. format_ver 3. |
| 2026-09-17 | Entity pack | Max sprites/frame **6**. Maxed def **532 B**. Draw origin is per **frame**. Hitbox is per **state**. |
| 2026-09-14 | Anim tiles | base..base+3 wrap in bank, default delay 6. |
| 2026-09-14 | Video timing | Sprites VBlank pass. BG0 HBlank ping-pong only. |
| 2026-09-14 | PCB layers | Initial: motherboard, cart, and pads all 2-layer. 4-layer mobo only later if bring-up / commercial SMD needs it. |
| 2026-09-14 | Mobo size | 170 x 120 mm. |
| 2026-09-14 | BG autoscroll | BG0 and BG1 may each autoscroll and/or wrap strips. Default is clamp. |
| 2026-09-16 | Missing BG1 slots | Default: show BG0 (then backdrop under BG0). Optional clip-to-BG1 flag restores backdrop-only outside BG1 slots. |
| 2026-09-14 | Player vs camera | Separate systems. Dead zone (e.g. 32x30). Axis lock, follow, or auto camera. |
| 2026-09-15 | IC comms | Idle-safe pulls, RDY OD, OAM SPI early VBlank, scroll in NMI/VBlank, cart OE/WE rules. See `ic-comms-risks.md`. |
| 2026-09-15 | Cart save UX | Multi-frame OK. Chunk I2C, short RDY, keep spinner/UI alive. No full-save picture freeze. |
| 2026-09-15 | Cart save IC | Prefer I2C FRAM when BOM allows (drops EEPROM page-program stalls). Transfer still chunked. See `ic-comms-risks.md` #13. |
| 2026-09-15 | Entity cart bytes | Studio packs locked EntityDef + u16 type directory. |
| 2026-09-15 | Emu soft fences | Scroll/pal pending until VBlank. Host Play OAM/scroll at early VB. Pads latch at VB. Short EE RDY handoff (250 us). |
| 2026-09-16 | Player item bank | Player art uses global SPR banks. |
| 2026-09-17 | Player patterns | Marked player uses global SPR banks. See `memory.md`. |
| 2026-09-17 | Host Play boot catchup | Phase 1 emu waits for a full start MAP stream (480 B) before Host Play takes the camera 2x2 from cart. See `apps/emu/README.md`. |
| 2026-09-19 | APU | 8-ch S2 software mix. `$7F40` is 8x4 regs (S2 never parses bytecode). BGM 1-5 / SFX 6-8. DPCM in S2 flash. NMI tracker on 6502. See `sound.md`. |
| 2026-09-20 | CHR / maps / entities | 16+16 global banks, 8 worlds, 64 BG1 / 16 BG0, 32 types (4x8x6). Attr 4-bit bank. No mapper. See `memory.md`. |
| 2026-09-21 | Collision solids | Bank+tile pattern list in RAM `$0200` (PRG `$8700`). Pal/flip ignored. Author `r01_solid_pattern_add` in `game_logic.c`. See `memory.md`. |
| 2026-09-21 | Author tick SDK | `r01_pad_down` / `r01_player_moving_x` / `r01_player_set_move_mul` / `r01_player_anim_set_frame_delay` from `r01_game_on_tick` in `game_logic.c`. Play is the packed PRG. See `software-api.md`. |
| 2026-09-20 | Instance + PA | PRG spawn 6 B. RAM live 12 B. One `PA` blob cart-wide (max 1031 B). See `software-api.md`. |
| 2026-09-22 | Worlds / BGM | **7** worlds. Compressed BGM in cart flash (MAP), outside PRG. `$80FE` boot index only. See `memory.md`, `sound.md`. |
| 2026-09-22 | Export tree | llvm-mos PRG from `game_logic.c`. Ctrl+E packs `.retr01`. Play is emu of that ROM. See `software-api.md`. |
| 2026-09-22 | Camera follow | Player and dead zone only. Empty BG1 slots and present-screen bbox do not stop the camera. See `world-scrolling.md`. |
