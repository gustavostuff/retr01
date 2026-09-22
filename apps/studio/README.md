# Retr01 Studio

Visual authoring for Retr01 worlds, screens, and `.retr01` carts. Two jobs in one app:

1. **Authoring.** Edit worlds, tiles, palettes, sprites, entities, and instances.
2. **Export + Play.** **Ctrl+E** or **Play** packs a cart and a generated game tree. **Play** then embeds the shared emu so preview matches `./scripts/emu.sh`.

Play always uses the shared emu after export. Hardware: [`docs/general/video-graphics.md`](../../docs/general/video-graphics.md). Runtime: [`apps/emu/`](../emu/README.md).

Stack: C11, SDL2, FreeType, `retr01_ui`, `libretr01_studio_core`, shared `retr01_emu`.

## Build

From the repo root:

```bash
./scripts/build-all.sh
./scripts/studio.sh path/to/project.r01proj
./scripts/unit-tests.sh
```

Needs CMake, a C compiler, SDL2, libpng, FreeType 2. Optional: X11 clipboard, `xclip` / `wl-clipboard`, Aseprite CLI.

This tree only:

```bash
cd apps/studio
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_studio
```

## Layout

```text
+---------------- Graphics | Audio | Code ----------------------+
| SIDEBAR (128) | Play/Stop 8px above PREVIEW | CTRL (128)      |
| Tracks        | BGM timeline                | channel boxes   |
|               | minimap                     |                 |
|               | Play / Pause / Stop         |                 |
+---------------------------------------------------------------+
```

Logical canvas **640x360** or **1280x720** (**Ctrl+Shift+R**). Window scale **Ctrl+1** / **Ctrl+2**. **8px** grid. Graphics sidebar is an accordion (Worlds, banks, entities, palettes). Host Play/Stop sits on the Graphics preview (64px wide). Audio is a BGM timeline plus an SFX stub. Audio Play / Pause / Stop sit 8px below the minimap, left aligned. Channel isolate uses checkboxes (All plus one per lane). An instrument dropdown sits to the right of each channel checkbox (Guitar, EGuitar, Piano, Flute). Host mix uses that table on BGM 1-3. Timeline Play/Stop encodes the grid into tracker bytecode and mixes `$7F40`. Strips snap to eighth notes (off-beat mids sit between quarter-note beats). Note strips are 2px-rounded rectangles with 1px top and bottom margin. Sidebar **-** / **+** zoom the timeline horizontally. Wheel on empty timeline pans the viewport in pixels. A **C** / **Do** button under zoom switches letter names and solfege on strip labels. Tracker tokens stay C / C# / Cb. Wheel on a strip steps the seven naturals (Do Re Mi Fa Sol La Si). Up / Down arrows do the same to the current selection. Shift+wheel and Shift+Up / Shift+Down step one semitone (black keys spell as sostenido going up, bemol going down). **S** toggles sostenido (sharp). **B** toggles bemol (flat). Sharp and flat are exclusive. A menor sound is two (or more) tones on separate channels, not a flag on one strip. **Ctrl+Z** / **Ctrl+Y** undo and redo paint, delete, move, resize, paste, pitch, S, and B. **Ctrl+A** selects all strips. **Ctrl+click** toggles. Shift-drag draws a marquee. The timeline scrolls when the pointer is at the visible edge. **Ctrl+C** / **Ctrl+V** copy and paste the selection. **P** plays the selected strips once. Space plays the whole track and loops at the last strip. Embedded Play attaches the emu window. Neither path is MCU-S2 PWM. Code is TBD.

## Authoring

**Worlds.** Seven slots. A pager steps the active world (**N/7**). A **BG1** / **BG0** control picks the map plane. World 1 starts as a 3x3 on a 16x16 map. Cart export packs **world 0** only (Studio World 1). BG1 is the playfield, BG0 is the parallax plane. Double-click an empty cell to create a screen. Right-click a BG1 cell for default screen / default world. PNG drop imports an atlas into the active world, BG bank 0.

**Paint.** Right-column **Work on** / **Hide** radios (BG, Sprite, Both). **Ctrl+click** stamps tiles. Set Solid marks a BG pattern (bank + tile) as collidable. `r01_solid_pattern_add` in `custom_logic.c` marks the same list. Palette and H/V flip do not matter. The list packs to PRG and copies into system RAM at boot. Drag an Entities row onto the preview to place an instance (switches to Sprite layer). CHR is **16 BG + 16 SPR** cart-wide. A pager steps the bank (**N/16**). A **BG** / **Sprites** control picks the plane. Caps: [`memory.md`](../../docs/general/memory.md).

**Entities.** Up to **4** states x **8** frames x **6** sprites. **Add** opens compose. **Import** reads `aseprite_entities/` next to the saved `.r01proj` (manual, never on open). Right-click **Mark as player**. The playable player belongs on world 0. Hitbox is per state. Draw origin is per frame. Caps and pack: [`software-api.md`](../../docs/general/software-api.md). Kit palettes: [`docs/general/palette/`](../../docs/general/palette/README.md).

## Play

