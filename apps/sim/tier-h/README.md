# Retr01 Tier H sim (imported)

Imported from the discarded `retr01-bkp-01` board sim. It links this repo's `apps/netlist_sim`, `apps/common`, and `apps/sim/common/assets`.

Matches [`docs/general/hardware.md`](../../../docs/general/hardware.md) idle-safe rules: soft I/O is `$7Fxx`, the color PROM is the AT27C256R, and the 74HC14 is not seated (canned PHI2). Cart `WE#` stays high unless a program cycle drives it, `/SS_S1` and `/SS_S2` idle high, field ALE idles low, and `CPU_RDY` is open-drain with a pull-up. The AD724 is not in this tree yet, so the mounted IC count is still 16 motherboard parts plus the two cart memories.

# Retr01 Board Simulator

IC-first board simulator for the Retr01 motherboard (arcade + console share one netlist). Separate from Retr01 Studio (authoring). Pin/behavior: [`hw/md/`](../../hw/md/). BOM and islands: [`docs/general/hardware.md`](../../../docs/general/hardware.md).

**Engine:** discrete IC / island / bus core lives in drop-in [`netlist_sim/`](../../netlist_sim/) (`Ns` / `ns_` API). This tree is the Retr01 board recipe, chips, Host Play, and SDL host. Temporary `R01s*` shims: [`include/retr01_sim/ns_compat.h`](include/retr01_sim/ns_compat.h).

**Goal:** simulate the Retr01 motherboard as discrete ICs (pins, package, datasheet behavior) wired like the real board. End state: boot a cart, accept pad input, show a digital playfield (logical 128x120 inside a 256x240 RGBS field / LCD sink). Accuracy tightens as tests demand it.

## Status (current)

**Netlist / bring-up:** 9 canvas islands (O first) + wired-only E/I/P sprite glue. Soft `$7Fxx` via **MCU-M** on L. Cart image from argv (default `example_01/example_01.retr01`). The console has no USB. Cart programming is the MCU-M bridge.

**UI chrome:** Compact board view only. LIVE / MANUAL, ARCADE / PADS, and SAVE layout controls are **removed** for now (revisit with breadboard electrical later). Bottom-right gamepad overlays remain. IC bodies use health-colored 1px outlines from `netlist_sim` (`ns_outline_rgb`).

### Breadboard proto

| Feature | State |
|---------|--------|
| Layout | Nano-style 830-point class, pitch **5** (matches DIP tip lattice). Implementation in `netlist_sim` |
| Interact | Drag, rotate, hole-rail hover highlight |
| IC snap | Tips snap to holes **while dragging** and on rotate / drop |
| Electrical | Soft air wires (LIVE). MANUAL mode UI removed for now |

### LIVE settle

Default path uses soft air-wire settle (LIVE). MANUAL breadboard electrical mode is deferred (HUD control removed).

### Passives tray

Full console passive BOM ([`docs/passive_bom.md`](../../docs/passive_bom.md)): **61** parts below ICs + breadboard, ordered OSC -> CCAP -> ECAP -> R.

| Kind | Role in tray | Notes |
|------|--------------|-------|
| **OSC** | Y1/Y2/Y3 | 3 crystals |
| **CCAP** | 100 nF bypass + crystal loads | 21 + 6 |
| **ECAP** | 220 uF bulk | Polarized flag set (polarity rules later) |
| **R** | DAC + 33 ohm series + pull-ups | 30 total. Only 11 are video DAC |
| **D** | Art present | Not spawned (not on this BOM) |

Sprites: `app/assets/png/passives/` (nano `scaled_down`). Pivot from filename `KIND_x_y.png`. Drag / snap / rotate about that pivot. **Passives are in `R01sBoard.pin_netlist`** (every BOM pin registered; bypass/DAC/series/pull-ups wired in `src/board_schematic.c` per [`docs/bringup/schematic-netlist-tier-h.md`](../../docs/bringup/schematic-netlist-tier-h.md)). Breadboard strip merge and bus settle through passives are still TODO.

**Preliminary Skidl export (not fab-ready):** [`skidl/export_netlist.sh`](skidl/export_netlist.sh) or `export_tier_h_netlist` + [`scripts/skidl_from_tier_h.py`](../../../scripts/skidl_from_tier_h.py). Outputs under [`skidl/`](skidl/). See [`docs/bringup/tier-h-skidl-export.md`](../../../docs/bringup/tier-h-skidl-export.md). Illustrative only; motherboard PCB is not ready for production.

