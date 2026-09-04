# Retr01 Passives, Ports, and RF

Non-IC parts and board-level RF / EMC practice. ICs, bus architecture, and island bring-up stay in [`hardware.md`](hardware.md) (that doc is chip-focused on purpose).

**Related:** [`hardware.md`](hardware.md) (32-IC BOM, islands). [`sound.md`](sound.md) (APU DAC). [`cart.md`](cart.md) (36-pin edge). Color PROM: [`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md). Pads: [`controllers.md`](controllers.md).

Power assumption: **stable external 5 V** (barrel / PSU). No on-board switching regulator in the baseline. Passives are **outside** the 32-IC goal.

---

## What actually bites first

RF emissions are real, but they rarely keep a discrete 5 V game board from **booting and painting**. Function-first risks are usually **propagation delay** across HC / PLD / SRAM paths (datasheet ranges, worth simulating). MCU clocks sit mostly **inside** the package. Board clocks (PHI2, dot) matter more for layout than for "will it work."

Take CE / FCC seriously when selling into regulated markets. Until then, prefer **direct traces**, a **continuous return path**, and comparative RF checks over over-engineering every harmonic.

---

## 4-layer stackup (return path first)

The highest-frequency content on this board is not the clock fundamental. It is the **harmonics of sharp digital edges**. Those currents need a **continuous, nearby return**.

**Avoid** the common trap **signal-power-ground-signal**:

- HF return wants to hug the adjacent plane.
- A layer change from top signal to bottom signal jumps the return from the power plane to the ground plane (or vice versa).
- That discontinuity radiates.
- Stitch caps between power and ground can pass some of that return (high-pass between planes), but real caps have inductance. The **highest** frequencies still see an open. Cap impedance vs frequency is **V-shaped**. Audio "bypass" intuition does not map cleanly onto digital edge harmonics.

**Target stackup for Retr01:**

| Layer | Role |
|-------|------|
| Top | Signals + **5 V pours** (flood around traces, pour last) |
| Inner 1 | **GND** |
| Inner 2 | **GND** |
| Bottom | Signals + **5 V pours** as needed |

Both inners as ground keeps a solid reference under every signal layer and avoids power-plane return hops. Distribute 5 V with shapes on the outer layers, not a dedicated mid-board power plane.

Still: keep high-activity nets relatively **direct**, return to ground **ASAP** (vias, stitching at connectors and under DIPs), and keep clocks / bus runs off long pad-port cables.

---

## Propagation delay (functional, not RF)

Before chasing antennas, budget chip-to-chip delay:

- PHI2 / decode / HC245 / latch / SRAM / PLD paths must close inside the 8 MHz and dot-clock windows.
- Use datasheet **min/typ/max** propagation. Worst-case stacks matter more than RF folklore.
- Sim (`app/sim`): default is zero-delay for catchup. Opt in with **`./sim ... DELAY=typical|max`** (or env `R01S_PROP_DELAY`) to print HC/PLD/SRAM path budget vs PHI2 half. Pin netlist stays combinatorial. See `tests/test_timing.c` and [`PERFORMANCE.md`](../app/sim/PERFORMANCE.md).

---

## Power entry and rail hygiene

External 5 V is trusted for regulation, not for abuse or cable noise. Treat the barrel as a noisy entry.

| Item | Role |
|------|------|
| Barrel jack | **CUI PJ-063AH** (2.1 mm ID, horizontal) | 5 V in (shared motherboard) |
| Series PPTC on VIN | Board-level short / overload. Hold above full-board idle, trip on hard short |
| Reverse-polarity diode (or P-FET ideal diode) | Blocks reverse barrel plug |
| Bulk cap at entry (**100-470 uF** low-ESR electrolytic or polymer) | Holds rail through plug bounce and load steps |
| Input ferrite (or CMC on 5 V / GND pair) | Damps cable-borne RF before the pours |
| Local **100 nF X7R** on every IC VCC pin (mm from pin) | HF bypass. Mandatory for HC / PLD / AVR edge rates |
| Local **1-10 uF** ceramic per island / large DIP | Mid-band reservoir (6502, 1284, 328P, PLD cluster, SRAM bank) |
| Ferrite + **10 uF** into **analog / video** spur | Isolates Color PROM R-2R and APU DAC from digital di/dt |

Never snake return current through video or pad-port copper. Stitch GND vias at every connector shell and under each DIP.

---

## Clocks and reset

| Item | Locked part / value |
|------|---------------------|
| **Y1** PHI2 | **Abracon ACO-8.000MHZ-EK** — 8.000 MHz, 5 V HCMOS, full-size DIP-14 can. Footprint `Oscillator:Oscillator_DIP-14` (pins **1=OE, 7=GND, 8=OUT, 14=Vcc**). OE tied high |
| **Y2** dot | **Abracon ACO-5.369318MHZ-EK** — same package/footprint. Exact NTSC/3 frequency; often **factory-order** from Abracon (not always DigiKey shelf stock). Any 5 V HCMOS ACO-footprint XO at **5.369318 MHz ±50 ppm** is OK |
| **Y3** AD725 4FSC | **Abracon ACO-14.31818MHZ-EK** — same package/footprint |
| **Y4** 1284 | **Abracon ABLS7M-20.000MHZ-D2Y-T** HC-49/U + **2× 22 pF** to GND |
| **Y5** 328P | **Abracon ABLS7M-16.000MHZ-D2Y-T** HC-49/U + **2× 22 pF** to GND |
| Series damping **22-47 ohm** on clock nets leaving a can / buffer | Softens edges into long traces |
| **74HC14** (outside 32-count) | Schmitt cleanup for reset / slow edges |
| RC + Schmitt (or supervisor, e.g. MCP120-class) on `/RESB` and AVR `RESET` | Power-on reset. Hold low until 5 V is solid |
| Pull-ups on open-drain resets / IRQB | Typical **4.7-10 kohm** |

Board clocks matter for layout cleanliness. They are not automatically a show-stopper for RF. Keep traces short, away from cart edge and pad jacks. No unterminated stubs.

---

## Locked connectors (buy list)

| Ref | Locked MPN | Footprint notes |
|-----|------------|-----------------|
| **J1** | **CUI PJ-063AH** (2.1 mm ID barrel) | Stock KiCad `BarrelJack_CUI_PJ-063AH_Horizontal` (1=tip, 2=sleeve, MP→GND) |
| **J2** | 1×5 pin header 2.54 mm | Stock `PinHeader_1x05_P2.54mm_Vertical` — RGBS harness |
| **J3/J4** | **Switchcraft 35RAPC2BVN4** | **Custom** `Retr01_Lib:Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical` |
| **J5/J6** | 1×10 pin header 2.54 mm | Stock vertical |
| **J7** | 1×4 pin header 2.54 mm | Stock vertical |
| **J8** | **CUI RCJ-012** (black RCA, audio) | **Custom** `Retr01_Lib:CUI_RCJ-01x_Vertical` (shared with J9) |
| **J9** | **CUI RCJ-014** (yellow RCA, composite) | Same footprint as J8 |
| **J36** | **EDAC 395-036-559-212** (right-angle 2×18) | Stock stand-in `PinSocket_2x18_P2.54mm_Horizontal` until EDAC CAD; arcade alt **395-036-520-201** (straight) is shell-only, not dual-footprint |
| **U725** | **AD725ARZ** | Stock `SOIC-16_3.9x9.9mm_P1.27mm` |

**You build yourself:** TRS jack footprint + RCJ RCA footprint (same holes for color variants). Optionally refine J36 from EDAC drawing.

---

## Video and audio analog

| Item | Role / locked value |
|------|---------------------|
| Color PROM **binary-weighted** DAC (**1%** metal film) | R3G3B2 → analog guns ([`AT27C256R`](../hw/md/AT27C256R.md)). Packing `(R<<5)\|(G<<2)\|B` |
| Video bit resistors (LSB→MSB) | Red/Green: **4.00 / 2.00 / 1.00 kΩ**. Blue: **2.00 / 1.00 kΩ** |
| **75.0 Ω** to **GND** on each R/G/B | Termination → **~0.7 Vpp** into 75 ohm video plant |
| Optional ferrite beads on RGBS | Cable RF. Place at connector |
| Composite encoder | **AD725** (SOIC-16): RGBS guns AC-coupled in, **CSYNC** on HSYNC, **NTSC**, **14.31818 MHz** 4FSC can. COMP → 75 Ω → J9. Outside 32-IC logic count (like 74HC14). **No S-Video jack** (LUMA/CRMA unpopulated) |
| APU **R-2R** from 328P `AUD0`–`AUD7` | Classic ladder **R = 10.0 kΩ**, **2R = 20.0 kΩ** (1%), then AC-couple to line out ([`sound.md`](sound.md)) |
| APU build-out | **1.0 kΩ** series + **10 µF** AC-coupling toward jack |
| Video / AV connectors | **J2** RGBS header + **J8** RCJ-012 audio RCA + **J9** RCJ-014 composite RCA. Levels bench-tuned |
| **SCALE** select | Single SPST DIP/`SW_SCALE`: **open = 2×** (default, soft pull-down on `SCALE_1X`), **closed = 1×** (ties `SCALE_1X` to +5 V). See [`hardware.md`](hardware.md) |


---

## Cart edge and user I/O ESD

Anything a human can touch gets a clamp **at the connector**, then a series limiter, then the IC. **36-pin** cart pinout: [`cart.md`](cart.md).

| Item | Role |
|------|------|
| TVS array (5 V working, e.g. PESD5V0-class) on cart address/data/control as needed | ESD into flash / HC245 domain |
| Series **22-100 ohm** on slow GPIO / pad DATA | Limits IC clamp current. Damps cable resonances |
| SCALE DIP + pull-ups/downs | **Locked:** open = **2×** default; closed drives `SCALE_1X` high for **1×** |


---

## Controller I/O (shared motherboard)

**Silicon / PCB target:** arcade and console use the **same PCB**. Both I/O styles are on every board (arcade microswitch headers **and** footprints for two TRS jacks). Shell / BOM population chooses which path you use. Software stays `$FE60` / `$FE61` via the 1284.

**Runners today:** Emu / Sim Host Play drive `$FE60` / `$FE61` only. They do not model TRS jacks or arcade header pinouts as separate netlist islands yet.

### Arcade controllers (microswitches)

Simple **switch-to-GND** circuits: sticks and buttons close contacts. No pad MCU on this path.

| Item | Spec |
|------|------|
| **J5** | **1×10** P1 header — pinout locked in [`controllers.md`](controllers.md#j5--player-1-110) |
| **J6** | **1×10** P2 header — same order for `$FE61` |
| **J7** | **1×4** `+5V` / `GND` / `RESET_N` / `GND` cabinet tap |
| Series **R** | **47 ohm** per signal line |
| Optional TVS | At connector (layout) |
| Bit contract | `$FE60` / `$FE61`, bit set = pressed |

### Aux pad ports (3.5 mm TRS footprints)

Design goal: **female jack on the motherboard** (2x) and on each optional pad board. Interconnect = commodity **male-male 3.5 mm aux**. No proprietary tether. Jacks may be **DNP** on a pure arcade build. **Footprints / solder holes stay on the PCB**.

| Item | Spec / role |
|------|-------------|
| Jack | **Switchcraft 35RAPC** series, **TRS (stereo)**. Mobo SKU lock: **35RAPC2BVN4** (vertical, non-threaded). Same family on pad PCBs |
| Conductors | **Tip / Ring / Sleeve** = **VCC / DATA / GND** (exact T/R assignment locked at schematic. Sleeve = GND + shell) |
| Port count | **2** (P1, P2) footprints on the motherboard |
| Pad MCU | **ATtiny85** on the controller board ([`controllers.md`](controllers.md)). 1284 still presents `$FE60` / `$FE61` |
| PPTC (Polyfuse) per port on **VCC** | Shorted aux tip-ring or crushed cable must not toast the plane. Size **Ihold** for one ATtiny85 + switches/LEDs (roughly **100-250 mA** class, **Vmax >= 6 V**). Place on the mobo **and** consider a mate on the pad board |
| TVS to GND on VCC and DATA at each jack | ESD / hot-plug. PTC alone is too slow for ESD |
| Series **R** on DATA (both ends if practical) | Current limit into MCU pins + RF damping on long aux runs |
| Local **100 nF** on port VCC after the PTC | Decouples the cable stub |

```text
  Mobo 5 V --[PPTC]--+--[TVS]-- Tip (VCC) ---- aux M-M ---- Tip --[TVS]--+--> pad 5 V
                     |                                                 |
                  100 nF                                              MCU
                     |                                                 |
  GND plane ---------+-- Sleeve (GND) ---------------- Sleeve ---------+
                     |
  1284 / pad bridge -+--[R]--[TVS]-- Ring (DATA) ---- Ring --[R]--[TVS]--> ATtiny85
```

**Why PTC + TVS:** PPTC covers **sustained shorts** (user cables). TVS covers **nanosecond ESD**. Neither replaces the other.

A long aux is still an antenna: keep the on-board DATA run short to the bridge, clamp at the jack, and avoid routing DATA parallel to PHI2 / dot clocks.

---

## Passive count mindset (planning)

Exact E24 values land at schematic time. Budget order-of-magnitude for one shared mobo (+ optional 2 pad boards):

- **~40-60x** 100 nF decoupling
- **~10-15x** 1-10 uF island caps + **1x** bulk at barrel
- **R-2R** networks (video + audio) + **75 ohm** build-outs
- **2x** port PPTC (TRS) + arcade-header series R / TVS as needed + entry PPTC/ferrite
- **TVS** packs at cart + both TRS footprints (+ audio/video if exposed)
- **2x** Switchcraft 35RAPC footprints on mobo (+ **1x** per optional pad board when built)
- Arcade controller headers / IDC
- Oscillators / crystals, reset RC, pull-ups, SCALE DIP, barrel, AV connectors

---

## Measuring RF (comparative)

For pre-compliance gut checks without a lab: a DIY **TEM cell** (copper foil + cardboard works, aluminum foil is a possible substitute) is enough for **comparative** measurements. Change a pour, ferrite, or clock damper and see if the reading moves. Conceptually it is a coax expanded so the DUT sits between the center conductor and the shield. Open-sided builds leak external noise. Treat results as relative, not absolute CE numbers.

Formal **CE / FCC** work comes later (Crowd Supply-class EU sales need marking). Expect ferrite / clamp iteration on the first real spin if emissions matter for distribution.

Further reading that clarifies stackup and return paths: Rick Hartley lectures on PCB layer arrangement (search by name, long-form video).

---

## Open topics

| Topic | Note |
|-------|------|
| PPTC Ihold per pad port | Bench ATtiny85 + LED budget, then pick family (e.g. Bourns MF-MSMF / Littelfuse 1206L) |
| First-spin RF | TEM comparative + ferrite/clamp tweaks. Formal CE only if selling into marked markets |
| Prop-delay budget | Sim: `./sim cart.retr01 DELAY=typical|max` (or env `R01S_PROP_DELAY`). Capture HC / PLD / SRAM stacks before PCB freeze |
