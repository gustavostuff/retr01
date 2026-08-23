# Retr01 Emulator

Software-visible C emulator for Retr01-A. Separate from the IC-first board simulator
([`retr01_sim/`](../retr01_sim/)). Contract: [`docs/02_graphics_worlds_memory.md`](../docs/02_graphics_worlds_memory.md).

Root helper: [`../emu`](../emu) (`build` · `run` · `build-run` · `unit`).

## Level

PHI2-ish 65C02 + logical `$FExx` + VRAM/CHR/MAP — not pin settle / DIP entities.
Empty-screen / player bounds are **game software** (same idea as Studio Play), not PPU silicon.

## Phases

| Phase | Status | What |
|-------|--------|------|
| **1** | **done** | Cart load (`.retr01`), RAM `$0000-$7FFF`, PRG `$8000+` with I/O hole, 65C02 core |
| **2** | **done** | `$FExx` ports (PPU, VRAM, OAM, pads, MAP, APU stub), host pads |
| **3** | **done** | Beam/frame pacing, BG render (2×2 camera + scroll), kit Color PROM, soft world boot |
| **4** | **done** | SDL host + atlas pan; runs `retr01_studio/project.retr01` (`START_WORLD=0`) |
| **5** | **next** | **Host Play soft-layer** — player sprite, pad-driven move/camera, refuse empty screens; viewport may still show empty cells |
| **6** | planned | **Sprites / OAM** — composite OAM over BG (8×8 / 8×16, bank/pal/flip/priority, 16/line cap) |
| **7** | planned | **MAP streamer** — shift 2×2 workbench from cart MAP (`$FE90`–`$FE93` / soft boot helper) when camera leaves loaded slots; retire host-only atlas hacks where PRG can drive it |
| **8** | planned | **NMI / raster** — VBlank NMI from `PPUCTRL`, `$FE04`/`$FE05` scanline IRQ hooks usable by real PRG |
| **9** | planned | **APU stub → hearable** — `$FE40`–`$FE5F` enough for Studio/cart smoke (not full 328P) |
| **10** | planned | **CPU / timing harden** — wider 65C02 coverage, tighter cycle counts vs beam; keep below `retr01_sim` pin level |

### Phase notes

**1–4 (cart viewer).** Soft world boot: Studio stub never streams MAP. `$FE30` / reset loads CHR/pals/screens/parallax into VRAM. Host arrows pan the atlas by shifting the 2×2 window.

**5 (next).** Mirror Studio Play feel on the loaded cart without waiting for real game PRG: one player meta/sprite, collision = AABB only on **present** screens, scroll modes later if useful. Still not silicon-accurate sprite pipeline.

**6.** OAM already has ports; render path does not composite yet.

**7.** Closes the gap between host pan and what shipping PRG must do at seams ([`02`](../docs/02_graphics_worlds_memory.md) camera workbench).

**8–10.** Needed once carts stop hanging after boot and exercise interrupts / audio / tighter timing.

## Build / run

```bash
# from repo root
./emu build-run
./emu unit

# or
cd retr01_emu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/retr01_emu ../retr01_studio/project.retr01
```

**Controls (phase 4):** arrows = atlas pan · **0–7** = world · Z X / 1 / Enter = P1 buttons · Space pause · R reset · Esc quit.

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
| `tests/` | Cart + boot smoke |
| `scripts/build-run.sh` | Local build+run |