Host Play/Stop sits 8px above the centered screen preview. Play uses ACTIVE green. Stop uses DANGER red. Chrome colors are in [`retr01_ui/metrics.h`](ui/include/retr01_ui/metrics.h). Space on the Graphics tab starts Play (export, boot wait, then emu). While Play is active, Studio chrome is locked. Only that button stays clickable. Space then is a pad button. Cart boots world 0. Spawn is the first instance of the marked player type, else the default screen center. Gameplay SoT is emu Host Play. Keyboard and SDL Game Controllers share the same pad bits (community `gamecontrollerdb.txt` plus SDL built-in mappings). First two pads are P1 / P2. Guide / Home opens Reset, Quit (Stop), **1x**/**2x**, and Mute On/Off. Default is top-down. `r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER)` in `custom_logic.c` enables gravity, face-Y jump (hold for full height), and Down crouch when a crouch state is mapped. `r01_platformer_set_meter` sets pixels per meter (default 16). `r01_custom_on_tick` may raise walk speed with `r01_player_set_move_mul` and override anim frame delay with `r01_player_anim_set_frame_delay` (example_01 holds face X while moving left/right). Player states (idle, walk, crouch, jump) are mapped in `custom_logic.c`. With no mapping, Play draws state 0 frame 0 and only X-flips for facing.

`custom_logic.c` is created on first export and never overwritten. The generated file sets the camera dead zone (packed size, live follow may snap 1 px inward, see `docs/general/world-scrolling.md`). Player anim maps stay commented until an author fills them in:

```c
void r01_custom_on_init(R01GameCtx *ctx) {
    r01_camera_set_deadzone(ctx, R01_CAM_DEADZONE_X_DEFAULT, R01_CAM_DEADZONE_Y_DEFAULT);
    /* r01_player_anim_set_idle_state(ctx, 0); */
    /* r01_player_anim_set_walk_all(ctx, 1); */
    /* r01_player_anim_set_crouch_state(ctx, 2); */
    /* r01_player_anim_set_jump_state(ctx, 3); */
    /* r01_bgm_play(ctx, 1); */
    /* r01_solid_pattern_add(ctx, 0, 1); */
}

void r01_custom_on_tick(R01GameCtx *ctx) {
    if (r01_pad_down(ctx, R01_PAD_X) && r01_player_moving_x(ctx)) {
        r01_player_set_move_mul(ctx, 2);
        r01_player_anim_set_frame_delay(ctx, 3);
    }
}
```

Generated API headers live in `C/include/r01_*.h` beside the project.

## Save and export

**Ctrl+S** / **Ctrl+O** the current path. First save (or unsaved) opens the Save project modal. Default parent is `apps/studio/projects/`. Quit does not auto-save. JSON version **18**. Save writes world 0. Worlds 2-7 are session-only until multi-world JSON. Load applies that world data to world 0.

**Ctrl+E** packs `<stem>.retr01` and regenerates `C/`, `ASM/`, and `data/` beside the project (or under `output/` if unsaved). Audio-tab tracks go into the cart BGM region. `r01_bgm_play(ctx, N)` in `custom_logic.c` selects the boot track at `$80FE`. `r01_solid_pattern_add(ctx, bank, tile)` packs the solid-pattern list at `$8700`. Export also compiles `C/r01_custom.so` from `custom_logic.c` when a host C compiler is present. Host Play loads that plugin and runs `r01_custom_on_tick` each frame.

| Path | Role |
|------|------|
| `<stem>.r01proj` | Authoring JSON |
| `<stem>.retr01` | Packed cart (world 0) |
| `C/custom_logic.c` | Author file, kept |
| `C/r01_custom.so` | Host Play author tick plugin |
| `C/`, `ASM/`, `data/` | Regenerated stubs and tables |

:warning: Those trees are parallel. Generated C does not assemble into PRG. The `ASM/` tree is not the cart build input. Phase 1 PRG bytes come from `prg_phase1.c`. Gameplay SoT is Host Play / emu. See [`software-api.md`](../../docs/general/software-api.md).

PROM and 512 KB flash images sit beside the cart. Layout: [`memory.md`](../../docs/general/memory.md).

## Keys

| Key | Action |
|-----|--------|
| Ctrl+S / Ctrl+O | Save / open |
| Ctrl+E | Export cart |
| Space | Graphics: start Play. While Play is active: pad |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+1 / Ctrl+2 | Window 1x / 2x |
| Gamepad Guide / Home | Play overlay: Reset / Quit / 1x-2x / Mute On/Off |
| Ctrl+F | Fullscreen |
| Ctrl+Shift+R | 640x360 / 1280x720 |

## Related

- [`docs/general/video-graphics.md`](../../docs/general/video-graphics.md) - video, entities, palettes
- [`docs/general/palette/`](../../docs/general/palette/README.md) - kit RGB
- [`docs/general/software-api.md`](../../docs/general/software-api.md) - entity pack, PRG vs hardware
- [`docs/general/memory.md`](../../docs/general/memory.md) - cart image
- [`apps/emu/README.md`](../emu/README.md) - cart emulator
