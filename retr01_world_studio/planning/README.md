# Retr01 World Studio — planning docs

Planning documents for **Retr01 World Studio**: a native SDL2 + Dear ImGui authoring tool that exports a full virtual cartridge (PRG + CHR + MAP) as a `.retr01` file.

These docs sit beside the hardware specification in [`markdown_v_01/`](../markdown_v_01/). When implementation starts, some of this material may be promoted into `markdown_v_01/13_world_studio.md`.

## Document index

| Doc | Topic |
|-----|--------|
| [01_overview.md](01_overview.md) | Goals, scope, repo layout, relationship to PPUX and the emulator |
| [02_ui.md](02_ui.md) | Application UI: panels, workflows, keyboard shortcuts, dock layout |
| [03_project_format.md](03_project_format.md) | `.r01proj` JSON schema — single source of truth for the build pipeline |
| [04_world_mode_packs.md](04_world_mode_packs.md) | How 6502 ASM is generated from world-mode packs and project data |
| [05_cart_assembly.md](05_cart_assembly.md) | How the final `.retr01` binary is assembled (PRG + CHR + MAP) |
| [06_data_formats.md](06_data_formats.md) | Screen layout, MAP-ROM, RLE codec, CHR packing — shared `core/` library |
| [07_build_pipeline.md](07_build_pipeline.md) | End-to-end build steps, cc65 integration, validation, phases |
| [08_preview_and_testing.md](08_preview_and_testing.md) | Live preview, emulator embed, test ROMs, lint rules |
| [09_master_palette.md](09_master_palette.md) | Canonical 64-color RGB ramp (`retr01_palette_v_01.txt`) |

## Quick reference

| Artifact | Extension | Role |
|----------|-----------|------|
| Editor project | `.r01proj` | JSON: worlds, screens, CHR, palette, world-mode pack |
| Per-screen blob | `.bin` | Intermediate MAP payload (build only) |
| Playable cart | `.retr01` | Full virtual cartridge for emulator / future flash (magic: `RETR01`) |

## Hardware spec pointers

- Screen geometry and attrs: [`markdown_v_01/02_graphics_and_cartridge.md`](../markdown_v_01/02_graphics_and_cartridge.md)
- Worlds, MAP directory, `load_screen`: [`markdown_v_01/04_worlds_and_screens.md`](../markdown_v_01/04_worlds_and_screens.md)
- Memory map, VRAM slots, `$FE90` MAP port: [`markdown_v_01/08_memory_map.md`](../markdown_v_01/08_memory_map.md)
- Emulator contract: [`markdown_v_01/07_emulator_specification.md`](../markdown_v_01/07_emulator_specification.md)
- Open items (B9 RLE): [`markdown_v_01/OPEN_QUESTIONS.md`](../markdown_v_01/OPEN_QUESTIONS.md)
- Master palette: [`retr01_palette_v_01.txt`](../retr01_palette_v_01.txt) — see [09_master_palette.md](09_master_palette.md)

## Implementation status

Not started. See [07_build_pipeline.md](07_build_pipeline.md) for phased delivery (Phase 0–4).
