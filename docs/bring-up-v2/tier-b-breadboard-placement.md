# Tier B breadboard: zones, spacing, pin map (delta)

Prerequisite: Tier A layout and nets in [`tier-a-breadboard-placement.md`](tier-a-breadboard-placement.md).
This file covers only what Tier B adds or rewires.

Bring-up narrative: [`docs/bringup/tier-b-video-lab.md`](../bringup/tier-b-video-lab.md).
Sim SoT: `apps/sim/tier-b/src/netlist.c` (refdes **UPLDC** for the Compositor).

---

## Zones

Keep the Tier A three islands. Add the Compositor on the **Beam** board (or a tight fourth board touching Beam and Analog).

| Zone | Change |
|------|--------|
| Clock | Unchanged (or already a canned DOT source) |
| Beam | **+ UPLDC** and its 100 nF. Beam X/Y stay. INDEX leaves Beam X |
| Analog out | Unchanged PROM/DAC/J2. INDEX source becomes UPLDC |

Desk flow:

```text
[Clock] -> [Beam X | Beam Y | Compositor] -> [PROM + DAC + J2]
```

---

## Close vs far (new rules only)

### Keep close

| Pair | Why |
|------|-----|
| UPLDC and its 100 nF | Same bypass rule as Tier A ICs |
| UPLDX / UPLDY and UPLDC | HBLANK, X5-7, VBLANK, Y5-7, shared DOT |
| UPLDC INDEX[5:0] and U24 A[5:0] | Replaces the old Beam-X index run. Keep it short |

### Keep apart

Same analog isolation as Tier A. Do not park long stub bundles for unused CPU/MAP pins across the DAC.

---

## Parts added

| Refdes | Part | Zone |
|--------|------|------|
| UPLDC | ATF22V10 Compositor | Beam |
| C6 (sim) | 100 nF at UPLDC VCC | Beam |

Motherboard docs may call the same role **UPLDV**. On the A-C labs the sim refdes is **UPLDC**.

Clock path may already be a canned Y2 OSC (Tier B/C sims). Discrete Pierce from Tier A remains valid on the desk.

---

## Connection map (delta only)

### Remove

| Old link | Action |
|----------|--------|
| UPLDX `INDEX0`..`INDEX5` to U24 `A0`..`A5` | Disconnect |

### Add

| From | To |
|------|----|
| DOT (after R12) | UPLDC `CLK` (same node as UPLDX `CLK`) |
| UPLDX `HBLANK` | UPLDC `HBLANK` |
| UPLDX `X5`, `X6`, `X7` | UPLDC `X5`, `X6`, `X7` |
| UPLDY `VBLANK` | UPLDC `VBLANK` |
| UPLDY `Y5`, `Y6`, `Y7` | UPLDC `Y5`, `Y6`, `Y7` |
| UPLDC `INDEX0`..`INDEX5` | U24 `A0`..`A5` |

### Safe stubs (no CPU yet)

| UPLDC pin / net | Lab tie |
|-----------------|---------|
| VCC, RES# | `+5V` |
| GND | `GND` |
| PHI2 | `GND` |
| RWB | `+5V` |
| MAP / soft SEL / VRAM decode pins unused by the lab JEDEC | Documented NC or driven inactive in equations |

PROM, DAC, J2, HWRAP, CSYNC, and power ties stay as in the Tier A map.

---

## Related

- Base layout: [`tier-a-breadboard-placement.md`](tier-a-breadboard-placement.md)
- Next delta: [`tier-c-breadboard-placement.md`](tier-c-breadboard-placement.md)
- [`docs/bringup/tier-b-video-lab.md`](../bringup/tier-b-video-lab.md)
