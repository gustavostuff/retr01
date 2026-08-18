# 11 — Testing policy

**Rule:** every feature ships with tests. Do not defer tests to “later phases” or a dedicated test sprint.

Unit and integration tests are added **in the same change** as the code they cover. A phase is not done until its tests pass in CI (or locally before merge).

## Principles

1. **Test with the feature** — new module → new test file in the same step.
2. **Test behavior, not implementation** — round-trips, invariants, error paths; avoid brittle white-box mocks.
3. **One decoder, one test suite** — `core/rle.c`, `cart_load.c`, etc. live in `libretr01_core`; studio and emulator both link it; tests run once against the library.
4. **Fixtures over hand-waving** — small binary/JSON files in `tests/fixtures/` for MAP, `.retr01`, `.r01proj`.
5. **No silent drift** — golden framebuffer hashes only change when visuals intentionally change (`--update-golden` documented in CI).
6. **UI is thin** — Screen Painter and ImGui get fewer unit tests; **`core/`**, **`pack/`**, and **`retr01_emu/`** get the coverage.

## Test pyramid

| Layer | What | When | Runner |
|-------|------|------|--------|
| **Unit** | Single function/module (RLE, attr, palette parse) | Every commit touching `core/` | C test binary / CTest |
| **Integration** | MAP build → load → decode → 1200 bytes | Phase 0, 2 | C test + fixtures |
| **Component** | E0 blit one screen; E1 N frames headless | E0, E1+ | `retr01_emu` test harness |
| **End-to-end** | `.r01proj` → build → emu headless | Phase 3+ | CI script |
| **Manual** | Painter UX, dock layout | Phase 1, 4 | Checklist in [08_preview_and_testing.md](08_preview_and_testing.md) |

Prefer **unit + integration** early; add E2E once Build exists.

## Layout (planning)

```text
retr01/
  retr01_world_studio/
    core/                    # production code
    pack/
    tests/
      unit/                  # test_rle.c, test_screen.c, test_palette.c, ...
      integration/           # test_map_roundtrip.c, test_cart_io.c, ...
      fixtures/              # one_screen.json, one_screen.retr01, ...
    CMakeLists.txt           # add_subdirectory(tests); CTest
  retr01_emu/
    test/
      test_ppu_blit.c        # E0
      test_bus_decode.c      # E1
      golden/                # expected *.raw framebuffer dumps
```

Test runner: **CMake + CTest** (or a minimal custom `main` that returns non-zero on failure — no heavy framework required for v1).

## Tests required per phase

### Phase 0 — `core/`

| Module | Tests (add when module lands) |
|--------|-------------------------------|
| `screen.c` / attrs | get/set all four corners; index math |
| `rle.c` | encode→decode 960+240; all-zero; all-literal; corrupt input fails |
| `map_builder.c` | 1-world 3-screen directory; 24-bit offsets; 64-row cap |
| `cart_io.c` | write/read `.retr01` RETR01 magic; PRG/CHR/MAP slices |
| `palette_io.c` | parse `retr01_palette_v_01.txt` → 64 entries |
| `project_io.c` | load/save minimal `.r01proj`; reject invalid schema |

**Exit:** `ctest` (or `make test`) green before E0 starts.

### Phase E0 — emulator viewer

| Module | Tests |
|--------|-------|
| MAP → VRAM | directory hit → exactly 1200 bytes in slot 0 |
| PPU BG | known tile + attr + CHR → single pixel color |
| Golden | `fixtures/one_screen.retr01` → hash or byte compare `golden/one_screen.raw` |

CLI `--dump-fb` doubles as a test helper.

### Phase 1 — Screen Painter

| Module | Tests |
|--------|-------|
| `pack/chr_pack.c` | dedupe; 256 cap error; attr generation on toy 16×16 canvas |
| Optional | load PNG fixture → N unique tiles (no SDL in test — feed buffer) |

ImGui panels: manual checklist only.

### Phase 2 — World grid / MAP export

| Module | Tests |
|--------|-------|
| `map_builder.c` | multi-screen world; parallax `flags=1`; empty `empty_off` |
| Integration | export MAP → E0 test harness loads each `(col,row)` |

### Phase E1 — CPU + cart

| Module | Tests |
|--------|-------|
| `cpu/` | known opcode sequence (nestest-style subset or custom tiny ROM) |
| `bus/` | RAM mirror bounds; `$FE60` write/read |
| Integration | `--headless --frames 60` exit 0 on test ROM |

### Phase 3 — PRG build

| Module | Tests |
|--------|-------|
| `asm_gen.c` | snapshot generated `.s` / `.inc` against committed fixtures |
| `validate.c` / pack linter | invalid pack override → error |
| E2E | `linear_h.r01proj` → build → E1 headless 300 frames |

### Phase E2 / 4 — fidelity + preview

| Module | Tests |
|--------|-------|
| Parallax / IRQ | scanline split with known nametable setup |
| Embed API | `retr01_emu_run_frame` × N without leak (valgrind optional) |

## Definition of done (every PR)

- [ ] New or changed behavior has a test (or explicit note why untestable — rare for `core/`).
- [ ] All existing tests pass.
- [ ] Fixtures committed if new format paths added.
- [ ] No `#ifdef TEST` hacks in production paths unless documented.

## CI (minimum)

```yaml
- cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
- cmake --build build
- ctest --test-dir build --output-on-failure
- build/retr01_emu --headless --cart retr01_world_studio/tests/fixtures/one_screen.retr01 --frames 60
```

Expand E2E after Phase 3.

## Related docs

- Phase gates: [07_build_pipeline.md](07_build_pipeline.md)
- Manual + golden checks: [08_preview_and_testing.md](08_preview_and_testing.md)
- Format invariants to assert: [06_data_formats.md](06_data_formats.md)
- Emulator milestones: [10_emulator_development.md](10_emulator_development.md)
