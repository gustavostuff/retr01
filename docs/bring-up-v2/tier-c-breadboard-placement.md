# Tier C breadboard: zones, spacing, pin map (delta)

Prerequisite: Tier A + Tier B layouts in [`tier-a-breadboard-placement.md`](tier-a-breadboard-placement.md) and [`tier-b-breadboard-placement.md`](tier-b-breadboard-placement.md).
This file covers only the MCU-S1 field cluster.

Bring-up narrative: [`docs/bringup/tier-c-video-lab.md`](../bringup/tier-c-video-lab.md).
Sim SoT: `apps/sim/tier-c/src/netlist.c`. Product PORT freeze: [`docs/general/hardware.md`](../general/hardware.md) (MCU-S1).

---

## Zones

Keep Clock, Beam (+ Compositor), and Analog out. Add a fourth island:

| Zone | Role | Parts |
|------|------|-------|
| **Field** | S1 write path into field SRAM | US1, U573, U41, plus 100 nF on each |

Desk flow:

```text
[Clock] -> [Beam + Compositor] -> [PROM + DAC + J2]
                |
                +-> [US1 | U573 | U41]   (Field)
```

Field sits beside Beam/Compositor so VBLANK reaches US1 on a short jumper. It stays off the DAC island.

---

## Close vs far (new rules only)

### Keep close (triangle)

| Pair | Why |
|------|-----|
| US1, U573, U41 | AD[7:0], ALE, A[14:8], /WE stay local |
| Each of those ICs and its 100 nF | AVR and SRAM edges are fast |
| UPLDY `VBLANK` and US1 `VBL` | Timing input |

### Keep apart

| Separation | Why |
|------------|-----|
| Field AD bus vs Analog out | Multiplexed AD is edge-rich |
| Field vs long DOT runs across the AVR | Clock into AD |
| UPDI Friend cable draped over DAC or crystals | Noise and shorts |

Bus discipline (spacing will not fix a logic overlap): AD hi-Z outside owned write windows. ALE low and /WE high when idle. Beam `/OE` and S1 `/WE` must not assert together on the product path.

---

## Parts added

| Refdes | Part | Zone |
|--------|------|------|
| US1 | AVR128DB28 MCU-S1 | Field |
| U573 | 74HC573 | Field |
| U41 | AS6C62256-55 field SRAM | Field |
| C7, C8, ... | 100 nF at US1 VDD, U573 VCC, U41 VCC | Field |

Optional 74HC574 (scroll X) is not required for the lab if scroll stays hardwired to 0.

---

## Connection map (Field only)

Video INDEX, DOT, sync, PROM, and DAC stay as in A/B.

### Power

| Net | Pins |
|-----|------|
| `+5V` | US1 VDD, VDDIO2, AVDD; U573 VCC; U41 VCC |
| `GND` | US1 GND, GND2; U573 GND; U41 VSS; U41 CE# |

Bypass: one 100 nF at each VCC pin cluster, within a few cm.

### Timing

| From | To |
|------|----|
| UPLDY `VBLANK` | US1 `VBL` (same net already feeds UPLDC `VBLANK`) |

### Latch and low address

| From | To |
|------|----|
| US1 `AD0`..`AD7` | U573 `D0`..`D7` and U41 `DQ0`..`DQ7` (shared AD bus) |
| US1 `ALE` | U573 `LE` |
| U573 `Q0`..`Q7` | U41 `A0`..`A7` |
| U573 `OE#` | `GND` (outputs enabled) |

### High address and write

| From | To |
|------|----|
| US1 `A8`..`A14` | U41 `A8`..`A14` |
| US1 `/WE` | U41 `WE#` (idle high with local pull-up recommended) |

### Lab vs product on OE#

The Tier C sim may hold U41 `OE#` low for a simple write-only bench. On the product path a beam PLD owns field `/OE` so display reads and S1 writes never overlap. Prefer the product rule when the Compositor starts sampling the field.

### UPDI

US1 pin 19 is UPDI for Adafruit UPDI Friend programming. Keep that header away from the DAC island.

---

## Related

- [`tier-a-breadboard-placement.md`](tier-a-breadboard-placement.md)
- [`tier-b-breadboard-placement.md`](tier-b-breadboard-placement.md)
- [`docs/bringup/tier-c-video-lab.md`](../bringup/tier-c-video-lab.md)
- [`docs/ic_behavior/74HC573.md`](../ic_behavior/74HC573.md), [`docs/ic_behavior/AVR128DB28.md`](../ic_behavior/AVR128DB28.md)
