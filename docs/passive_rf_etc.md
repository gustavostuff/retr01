# Retr01 Passives, Ports, and RF

Non-IC parts and board-level RF / EMC practice. ICs, bus architecture, and island bring-up stay in [`hardware.md`](hardware.md) (that doc is chip-focused on purpose).

**Related:** [`hardware.md`](hardware.md) (23-IC BOM, islands). [`sound.md`](sound.md) (APU DAC). [`cart.md`](cart.md) (36-pin edge). Color PROM: [`hw/md/AT27C256R.md`](../hw/md/AT27C256R.md). Pads: [`controllers.md`](controllers.md).

Power assumption: **stable external 5 V** (barrel / PSU). No on-board switching regulator in the baseline. Passives are **outside** the 23-IC goal.

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
| Barrel jack (**CUI PJ-063AH**, 2.1 mm ID) | 5 V in (shared motherboard) |
| Series PPTC on VIN | Board-level short / overload. Hold above full-board idle, trip on hard short |
| Reverse-polarity diode (or P-FET ideal diode) | Blocks reverse barrel plug |
| Bulk cap at entry (**220 uF** low-ESR electrolytic or polymer, locked) | Holds rail through plug bounce and load steps |
| Input ferrite (or CMC on 5 V / GND pair) | Damps cable-borne RF before the pours |
| Local **100 nF X7R** on every IC VCC pin (mm from pin) | HF bypass. Mandatory for HC / PLD / AVR edge rates |
| Local **4.7 uF** ceramic per island / large DIP (default in 1-10 uF class) | Mid-band reservoir (6502, 1284, 328P, PLD cluster, SRAM bank) |
| Ferrite + **10 uF** into **analog / video** spur | Isolates Color PROM DAC and APU DAC from digital di/dt |

Never snake return current through video or pad-port copper. Stitch GND vias at every connector shell and under each DIP.

**Package policy (arcade / console):** all mobo passives are **THT** (axial R, disc ceramic C, radial electrolytic where noted, axial TVS/inductor). The only SMD silicon is **AD725ARZ** on **Proto Advantage PA0006** (DIP-16 holes on the PCB). Retr01-H may revisit SMD later.

**KiCad footprints** (from `bom.py`): standing axials for density. R `Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical`, ESD TVS stand-in `Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp`, reverse Schottky `Diode_THT:D_DO-41_SOD81_P2.54mm_Vertical_CathodeUp`, YTRAP L `Inductor_THT:L_Axial_L11.0mm_D4.5mm_P5.08mm_Vertical_Fastron_MECC`. PPTC / ferrite beads reuse the resistor axial hole pattern until a THT MPN is locked.

### Footprint audit (KiCad vs locked silicon)

| Class | Verdict |
|-------|---------|
| **ATF22V10**, **ATmega328P** | `DIP-*_W7.62mm` (300 mil). Was wrongly 600 mil until fixed |
| **AS6C62256**, **AT27C256R**, **SST39SF040**, **W65C02S**, **ATmega1284P** | `DIP-*_W15.24mm` (600 mil). Correct |
| **74HC*** | `DIP-*_W7.62mm`. Correct |
| **Y1-Y3** ACH half-size | `Oscillator_DIP-8`. Correct |
| **Y4/Y5** ABLS7M | `Crystal_HC49-U_Vertical`. Correct |
| **U725** AD725 | DIP-16 for **PA0006** adapter. Intentional |
| **R / disc C / YTRAP L** | Vertical THT class footprints. OK for 1/4 W / disc / MECC-class |
| **Cbulk / Cva** | `CP_Radial_D8.0mm_P3.50mm`. Fine for many 10 uF. Confirm **220 uF** can body when locking MPN (some need D10 / P5.00) |
| **D1** Schottky | **DO-41** (not DO-35). 1N581x rectifier class |
| **Tvs*** | DO-35 THT stand-in. Docs target SOD-323 PESD for production SMD |
| **F1/F2/F3 PPTC**, **FB1/FB2** ferrite | Resistor-axial **stand-in** until THT MPN locked |
| **J36** | PinSocket stand-in for EDAC hole pattern |
| **J3/J4**, **J8/J9** | Custom `Retr01_Lib` (locked drawings) |
| **SW1** SCALE | 1x02 header stand-in for SPST / DIP switch |

