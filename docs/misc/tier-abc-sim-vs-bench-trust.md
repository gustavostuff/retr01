# Tier A / B / C simulation vs breadboard trust

**Status:** Working note (2026). **SoT for product wiring:** `docs/general/hardware.md`, tier bring-up under `docs/bringup/`, per-chip notes under `docs/ic_behavior/`. **SoT for lab Auto wiring:** `apps/sim/tier-{a,b,c}/src/netlist.c` and matching `pin_net_build()` in each `ui.c`.

This note answers how far the Tier A, B, and C desktop sims can stand in for real breadboards, with emphasis on net names, Auto netlists, and what the chip models actually do.

---

## 1. What the sim is

The Tier A/B/C apps are one **VIDEO LAB** island built on `apps/netlist_sim/`:

| Layer | Models | Does not model |
| --- | --- | --- |
| **Connectivity** | Named pins, H/L/Z/X levels, breadboard strips, jumpers, resistors as series ties | Parasitic C/L, trace length, ground bounce, analog settling to ns |
| **Raster / video math** | C counters for Beam X/Y, Compositor priority in C (`atf22v10.c`, `r01a_raster.h`) | Fuse maps inside real ATF22V10s |
| **Color path** | AT27C256R kit image, R3G3B2 resistor network, AD724 lock heuristic | Real ~0.7 Vpp analog, chroma lock quality, temperature drift |
| **Tier C field** | AS6C62256 array in RAM, Compositor reads kit indices by beam address | Beam-driven `/OE` on field SRAM, AVR firmware timing on AD/ALE/`/WE` |

Auto mode applies a **reference netlist** (virtual wires). Manual mode routes only through seated pins, jumpers, and resistors on protoboards. Manual mode still uses the same reference netlist for **open/short/missing** checks against the intended lab wiring.

Bus fights default to **non-fatal** (pins go X, sim keeps running). That matches breadboard debug; it is not a guarantee the bench tolerates the same conflict.

---

## 2. Goal: breadboard tiers vs later PCB tiers

| Tiers | Intended build | Sim role |
| --- | --- | --- |
| **A, B, C** | Solderless breadboards, short wires, discrete passives | Primary planning tool for **net names**, **power/clock/sync topology**, and **bring-up order** |
| **D onward** | Mix of breadboard experiments and eventually **2-layer PCB** (Tier H reference: `apps/sim/tier-h/`, KiCad under `apps/sim/tier-h/kicad/`) | Tier H sim is much closer to full machine behavior; A/B/C stay video-slice labs |

Tiers A through C deliberately omit the 6502, cart, interleaved VRAM, SPI mailboxes, and multi-MCU traffic from `docs/general/ic-comms-risks.md`. Those risks dominate trust once PHI2, VRAM mux, and shared **D[7:0]** appear (Tier E/F and beyond).

### Why later tiers push toward a PCB

Breadboards remain usable for **isolated** bring-up (Tier C field writes scoped to VBlank, one SPI slave, and similar). The full Retr01 stack adds conditions breadboards handle poorly:

| Factor | Breadboard limit | PCB benefit |
| --- | --- | --- |
| **PHI2 @ 8 MHz + DOT @ 5.37 MHz** | Long jumper runs, stubby enables | Controlled impedance-ish short runs, solid ground |
| **VRAM 157 mux + PHI2 phase** | Setup/hold marginal on 55 ns SRAM | Short address/data paths, known G on 157 |
| **CPU D bus multi-master** | Silent fights, wrong levels | Single ground plane, explicit `/OE` trees |
| **AD[7:0] field turnaround** | S1 vs beam `/OE` overlap | Defined length, scope-friendly test points |
| **SPI / I2C @ 24 MHz MCU** | Capacitive loading, crosstalk | Dedicated routing, series term where needed |
| **Audio PWM, pad UART** | Noise on video analog | Split analog/digital areas |

Tier D (OAM SPI), E (VRAM), F (6502 soft I/O), G (cart), and H (full integration) should treat **Tier H netlist / SKiDL export** (`docs/bringup/tier-h-skidl-export.md`) as the wiring authority, not the A/B/C Auto netlist alone.

---

## 3. Reference netlist fidelity (Auto mode)

