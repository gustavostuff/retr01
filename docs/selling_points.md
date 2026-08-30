# Retr01 Selling Points

Why Retr01 exists, what you can build with it, and how the software toolchain helps.

**Related:** [`graphics.md`](graphics.md) (VRAM, cart, registers). [`hardware_architecture.md`](hardware_architecture.md) (32-IC BOM). [`sounds.md`](sounds.md) (APU). Tools: [`retr01_studio/`](../retr01_studio/README.md), [`retr01_emu/`](../retr01_emu/README.md), [`retr01_sim/`](../retr01_sim/README.md).

---

## Family and roadmap

Retr01 is a family of discrete-logic 2D machines that share one CPU model, one graphics model, one memory map, and one cartridge format.

| Variant | Role |
|---------|------|
| **Retr01-A** | Arcade motherboard, through-hole, first hardware target (~14 x 12 cm PCB) |
| **Retr01-C** | Home console, same architecture, different I/O shell |
| **Retr01-H** | Handheld, later SMD variant, same software contract |

Built for people who want to *make* 8-bit games, not only play them.

---

## At a glance

| Aspect | Description |
|--------|-------------|
| CPU | W65C02S @ **8 MHz** |
| Playfield | **128 x 120** logical (**16 x 15** tiles), board **2x** to **256 x 240** RGBS |
| Art | **8 x 8** tiles, **2 bpp**, **64** master colors on-board Color PROM |
| Worlds | up to **8** worlds, **32** screens each on a **512 KB** cart (**32 KB** PRG) |
| Scroll | **2 x 2** live nametable window crossing screen borders |
| Sprites | **64** OAM entries, **16** per scanline |
| VRAM / RAM | **32 KB** interleaved VRAM + **32 KB** system RAM |

---

## Why 32 KB PRG is enough

Retr01 uses a fixed **32 KB** PRG window at `$8000-$FFFF`. That is the same raw size many classic NES NROM titles used. On Retr01 it buys far more game logic.

### Why the same 32 KB goes further

| Factor | NES (NROM-era) | Retr01 |
|--------|----------------|--------|
| CPU clock | ~1.79 MHz | 8.000 MHz |
| Cycles per frame (~60 Hz) | ~29 800 | ~133 000 (~4.5x) |
| Graphics work in PRG | Heavy: software scrolling, sprite multiplexing, VBlank-only nametable updates, attribute tables | Minimal: hardware 2x2 camera window, interleaved VRAM, sprite line-buffer (1284), MAP streaming via $FE93, per-tile bank/attr already in hardware |
| System RAM | 2 KB | 32 KB |
| PRG role | Code + data + rendering hacks | Mostly pure game logic and tables |

