# Retr01 Costs and Open Questions

This doc merges the old cost sheet and decision log into one planning file.

**Authority:** [`01`](01_architecture_overview.md) *Sources of truth*. Locked rows here agree with current [`05`](05_hardware_v1_32ic.md) HW and [`02`](02_graphics_worlds_memory.md) software. This file does not replace `02` for `$FExx` text.

## Locked decisions

| Topic | Decision |
|------|----------|
| Name/family | **Retr01**, rollout **A -> C -> H** |
| Retr01-A HW BOM | **32 IC** system ([`05`](05_hardware_v1_32ic.md)): 31 motherboard + 1 cart I2C save. Escape +1 PLD -> 33 |
| PCB envelope (A) | ~**12 x 12 cm** 4-layer THT target |
| Worlds | **8** max (indices 0-7) |
| World layout | sparse virtual grid up to **8x8**, **32 present screens max** per world (playfield). **Other screens** (title / interstitial / credits pages) are global ROM, not on grid |
| Parallax (per world) | **0..8** screens (`PARALLAX_MAX` = **8**). **Single** (repeat H/V) or **pair** (2 screens side-by-side or stacked). Live VRAM slots **4-5** only. Optional **slices**: **1..120** bands of variable thickness (H-band or V-band offsets. Roads/curves). Not mid-frame playfield camera shifts. See [`02`](02_graphics_worlds_memory.md#parallax) |
| Other screens (global) | Max **48** MAP payloads (`OTHER_MAX`): **0** = title, **1** = interstitial, **2+** = credits. Credits pages: **min 0**, **max 46** (`CREDITS_MIN` / `CREDITS_MAX`). Each payload raw **480 B** or **RLE** (`flags.RLE`). Soft `len_other` budget **~64 KB**. PRG owns presentation |
| Credits | **Credits pages** inside other screens (**0..46**). Text + small graphics as normal tilemaps. Legacy `off_credits`/`len_credits` reserved **0** |
| Screen format | **16x15** tiles (**128x120**), **240-byte** tile plane + **240-byte** attr plane (one attr/tile) |
| MAP screen format | Playfield: **480 B** raw per screen. Other screens: raw **480 B** or byte-RLE on the **480 B** stream |
| Pixel aspect | square logical pixels, storage **16:15**. **2x** fills **256x240** RGBS with no letterbox |
| RGBS active field | always **256x240** inside **341x262** timing |
| Scale | board **SCALE** DIP: **2x** default (**256x240**), **1x** optional (centered **128x120**) |
| CHR layout | **4 BG banks + 4 sprite banks** per world |
| Bank sizes | **256 tiles** per bank, **4 KB** each |
| Master palette (logical) | **64 indices** in board **Color PROM**. Same logical kit on Retr01-A/C/H. **Not** in cart |
| Color PROM (encoding) | **1x** PROM/OTP: packed **R3G3B2** `{RRRGGGBB}`, 1-dot pipeline. Studio quantizes kit swatches |
| Cart global palettes | **8 BG palette rows** + **8 sprite palette rows** (**32 + 32** palettes, **256 B** total), indices only |
| MAP / cart layout | Fixed header + **pointer table**, world table, per-world CHR + screen dir + payloads. See [`02`](02_graphics_worlds_memory.md) *Cart image* |
| MAP world header | `start_col`/`start_row`, **`default_bg_bank`**, **`default_spr_bank`**, optional `default_pal_row` (**0-7**) |
| Palette cart storage | **uncompressed** master **indices** in the two global blobs. **Pointer table** locates them. **No** master RGB in cart |
| Active palette buffer | **4 BG + 4 sprite palettes** from one selected **palette row** via `$FE08`/`$FE09` (indices into Color PROM) |
| Palette row selection | BG palette row **N** and sprite palette row **N** are always selected together |
| Shared backdrop | all **8** active palettes use the same **color 0** master index (software must write it into every slot when loading a row) |
| Palette fallback | cart globals -> **system default** indices (**software** only: kit/Studio/boot). Bare ASM/C that never writes `$FE08`/`$FE09` gets **undefined/garbage** colors. Master RGB always from Color PROM |
| Runtime BG banking | per **8x8 tile** (attr `BANK` bits). Screens may stamp a default only at load |
| BG attr byte | per tile: `BANK` (1-0), `PAL` (3-2), `FLIP_H` (4), `FLIP_V` (5) **hardware**. `SOLID` (6), `ANIM` (7) **software**. Same low fields as OAM. See [`02`](02_graphics_worlds_memory.md) |
| BG living tiles | `ANIM=1`: **4** consecutive CHR indices `B..B+3`, `B` 4-aligned. Software advances nametable |
| BG collision | `SOLID` bit + **RAM shadow**. Video ignores `SOLID` |
| Runtime sprite banking | per **OAM entry** (attr `BANK` bits). `$FE37` optional stamp only |
| OAM attr byte | `BANK` (1-0), `PAL` (3-2), `FLIP_H` (4), `FLIP_V` (5), `PRIORITY` (6), `SIZE` (7). See [`02`](02_graphics_worlds_memory.md) |
| VRAM | **32 KB**, interleaved. Slots **512 B** aligned (240+240 used) |
| System RAM | **32 KB**, CPU-only |
| Line buffer | third **32 KB** SRAM, sprite ping-pong, **128 px**/half used |
| OAM | in **ATmega1284P**, no DMA. **`$FE20`** = addr, **`$FE21`** = data (auto-inc). Entry `Y, tile, attr, X`. **Host Play:** X/Y packed as signed viewport-relative bytes. Partial sprites clip to **128x120** |
| Sprite cap | **16** per **logical** scanline |
| Scroll | `scroll_x` **0-127**, `scroll_y` **0-119**, wrap |
| `$FExx` logical map | Draft in [`02`](02_graphics_worlds_memory.md). Silicon: **9x HC573** bit-packed (bitfield table open, Q21) |
| Machine config storage | **1284 internal 4 KB EEPROM** (handshake via `$FE70`-`$FE72` band, protocol TBD, Q20) |
| Cart game saves | **I2C EEPROM on cart** (in the 32). HAL / port TBD (Q20) |
| MAP access | **`$FE90`-`$FE92`** addr, **`$FE93`** data + auto-inc |
| PRG layout | **One global 32 KB** PRG section per cart at `$8000` (I/O hole at `$FE00-$FEFF`). No banking. **`$FE80`** unused (leave 0) |
| PRG size | **32 KB** (cart image = CPU window) |
| Cart fit | **Standard cart 512 KB**. Full 8-world playfield + **8** parallax/world + **32 KB** PRG = **452724 B** (~**442.1 KB**). **71564 B** (~**69.9 KB**) free. See [`02`](02_graphics_worlds_memory.md) |
| Cart flash | **512 KB** parallel NOR (**SST39SF040**). On cartridge (socket OK for early bring-up). Same `.retr01` image |
| Beam / glue | Beam in **2x ATF22V10**, glue absorbed, **5** PLDs (compositor = priority mux) |
| Bus | **3x HC245** |
| Parallax camera lock | if **any** H or V parallax band is enabled, main camera locks to that axis for the **whole frame** |
| CPU map | RAM at `$0000-$7FFF`, I/O at `$FE00-$FEFF` |
| Controls | one byte per player at `$FE60/$FE61` |
| Retr01-C pad transport | **draft:** **ATtiny85** in pad. Wires **VCC, GND, DATA**. 1284 master-polls. Same `$FE60/$FE61` bytes |
| CPU clock | **8.000 MHz** |
| Dot clock | **5.369318 MHz** |
| Raster | scanline compare + IRQ |
| APU | separate **ATmega328P** (`$FE40-$FE5F`) |
| Near-term software | **Retr01 Studio** + **Emulator** + board sim (studio/emu/sim READMEs) |
| Studio project files | **JSON v7** (one world's map/CHR per save + global `other_screens`. See Studio README) |
| Validation tools | board IC simulator ([`retr01_sim/README.md`](../retr01_sim/README.md)). Software emu ([`retr01_emu/`](../retr01_emu/)) |

## Cost snapshot

Qty-1 planning numbers, not a quote.

### Motherboard core

| Item | Ballpark |
|------|----------|
| CPU, SRAMs, AVRs, PLDs, Color PROM, glue | about **$100-$115** (32-IC BOM) |
| sockets, passives, connectors | about **$50-$60** |
| proto motherboard PCB share | about **$20** |

### Cartridge

Flash + I2C save on cart PCB. Motherboard + cart proto still targets roughly the **~$200** band.

## Why cost is still approximate

- cart PCB/connector not drawn yet (flash moves off-board later)
- board dimensions may still move
- analog output details may still shift resistor and connector choices
- distributor pricing changes

## Still open

| ID | Topic | Note |
|----|-------|------|
| Q2 | RGBS analog levels/sync polarity tuning | digital timing is locked. Bench tuning still needed |
| Q10 | OAM attr bitfields | **Locked:** bank/pal/flip/priority/size in [`02`](02_graphics_worlds_memory.md). 8x16 tile-pair fetch detail on 1284 firmware still micro-rev |
| Q11 | `$FE07` plane band end/dual-band detail | start scanline drafted. End-of-band pairing may need a second latch |
| Q12 | PRG/CHR/MAP offsets inside **512 KB** flash | **Locked.** `format_ver` **2** only: 36 B pointer table, **8** world slots, **32 present screens**/world max, **other screens** (title/inter/credits pages, RLE or raw). Legacy credits ASCII ptrs reserved **0**. PRG **32 KB** (no banking). See [`02`](02_graphics_worlds_memory.md) |
| Q13 | Retr01-C pad bit timing | ATtiny85 + 3-wire draft locked. Baud/poll edge details later |
| Q14 | Color PROM DAC depth | Packed R3G3B2 is the norm. How many resistor steps / levels on the bench still tunable |
| Q15 | Color PROM part (AT28C16 vs faster OTP) | **Pinned candidate:** **AT27C256R-70PU** (70 ns, DIP-28) if 150 ns is tight. Footprint DIP-24 vs DIP-28 |
| Q20 | Machine EEPROM handshake + cart I2C API | Protocol / `$FExx` for 1284 mailbox + cart save HAL. TBD in [`02`](02_graphics_worlds_memory.md) |
| Q21 | HC573 bitfield packing | 9-chip packed map must be written in `02` |
| Q16 | Default living-tile list cap | Recommend **32** vs **64** cells per camera workbench (`retr01_ANIM_MAX`) |
| Q17 | BG anim rate | Fixed global `rate_shift`, or per-game constant only? |
| Q18 | BG flip+bank silicon timing | Prove on BG fetch island before locking attr UI in a later Studio phase |
| Q19 | Cart flash part | **Locked:** **SST39SF040** (512 KB, DIP-32) |

## Practical next decisions

1. tune RGBS on real hardware (Q2)
2. Studio game modules / later phases ([`07`](07_game_modules.md))
3. freeze save/mailbox APIs + HC573 bitfields (Q20, Q21). Confirm PROM part (Q15)
4. flesh out ATtiny85 poll timing when Retr01-C work starts (Q13)
