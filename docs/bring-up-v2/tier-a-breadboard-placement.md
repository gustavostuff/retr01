# Tier A breadboard: zones, spacing, pin map

Physical desk layout for the Tier A video lab (DOT, Beam X/Y, color PROM, DAC, J2 RGBS).
No hole coordinates. Exact JEDEC fuse maps and programming live in [`docs/bringup/tier-a-video-lab.md`](../bringup/tier-a-video-lab.md) and [`docs/bringup/programming-tier-abc.md`](../bringup/programming-tier-abc.md).

Sim SoT for Auto nets: `apps/sim/tier-a/src/netlist.c`.

---

## Zones (yes, still group)

Three breadboards are typical. Treat them as islands, not one soup.

| Zone | Role | Parts |
|------|------|-------|
| **Clock** | Pierce + divide to DOT | Y2, U04, U74, R12, R13, C6, C7, C1, C2 |
| **Beam** | Raster and sync | UPLDX, UPLDY, C3, C4 |
| **Analog out** | Index, PROM, DAC, monitor | U24, R1-R11, J2, C5 |

Power entry (**E1** bulk, 5 V and GND into BB1 rails) sits at the clock or beam board edge so both digital islands share one clean return.

Flow on the desk:

```text
[Clock] -> [Beam X | Beam Y] -> [PROM + DAC + J2]
```

---

## Close vs far

### Keep close

| Pair | Why |
|------|-----|
| Each IC and its 100 nF | Bypass loop under a few cm |
| Y2, R13, C6, C7, U04 gate 1 | Pierce loop area stays small |
| U04 gate 2 out and U74 | 21 MHz into the divider |
| U74 DOT out, R12, UPLDX CLK | Short clock run |
| UPLDX and UPLDY | HWRAP is the line tick |
| U24 and R1-R11 and J2 | Analog island. DAC sums stay short |
| UPLDX INDEX[5:0] and U24 A[5:0] | Index bus stays short (under about one board length) |

### Keep apart / isolated

| Separation | Why |
|------------|-----|
| Analog out vs long DOT / HWRAP runs | Clock edges into RGB |
| Monitor cable strain on J2 vs crystal cans | Mechanical and HF coupling |
| Spare jumper nests over the DAC | Accidental shorts and noise |

Rails: bridge +5 V and GND on both sides of each board in use. Extra boards stay dead until a jumper reaches BB1 rails of that polarity.

---

## Part list (refdes)

| Refdes | Part | Zone |
|--------|------|------|
| Y2 | 21.47727 MHz crystal | Clock |
| U04 | 74HCU04 | Clock |
| U74 | 74HC74 | Clock |
| UPLDX | ATF22V10 Beam X | Beam |
| UPLDY | ATF22V10 Beam Y | Beam |
| U24 | AT27C256R | Analog out |
| J2 | 2x3 RGBS header | Analog out |
| C1-C5 | 100 nF | At U04, U74, UPLDX, UPLDY, U24 |
| C6, C7 | ~20 pF | Y2 loads |
| E1 | ~220 uF | 5 V entry |
| R12 | 33 ohm | DOT series |
| R13 | 1M | Pierce feedback |
| R1-R8 | DAC weights | Analog out |
| R9-R11 | 75 ohm | Analog out |

A canned 5.37 MHz oscillator or Si5351 may replace the Y2/U04/U74 Pierce chain. DOT still enters UPLDX CLK through R12.

---

## Connection map (logical pins)

Names match the sim netlist. Physical DIP numbers follow the part datasheets / `hw/md/`.

### Power and static ties

| Net | Pins |
|-----|------|
| `+5V` | U04 VCC, U74 VCC, UPLDX VCC, UPLDY VCC, U24 VCC, U24 VPP, U24 PGM#, UPLDX RES#, UPLDY RES#, U74 PRE#/CLR# (both stages), E1(+) |
| `GND` | All IC GND, U24 CE#, U24 OE#, U24 A6-A13, J2 GND pins, E1(-), unused U04 gate inputs tied inactive |

Bypass: C1 at U04, C2 at U74, C3 at UPLDX, C4 at UPLDY, C5 at U24 (cap pin 1 to VCC, pin 2 to GND).

### Clock island

| From | To |
|------|----|
| Y2 pin 1 | U04 `1A`, R13, C6 |
| Y2 pin 2 | U04 `1Y`, R13, C7, U04 `2A` |
| U04 `2Y` | U74 `1CLK` |
| U74 `1/Q` | U74 `1D` |
| U74 `1Q` | U74 `2CLK` |
| U74 `2/Q` | U74 `2D` |
| U74 `2Q` | R12 then UPLDX `CLK` (**DOT**) |

### Beam and sync

| From | To |
|------|----|
| UPLDX `HWRAP` | UPLDY `CLK` |
| UPLDX `CSYNC` | J2 `CSYNC` |

### Color index (Tier A only)

| From | To |
|------|----|
| UPLDX `INDEX0`..`INDEX5` | U24 `A0`..`A5` |

### DAC to J2

Packing `{RRRGGGBB}` on PROM `O7`..`O0`:

| Gun | PROM outs | Series Rs | Load | Header |
|-----|-----------|-----------|------|--------|
| R | O7, O6, O5 | R1 4k, R2 2k, R3 1k | R9 75 ohm to GND | J2 `R` |
| G | O4, O3, O2 | R4 4k, R5 2k, R6 1k | R10 75 ohm to GND | J2 `G` |
| B | O1, O0 | R7 2k, R8 1k | R11 75 ohm to GND | J2 `B` |

Each gun: the three (or two) weight resistors meet at one node. That node feeds the 75 ohm and the J2 color pin.

### J2

| Pin | Net |
|-----|-----|
| 1 | R |
| 2 | G |
| 3 | B |
| 4 | CSYNC |
| 5-6 | GND |

---

## Related

- [`docs/bringup/tier-a-video-lab.md`](../bringup/tier-a-video-lab.md)
- Tier B delta: [`tier-b-breadboard-placement.md`](tier-b-breadboard-placement.md)
- Tier C delta: [`tier-c-breadboard-placement.md`](tier-c-breadboard-placement.md)
