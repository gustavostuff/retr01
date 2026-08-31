# Retr01 Passives, Ports, and RF

Non-IC parts and board-level RF / EMC practice. ICs, bus architecture, and island bring-up stay in [`hardware.md`](hardware.md) (that doc is chip-focused on purpose).

**Related:** [`hardware.md`](hardware.md) (32-IC BOM, islands). [`sound.md`](sound.md) (APU DAC). Color PROM path: [`hw/md/AT28C16.md`](../hw/md/AT28C16.md).

Power assumption: **stable external 5 V** (barrel / PSU). No on-board switching regulator in the baseline. Passives are **outside** the 32-IC goal.

---

## What actually bites first

RF emissions are real, but they rarely keep a discrete 5 V game board from **booting and painting**. Function-first risks are usually **propagation delay** across HC / PLD / SRAM paths (datasheet ranges; worth simulating). MCU clocks sit mostly **inside** the package; board clocks (PHI2, dot) matter more for layout than for “will it work.”

Take CE / FCC seriously when selling into regulated markets. Until then, prefer **direct traces**, a **continuous return path**, and comparative RF checks over over-engineering every harmonic.

---

## 4-layer stackup (return path first)

The highest-frequency content on this board is not the clock fundamental — it is the **harmonics of sharp digital edges**. Those currents need a **continuous, nearby return**.

**Avoid** the common trap **signal–power–ground–signal**:

- HF return wants to hug the adjacent plane.
- A layer change from top signal to bottom signal jumps the return from the power plane to the ground plane (or vice versa).
- That discontinuity radiates.
- Stitch caps between power and ground can pass some of that return (high-pass between planes), but real caps have inductance; the **highest** frequencies still see an open. Cap impedance vs frequency is **V-shaped** — audio “bypass” intuition does not map cleanly onto digital edge harmonics.

**Target stackup for Retr01:**

| Layer | Role |
|-------|------|
| Top | Signals + **5 V pours** (flood around traces; pour last) |
| Inner 1 | **GND** |
| Inner 2 | **GND** |
| Bottom | Signals + **5 V pours** as needed |

Both inners as ground keeps a solid reference under every signal layer and avoids power-plane return hops. Distribute 5 V with shapes on the outer layers, not a dedicated mid-board power plane.

Still: keep high-activity nets relatively **direct**, return to ground ** ASAP** (vias, stitching at connectors and under DIPs), and keep clocks / bus runs off long pad-port cables.

---

## Propagation delay (functional, not RF)

Before chasing antennas, budget chip-to-chip delay:

- PHI2 / decode / HC245 / latch / SRAM / PLD paths must close inside the 8 MHz and dot-clock windows.
- Use datasheet **min/typ/max** propagation; worst-case stacks matter more than RF folklore.
- Sim (`retr01_sim`) and island bring-up are the right places to catch this early.

---

## Power entry and rail hygiene

External 5 V is trusted for regulation, not for abuse or cable noise. Treat the barrel as a noisy entry.

| Item | Role |
|------|------|
| Barrel jack (2.1 mm class) | 5 V in. Retr01-A / -C |
| Series PPTC on VIN | Board-level short / overload. Hold above full-board idle, trip on hard short |
| Reverse-polarity diode (or P-FET ideal diode) | Blocks reverse barrel plug |
| Bulk cap at entry (**100–470 µF** low-ESR electrolytic or polymer) | Holds rail through plug bounce and load steps |
| Input ferrite (or CMC on 5 V / GND pair) | Damps cable-borne RF before the pours |
| Local **100 nF X7R** on every IC VCC pin (mm from pin) | HF bypass; mandatory for HC / PLD / AVR edge rates |
| Local **1–10 µF** ceramic per island / large DIP | Mid-band reservoir (6502, 1284, 328P, PLD cluster, SRAM bank) |
| Ferrite + **10 µF** into **analog / video** spur | Isolates Color PROM R-2R and APU DAC from digital di/dt |

Never snake return current through video or pad-port copper. Stitch GND vias at every connector shell and under each DIP.

---

## Clocks and reset

| Item | Role |
|------|------|
| Canned oscillators (PHI2 8 MHz, dot ~5.369 MHz) | Prefer cans over bare crystals for edge control |
| Crystals + load caps for AVRs if not using cans | 20 MHz (1284), 16 MHz (328P); keep loops tiny |
| Series damping **22–47 Ω** on clock nets leaving a can / buffer | Softens edges into long traces |
| **74HC14** (outside 32-count if needed) | Schmitt cleanup for reset / slow edges |
| RC + Schmitt (or supervisor, e.g. MCP120-class) on `/RESB` and AVR `RESET` | Power-on reset; hold low until 5 V is solid |
| Pull-ups on open-drain resets / IRQB | Typical **4.7–10 kΩ** |

Board clocks matter for layout cleanliness; they are not automatically a show-stopper for RF. Keep traces short, away from cart edge and pad jacks. No unterminated stubs.

---

## Video and audio analog