### Canvas islands

| Island | Components (canvas) |
|--------|---------------------|
| O Video | `COMPOSITOR` + `AT27C256R` + `LCD_SINK` |
| A Power+clk | `PWR5V` + `OSC8M` + `SN74HC14` (not shown as UI chips) |
| C CPU + decode | `W65C02S`, `AS6C62256`, decode helper PLD (non-BOM wire entity) |
| G VRAM | 2nd `AS6C62256` + **3x** `SN74HC157` + VRAM glue PLD helper |
| H Beam | `OSC_DOT` + `BEAM_XY` (X PLD) + Beam Y PLD vs soft `$7F04` + **`SN74HC574`** (`$7F02` SX) |
| J Cart | Console socket. Flash and save live on **N** |
| K APU + pads | **`AVR128DB28` MCU-S2**: `$7F40`-`$7F5F`, pads `$7F60`/`$7F61`, 8-voice mix + PWM + WAVE monitor |
| L MCU + field | **`AVR128DB28` MCU-M** (soft `$7Fxx` + SPI mailbox master) + **MCU-S1** (OAM apply / field) + field `AS6C62256` + **`SN74HC573`** ALE |
| N Cart module | `SST39SF040` + cart `24C64` (detachable module. Argv `.retr01` is copied into flash) |

**Wired on the netlist, not separate canvas frames.** Soft `$7Fxx` settle on board RAM (mirrors MCU-M). OAM/APU CPU/`poke_fe` path flushes via behavioral SPI mailbox to S1/S2. Host Play may still call `r01s_avr128db28_s1_oam_poke` as a preview overlay. **E** pads via MCU-S2, **I** `BG_FETCH`, sprite field fill stats, **P** integration / NMI stats.

Bench-only (wired, not on canvas): `PRG_ROM` fallback when cart does not own `$8000+`.

**Letter note:** Silicon bring-up docs use **N** for the sprite path ([`docs/general/hardware.md`](../../../docs/general/hardware.md)). On the sim canvas, **N** is the detachable **cart module** island. Sprite milestones still show as **N** in the health strip detail line.

**BOM:** mounted visuals are 16 motherboard ICs plus cart flash and the 24C64. The locked motherboard is 17 once the AD724 is modeled. Helper tick domain is 24 MHz.

**Cart load:** argv, or `example_01/example_01.retr01` when omitted, is copied into cart `SST39SF040`. The app then overlays a short boot program that streams the palette and the start screen through `$7Fxx` on the pin netlist. That catchup is the boot wait. Host Play starts after it and drives the wave monitor from `game_logic.c` beside the cart. `board_build` still installs a synthetic image for unit tests that do not call `r01s_board_load_cart`.

**Cart programming:** MCU-M bridge on a seated cart. This tree does not model a USB flasher.

**1_FRAME_DEBUG:** `1_FRAME_DEBUG=true ./sim cart.retr01` logs byte-scale IC traffic (PRG/MAP/VRAM/beam/BG0/sprites/play) into a paginated on-screen panel (`[` `]` or PgUp/PgDn). Logging stops once BG (MAP->VRAM) and the first sprite field fill are done. Full dump: `retr01_sim/debug/frame1_trace.log`.

Why the worker exists: [`CATCHUP_THREADING.md`](CATCHUP_THREADING.md).

### Next (Sim)

- Breadboard strip routing as real MANUAL electrical (beyond soft-gate + overlay)
- Passive polarity (ECAP / diode) and passive netlist
- Optional machine EEPROM (MCU-M 512 B path)
- Retire bring-up Host Play overlay when game PRG owns MAP


## Test layers

```text
  Layer 1: Unit (one IC)
       |
       v
  Layer 2: Island (few ICs + wires). See docs/general/hardware.md + test_island_abcdeghiojklmnp.c
       |
       v
  Layer 3: System (full board + cart + input + screen)
```

Layer 1: per-chip harness tests in `tests/`. Layer 2: island smoke in `tests/test_island_abcdeghiojklmnp.c`. Layer 3: full netlist + golden cart `output/test.retr01` (`test_host_play_smoke`: catchup + Host Play BG/sprites/APU).

## Cart ROM vs runners (triage)

When something looks wrong on screen, do not assume the `.retr01` is bad and do not assume the emu/sim is bad. Studio authoring, cart image, emu, and sim are different layers.

