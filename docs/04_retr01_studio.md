# Retr01 Studio

**Product SoT:** [`retr01_studio/README.md`](../retr01_studio/README.md). Build, controls, UI, export.

C11 + SDL2 authoring app. Phase 2 (current): 7-world sidebar, tile edit/paint, solid attrs, global palettes, default spawn. Phase 1 still: PNG import, Play, `.retr01` export (world 0 only).

**Caveats:** JSON v4 saves **one world's** map + `bg_bank0` (the active world on Ctrl+S). Load applies it to **world 0**. Play switches to `default_world` (map menu -> **Make default world**), not necessarily the sidebar selection.

Hardware contract: [`02`](02_graphics_worlds_memory.md). Not the board IC simulator ([`retr01_sim/README.md`](../retr01_sim/README.md)).

```bash
scripts/run-studio rom/test.r01proj
```