Do not assume pin-count defaults (24/28 pin => 600 mil). Match each MPN datasheet package code (24P3 / 28P3 = 300 mil).

**Passive profiles** (`python generate.py`):

| Profile | Flag | What changes |
|---------|------|--------------|
| **BRINGUP** (default) | (none) | No cart/TRS **TVS**. No arcade **47 ohm** series (J5/J6 wire straight to 1284). Keeps cart **33 ohm** series, R-2R, bypass, Quilter 0R bridges. |
| **FULL** | `--full-esd` | Production ESD clamps + arcade series resistors. |

Populate TVS / arcade series on a later spin or regenerate with `--full-esd` when you want the full I/O protection BOM.

### Quilter power nets (Circuit Comprehension)

SKiDL uses per-IC `+5V_<refdes>` stubs (via `RD*` 0 ohm) so Quilter can parent each bypass cap. Quilter often auto-lists those stubs plus some signal nets as high-current / power. Fix the list before pour:

| Action | Nets |
|--------|------|
| **Pour on** (only these) | `+5V`, `+5V_ANALOG` |
| **Keep listed, pour off** | every `+5V_U*`, `+5V_BULK`, `VIN_RAW`, `VIN_PROT`, `VIN_FUSED` |
| **Delete from Power Nets** | `VIDEO_R` / `VIDEO_G` / `VIDEO_B`, all `VRAM_A*` (DAC outs and address bus, not power) |

Quick rule: pour only on bare `+5V` and `+5V_ANALOG`. Everything else in that table is a stub, input chain, or a misclassified signal.

### Quilter bypass capacitors (Circuit Comprehension)

Quilter auto-lists many caps as IC bypass. Keep only true VCC/pin bypass. Trash the rest before place/route.

| Action | Caps |
|--------|------|
| **Keep** | `CD1`-`CD21` (per-IC 100 nF on `+5V_<refdes>`). `Cd725a` / `Cd725d` (U725 APOS pin **4** / DPOS pin **14**) |
| **Delete** | `Cbulk` (entry bulk, often parented to `RDbulk`). `Cva` (analog spur reservoir). `Cxtal4a`/`Cxtal4b`/`Cxtal5a`/`Cxtal5b` (Y4/Y5 **load** caps, 22 pF. Crystal Oscillators section owns those). `Cytrap` if listed (YTRAP tank C, not pin bypass) |
| **Delete (optional)** | `Cpad1` / `Cpad2` (TRS VCC after PPTC at J3/J4. Correct near the jack, wrong for the bypass engine) |

Expected keep set: **23** rows (21x `CD*` + 2x `Cd725*`). Do not chase high confidence on deleted rows. They are not IC bypass by design.

---

## Clocks and reset

| Item | Locked part / value |
|------|---------------------|
| **Y1** PHI2 | **Abracon ACH-8.000MHZ-EK** - 8.000 MHz, 5 V HCMOS, half-size DIP-8 can (~12.7x12.7 mm). Footprint `Oscillator:Oscillator_DIP-8` (pins **1=OE, 4=GND, 5=OUT, 8=Vcc**). OE tied high |
| **Y2** dot | **Abracon ACH-5.369318MHZ-EK** - same package/footprint. Exact NTSC/3. **Primary:** Abracon factory-order. **Alternate:** any 5 V HCMOS DIP-8 XO at **5.369318 MHz +/-50 ppm**. Do **not** use 5.000/6.000 MHz for video |
| **Y3** AD725 4FSC | **Abracon ACH-14.31818MHZ-EK** - same package/footprint |
| **Y4** 1284 | **Abracon ABLS7M-20.000MHZ-D2Y-T** HC-49/U + **2x 22 pF** to GND |
| **Y5** 328P | **Abracon ABLS7M-16.000MHZ-D2Y-T** HC-49/U + **2x 22 pF** to GND |
| Series damping | **33 ohm** locked (`Rphi`, `Rdot`) on PHI2 and DOT at the can output |
| **74HC14** (outside 32-count) | Schmitt cleanup for reset / slow edges |
| RC + Schmitt (or supervisor, e.g. MCP120-class) on `/RESB` and AVR `RESET` | Power-on reset. Hold low until 5 V is solid |
| Pull-ups on open-drain resets / IRQB | Typical **4.7-10 kohm** |

