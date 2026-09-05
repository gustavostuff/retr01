# Retr01 schematic generator (SKiDL)

Python codebase that turns the Retr01 **32-IC** motherboard wiring into a KiCad netlist. It mirrors the pin-level sim (`app/sim/`) and hardware docs so logic is verified in C first, then exported to copper.

## Code-to-copper pipeline

Aligned with `temp/AI_flow_for_PCB.pdf`:

1. **Verify logic** - run `./sim`, per-chip tests, and `./unit-tests`.
2. **Define wiring** - this package: modular island modules + `manifest.py` (from `app/sim/src/board.c`).
3. **Lock symbols** - populate `library/Retr01_Lib` from KiCad S-expressions (see `library/README.md`).
4. **Generate netlists** - `python generate.py` -> `output/retr01_mobo.net` + `output/retr01_cart.net`.
5. **Mechanical layout** - two KiCad projects: motherboard + cartridge. Lock I/O / edge.
6. **Route** - Quilter AI (flag `CRITICAL_NETS` in `retr01_schem/nets.py`).

## Layout

```
app/schematic_generator/
+-- generate.py              # CLI entry point (writes both netlists)
+-- requirements.txt         # skidl
+-- retr01_schem/
|   +-- bom.py               # System 32-IC BOM; board=mobo|cart
|   +-- manifest.py          # Motherboard wiring (stops at J36)
|   +-- cart_manifest.py     # Cartridge wiring (J36 <-> U40/U50)
|   +-- parts.py             # SKiDL Part templates
|   +-- connect.py           # Apply manifest -> nets
|   +-- board.py             # Mobo + cart assembly + J36 contract check
|   +-- nets.py              # Named net constants
|   +-- islands/             # One module per sim canvas island (A-O)
+-- library/                 # Retr01_Lib KiCad symbols (TBD)
+-- output/                  # Generated netlists + manifest JSON
+-- tests/
```

## Quick start

```bash
cd app/schematic_generator
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

python generate.py --check          # BOM validation only
python generate.py --manifest-only  # wiring JSON + gap list
python generate.py                  # KiCad netlist
python -m unittest discover -s tests
```

## Source of truth

| Layer | Location |
|-------|----------|
| Wiring (sim) | `app/sim/src/board.c` (`wire_*`), `ui_pin_net.c` |
| BOM / refdes | `app/sim/include/retr01_sim/bom32.h`, `docs/hardware.md` |
| Cart | `docs/cart.md` |
| Passives / RF | `docs/passive_rf_etc.md`, `hw/md/*.md` |

`manifest_gaps()` now tracks only post-closure leftovers (arcade pin-order lock, TVS at layout, APU audio ladder, SCALE DIP, R-2R values, FE06/FE07 soft path).

## Islands (sim canvas letters)

| Letter | Module | Key ICs |
|--------|--------|---------|
| A | `islands/power_clk.py` | Y1, U2 |
| C | `islands/cpu.py` | U1, U3, UPLDA, U20A |
| D | `islands/io_latch.py` | U5A-U5I |
| G | `islands/vram.py` | U6, U7A-U7C, UPLDB |
| H | `islands/beam.py` | UPLDX/Y, Y2, U5D (raster Y) |
| J | `islands/cart_socket.py` | J36, U20C |
| K | `islands/apu.py` | U328 |
| L | `islands/mcu_linebuf.py` | U1284, U41, U7D-U7F |
| O | `islands/video.py` | U24, UPLDV, U20B, J2 |
| N | `islands/cart_module.py` | U40, U50 |

## Next steps

- Add Retr01_Lib `.kicad_sym` files and switch `parts.py` from inline pins to library parts (needs KiCad).
- Lock arcade header pin order in `docs/controllers.md`, then update `manifest.py`.
- Import `output/retr01_mobo.net` into KiCad. Lock I/O. Quilter on `CRITICAL_NETS`.