The following tables compare **Auto** links in sim to **bring-up docs**. Refdes names match the sim (`Y2` DOT osc, `Y3` FSC, `UPLDX`/`UPLDY`/`UPLDC`, `U24` PROM, `UENC` AD724).

### Tier A (video only)

| Connection | Sim Auto netlist | Bring-up / hardware | Trust |
| --- | --- | --- | --- |
| DOT -> Beam X CLK, series R | Y2 DOT -> R12 -> UPLDX CLK | Same intent | **High** for topology |
| FSC -> AD724 FIN, series R | Y3 FSC -> R13 -> UENC FIN | Same | **High** |
| Beam X HWRAP -> Beam Y CLK | Linked | Line tick from X | **High** |
| Beam X CSYNC -> AD724 HSYNC | Linked | CSYNC mode recipe | **Med** (real AD724 needs valid FSC edges and levels) |
| Beam X INDEX[5:0] -> PROM A[5:0] | Linked | Tier A Method B in bring-up | **High** for lab Method B |
| PROM A[13:6] -> GND | Linked via Y2 GND | Required | **High** |
| PROM CE#, OE# -> GND | Linked | Lab hard-enable | **High** |
| PROM O[7:0] -> R1..R8 -> AD724 R/G/B | Linked | R3G3B2 + 75 ohm loads | **Med** (sim checks encode lock, not volts) |
| Oscillator OE# -> VDD | Linked | May float on bench if osc has OE# | **Low-Med** (check canned osc datasheet) |
| AD724 VSYNC -> VDD | Linked | Inactive VSYNC in CSYNC mode | **Med** (matches AD724 doc intent) |

Tier A sim **does not** run CUPL/JEDEC. Beam equations live in C. A working bench **must** still burn PLDs; the sim only proves the **intended** counter/sync/index relationships for Method B bars.

### Tier B (+ Compositor)

| Connection | Sim Auto netlist | Bring-up §7 / Tier B doc | Trust |
| --- | --- | --- | --- |
| DOT -> Compositor CLK | Linked | 1-dot INDEX latch clock | **High** |
| Beam X HBLANK, X5-X7 -> Compositor | Linked | Count bits for BG1 pattern | **High** |
| Beam Y VBLANK, Y5-Y7 -> Compositor | Linked | BG0 bands + VBlank flag | **High** |
| Compositor INDEX[5:0] -> PROM A[5:0] | Linked | Replaces Tier A direct INDEX from Beam X | **High** |
| Compositor PHI2 -> GND, RWB -> VDD | Linked | Safe stubs | **High** for lab |
| Compositor IO20-23 | Driven 0 in `board.c` bind | MAP stand-ins | **High** (not real MAP) |

Priority demo (sprite box in Tier B sim) is **algorithmically** aligned with `docs/bringup/tier-b-video-lab.md`. It is not proof the Compositor JEDEC fits macrocell limits.

### Tier C (+ S1 path)

| Connection | Sim Auto netlist | `docs/general/hardware.md` / tier-c bring-up §8 | Trust |
| --- | --- | --- | --- |
| US1 AD0-7 -> U573 D0-7 -> U41 A0-7 | Linked | Field low address + latch | **High** for **names and topology** |
| US1 A8-A14 -> U41 A8-A14 | Linked | Matches product field address width | **High** |
| US1 ALE -> U573 LE | Linked | HC573 latch enable | **High** |
| US1 `/WE` -> U41 WE# | Linked | S1 write strobe | **High** |
| Beam Y VBLANK -> US1 VBL | Linked | VBlank edge for firmware | **High** |
| U573 OE# -> VDD (inactive) | Sim ties **OE# low** in `board_bind_rails` (outputs enabled) | Product: `/OE` low on field 573 | **Med** (polarity correct; timing not simulated) |
| U41 CE#, OE# -> GND in Auto netlist | Always enabled read | Product: PLD owns `/CE`/`/OE`, mutual exclusion with `/WE` | **Low for bench discipline** |
| US1 RESET#, UPDI, SPI, RUN, S1_RDY | **Not** in Auto netlist | RESET pull-up, UPDI header, Tier D SPI | **Manual wiring required** |
| Field **data** on DQ[7:0] | **Not** modeled on shared bus | Beam read vs S1 write on same SRAM | **Low** (see §5) |