Board clocks matter for layout cleanliness. They are not automatically a show-stopper for RF. Keep traces short, away from cart edge and pad jacks. No unterminated stubs.

---

## Locked connectors (buy list)

| Ref | Locked MPN | Footprint notes |
|-----|------------|-----------------|
| **J1** | **CUI PJ-063AH** (2.1 mm ID barrel) | Stock KiCad `BarrelJack_CUI_PJ-063AH_Horizontal` (1=tip, 2=sleeve, MP->GND) |
| **J2** | 1x5 pin header 2.54 mm | Stock `PinHeader_1x05_P2.54mm_Vertical` - RGBS harness |
| **J3/J4** | **Switchcraft 35RAPC2BVN4** (vertical) | `Retr01_Lib:Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical`. Switchcraft **VN4 CD** recommended layout: **5 PTH dia 2.00 mm**. Tip=**4**, Ring=**2**, Sleeve=**1**. Pads **3** and **5** are plated for mechanical hold and stay **NC** on **2BVN4** (no switch contacts). Same hole pattern also accepts **35RAPC3BVN4** / **4BVN4** mechanically (those SKUs use pads 3/5 for switches. We do not wire them). This is **not** a dual footprint for arbitrary "all pins in-line" vs "2+1 offset" third-party jacks. Locked to the VN4 family drawing only. |
| **J5/J6** | 1x10 pin header 2.54 mm | Stock vertical |
| **J7** | 1x4 pin header 2.54 mm | Stock vertical |
| **J8** | **CUI RCJ-012** (black RCA, audio) | Symbol: stock **`Conn_Coaxial`**. Footprint: `Retr01_Lib:CUI_RCJ-01x_Vertical` (pad **1**=tip, **2**=shellx2) |
| **J9** | **CUI RCJ-014** (yellow RCA, composite) | Same symbol + footprint as J8 |
| **J36** | **EDAC 395-036-520-201** (straight / vertical 2x18) | Stock stand-in `PinSocket_2x18_P2.54mm_Vertical` until EDAC CAD. Retr01-C console later: **395-036-559-212** right-angle + Horizontal stand-in (separate board, not dual-footprint) |
| **U725** | **AD725ARZ** | Chip is **wide-body SOIC-16** (7.50 mm / 300 mil, no DIP SKU from ADI). **Only intentional SMD IC on the mobo.** **Mobo footprint: DIP-16** (`Package_DIP:DIP-16_W7.62mm`). Adapter: **Proto Advantage PA0006** (SOIC-16 300 mil to DIP-16), [store link](http://www.proto-advantage.com/store/product_info.php?products_id=2200006). First-spin board stays fully THT via that adapter. |

**You build yourself:** Optionally refine J36 from EDAC drawing. TRS + RCJ footprints are in `Retr01_Lib.pretty`.

---

## Video and audio analog

| Item | Role / locked value |
|------|---------------------|
| Color PROM **binary-weighted** DAC (**1%** metal film) | R3G3B2 -> analog guns ([`AT27C256R`](../hw/md/AT27C256R.md)). Packing `(R<<5)\|(G<<2)\|B` |
| Video bit resistors (LSB->MSB) | Red/Green: **4.00 / 2.00 / 1.00 kohm**. Blue: **2.00 / 1.00 kohm** |
| **75.0 ohm** to **GND** on each R/G/B | Termination -> **~0.7 Vpp** into 75 ohm video plant |
| Optional ferrite beads on RGBS | Cable RF. Place at connector |
| Composite encoder | **AD725ARZ**: RGB AC-coupled in, **CSYNC** on HSYNC, **NTSC**, **14.31818 MHz** 4FSC. COMP -> 75 ohm -> J9. Outside 23-IC count. **AV outs: RGBS + composite** (AD725 pins 9/11 unused). Chip is wide SOIC-16. **Board: DIP-16 + Proto Advantage PA0006** (see **U725**) |
| Bench video levels (locked targets) | Guns **~0.7 Vpp** into 75 ohm. AD725 RGB inputs **0-714 mV** AC-coupled (datasheet black ~0.8 V DC after clamp). No BOM change until first-spin scope |
| APU **R-2R** from 328P `AUD0`-`AUD7` | Classic ladder **R = 10.0 kohm**, **2R = 20.0 kohm** (1%), then AC-couple to line out ([`sound.md`](sound.md)) |
| APU build-out | **1.0 kohm** series + **10 uF** AC-coupling toward jack |
| Video / AV connectors | **J2** RGBS header + **J8** RCJ-012 audio RCA + **J9** RCJ-014 composite RCA. Levels bench-tuned |
| **SCALE** select | Single SPST DIP/`SW_SCALE`: **open = 2x** (default, soft pull-down on `SCALE_1X`), **closed = 1x** (ties `SCALE_1X` to +5 V). See [`hardware.md`](hardware.md) |


---

## Cart edge and user I/O ESD

Anything a human can touch gets a clamp **at the connector**, then a series limiter, then the IC. **36-pin** cart pinout: [`cart.md`](cart.md).

| Item | Role |
|------|------|
| **TVS** (PESD5V0-class SOD-323) on cart **A0-A18**, **D0-D7**, **OE#**, **WE#**, **SDA**, **SCL** at J36 | ESD into flash / HC245 domain. In SKiDL as `TvsCa*` / `TvsCd*` / `TvsOe` / `TvsWe` / `TvsSda` / `TvsScl`. **BRINGUP** netlist omits these (`python generate.py`). Use `--full-esd` for production. |
| Series **33 ohm** on cart **D0-D7**, **OE#**, **WE#**, **SDA**, **SCL** (not on address - timing) | Clamp current + cable damp |
| Arcade **47 ohm** series on each J5/J6 bitfield line | Locked in SKiDL (`Rarc1_*` / `Rarc2_*`) |
| SCALE DIP + pull-ups/downs | **Locked:** open = **2x** default. Closed drives `SCALE_1X` high for **1x** |


---

## Controller I/O (shared motherboard)

**Silicon / PCB target:** arcade and console use the **same PCB**. Both I/O styles are on every board (arcade microswitch headers **and** footprints for two TRS jacks). Shell / BOM population chooses which path you use. Software stays `$FE60` / `$FE61` via the 1284.

**Runners today:** Emu / Sim Host Play drive `$FE60` / `$FE61` only. They do not model TRS jacks or arcade header pinouts as separate netlist islands yet.

### Arcade controllers (microswitches)

Simple **switch-to-GND** circuits: sticks and buttons close contacts. No pad MCU on this path.

| Item | Spec |
|------|------|
| **J5** | **1x10** P1 header - pinout locked in [`controllers.md`](controllers.md#j5--player-1-110) |
| **J6** | **1x10** P2 header - same order for `$FE61` |
| **J7** | **1x4** `+5V` / `GND` / `RESET_N` / `GND` cabinet tap |
| Series **R** | **47 ohm** per signal line - in SKiDL (`Rarc1_1`...`Rarc1_8`, `Rarc2_*`) |
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
