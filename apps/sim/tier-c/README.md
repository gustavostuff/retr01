# Retr01 Tier C sim

Isolated hardware lab for [docs/bringup/tier-c-video-lab.md](../../../docs/bringup/tier-c-video-lab.md). Built on [Tier B](../tier-b/README.md): same beam, Compositor, PROM, DAC, and **RGBS** path. Tier C adds **MCU-S1**, **field SRAM**, and **74HC573** so S1 firmware fills a **sprite field** in VBlank (plus optional **BG0** lines in the ping-pong region on the same chip).

Engine: [`tools/discrete_ic/`](../../../tools/discrete_ic/). Shared assets: [`apps/sim/common/`](../common/).

## Additions (not in Tier B)

| What | Where |
| --- | --- |
| **US1** AVR128DB28 (S1 shell) | `chips/avr128db28_s1.c` |
| **U41** AS6C62256 field SRAM | `chips/as6c62256.c` |
| **U573** 74HC573 address latch | `chips/sn74hc573.c` |
| Lab blitter: 8×8 metasprite + walk frame, BG0 hill lines | `src/s1_lab.c` |
| Compositor reads **field** + **BG0 ping** (no fixed sprite box) | `include/r01a_raster.h` |
| Auto netlist: S1 ↔ latch ↔ field, **VBL** from Beam Y | `src/netlist.c` |
| Tests `test_tier_c_field`, `test_tier_c_priority` | `tests/` |

## Wiring delta (breadboard)

Everything from Tier B stays. New jumpers (see bring-up doc section 8):

- **US1** `AD0`-`AD7` -> **U573** `D0`-`D7` -> **U41** `A0`-`A7`
- **US1** `A8`-`A14` -> **U41** `A8`-`A14`
- **US1** `ALE` -> **U573** `LE`; **US1** `/WE` -> **U41** `WE#`
- **U41** `CE#` low, beam **`OE#`** vs S1 **`/WE`** mutually exclusive (same as product)
- **Beam Y** `VBLANK` -> **US1** `VBL` (optional debug / RUN)
- **C6** on **US1** VDD, **C7** on **U573**, **C8** on **U41**; plus the Tier B set **C1**-**C5** and bulk **E1**

The sim uses the same island window as Tier A and Tier B. Those links are in the soft netlist, so the LCD demo runs without jumpers. **US1**, **U573**, and **U41** start on the DIGITAL island. Placement saves to this folder's `island_layout.json`. `R01S_LAYOUT` overrides that path.

## Build

```text
cmake -S apps/sim/tier-c -B apps/sim/tier-c/build -DCMAKE_BUILD_TYPE=Release
cmake --build apps/sim/tier-c/build -j
ctest --test-dir apps/sim/tier-c/build --output-on-failure
```

Repo: `./scripts/build-all.sh` installs `bin/sim-tier-c`. `./scripts/sim-tier-c.sh` runs it.

Controls match Tier A. See [tier-a/README.md](../tier-a/README.md).