| Item | Role |
|------|------|
| Color PROM **R-2R** ladder (**1%** metal film) | R3G3B2 -> analog guns ([`AT28C16`](../hw/md/AT28C16.md)) |
| **75 Ω** series per R/G/B (+ sync termination as needed) | Drive RGBS into 75 Ω video plant |
| Optional ferrite beads on RGBS | Cable RF; place at connector |
| APU **R-2R** (or PWM RC) from 328P | Line-level mix ([`sound.md`](sound.md)) |
| AC-coupling cap + series build-out on audio out | Blocks DC into TVs / amps |
| Video / AV connectors | Retr01-A: RGBS (+ S-Video / composite path TBD). Levels bench-tuned |

---

## Cart edge and user I/O ESD

Anything a human can touch gets a clamp **at the connector**, then a series limiter, then the IC.

| Item | Role |
|------|------|
| TVS array (5 V working, e.g. PESD5V0-class) on cart address/data/control as needed | ESD into flash / HC245 domain |
| Series **22–100 Ω** on slow GPIO / pad DATA | Limits IC clamp current; damps cable resonances |
| SCALE DIP + pull-ups/downs | 1x / 2x select; define idle state |

---

## Retr01-C controller ports (3.5 mm TRS)

Design goal: **female jack on console and on each controller**. The interconnect is a commodity **male–male 3.5 mm aux** cable of any length. No proprietary tether.

| Item | Spec / role |
|------|-------------|
| Jack | **Switchcraft 35RAPC** series, **TRS (stereo)** — e.g. **35RAPC3BH3** (horizontal, threaded bushing) for panel/PCB. Same family on pad PCBs |
| Conductors | **Tip / Ring / Sleeve** = **VCC / DATA / GND** (exact T/R assignment locked at schematic; Sleeve = GND + shell) |
| Port count | **2** (P1, P2) on console |
| Pad MCU | **ATtiny85** draft on the controller board; 1284 still presents `$FE60` / `$FE61` |
| PPTC (Polyfuse) per port on **VCC** | Shorted aux tip–ring or crushed cable must not toast the plane. Size **Ihold** for one ATtiny85 + switches/LEDs (roughly **100–250 mA** class, **Vmax ≥ 6 V**); place on the console **and** consider a mate on the pad board |
| TVS to GND on VCC and DATA at each jack | ESD / hot-plug; PTC alone is too slow for ESD |
| Series **R** on DATA (both ends if practical) | Current limit into MCU pins + RF damping on long aux runs |
| Local **100 nF** on port VCC after the PTC | Decouples the cable stub |

```text
  Console 5 V --[PPTC]--+--[TVS]-- Tip (VCC) ---- aux M-M ---- Tip --[TVS]--+--> pad 5 V
                        |                                                 |
                     100 nF                                              MCU
                        |                                                 |
  GND plane ------------+-- Sleeve (GND) ---------------- Sleeve ---------+
                        |
  1284 / pad bridge ----+--[R]--[TVS]-- Ring (DATA) ---- Ring --[R]--[TVS]--> ATtiny85
```

**Why PTC + TVS:** PPTC covers **sustained shorts** (user cables). TVS covers **nanosecond ESD**. Neither replaces the other.

A long aux is still an antenna: keep the on-console DATA run short to the bridge, clamp at the jack, and avoid routing DATA parallel to PHI2 / dot clocks.

### Retr01-A cabinet I/O (contrast)

| Item | Role |
|------|------|
| IDC / discrete wiring to sticks and buttons | Direct GPIO into 1284 (with series R + optional TVS) |
| No 3.5 mm pad ports on the arcade shell | Controllers are built into the cabinet |

---

## Passive count mindset (planning)

Exact E24 values land at schematic time. Budget order-of-magnitude for a Retr01-C mobo + 2 pads:

- **~40–60×** 100 nF decoupling
- **~10–15×** 1–10 µF island caps + **1×** bulk at barrel
- **R-2R** networks (video + audio) + **75 Ω** build-outs
- **2×** port PPTC + **2–4×** board/entry PPTC/ferrite as needed
- **TVS** packs at cart + both pad jacks (+ audio/video if exposed)
- **4×** Switchcraft 35RAPC TRS (2 console + 1 per controller)
- Oscillators / crystals, reset RC, pull-ups, SCALE DIP, barrel, AV connectors

---

## Measuring RF (comparative)

For pre-compliance gut checks without a lab: a DIY **TEM cell** (copper foil + cardboard works; aluminum foil is a possible substitute) is enough for **comparative** measurements — change a pour, ferrite, or clock damper and see if the reading moves. Conceptually it is a coax expanded so the DUT sits between the center conductor and the shield. Open-sided builds leak external noise; treat results as relative, not absolute CE numbers.

Formal **CE / FCC** work comes later (Crowd Supply–class EU sales need marking). Expect ferrite / clamp iteration on the first real spin if emissions matter for distribution.

Further reading that clarifies stackup and return paths: Rick Hartley lectures on PCB layer arrangement (search by name; long-form video).

---

## Open topics

| Topic | Note |
|-------|------|
| TRS pin map (T/R = VCC/DATA) | Lock at schematic; document for third-party pads |
| PPTC Ihold per pad port | Bench ATtiny85 + LED budget, then pick family (e.g. Bourns MF-MSMF / Littelfuse 1206L) |
| First-spin RF | TEM comparative + ferrite/clamp tweaks; formal CE only if selling into marked markets |
| Prop-delay budget | Capture HC / PLD / SRAM stacks in sim before PCB freeze |
