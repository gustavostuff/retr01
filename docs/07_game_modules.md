# Retr01 Studio Game Modules

Gameplay modules are **attachable functionalities** that define mechanics and runtime behavior for a game (cart). Studio phases that implement each module are chosen later. This file is the **product / runtime contract**.

**Related:** software map [`02`](02_graphics_worlds_memory.md). Collision is software-only ([`01`](01_architecture_overview.md)). Audio protocol [`06`](06_audio_architecture.md).

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

Use these when estimating frame work. Clocks from [`02`](02_graphics_worlds_memory.md).

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
| Frames per state | **3** | **4** | Entity sprite frames (soft). Separate from BG living tiles (`ANIM` attr, also soft, [`02`](02_graphics_worlds_memory.md)) |
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
- Matches current host Play feel (smooth follow, no grid snap).
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

Dead zone is centered on the viewport unless Studio later allows offsets.

| Dead zone | Behavior |
|-----------|----------|
| **0 x 0** | Camera tracks immediately. Player stays centered |
| **128 x 120** | Camera never moves (player walks inside fixed view) |
| e.g. **32 x 30** | Camera moves once the player crosses that inner rect |
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

- BG scroll for the body is game/script driven (not the Camera module profile). It may use main scroll latches and/or a parallax plane slot ([`02`](02_graphics_worlds_memory.md) slots 4-5, optional variable-thickness slices) when the playfield must stay independent. Exact port recipe is an open item.
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

Short stub only. Full protocol lives in [`06_audio_architecture.md`](06_audio_architecture.md).

| Intent | Notes |
|--------|-------|
| Role | Studio tools + generated ASM macros/routines to start/stop BGM and fire SFX |
| Hardware | 6502 sequencer -> `$FE40`-`$FE5F` -> ATmega328P mixer ([`06`](06_audio_architecture.md)) |
| Status | **TBD**. Channel map, bytecode authoring UI, and module profiles not frozen here |

---

## 5. Collision (Studio + ASM peek)

Hardware has **no** sprite-vs-BG or sprite-vs-sprite hit logic ([`01`](01_architecture_overview.md), [`03`](03_hardware_implementation.md)). All gameplay collision is **software**.

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

- Author `SOLID` on tiles via tile context **Set Solid** (Phase 2 Studio).
- Author entity **origins** and **hitboxes** in the entity modal (guides checkbox toggles overlay + interaction). Studio **Phase 4** Play uses the marked player’s hitbox vs BG solid; entity-vs-entity collision is still future.
- Play preview uses the same region rules (ACTIVE only). Sprites **clip to the 128x120 viewport** when partially off-screen (Studio, emu, sim Host Play).
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
| Audio | `$FE40`-`$FE5F` ([`06`](06_audio_architecture.md)) |

---

## Open items (freeze later)

- Exact entity instance struct sizes and ZP layout
- Meter / fixed-point format for profile 1.3 (defaults locked: **8 px** meter, Earth gravity. Tunables TBD)
- Damage / invuln frames pipeline
- BG boss scroll recipe (main `$FE02`/`$FE03` vs plane slots 4-5 vs scripted MAP)
- Trigger volumes as first-class colliders
- Whether FROZEN entities may keep cheap timers (currently: **no** updates)
- BGM/SFX Studio profiles ([`06`](06_audio_architecture.md))

---

## Where this sits

| Doc | Role |
|-----|------|
| This file | Game module contract + budgets |
| [`02`](02_graphics_worlds_memory.md) | Display, VRAM, MAP, `$FExx` |
| Studio README | Current Studio Phase 2 product (may lag this file) |
| [`06`](06_audio_architecture.md) | APU / bytecode SoT |
| [`01`](01_architecture_overview.md) | Sources of truth index |