Bring-up §8 uses `A[14:8]` wording; hardware and sim use **A8-A14** (seven lines). Same nets.

---

## 4. Logical pin names vs silicon pinout

Sim ICs use **DIP pin numbers** on the package graphic plus **functional net names** (`AD0`, `INDEX3`, `LE`). Those names match Retr01 nets in `docs/general/hardware.md`, not always the Microchip pin names on the AVR128DB28 SPDIP.

Example (MCU-S1 product freeze vs Tier C sim shell):

| Retr01 net | AVR port (hardware.md) | Sim US1 pin label |
| --- | --- | --- |
| AD0-AD7 | PA0-PA7 | Pins 2-9 `AD0`-`AD7` |
| A8-A11 | PC0-PC3 | Pins 12-15 `A8`-`A11` |
| A12-A14 | PD1-PD3 | Pins 16-18 `A12`-`A14` |
| ALE, `/WE` | PF0, PF1 | Pins 23, 24 |
| SPI, `/SS_S1` | PD4-PD7 | Pins 19-22 |
| VBL | (from Beam PLD) | Pin 26 |

For breadboard work, **trust the Retr01 net names and Auto air wires** in Tier C. UPDI programming still uses **physical pin 19** on the real chip (`docs/ic_behavior/AVR128DB28.md`).

ATF22V10 sim pin names (`X5`, `INDEX2`, `HWRAP`, and similar) match the lab split documented in bring-up and README files. They are **not** a 1:1 map to every CUPL pin assignment until JEDEC is frozen.

SN74HC573 and AS6C62256 sim packages use standard datasheet pin numbering with nets `D0-7`, `Q0-7`, `LE`, `OE#`, `A0-14`, `DQ0-7`, `WE#`, `CE#`, `OE#`. `apps/sim/tier-c/chips/sn74hc573.c` and `as6c62256.c` match common DIP pinouts used in Tier C BOM.

---

## 5. Chip behavior depth (what sim executes)

### ATF22V10 (Beam X, Beam Y, Compositor)

| Aspect | Sim | Real chip |
| --- | --- | --- |
| Logic | C code, fixed raster constants in `r01a_raster.h` | Fuse map |
| Timing | One DOT step per sim tick pair | Propagation and pin-to-pin skew |
| Trust for breadboard | **High** for "what should be connected to whom" | **Low** until JEDEC matches sim counters |

### AT27C256R + DAC + AD724

| Aspect | Sim | Real chip |
| --- | --- | --- |
| PROM | Shared kit binary with emu | OTP burn |
| DAC | Threshold on digital levels into AD724 | Resistor ratios, 0.7 Vpp |
| AD724 | `encode_ok` after FSC edge + control pins high | Analog composite lock |

Tier A pass on bench: stable sync and recognizable colors. Tier sim pass: `test_ad724`, priority tests.

### AVR128DB28 (US1) in Tier C

| Aspect | Sim | Real chip |
| --- | --- | --- |
| Firmware | **Not executed** | UPDI flash, 24 MHz HFOSC |
| AD bus | `s1_eval` holds AD hi-Z; **no** write sequence from MCU | ALE/`/WE` state machine |
| Field fill | `r01c_s1_vblank_field()` in `board.c` / `s1_lab.c` writes `field_sram.mem` every VBlank | S1 firmware must finish in VBlank window |
| VBL | `RUN` toggles from VBL sense in `s1_tick` | Optional debug only |

**Critical:** In Auto and Manual, the sim **still runs the lab blitter** into field SRAM on VBlank (see `apps/sim/tier-c/README.md`). Manual mode only gates **LCD encode** on netlist match. A breadboard can show the Tier C demo picture with **no US1 firmware** while the sim does the same. Proving **real S1 field writes** requires AVR code on hardware and a scope on AD/ALE/`/WE`, not sim parity alone.

### SN74HC573

| Aspect | Sim | Real chip |
| --- | --- | --- |
| LE high | Latch D -> internal register same eval pass | Setup/hold on D |
| OE# low | Q drives H/L onto Q nets | Drives SRAM A0-7 |

Manual mode can propagate latch outputs onto breadboard strips if wired. Sim does **not** connect Q outputs to U41 address pins through routed nets for Compositor fetch; Compositor reads **`field_sram.mem` directly**.

