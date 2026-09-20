# Retr01 Studio

Visual authoring for Retr01 worlds, screens, and `.retr01` carts. Two jobs in one app:

1. **Authoring.** Edit worlds, tiles, palettes, sprites, entities, and instances.
2. **Export + Play.** **Ctrl+E** or **Play** packs a cart and a generated game tree. **Play** then embeds the shared emu so preview matches `./scripts/emu.sh`.

There is no Studio-only Play path. Sim is not involved. Hardware: [`general_docs/video-graphics.md`](../../general_docs/video-graphics.md). Runtime: [`apps/emu/`](../emu/README.md).

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
+---------------- Graphics | Audio | Code ------------------+
| SIDEBAR (128) | PREVIEW (centered) | CTRL (128)           |
| Tracks        | BGM timeline       | Play / Pause / Stop  |
+-----------------------------------------------------------+
```

Logical canvas **640x360** or **1280x720** (**Ctrl+Shift+R**). Window scale **Ctrl+1** / **Ctrl+2**. **8px** grid. Graphics sidebar is an accordion (Worlds, banks, entities, palettes). Audio is a BGM timeline plus an SFX stub. Timeline Play/Stop encodes the grid into tracker bytecode and mixes `$7F40`. Strips snap to eighth notes (off-beat mids sit between quarter-note beats). Sidebar **-** / **+** zoom the timeline horizontally. A **C** / **Do** button under zoom switches letter names and solfege on strip labels. Tracker tokens stay C / C# / Cb. Wheel on a strip steps the seven naturals (Do Re Mi Fa Sol La Si). Up / Down arrows do the same to the current selection. **S** toggles sostenido (sharp). **B** toggles bemol (flat). Sharp and flat are exclusive. A menor sound is two (or more) tones on separate channels, not a flag on one strip. **Ctrl+A** selects all strips. **Ctrl+click** toggles. Shift-drag draws a marquee. **Ctrl+C** / **Ctrl+V** copy and paste the selection. **P** plays the selected strips once. Space plays the whole track. Embedded Play attaches the emu window. Neither path is MCU-S2 PWM. Code is TBD.

## Authoring

**Worlds.** Seven slots. World 1 starts as a 3x3 on a 16x16 map. Cart export packs **world 0** only (Studio World 1). BG1 is the playfield, BG0 is the parallax plane. Double-click an empty cell to create a screen. Right-click a BG1 cell for default screen / default world. PNG drop imports an atlas into the active world, BG bank 0.

**Paint.** Right-column **Work on** / **Hide** radios (BG, Sprite, Both). **Ctrl+click** stamps tiles. Drag an Entities row onto the preview to place an instance (switches to Sprite layer). World CHR vs global other CHR: [`memory.md`](../../general_docs/memory.md).

**Entities.** Up to **4** states x **4** frames x **6** sprites. **Add** opens compose. **Import** reads `aseprite_entities/` next to the saved `.r01proj` (manual, never on open). Right-click **Mark as player**. The playable player belongs on world 0. Hitbox is per state. Draw origin is per frame. Caps and pack: [`software-api.md`](../../general_docs/software-api.md). Kit palettes: [`general_docs/palette/`](../../general_docs/palette/README.md).

## Play

**Space** / Play always exports, shows a boot wait, then embeds emu. Cart boots world 0. Spawn is the first instance of the marked player type, else the default screen center. Gameplay SoT is emu Host Play, not `core/src/play.c`. Default is top-down. `r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER)` in `custom_logic.c` enables gravity, face-Y jump (hold for full height), and Down crouch when a crouch state is mapped. `r01_platformer_set_meter` sets pixels per meter (default 16). Player states (idle, walk, crouch, jump) are mapped in `custom_logic.c`. With no mapping, Play draws state 0 frame 0 and only X-flips for facing.

`custom_logic.c` is created on first export and never overwritten. The generated file sets the camera dead zone. Player anim maps stay commented until an author fills them in:

```c
void r01_custom_on_init(R01GameCtx *ctx) {
    r01_camera_set_deadzone(ctx, R01_CAM_DEADZONE_X_DEFAULT, R01_CAM_DEADZONE_Y_DEFAULT);
    /* r01_player_anim_set_idle_state(ctx, 0); */
    /* r01_player_anim_set_walk_all(ctx, 1); */
    /* r01_player_anim_set_crouch_state(ctx, 2); */
    /* r01_player_anim_set_jump_state(ctx, 3); */
    /* r01_bgm_play(ctx, 1); */
}
```

Generated API headers live in `C/include/r01_*.h` beside the project.

## Save and export

**Ctrl+S** / **Ctrl+O** the current path. First save (or unsaved) opens the Save project modal. Default parent is `apps/studio/projects/`. Quit does not auto-save. JSON version **15**. Save writes the active world. Worlds 1-7 are session-only until multi-world JSON. Load applies that world data to world 0.

**Ctrl+E** packs `<stem>.retr01` and regenerates `C/`, `ASM/`, and `data/` beside the project (or under `output/` if unsaved). Audio-tab tracks go into PRG at `$B000`. `r01_bgm_play(ctx, N)` in `custom_logic.c` selects the boot track at `$80FE`.

| Path | Role |
|------|------|
| `<stem>.r01proj` | Authoring JSON |
| `<stem>.retr01` | Packed cart (world 0) |
| `C/custom_logic.c` | Author file, kept |
| `C/`, `ASM/`, `data/` | Regenerated stubs and tables |

PROM and 512 KB flash images sit beside the cart. Layout: [`memory.md`](../../general_docs/memory.md).

## Keys

| Key | Action |
|-----|--------|
| Ctrl+S / Ctrl+O | Save / open |
| Ctrl+E | Export cart |
| Space | Play (export, then emu) |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+1 / Ctrl+2 | Window 1x / 2x |
| Ctrl+F | Fullscreen |
| Ctrl+Shift+R | 640x360 / 1280x720 |

## Related

- [`general_docs/video-graphics.md`](../../general_docs/video-graphics.md) - video, entities, palettes
- [`general_docs/palette/`](../../general_docs/palette/README.md) - kit RGB
- [`general_docs/software-api.md`](../../general_docs/software-api.md) - entity pack, PRG vs hardware
- [`general_docs/memory.md`](../../general_docs/memory.md) - cart image
- [`apps/emu/README.md`](../emu/README.md) - cart emulator
