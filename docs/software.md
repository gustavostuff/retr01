# Retr01 Software

**Status:** Draft v1.2  
**Parent:** [overview.md](overview.md)

The software model is **entity contracts**. That is the normal way to build Retr01 games. Cartridges never carry code.

## 1. Cartridge data (no code)

The cartridge supplies:

- World definitions and screen nametables
- Entity visual definitions plus contract attachments (no placements on cart)
- Tile banks (CHR)
- Save slots on FRAM

There is no native code and no bytecode on the cart.

## 2. Cartridge header (v1)

Header lives at the start of the World / CHR chip. Little-endian multi-byte fields. ASCII where noted.

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0x00 | 4 | magic | ASCII `R01\0` (0x52 0x30 0x31 0x00) |
| 0x04 | 1 | spec_major | Spec major this cart expects (1 for v1.x) |
| 0x05 | 1 | spec_minor | Spec minor (2 for this draft) |
| 0x06 | 1 | fw_major_min | Minimum System firmware major |
| 0x07 | 1 | fw_minor_min | Minimum System firmware minor |
| 0x08 | 16 | title | ASCII title, padded with 0x00 |
| 0x18 | 1 | flags | See below |
| 0x19 | 1 | screen_count | Number of screens in the world (1-16) |
| 0x1A | 1 | entity_type_count | Number of entity types (1-16) |
| 0x1B | 1 | bg_bank_mask | Bit i set if BG bank i is used |
| 0x1C | 1 | spr_bank_mask | Bit i set if sprite bank i is used |
| 0x1D | 1 | reserved0 | Write 0 |
| 0x1E | 2 | data_crc16 | CRC-16/CCITT over payload after header (or 0x0000 if unused) |
| 0x20 | ... | payload | Worlds, CHR, entity tables, contracts (layout TBD in tooling) |

**flags (bit 0 = LSB):**

| Bit | Meaning |
|-----|---------|
| 0 | Has save data expected on FRAM |
| 1 | Prefers scroll transitions (hint only) |
| 2-7 | Reserved (0) |

On boot the System MCU checks magic, then `spec_*` and `fw_*_min`. Mismatch shows a simple error state and does not run the game.

Header size is **0x20** bytes before payload. Payload layout can evolve behind tooling as long as the header stays stable.

## 3. Entities and visuals

### 3.1 Limits

- Max **16** entity types per game
- Each type: up to **4** states (idle, run, jump, attack, ...)
- Each state: up to **4** animation frames
- Each frame: up to **4** hardware sprites (8x8)

Worst case unique patterns with no reuse:

```
16 types x 4 states x 4 frames x 4 sprites = 1024 patterns
```

That fills the four sprite banks (256 tiles each). Reuse is expected and encouraged.

### 3.2 HUD and UI

HUD and UI use the same entity system (health bars, meters, maps, scores, and similar).

### 3.3 Placements

Entity placements are **not** stored on the cartridge. Spawning happens at runtime through contracts or engine hooks on screen load and events.

## 4. Contracts (v1 set)

Contracts live only in System MCU firmware. The cart picks which contracts attach to each entity type and supplies parameters.

A contract runs every frame (or on an event the contract defines) after pads are read. Parameters are small fixed fields so timing stays predictable.

### 4.1 Core lifecycle

| Contract | Role | Main parameters (sketch) |
|----------|------|--------------------------|
| spawn_on_screen | Create instance when a screen becomes active | screen_id, x, y, facing |
| despawn | Remove instance | condition (offscreen, flag, timer) |
| spawn_child | Spawn another type relative to self | type_id, dx, dy, limit |

### 4.2 Motion

| Contract | Role | Main parameters (sketch) |
|----------|------|--------------------------|
| move_axes | Walk on X/Y from input or AI intent | max_speed, accel, friction |
| move_ballistic | Jump / toss with gravity | vx0, vy0, gravity, floor_mode |
| fly | Free 2D motion without gravity | max_speed, drag |
| patrol | Move between points | point list id, speed, wait_frames |
| follow_path | Follow waypoints | path_id, speed, loop |
| chase | Move toward player | speed, stop_distance |
| flee | Move away from player | speed, safe_distance |