### Who owns what

| Layer | Artifact | On silicon / runners? | Notes |
|-------|----------|----------------------|--------|
| **Studio editor** | `output/test.r01proj` (+ UI) | **No** | Authoring only. Does not execute PRG |
| **Studio Play** | export then shared emu | **Yes** (via cart) | Same path as **Ctrl+E** + emu render. No Studio-only soft preview |
| **Cart image** | `output/test.retr01` (+ `test_flash.bin`) | **Yes** (flash) | Packed bytes SoT for PRG/CHR/MAP/pals. Layout in [`docs/graphics.md`](../../docs/graphics.md) |
| **Color PROM burn** | `test_prom.bin` | **Yes** (motherboard) | Not inside the cart. Kit -> R3G3B2. Target part **AT27C256R** ([`hw/md/AT27C256R.md`](../../hw/md/AT27C256R.md)) |
| **Boot asm listing** | `test_boot.s` | Human-readable only | Binary inside `.retr01` is what runners execute |
| **Emulator** | `retr01_emu` | Software-visible CPU/`$7Fxx` | Loads `.retr01`. Default: PRG catchup streams pals + start MAP. Softboot opt-in (`R01E_SOFTBOOT=1`). Host Play for camera/player. Used by Studio Play and standalone `./emu` |
| **Board sim** | `retr01_sim` | IC / island netlist | Copies argv/default `.retr01` into cart `SST39SF040`. Catchup is pin-level. Softboot opt-in (`R01S_SOFTBOOT=1`). Host Play after catchup |

### What is in `test.retr01` today

| In ROM | Meaning |
|--------|---------|
| Header + pointer table | magic `retr01`, `format_ver` **2** only, world count, **36 B** ptr table (24-bit offs + lens) |
| Other screens + credits | Title + interstitial + credits **pages** in other-screens blob (raw 480 B or RLE). PRG owns presentation |
| Global BG/sprite palettes | 8 BG rows + 8 sprite rows (master indices, not RGB) |
| SPR CHR banks | Real tile bytes from Studio export (4 banks x up to 256 tiles) |
| Entity tables | Per-world type records (origin, hitbox, up to 4 parts) + instance list (type, world x/y) |
| 32 KB PRG | Palette + start-screen MAP stream (`$7F93` -> `$7F08`/`$7F09`/`$7F12`). Scroll/player/warps still **host** Play |
| World table + blobs | CHR banks, screen dir, 480 B present-screen payloads |

| **Not** in ROM | Meaning |
|----------------|---------|
| Host Play motion, camera dead zone, player anim, warps | Emu / sim Host Play + `../common/r01_play_camera.c` (cart-backed). Not a Studio-only preview |
| Host Play BG0 | Emu + sim Host Play: cart BG0 cache, show-through under BG1 color 0, proportional scroll from present extents. Host overlay only (does not drive IC VRAM slots 4-7 or `$7F06`/`$7F07`) |
| Host collision source | Cart flash MAP attrs (`R01_ATTR_SOLID`). PRG collision stub not used by host runners |
| Editor UI state | UI only. Cart boots world **0** |
| Live camera seam streaming (2x2 shift) | Phase 1 PRG loads start screen only |
| Full game loop in 6502 | Still future. Host Play stands in |

### How to tell ROM bug vs runner bug

1. **Hex / dump the cart first.** If the dump is wrong, it is Studio export. If right, blame the runner or soft helpers.
2. **Same `.retr01` on emu and sim.** Both wrong the same way -> ROM/content or shared contract (`02`). Only one fails -> that runner.
3. **Studio Play is cart-backed.** It exports then runs emu. If Studio Play looks wrong, dump the cart. Do not assume a separate Studio compositor.
4. **Call out soft helpers.** `R01E_SOFTBOOT=1` / `R01S_SOFTBOOT=1` are opt-in host poke only.
5. **Color wrong?** Check `*_prom.bin` / board PROM path separately from cart palette indices.

## Architecture (summary)

