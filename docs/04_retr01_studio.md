# Retr01 Studio

**Product SoT:** [`retr01_studio/README.md`](../retr01_studio/README.md). Build, controls, UI, export.

C11 + SDL2 authoring app. Phase 3D (current): cart packs real SPR CHR + entity/instance tables; emu/sim Play OAM parity. Phase 3C: drag sprite/entity onto screen, place instances, Studio Play OAM. Phase 3B: Game entities accordion + Add/Edit entity modal. Phase 3A: Game sprites accordion + Create/Edit sprite modal, SPR catalog. Phase 2: 7-world sidebar, tile edit/paint, solid attrs, global palettes, default spawn. Phase 1 still: PNG import, Play, `.retr01` export (world 0 only).

**Caveats:** JSON v5 saves **one world's** map + `bg_bank0` + `spr_banks` + sprite catalog + entities + instances (the active world on Ctrl+S). Load applies it to **world 0**. Play switches to `default_world` (map menu -> **Make default world**), not necessarily the sidebar selection. Placed entities do not collide or move yet. Cart **SPR bank 0 tile 1** is reserved for the player stub.

Hardware contract: [`02`](02_graphics_worlds_memory.md). Not the board IC simulator ([`retr01_sim/README.md`](../retr01_sim/README.md)).

```bash
scripts/run-studio rom/test.r01proj
```