### 4.3 Combat and interaction

| Contract | Role | Main parameters (sketch) |
|----------|------|--------------------------|
| health | Hit points and death | max_hp, iframe_frames |
| damage_touch | Hurt overlapping target tags | amount, cooldown, target_mask |
| attack_melee | State-timed melee hitbox | state_id, box, damage |
| shoot | Fire a projectile type | type_id, rate, muzzle_dx/dy, aim_mode |
| solid | Block movement / resolve bumps | layer_mask |

### 4.4 Logic and state

| Contract | Role | Main parameters (sketch) |
|----------|------|--------------------------|
| state_on_timer | Change state after N frames | state_id, frames |
| state_on_flag | Change state when a flag matches | flag_id, value, state_id |
| state_on_proximity | Change state near player or tag | radius, state_id |
| state_on_collision | Change state on hit | target_mask, state_id |
| set_flag | Write a global or screen flag | flag_id, value |
| timer_pulse | Toggle or pulse a flag on a period | flag_id, period |

### 4.5 Camera, HUD, input

| Contract | Role | Main parameters (sketch) |
|----------|------|--------------------------|
| camera_focus | Keep camera or scroll bias on entity | deadzone_x, deadzone_y |
| hud_link | Bind this entity visuals to a value source | source (hp, score, lives), style |
| player_control | Map pad bits into move / action intents | player_index, map_profile |

### 4.6 Rules of the model

- Games configure and combine these contracts. They do not ship new code on the cart.
- Truly new behaviour needs a firmware update that adds a contract.
- Parameter byte layouts will be frozen when the first tooling pass lands. The tables above are the v1 intent, not yet a binary ABI.

## 5. Runtime model

1. Cart supplies visuals, contract attachments, and parameters (plus header).
2. System MCU instantiates entities and runs contracts each frame after reading pads.
3. Active entities become the sprite list (and nametable updates) sent to the Video MCU.
4. Behaviour stays inside trusted fixed firmware.

## 6. Boot and cartridge detect

Suggested v1 sequence (may be refined when the padout exists):

1. Power on and reset both MCUs. Video starts a safe blank or test image.
2. System probes the World / CHR chip. If absent or unreadable, show a **no cartridge** state and idle (still poll pads).
3. Read and validate the header (magic, spec, firmware minimum, optional CRC).
4. On failure, show an **unsupported cart** or **bad data** state.
5. On success, init FRAM if `flags` bit 0 is set, load CHR banks as needed, enter the game (title screen or first screen as the payload defines later).

Cart detect details depend on the final edge connector. Until then, treat "probe SPI ID or magic" as the detect method.

## 7. Frame loop (System MCU)

Required order in vblank:

1. Very start of vblank: poll both serial controllers (<= 100 us) and sample arcade GPIO
2. Run contracts and entity updates with fresh input
3. Build nametable updates and the sprite list
4. SPI transfer to the Video MCU (`SET_NAMETABLE` if needed, `SET_SPRITES`, optional scroll)
5. Remaining vblank time for audio, cartridge reads, saves

## 8. Audio

**Placeholder.** Channel layout and register or frequency format are deferred. An audio engine design is expected in a later pass.

Earlier sketch (not binding until that pass):

| Channel | Type | Typical use |
|---------|------|-------------|
| Pulse 1 | Square / pulse | Music |
| Pulse 2 | Square / pulse | Music |
| SFX | Flexible | Effects |

Do not implement cart formats against this table yet.

## 9. Open software items

- Final binary layouts for contract parameters and payload after the header
- Audio engine and any cart-side music or SFX data format
- Exact CHR packing and entity table encoding in tooling
- Precise on-screen error UI for boot failures
