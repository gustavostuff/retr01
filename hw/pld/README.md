# Retr01 ATF22V10 equation stubs (HC573-zero)

Draft CUPL-style equations for WinCUPL / Afterburner. Not fitted JEDEC yet.
Run the fitter before freezing pin maps. Escape: +1 PLD or 1-2 discrete latches.

**Pin authority:** [`../md/ATF22V10.md`](../md/ATF22V10.md), [`../../app/schematic_generator/retr01_schem/pinmap.py`](../../app/schematic_generator/retr01_schem/pinmap.py).

| File | PLD | Focus |
|------|-----|-------|
| [`upldb_scroll_x.cupl`](upldb_scroll_x.cupl) | UPLDB | Scroll X register + existing VRAM glue notes |
| [`upldx_scroll_y.cupl`](upldx_scroll_y.cupl) | UPLDX | Scroll Y register + beam X notes |
| [`upldy_raster.cupl`](upldy_raster.cupl) | UPLDY | Raster Y register + internal compare |
| [`upldv_map_a.cupl`](upldv_map_a.cupl) | UPLDV | CART_A14-A18 MAP export |

Decode (UPLDA) SEL equations are assumed present. Soft `$FExx` on 1284 need no PLD registers.
