# Retr01 Emulator

Software-visible C emulator for Retr01-A. Separate from the IC-first board simulator
([`retr01_sim/`](../retr01_sim/)). Contract: [`docs/02_graphics_worlds_memory.md`](../docs/02_graphics_worlds_memory.md).

## Level

PHI2-ish 65C02 + logical `$FExx` + VRAM/CHR/MAP — not pin settle / DIP entities.

## Phases

| Phase | Status | What |
|-------|--------|------|
| **1** | done | Cart load (`.retr01`), RAM `$0000-$7FFF`, PRG `$8000+` with I/O hole, 65C02 core |
| **2** | done | `$FExx` ports (PPU, VRAM, OAM, pads, MAP, APU stub), host pads |
| **3** | done | Beam/frame pacing, BG render (2×2 camera + scroll), kit Color PROM, soft world boot |
| **4** | done | SDL host runs `retr01_studio/project.retr01` |

**Soft world boot:** Studio’s stub PRG never streams MAP. Writing `$FE30` (and reset) loads CHR/pals/screens/parallax from the cart world blob into VRAM — what a fuller boot ROM would do.

## Build / run

```bash
cd retr01_emu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/retr01_emu ../retr01_studio/project.retr01
```

Or: `./scripts/build-run.sh [path.retr01]`

**Controls:** arrows = atlas pan (host shifts 2×2 VRAM; stub has no MAP streamer) · **0–7** = select world · Z X / 1 / Enter = P1 buttons · Space pause · R reset · Esc quit.

**Boot world:** stub `START_WORLD` is always **0** (editor `active_world` is not a cart field).

## Layout

| Path | Role |
|------|------|
| `include/retr01_emu/` | Public headers |
| `src/cart.c` | `.retr01` parse |
| `src/cpu.c` | 65C02-ish core |
| `src/ppu.c` | `$FExx` + video + soft boot |
| `src/machine.c` | Bus + frame step |
| `src/main.c` | SDL host |
