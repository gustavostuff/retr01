# Retr01 Studio

**Product SoT:** [`retr01_studio/README.md`](../retr01_studio/README.md). Build, controls, UI, export.

C11 + SDL2 authoring app. **Phase 3E** (current): **Metasprites** accordion + modal (multi-part SPR groups), entity compose from metasprite catalog, JSON v6 `metasprites` field. **Phase 3D**: cart packs real SPR CHR + entity/instance tables; emu/sim Play OAM parity. **Phase 3C**: drag sprite/metasprite/entity onto screen, place instances, Studio Play OAM. **Phase 3B**: Entities accordion + Add/Edit entity modal. **Phase 3A**: Sprites accordion + Create/Edit sprite modal, SPR catalog. Phase 2: 8-world sidebar, tile edit/paint, solid attrs, global palettes, default spawn (**30 present screens**/world cart cap). Phase 1 still: PNG import, Play, `.retr01` export (world 0 only).

**Caveats:** JSON v6 saves **one world's** map + `bg_bank0` + `spr_banks` + sprite catalog + **metasprites** + entities + instances (the active world on Ctrl+S). Load applies it to **world 0**. **Metasprites are Studio-only** -- cart export flattens composed entity frames (no separate metasprite table in ROM). Play switches to `default_world` (map menu -> **Make default world**), not necessarily the sidebar selection. Placed entities do not collide or move yet. Cart **SPR bank 0 tile 1** is reserved for the player stub. Host Play OAM uses **signed viewport-relative** X/Y bytes; sprites clip to the **128x120** viewport.

Hardware contract: [`02`](02_graphics_worlds_memory.md). Not the board IC simulator ([`retr01_sim/README.md`](../retr01_sim/README.md)).

```bash
scripts/run-studio rom/test.r01proj
```
