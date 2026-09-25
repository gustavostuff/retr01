# Retr01 Tier B sim

Isolated hardware lab for [docs/bringup/tier-b-video-lab.md](../../../docs/bringup/tier-b-video-lab.md). The board recipe and netlist stay in this folder. Clocks, PROM, AD724, the layout loader, the font, and the PNG chrome come from [`apps/sim/common/`](../common/). Engine: [`apps/netlist_sim/`](../../netlist_sim/).

Auto mode runs the priority picture with no jumpers. Manual mode inherits the Tier A breadboard layout (`apps/sim/tier-a/ui_layout.json`) on first launch: same part positions and the same jumpers. Tier B's own `ui_layout.json` is written on quit and used after that. The Compositor is the only part that layout does not already place. In Manual, the LCD stays blank while the netlist check reports any open or short, and pins named in a short pulse red and black.

## Additions (not in Tier A)

| What | Where |
| --- | --- |
| Third ATF22V10, refdes `UPLDC`, role Compositor | `chips/atf22v10.c` |
| 1-dot `INDEX[5:0]` latch on DOT | Compositor CLK |
| Priority: sprite box (kit 63) over BG1 bars over BG0 bands. BG1 cell 2 is kit 0, so the Y bands show through | `include/r01a_raster.h` |
| Count bits into the Compositor: Beam X `X5` `X6` `X7` `HBLANK`, Beam Y `Y5` `Y6` `Y7` `VBLANK` | PLD pinouts |
| Safe stubs on the Compositor: `PHI2` tied low, `RWB` tied high. `IO20`-`IO23` driven 0 (MAP stand-ins, no cart) | Auto netlist |
| First launch loads the Tier A layout (three boards, positions, jumpers) | `src/ui.c` |
| Tests `test_compositor`, `test_tier_b_priority`, `test_tier_b_seat` | `tests/` |

Sprite box is X cells 3-4 and Y cells 2-3 (32-dot cells). That is X 96-159, Y 64-127.

## Modifications (Tier A behavior that changed)

| Tier A | Tier B |
| --- | --- |
| Beam X drives `INDEX[5:0]` straight to the PROM | Beam X drives count bits only. Index comes from `UPLDC` |
| Method B bars on every active X cell, including cell 2 (kit 55) | Cell 2 is kit 0 so BG0 shows through. The other bar indices are unchanged |
| Default code cluster is one empty breadboard | Same default cluster. A saved Tier A layout supplies BB2, BB3, positions, and jumpers |
| Window title "Retr01 Tier A" | "Retr01 Tier B" |

Unchanged from Tier A: DOT and FSC, Beam X/Y raster and sync, PROM kit image, resistor DAC, AD724, LCD, Auto/Manual routing, jumper editing. Sync still does not go through the Compositor.

## Additional wiring

Power, clocks, sync, the DAC, and the PROM control jumpers stay as in Tier A. The bring-up delta (`docs/bringup/tier-b-video-lab.md` section 7) is the new work:

- Seat `UPLDC` on the existing boards. Decoupling stays the Tier A set (`C1`-`C7` and `E1`). |
- Beam X pins 14-19 are no longer `INDEX[5:0]` (they are `X5` `X6` `X7` and three unused pins). The six Tier A index jumpers into PROM `A[5:0]` have to move to the Compositor's `INDEX[5:0]`.
- New jumpers: DOT to Compositor CLK, `HBLANK` and `X5`-`X7` from Beam X, `VBLANK` and `Y5`-`Y7` from Beam Y, Compositor VCC/GND/RES#, `PHI2` to GND, `RWB` to 5 V.

BB2 and BB3 power rails stay dark until a jumper reaches a BB1 rail of the same polarity, same rule as Tier A.

## Build

```text
cmake -S apps/sim/tier-b -B apps/sim/tier-b/build -DCMAKE_BUILD_TYPE=Release
cmake --build apps/sim/tier-b/build -j
ctest --test-dir apps/sim/tier-b/build --output-on-failure
```

Repo wrappers: `./scripts/build-all.sh` installs `bin/sim-tier-b`. `./scripts/sim-tier-b.sh` runs it.

Controls match Tier A. See [tier-a/README.md](../tier-a/README.md).