| Topic | Choice |
|-------|--------|
| Time base | PHI2 half-steps from `OSC8M` (`R01S_PHI2_HALF_NS`). Combinatorial settle. `DELAY=typical|max` prints datasheet path budget (pin netlist stays zero-delay so catchup works) |
| Bus | Settle loop (`R01S_SETTLE_PASSES`). H+L -> hard abort. Undriven -> pull-up HIGH (`$FF`) |
| Wiring | Explicit `wire_*` in `board.c`. Global netlist deferred |
| Boot UX | Worker-thread MAP catchup. See [`CATCHUP_THREADING.md`](CATCHUP_THREADING.md) |
| Perf | [`PERFORMANCE.md`](PERFORMANCE.md) |

**Model:** every IC is an `R01sEntity` (pins + vtable). Islands hold entities. Island groups wire them. `r01s_board_build()` binds the full netlist onto the canvas islands above. Undriven pins pull high. H+L aborts with a bus-fight report.

## Build

From the repo root:

```bash
./scripts/build-all.sh
./scripts/sim-tier-h.sh
./scripts/unit-tests.sh
```

Developer rebuild of this tree only:

```bash
cmake -S apps/sim/tier-h -B apps/sim/tier-h/build -DCMAKE_BUILD_TYPE=Release
cmake --build apps/sim/tier-h/build -j
ctest --test-dir apps/sim/tier-h/build --output-on-failure
./scripts/sim-tier-h.sh
```

Needs: CMake, a C compiler, SDL2 (`sdl2` package).

## Run

```bash
./scripts/sim-tier-h.sh
./scripts/sim-tier-h.sh example_01/example_01.retr01
./scripts/sim-tier-h.sh DELAY=typical   # print typ path budget vs PHI2 half
./scripts/sim-tier-h.sh DELAY=max       # print max (worst-case) path budget
```

`DELAY=typical|max` selects the datasheet corner and prints **path budget** (decode+245+573 vs PHI2 half). The pin netlist stays combinatorial. Deferred HC/PLD outputs miss `STA $7Fxx` in this settle model. Wall-clock UI FPS != sim ns. See [`PERFORMANCE.md`](PERFORMANCE.md).

**Controls:** `Space` pause/resume * `Ctrl+R` reset * `Ctrl+1` / `Ctrl+2` present scale * `R` rotate selected IC * **SCALE 1X/2X** (`G`. **2X** grows the video island to fit SCR1) * `.` single-step (while paused) * **LIVE / MANUAL** (HUD, soft netlist vs breadboard-gated) * **left-drag chip** move (snaps to breadboard) * **right-click chip** rotate * **Shift+arrows / wheel / middle-drag** pan * `Esc` quit.

**Layout persistence:** island frames + chip positions saved to `app/sim/ui_layout.json` (override with `R01S_LAYOUT`).

**Gamepads (island E -> `$7F60`/`$7F61`):** bottom-right panels or keyboard. HUD **ARCADE** (default) injects the bitfield directly. **PADS** routes through ATtiny85 poll/reply (`0x55`/`0xAA`) then into the same ports. Top-right **LIVE** / **MANUAL** toggles soft bus settle (see Status). After boot catchup, **Host Play** uses P1 for move + warps (Studio/emu rules: dead-zone camera, player anim blob, collision from cart MAP attrs):

| Player | Stick | X | Y | Coin | Start |
|--|-------|---|---|------|-------|
| **P1** | WASD (8-way) | **G** | **H** | 1 | 2 |
| **P2** | Arrows (8-way) | **,** / KP1 | **.** / KP2 | Shift | Enter |

Same keyboard map is used by standalone **emu** and Studio Play.

**Wave monitor (island K):** bottom-left overlay (drag, position in `ui_layout.json`). Lanes **B1-B5** / **S6-S8** draw stable math waveforms from the active voice (period = pitch). **A** is the mixed analog scope ring. After Host Play, silent BGM matches emu: scans `custom_logic.c` for `r01_bgm_play(ctx, N)` and drives B1-B5 from `output/data/bgm_trackN.bin` (Studio **Export**). No host speaker. See [`docs/sound.md`](../../docs/sound.md).

Live probe (top-right) shows **VDD / PHI2 / RESB**. Status bar shows CPU `PC` / `AB` / phase / cycle count.

## Layout

| Path | Role |
|------|------|
| `include/retr01_sim/` | Public headers (`entity`, `pin`, `bus`, `board`, `island*`, `types`) |
| `src/board.c` | Board recipe. 9 canvas islands, wiring, settle loop |
| `src/main.c` | SDL entry |
| `chips/` | Per-part models |
| `tests/` | Layer-1 unit tests + `test_island_abcdeghiojklmnp` (layer 2) |