### AS6C62256 (field)

| Aspect | Sim | Real chip |
| --- | --- | --- |
| Storage | 32 KiB heap | Physical SRAM |
| Read/write | `eval` when CE/OE/WE levels allow | ns timing |
| Beam fetch | Not driven from PLD `/OE` in Tier C sim | PLD generates read cycles |

---

## 6. Breadboard model vs physical protoboard

| Feature | Sim | Physical |
| --- | --- | --- |
| Strip grouping | `ns_breadboard_strip_id()` (column-based lanes A-E and F-J) | 5-hole strips, power rails |
| Seating | Pin tip must hit hole center | Mechanical tolerance |
| Caps | Occupy holes, no DC continuity | Same |
| Netlist shorts | Auto reference vs seated pins + jumper union | Same rules if layout matches |
| Intra-chip false shorts | Manual mode skips same-IC Auto-net pairs on one island | N/A |

Long jumpers on BB2/BB3 in saved layouts can merge strips in the **checker** without matching visible "signal" intent. The checker follows **electrical** union, not "this jumper is only for power."

---

## 7. Practical trust summary for A/B/C breadboards

| Question | Tier A | Tier B | Tier C |
| --- | --- | --- | --- |
| Are Auto air-wires the right **net list** for the lab? | Yes | Yes | Yes, except field `/OE`/`/CE` always-on simplification |
| Will Manual mode match continuity when wired correctly? | Yes, within strip model | Yes | Yes, for S1/latch/SRAM **address** nets |
| Does sim prove PLD/JEDEC will work? | No | No | No |
| Does sim prove AVR field firmware timing? | N/A | N/A | No (lab blitter substitutes) |
| Does sim prove analog video quality? | No | No | No |
| Good for teaching **what to wire next**? | Yes | Yes | Yes |

Recommended bench order stays the bring-up docs (Tier A sync -> color -> Tier B priority -> Tier C scope on AD/ALE/`/WE` then firmware).

---

## 8. Gaps worth fixing in sim (optional engineering backlog)

These are **documentation of known mismatches**, not blockers for breadboard Tier C if hardware.md rules are followed on the bench.

1. **Field SRAM `/OE`/`/CE`:** Auto netlist holds U41 read enabled. Product uses PLD decode; `/WE` and beam `/OE` must not overlap (`docs/general/ic-comms-risks.md` §5).
2. **No DQ bus between beam and U41:** Compositor bypasses SRAM pins for video data.
3. **US1 not running firmware:** Field content is injected in `board_on_vblank`.
4. **RESET#` on US1:** Not in Auto netlist; bench needs pull-up per AVR rules.
5. **Tier A vs B INDEX source:** Correct tier must be wired (Beam X direct vs Compositor); sim tier matches its own netlist only.

---

## 9. Related tests (regression anchors)

| Test | Asserts |
| --- | --- |
| `apps/sim/tier-a/tests/test_tier_a_bars.c` | Method B bar indices |
| `apps/sim/tier-b/tests/test_tier_b_priority.c` | Compositor priority |
| `apps/sim/tier-c/tests/test_tier_c_priority.c` | Field + encode path |
| `apps/sim/tier-c/tests/test_netlist.c` | Manual opens/shorts |
| `apps/netlist_sim/tests/test_bus.c` | H/L/Z/X merge |

Tier H tests (`test_board_netlist`, island tests) cover full-machine nets; they do not replace Tier C lab checks for the video slice.

---

## 10. Document cross-check

| Source | Role |
| --- | --- |
| `docs/bringup/tier-a-video-lab.md` | Tier A bench BOM and rules |
| `docs/bringup/tier-b-video-lab.md` | Compositor lab |
| `docs/bringup/tier-c-video-lab.md` | S1 + field SRAM |
| `docs/general/hardware.md` | Product net and PORT freeze |
| `docs/general/ic-comms-risks.md` | Multi-driver and timing risks (mostly post-C) |
| `apps/sim/tier-{a,b,c}/README.md` | Sim-specific behavior (Manual blitter, layout inheritance) |

No contradiction is intended between tier-c bring-up §8 wiring sketch and sim Auto links: topology matches; sim omits PLD field `/OE` and AVR firmware.