On the NES a large fraction of the 32 KB (and of every frame's cycles) was spent just making the picture appear and scroll. On Retr01 those costs are already paid by the discrete-logic path and the two AVRs. The 32 KB PRG and the ~133K cycles/frame are therefore available almost entirely for gameplay systems.

### What fits in 32 KB PRG

- 30-60 active entities with individual state machines, simple pathfinding or flocking, and data-driven behavior tables
- Solid physics and collision: tile-based or soft-pixel, platforms, slopes, one-way platforms, multiple hitboxes per entity
- Full player systems: platformer or top-down movement, wall-jumps, dashes, inventory, equipment, status effects, multi-stage attacks
- World and camera logic: multi-screen seamless scrolling (helped by the live 2x2 VRAM window), doors/warps, triggers, cutscene scripting, simple dialogue
- Game flow: title to world select to up to 8 worlds x 32 screens, save points, quest flags, ending sequence
- Audio driver: modest music + SFX bytecode that the ATmega328P mixes

Classic NES 32 KB titles already delivered complete games under far tighter constraints. On Retr01 the same designs leave headroom for richer AI, better physics, inventory systems, and multi-world structure while still fitting in the fixed 32 KB window.

### Design posture

Keep vectors, interrupt stubs, common library code and any tiny HAL helpers in a fixed home within the 32 KB window. Put the bulk of game systems and tables in the remaining space. Because graphics and streaming are hardware-assisted, most of the 32 KB can stay readable, modular 6502 (or compiled C) instead of cycle-counting rendering tricks.

**32 KB of PRG on Retr01 is enough for a complete, polished 8-bit action, platform or adventure game with multiple worlds and modern-feeling camera work.**

---

## Software toolchain

| Tool | Role |
|------|------|
| **Retr01 Studio** | Visual authoring for worlds, screens, palettes, sprites, entities. **Play** exports a cart and runs the shared emulator render path in-app |
| **Retr01 Emu** | Software-visible 65C02 + `$FExx` + video. Standalone `./emu` and the library Studio Play uses |
| **Retr01 Sim** | Pin-level model of the 32-IC Retr01-A netlist. Interactive SDL board UI for bring-up |

Studio **Save** writes `output/<stem>.r01proj` (JSON). **Export** regenerates `output/C/`, `output/ASM/`, `output/data/`, and packed cart bytes. **`custom_logic.c`** is created on first export and never overwritten.

**Studio Play** exports (same path as **Ctrl+E**), shows a brief boot-wait spinner, then runs the **emulator render embedded** in Studio — same pixels as standalone `./emu`. **Sim is not involved** in the Studio Play path.

Host Play (movement, camera, OAM in emu) lives in `retr01_emu` + `common/r01_play_camera.c` / `r01_play_anim*.c`. Studio UI Play uses the shared emu core after export.

---

## Cost snapshot (planning)

Qty-1 planning numbers, not a quote.

| Item | Ballpark |
|------|----------|
| CPU, SRAMs, AVRs, PLDs, Color PROM, glue (32-IC BOM) | about **$100-$115** |
| sockets, passives, connectors | about **$50-$60** |
| proto motherboard PCB share | about **$20** |

Flash + I2C save on cart PCB. Motherboard + cart proto still targets roughly the **~$200** band. Cart PCB/connector, board dimensions, and analog output details may still shift distributor pricing.

---

## Game modules (Studio + runtime contract)

Gameplay modules are **attachable functionalities** that define mechanics and runtime behavior for a game (cart). Studio implements subsets of this contract over time. This section is the **product / runtime contract**.

**Related:** [`graphics.md`](graphics.md). Collision is software-only. Audio protocol [`sounds.md`](sounds.md).

## Scope and attachment

| Rule | Value |
|------|-------|
| Attach unit | **One profile per module family** (for now) |
| Attach target | **Game (cart) level** |
| Profiles | Mutually exclusive within a family until a later rev allows mixing |
| Studio phases | Not fixed here. Implement subsets of this contract over time |

Module families in this doc:

1. **Player movement**
2. **Camera**
3. **Game entity**
4. **BGM / SFX** (TBD stub)
5. **Collision** (cross-cutting: Studio + ASM sketch)

```text
  Game project
       |
       +-- Player movement profile  (pick one)
       +-- Camera profile           (pick one)
       +-- Entity + collision rules (always, budgets below)
       +-- BGM/SFX profile          (TBD)
       |
       v
  Export / codegen -> .retr01 PRG helpers + data tables
```

---

## Timing baseline (budget math)

Use these when estimating frame work. Clocks from [`02`](graphics.md).

| Item | Value |
|------|-------|
| CPU | **8.000 MHz** (W65C02S) |
| Dot | **5.369318 MHz** (independent) |
| Frame | **341 x 262**, ~**60.098 Hz** |
| Cycles / frame | ~**133116** (`8e6 / 60.098`) |
| Logical playfield | **128 x 120** |
| VRAM camera workbench | **2 x 2** screens (**256 x 240** logical) |
| OAM | **64** entries |
| Sprite line cap | **16** sprites / logical scanline |

Rough split of one frame (design guidance, not silicon law):

```text
  ~133k cycles / frame
      |
      +-- ~25-40%  MAP seam / VRAM / scroll / pad / audio sequencer
      +-- ~25-40%  entities (update + collision + OAM build)
      +-- ~20-40%  headroom / spikes / rail scroll / rare events
```

Conservative gameplay budget for entity work: about **30k-50k** cycles/frame.
Aggressive (simple game, little streaming): up to ~**70k**, still leave headroom.

---

## Activity regions (freeze model)

Entities live relative to the camera workbench and the visible viewport.

```text
  VRAM 2x2 workbench (256x240 logical)
  +---------------------------+
  |  FROZEN                   |
  |     +-------------+       |
  |     |  ACTIVE     |       |
  |     |  128x120    |       |
  |     |  viewport   |       |
  |     +-------------+       |
  |  FROZEN                   |
  +---------------------------+

  Outside workbench -> INACTIVE (not updated, may despawn or sleep in tables)
```

| Region | Draw | Position / AI update | Collision |
|--------|------|----------------------|-----------|
| **ACTIVE** (in viewport) | Yes | Yes | Yes |
| **FROZEN** (in workbench, outside viewport) | No | No | No |
| **INACTIVE** (outside workbench) | No | No | No |

**Why freeze:** skips draw, motion, and collision for entities the player cannot see yet, but keeps them resident so a short camera pan does not thrash spawn tables. When an entity crosses into the viewport, promote **FROZEN -> ACTIVE** (and the reverse when it leaves).

Studio Play and generated ASM should share this three-state model.

---

## Recommended maxima (conservative defaults)

Targets assume: software collision, OAM builds every frame, some MAP streaming, and the freeze model above. Push higher only with profiling on emu/sim.

### Counts

| Budget | Recommended max | Hard ceiling (HW / RAM) | Notes |
|--------|-----------------|-------------------------|-------|
| Entity slots (player included) | **32** total resident | **64** soft cap | Player uses **one** slot |
| ACTIVE entities | **16** (incl. player) | Bound by OAM + CPU | Typical action scene |
| FROZEN entities | **16** | Rest of resident pool | Workbench only, no tick |
| OAM entries used by entities | **48** | **64** | Leave spare for FX / HUD sprites |
| Sprites on one scanline | Design for **<= 12** | **16** HW drop | Multi-sprite bosses need Y spread |
| States per entity *type* | **3** | **4** | Idle / run / hurt / ... |
| Frames per state | **3** | **4** | Entity sprite frames (soft). Separate from BG living tiles (`ANIM` attr, also soft, [`02`](graphics.md)) |
| Sprites (OAM) per state frame | **1-4** typical | **16** | Sprite-only bosses: recommend **8-16** on one entity. BG bosses: see below |
| Entity-entity AABB tests / frame | **~120** naive pairs @ 16 active | Prefer grids if higher | See collision section |
| Hitboxes per entity (active state) | **1** | **1** | One box per entity (no multi-hitbox on a single entity) |
| Boss weak-part entities (BG boss) | **1-4** | **4** | Sprite entities as dynamic / weak parts. Each still has **1** hitbox |

### Cycle sketch (order of magnitude)

Assume ACTIVE = 16, average **2** OAM sprites each, simple patrol AI, AABB vs player + coarse vs BG solids.

| Work | Approx cycles (ballpark) |
|------|--------------------------|
| Promote / freeze region tests | ~1-2k |
| Simple AI / motion x16 | ~3-8k |
| Build OAM (~32 sprites) | ~4-8k |
| Entity vs BG solid (coarse) | ~5-15k |
| Entity vs entity (player-centric) | ~2-6k |
| **Total entity path** | **~15-40k** -> fits conservative frame budget |

If every ACTIVE entity tests every other (16 choose 2 = 120 pairs) with fat AABBs, collision alone can eat the frame. Prefer **player-centric** and **grid / screen-bin** checks (below).

### Platformer / physics note

Velocity, gravity, and jumps (Player movement 1.3) cost more per ACTIVE entity than grid walkers. Budget fewer ACTIVE movers (e.g. **8-12**) or simpler collision when meter resolution is high (subpixel / small meter).

---

## 1. Player movement module

Pick **one** profile per game.

The player is a **Game entity** slot that mainly follows pad input (`$FE60` / `$FE61`).

### 1.1 Free 8-way smooth (default)

- Pixel movement on X/Y. 8 directions from pad.
- Matches current emu Host Play feel (smooth follow, dead-zone camera from `custom_logic.c`).
- Good default for open exploration.

### 1.2 Grid-tied 4-way

- 4 directions only (no diagonals), smooth or immediate step.
- Motion quantized to **8 px** or **16 px** increments (Studio setting).
- Good for board / grid / puzzle games.

### 1.3 Platformer physics (later in this family)

- Velocity, gravity, jump.
- **Meter size** scales physics feel in logical pixels. **Default = 8 px** with **Earth** gravity feel at that meter.
- Smaller meter (e.g. 4 px) = slower / "moon" motion. Larger meter (e.g. 16 px) = snappier / heavier Earth-like response.
- Intended as the last profile added in this family.
- Exact fixed-point format TBD when implemented. Document accel / max-fall / jump impulse relative to the active meter (default reference: **8 px**, Earth).

```text
  Profiles (exclusive for now):
    [1.1] 8-way free smooth
    [1.2] 4-way grid (8 or 16 px)
    [1.3] platformer (vel / grav / jump + meter)
```

---

## 2. Camera module

Pick **one** camera strategy per game. Player cannot mix free dead-zone, rail, and auto-scroll in v1.

### Profiles

| Profile | Player moves camera? | Notes |
|---------|----------------------|-------|
| **2.A Dead-zone** | Yes | Centered dead zone (see below) |
| **2.B Camera off** | No | No dead-zone checks (best ASM cost) |
| **2.C Rail (TLOZ-style)** | Indirect | Screen-exit triggers full scroll, pads muted during scroll |
| **2.D Auto-scroll** | No | World-driven H or V scroll at variable speed |

### 2.A Dead-zone camera

Movement axes (game setting): **free (X+Y)**, **X only**, or **Y only**.

Dead zone is centered on the viewport unless Studio later allows offsets. `r01_camera_set_deadzone(ctx, W, H)` sets the **width and height** of that inner rectangle (not edge margins). Host Play in emu and sim implements this profile today via `common/r01_play_camera.c`. Export packs `W`/`H` into the world header. Studio Play uses the same cart path (export then emu).

| Dead zone | Behavior |
|-----------|----------|
| **0 x 0** | Camera tracks immediately. Player stays centered |
| **128 x 120** | Camera never moves (player walks inside fixed view) |
| e.g. **32 x 30** | Camera moves once the player origin crosses that centered inner rect |
| **X range only** | Vertical cam locked. Horizontal dead range only |
| **Y range only** | Horizontal cam locked. Vertical dead range only |

```text
  Viewport 128x120
  +----------------------+
  |                      |
  |    +------------+    |
  |    | dead zone  |    |  camera still
  |    |            |    |
  |    +------------+    |
  |         ^            |  player exits -> scroll
  +----------------------+
```

### 2.B Camera off

- No dead-zone tests, no player-driven scroll writes beyond what the game script sets.
- Prefer for single-screen games or scripted cams.

### 2.C Rail guided scroll (NES Zelda-like)

- When the player entity **origin** crosses the current screen edge into another **present** screen, start a rail scroll to that screen.
- During the scroll: **controllers disabled**.
- Default speed: **4 px / frame** (full 128 px edge ~**32** frames ~**0.53 s**). 1 px/frame is allowed but feels slow (~2.1 s).
- **VRAM workbench updates during the slide**, not after. As soon as the next frame would show a screen piece that is not already in the 2x2 camera slots, stream that screen (MAP -> VRAM) in time for that frame. Prefer filling the destination / seam slots early in the transition so the sliding viewport never samples an unloaded cell.
- After settle: re-enable pads. Refresh entity ACTIVE / FROZEN / INACTIVE for the new viewport + workbench.

### 2.D Automatic camera

- Axis: **horizontal** or **vertical** (game setting).
- Speed: variable. May change on events / circumstances.
- Player **cannot** move the camera. The movement module still moves the player entity inside the scrolling world (collision / death pits are game-defined).

---

## 3. Game entity module

Entities are gameplay objects built from sprites. In ASM they are **grouped bytes** in RAM (live instance) plus ROM tables (type / states / frames).

### 3.1 Structure

```text
  Entity type (ROM)
    state[0..S)
      origin_x, origin_y     (draw origin for this state)
      hitbox                 (independent of origin)
      frame[0..F)
        sprite[0..N)         (tile, attr, dx, dy relative to origin)

  Entity instance (RAM)
    type_id, state_id, frame_id, frame_timer
    world_x, world_y
    flags (ACTIVE / FROZEN / INACTIVE, facing, ...)
    ai / motion fields
```

Rules:

- Each entity may have multiple **states** (idle, run, crouch, ...).
- Each state may have multiple **frames**. Each frame may use multiple **sprites** (OAM entries).
- All frames of a state share that state's **X,Y origin** for drawing.
- Each state has a **hitbox** independent of the draw origin (one box only per entity. May be offset from the draw origin).
- States change on **events**, actions, or circumstances (pads, timers, collisions, scripts).

### 3.2 Player

- The player is a normal entity **slot** (counts toward budgets).
- Input-driven. Uses the attached **Player movement** profile.
- Host Play (emu / sim, and Studio Play via shared emu): **8-dir idle/walk** animation from the cart player anim blob when `custom_logic.c` configures states. Collision uses the **current state** hitbox. Other entities remain state0/frame0 in preview.

### 3.3 Non-player AI (v1 document)

For now, document only simple motion cycles:

| Cycle | Behavior |
|-------|----------|
| Left-right | Oscillate on X between two world anchors |
| Up-down | Oscillate on Y between two world anchors |
| Linear | Constant velocity until despawn / bounce / script stop |

Richer AI (chase, pathfind, scripted boss phases) is out of scope for this revision beyond the boss render patterns below.

### 3.4 Large / BG bosses

Two authoring patterns (may combine):

| Pattern | Body | Weak / dynamic parts |
|---------|------|----------------------|
| **Sprite boss** | One entity, recommend **8-16** OAM sprites per state frame | Same entity hitbox (one box) |
| **BG boss** | Boss art is **BG** (nametable / plane). Scroll the BG in **X**, **Y**, or **both** so a very large body can move past the 128x120 view | **1-4** sprite **entities** as moving parts (eyes, hands, cores, ...). Each part is a normal entity with **1** hitbox. A part may be a weak point, a hazard, or both |

```text
  BG boss (example)
  +---------------------------+  workbench / scrolled BG art
  |  #######################  |
  |  ##                   ##  |
  |  ##   [eye]   [eye]   ##  |  <- sprite entities (1-4)
  |  ##      [core]       ##  |     each: 1 hitbox
  |  #######################  |
  |                           |
  |                           |
  +---------------------------+
           viewport 128x120
```

Rules for **BG boss**:

- BG scroll for the body is game/script driven (not the Camera module profile). It may use main scroll latches and/or a parallax plane slot ([`02`](graphics.md) slots 4-5, optional variable-thickness slices) when the playfield must stay independent. Exact port recipe is an open item.
- Weak points are **not** multi-hitboxes on one entity. They are **separate sprite entities** (up to **4**) that move with or relative to the BG body.
- Those part entities count toward ACTIVE / OAM / freeze budgets like any other entity.
- Damage: player hitbox vs each part entity hitbox (player-centric). Hitting the BG pixels alone does not register unless a part entity (or a future trigger volume) covers that region.

### 3.5 Studio authoring (intent)

- Place entity instances in the world (or spawn tables).
- Build **metasprites** (multi-part SPR groups, no origin/hitbox) in the Metasprites modal. Drag metasprite catalog rows onto the **entity** compose canvas to assemble frames.
- Edit entity type: states, frames, origins, hitboxes, per-part paint (LMB select/drag, RMB paint).
- Attach motion cycle parameters for non-player entities.
- Mark boss encounters: sprite-only vs BG body + part entities, BG scroll axes.
- Preview ACTIVE / FROZEN behavior against the camera profile in Play.

---

## 4. BGM and SFX module (TBD)

Short stub only. Full protocol lives in [`sounds.md`](sounds.md).

| Intent | Notes |
|--------|-------|
| Role | Studio tools + generated ASM macros/routines to start/stop BGM and fire SFX |
| Hardware | 6502 sequencer -> `$FE40`-`$FE5F` -> ATmega328P mixer ([`sounds.md`](sounds.md)) |
| Status | **TBD**. Channel map, bytecode authoring UI, and module profiles TBD |

---

## 5. Collision (Studio + ASM peek)

Hardware has **no** sprite-vs-BG or sprite-vs-sprite hit logic ([`hardware_architecture.md`](hardware_architecture.md)). All gameplay collision is **software**.

### 5.1 Layers

| Layer | Source | Use |
|-------|--------|-----|
| **BG solid** | Screen attr `SOLID` bit (software bit, video ignores it) + optional RAM shadow | Floors, walls, blocks |
| **Entity hitbox** | Active state's **one** hitbox at instance `world_x/y` (+ origin policy below) | Body, interaction, or one boss part / weak point |
| **Triggers** | Optional non-solid volumes (future) | Cameras, scripts, warps |

**Hitbox vs draw origin:** hitbox is authored in state space. At runtime, place it relative to the entity instance position (recommended default: hitbox offset from the same world position used for the state origin). Do not require hitbox == sprite AABB.

### 5.2 Who collides with what (v1)

```text
  Player ACTIVE  <->  BG SOLID          (movement resolution)
  Player ACTIVE  <->  NPC/enemy ACTIVE  (damage / bump)
  Player ACTIVE  <->  boss part entities (1-4 weak / dynamic parts)
  NPC ACTIVE     <->  BG SOLID          (optional, patrols may use anchors only)
  FROZEN / INACTIVE                     (skipped)
```

Entity-entity among all NPCs is **opt-in** and expensive. Default generated loops should be **player-centric**.

### 5.3 Studio behavior

- Author `SOLID` on tiles via tile context **Set Solid**.
- Author entity **origins** and **hitboxes** in the entity modal (guides checkbox toggles overlay + interaction). Emu Host Play uses the marked player’s hitbox vs BG solid; entity-vs-entity collision is still future.
- Play preview (Studio via emu, standalone emu, sim Host Play) uses the same region rules (ACTIVE only). Sprites **clip to the 128x120 viewport** when partially off-screen.
- Show hitboxes / solids as optional overlay in Play (implementation phase TBD).
- Camera rail / auto-scroll must pause or redefine collision during transitions if pads are muted (rail: typically freeze gameplay collisions until scroll ends).

### 5.4 ASM peek (generated pattern)

Not a full HAL yet. Shape the codegen toward this:

```text
  NMI or main frame:
    1. read pads (unless rail mute)
    2. update player with movement profile
    3. resolve player vs BG SOLID (tile lookup from MAP/shadow)
    4. for each ACTIVE non-player:
         update AI cycle
         optional vs BG
    5. for each ACTIVE non-player:
         AABB(player_hit, entity_hit) -> events
    6. apply state changes / damage / despawn
    7. rebuild OAM from ACTIVE entities only
    8. scroll / camera profile
    9. promote/freeze entities vs viewport + workbench
```

**BG solid probe (sketch):** convert entity feet / corners to tile coords on the current screen (and neighbor if near a seam), read attr byte, test `SOLID`. Prefer a **RAM solid shadow** for the workbench if MAP reads are too hot mid-frame.

**AABB (sketch):**

```text
  hit_x = entity_x + box.dx
  hit_y = entity_y + box.dy
  overlap if
    ax < bx + bw  &&  ax + aw > bx  &&
    ay < by + bh  &&  ay + ah > by
```

**Spatial hint:** bin ACTIVE entities by coarse cells (e.g. 32x30) so pair tests stay near the recommended pair budget.

---

## Module <-> hardware quick map

| Module concern | Ports / HW |
|----------------|------------|
| Pads | `$FE60` / `$FE61` |
| Scroll / camera | `$FE02` / `$FE03` (+ MAP `$FE90`-`$FE93`, VRAM `$FE10`-`$FE12`) |
| Sprites | OAM `$FE20` / `$FE21` (1284 fill, **16**/line) |
| BG solid data | MAP/VRAM attrs. CPU tests `SOLID` |
| Audio | `$FE40`-`$FE5F` ([`sounds.md`](sounds.md)) |

---

## Open topics (gameplay)

- Exact entity instance struct sizes and ZP layout
- Meter / fixed-point format for profile 1.3 (defaults: **8 px** meter, Earth gravity. Tunables TBD)
- Damage / invuln frames pipeline
- BG boss scroll recipe (main `$FE02`/`$FE03` vs plane slots 4-5 vs scripted MAP)
- Trigger volumes as first-class colliders
- Whether FROZEN entities may keep cheap timers (currently: **no** updates)
- BGM/SFX Studio profiles ([`sounds.md`](sounds.md))

---

## Documentation index

| Doc | Topic |
|-----|-------|
| [`hardware_architecture.md`](hardware_architecture.md) | 32-IC BOM, bring-up islands, silicon pathways |
| [`graphics.md`](graphics.md) | VRAM, cart image, `$FExx`, palettes |
| [`sounds.md`](sounds.md) | APU, bytecode, 8-channel mixer |
| **This file** | Product pitch, PRG headroom, game modules |
