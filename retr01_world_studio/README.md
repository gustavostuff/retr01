# Retr01 World Studio

Visual authoring tool + shared `core/` library for Retr01 `.retr01` cartridges.

Planning docs: [`planning/README.md`](planning/README.md)

## Build

```bash
cmake -B build -S ..
cmake --build build
ctest --test-dir build --output-on-failure
```

## Layout

| Path | Role |
|------|------|
| `core/` | Shared library: RLE, MAP, cart I/O, palette (used by studio + emulator) |
| `tests/` | Unit/integration tests (`ctest`) |
| `planning/` | Design documents |
| `tools/` | Fixture generators |
| `retr01_palette_v_01.txt` | Canonical 64-color master palette |

Sibling project: [`../retr01_emu/`](../retr01_emu/) — E0 PPU viewer (Phase E0).

## Emulator smoke test

```bash
./build/retr01_world_studio/gen_one_screen_fixture tests/fixtures/one_screen.retr01
./build/retr01_emu/retr01_emu \
  --cart tests/fixtures/one_screen.retr01 \
  --world 0 --col 0 --row 0 \
  --dump-fb /tmp/one_screen.rgba
```

## Status

- [x] Phase 0 — `libretr01_core` + tests
- [x] E0 — `retr01_emu` static viewer + PPU blit test
- [ ] Phase 1 — SDL2 + ImGui Screen Painter (`RETR01_BUILD_STUDIO=ON` when ready)
