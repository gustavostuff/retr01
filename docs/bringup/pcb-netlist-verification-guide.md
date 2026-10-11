# PCB netlist verification guide (Skidl to KiCad PCB)

Verification methodology, automated comparison tooling, and audit procedures for validating the physical KiCad PCB layout directly against the simulation pin netlist without intermediate graphical schematics.

---

## 1. The Skidl schematic-less workflow

Retr01 does not rely on manual graphical schematic capture in KiCad Eeschema (`.kicad_sch`). Instead, the design uses a programmatic, code-driven schematic pipeline:

```
[C Simulation Pin Graph]
  apps/sim/tier-h/src/board_schematic.c
  apps/sim/tier-h/src/board_netlist.c
           |
           v (export_tier_h_netlist)
[JSON Pin Netlist]
  apps/sim/tier-h/skidl/retr01_tier_h.json
           |
           v (scripts/skidl_from_tier_h.py)
[KiCad Netlist (.net)]
  apps/sim/tier-h/skidl/retr01_prelim.net
           |
           v (Import Netlist / Update PCB)
[Physical KiCad PCB Layout]
  apps/sim/tier-h/kicad/main-pcb/v_02.kicad_pcb
```

Without a KiCad schematic sheet, `v_02.kicad_pcb` still has to match the simulation pin graph (`retr01_tier_h.json`) with no missing or swapped connections. The comparison below is that check.

---

## 2. Architecture of the automated netlist comparison tool

A dedicated Python verification tool compares the virtual pin netlist (`retr01_tier_h.json`) directly against the physical board file (`v_02.kicad_pcb`).

### Data structures being compared

1. **Simulation JSON (`retr01_tier_h.json`):**
   A list of named nets, where each net contains an array of pin nodes:
   ```json
   {
     "name": "NET_11",
     "nodes": [
       {"ref": "U1", "pin": "IRQB", "num": 4, "part": "W65C02S"},
       {"ref": "UPLDY", "pin": "CLK", "num": 1, "part": "ATF22V10"}
     ]
   }
   ```
2. **KiCad PCB file (`v_02.kicad_pcb`):**
   A standard S-expression text file defining footprints, pads, and assigned net IDs:
   ```lisp
   (footprint "Retr01_Lib:DIP-40_W15.24mm"
     (property "Reference" "U1" ...)
     (pad "4" thru_hole ... (net "CPU_IRQ#"))
   )
   ```

### Verification checks performed by the comparison tool

The script constructs an equivalence graph of `(component_ref, pad_number)` pairs from both files and checks five invariants:

| Check | Failure condition | What it catches |
| --- | --- | --- |
| **Node parity** | A pin is present in JSON but missing on the PCB (or vice versa) | Missing IC footprints, unplaced passives, or dropped connector pins |
| **Short circuit** | Two distinct JSON nets share pads on a single PCB net | Accidental net merges or bridged power domains |
| **Open circuit** | Pads belonging to one JSON net are split into multiple PCB nets | Incomplete bus connections or broken nets |
| **Power rail integrity** | An IC VCC/GND pin is mapped to a signal net instead of +5V/GND | Inverted power pins or missing bypass connections |
| **NC isolation** | A pin designated as unconnected in simulation is wired to an active net | Floating inputs accidentally tied to digital buses |

---

## 3. Comparison tool

`scripts/verify_pcb_netlist.py` reads `retr01_tier_h.json` and `v_02.kicad_pcb`.

KiCad 10 footprints store the refdes as `(property "Reference" "U1")` and pad nets as `(net "GND")` (name only). Older boards may still use `(fp_text reference ...)` and `(net 11 "NET_11")`. The script accepts both.

```bash
./scripts/verify_pcb_netlist.py
./scripts/verify_pcb_netlist.py apps/sim/tier-h/skidl/retr01_tier_h.json apps/sim/tier-h/kicad/main-pcb/v_02.kicad_pcb
```

---

## 4. Distinguishing logical connectivity from physical copper routing

Verifying the netlist ensures that KiCad knows which pads belong together. However, a board cannot be fabricated until physical copper traces connect those pads.

Verification involves two distinct levels:

### Level 1: Logical netlist match (Python comparison tool)
- Proves that the Skidl netlist import correctly associated every footprint pad with its intended net.
- Guarantees zero swapped pins or missing nodes in the KiCad ratsnest.

### Level 2: Physical copper continuity (KiCad DRC)
- After traces are routed in Pcbnew, **Inspect -> Design Rules Checker (DRC)** confirms that every logical net is fully routed.
- DRC output must read:
  - **Unconnected items: 0**
  - **Track clearance violations: 0**
  - **Copper zone collisions: 0**

---

## 5. Footprint pin-mapping audit (Preventing symbol vs footprint skew)

Because Skidl maps component pins by number and name, a critical risk is a mismatch between an IC symbol's logical pin definition and the physical footprint's pad numbers.

A manual datasheet audit covers every counted IC package:

| Component | Footprint | High-risk pins to verify against datasheet |
| --- | --- | --- |
| **W65C02S** | DIP-40 (600 mil) | Pin 8 (VDD), Pin 21 (VSS), Pin 40 (RESB), Pin 37 (PHI2) |
| **ATF22V10** | DIP-24 (300 mil) | Pin 24 (VCC), Pin 12 (GND), Pin 1 (CLK/IN) |
| **AT27C256R** | DIP-28 (600 mil) | Pin 28 (VCC), Pin 14 (GND), Pin 20 (CE#), Pin 22 (OE#) |
| **AS6C62256** | DIP-28 (600 mil) | Pin 28 (VCC), Pin 14 (GND), Pin 27 (WE#), Pin 22 (OE#), Pin 20 (CE#) |
| **AVR128DB28** | SPDIP-28 (300 mil) | Pin 20 (VDD), pins 15 and 21 (GND), pin 19 (UPDI) |
| **74HC157** | DIP-16 (300 mil) | Pin 16 (VCC), Pin 8 (GND), Pin 1 (S), Pin 15 (G) |
| **74HCU04** | DIP-14 (300 mil) | Pin 14 (VCC), Pin 7 (GND) |
| **74HC74** | DIP-14 (300 mil) | Pin 14 (VCC), Pin 7 (GND) |
| **AD724** | SOIC-16 (150 mil) | Pin 4 (APOS), pin 14 (DPOS), pin 2 (AGND), pin 13 (DGND), pin 3 (FIN) |
| **CUI SJ1-3535NG TRS** | `Connector_Audio:Jack_3.5mm_CUI_SJ1-3535NG_Horizontal` | Tip / ring / sleeve vs datasheet |
| **GCT DCJ200 barrel** | `Retr01_Lib:BarrelJack_GCT_DCJ200-10-A_Horizontal` | Center pin (+5.0 V) vs sleeve (GND) |

---

## 6. Pre-fabrication sign-off checklist

Gerber export for a fab house follows this order:

1. **Sim export:** `export_netlist.sh` regenerates `retr01_tier_h.json` and `retr01_prelim.net`.
2. **Netlist import:** `retr01_prelim.net` is re-imported into `v_02.kicad_pcb` so upstream netlist edits are present.
3. **Automated script comparison:** `scripts/verify_pcb_netlist.py` reports `PASS: 100% equivalence`.
4. **KiCad DRC:** Pcbnew Design Rules Checker reports **0 unrouted nets** and **0 DRC errors**.
5. **Physical 1:1 paper printout:** A 100% scale print of the layout is checked against real DIP sockets and jacks for lead spacing and drill sizes.
