# Retr01 / Multi-Chip CPU Bus — Clean Shared Bus + SKiDL Outline

This note covers two related topics for an MCU-assisted 8-bit design (W65C02S + RAMs + cart + helper MCUs + PLDs):

1. How to keep a shared CPU data/address bus readable and routable (schematic + PCB).
2. How to describe the same bus cleanly in **SKiDL** (Python) instead of drawing every parallel wire in a graphical schematic.

---

## 1. The problem

Several chips can drive or sense the same CPU bus (`D[7:0]`, address lines, control strobes). In a conventional KiCad schematic this produces a dense tangle of parallel wires (or a forest of net labels). On the PCB the same connections become a rat’s-nest of airwires that are hard to route as orderly copper.

Electrically the design already has the classic discipline:

- Only one driver active at a time (PLD `/OE` + cart `OE#` + MCU-M data pins held in hi-Z when not serving a soft cycle).
- Idle-safe pull-ups / pull-downs on critical enables.
- Short, well-terminated high-speed edges (`PHI2`, `DOT`, etc.).

The remaining pain is presentation and layout, not the electrical rules.

---

## 2. Clean shared-bus practices (schematic + PCB)

### Schematic

- Treat the shared signals as **buses**, not individual wires.
  - Vector bus: `CPU_D[7:0]`, `CPU_A[15:0]`.
  - Or a group bus / alias for mixed control signals.
- Place the CPU (and main decode PLD) on a “bus spine” sheet.
- Give each peripheral its own hierarchical sheet; connect via hierarchical bus pins or net labels.
- Prefer net labels + bus entries over continuous wire lines between every chip.
- Keep pure power and pure enable nets out of the data/address buses so the main bus stays focused.

### PCB (4-layer, 170 x 120 mm)

1. **Placement first**  
   Cluster every chip that touches the bus so their data/address pins face a common corridor. CPU near the centre or one end; RAMs, cart edge and MCU-M nearby.

2. **Route as parallel corridors**  
   Keep the eight data lines (and the active address lines) together as a group of parallel traces for as long as possible. Use the two signal layers; prefer top-layer runs with bottom as secondary. Vias only when necessary.

3. **Ground & power**  
   Solid inner GND planes + stitching vias under the bus. 100 nF at every VCC pin, bulk at the entry. Series damping (~33 Ω) on selected control lines as already planned.

4. **Workflow tip**  
   Hide non-bus nets after placement, route the bus groups first, then finish the short local stubs. The airwire count drops dramatically once the corridor exists.

These practices turn the “mess of parallel airwires” into a few thick schematic buses and a clean parallel copper corridor.

### Retr01 motherboard (current)

The electrical share is locked: one `CPU_D[7:0]` and one `CPU_A[15:0]`, hi-Z when idle. An extra 74HC245 on D would break the 19-IC motherboard count, so the “dedicated bus” is a Pcbnew presentation, not new silicon.

| Split | What it is |
|-------|------------|
| **CPU_A** net class (magenta) | `CPU_A0`-`CPU_A15`. Corridor from U1 left/right A pins to U3 A, 157 I0, J36 odd/side A. |
| **CPU_D** net class (green) | `CPU_D0`-`CPU_D7`. Corridor from U1 D pins to U3 DQ, U6 DQ, U574 D, UM port pins, J36 even/side B. |
| **Quiet** net class (orange) | MAP `CART_A14`-`CART_A18` and `CPU_A_SAMPLE` stay off the CPU bus classes. |

SKiDL already names those nets from U1 (`net_names.py`). KiCad classes and `netclass_patterns` live in `apps/sim/tier-h/skidl/retr01_kicad/bus_nets.py` and are written into `main_pcb.kicad_pro` on export. Wrapping the existing nets in a SKiDL `Bus()` object is optional and does not change copper.

J36 is already interleaved: odd pads address, even pads data. Place U1 above J36 so A and D leave the CPU toward those two rows, then keep each group parallel on layer 1.

---

## 3. Why SKiDL helps

SKiDL lets you describe the circuit in Python. A single `Bus` object is electrically identical to eight individual nets; every chip simply connects to that same object. Hierarchy is ordinary Python functions, version control is plain text, and the resulting netlist imports directly into KiCad’s PCB editor.

You keep full interactive control of placement and copper while the connectivity description stays compact and reviewable.

---

## 4. SKiDL code outline

```python
#!/usr/bin/env python3
"""
Minimal SKiDL skeleton for a shared CPU bus
(Retr01-style: W65C02S + SRAM + cart + MCU-M data pins).
Expand with the real part libraries, footprints and remaining chips.
"""

from skidl import *

# ----------------------------------------------------------------------
# Global nets & buses (create once, share everywhere)
# ----------------------------------------------------------------------
vcc = Net('VCC')
gnd = Net('GND')

cpu_d = Bus('CPU_D', 8)          # D[7:0]
cpu_a = Bus('CPU_A', 16)         # A[15:0] (adjust width as needed)

phi2  = Net('PHI2')
rdy   = Net('CPU_RDY')
resb  = Net('RESB')
# add other control nets: RWB, IRQB, NMIB, etc.

# ----------------------------------------------------------------------
# Helper: attach power + bypass (call for every IC)
# ----------------------------------------------------------------------
def power_and_bypass(part, vcc_net=vcc, gnd_net=gnd, value='100n'):
    """Connect VCC/GND and add a local bypass capacitor."""
    # Adjust pin names to match the real symbol
    part['VCC'] += vcc_net
    part['GND'] += gnd_net
    bypass = Part('Device', 'C', value=value, footprint='...')
    bypass[1] += vcc_net
    bypass[2] += gnd_net

# ----------------------------------------------------------------------
# CPU
# ----------------------------------------------------------------------
cpu = Part('MCU_Module', 'W65C02S', footprint='Package_DIP:DIP-40_W15.24mm')
# (use the real KiCad library name / symbol / footprint)

cpu['D[0:7]']  += cpu_d
cpu['A[0:15]'] += cpu_a
cpu['PHI2']    += phi2
cpu['RDY']     += rdy
cpu['RESB']    += resb
# ... remaining pins

power_and_bypass(cpu)

# ----------------------------------------------------------------------
# System RAM example
# ----------------------------------------------------------------------
sram = Part('Memory_RAM', 'AS6C62256', footprint='Package_DIP:DIP-28_W15.24mm')

sram['DQ[0:7]'] += cpu_d
sram['A[0:14]'] += cpu_a[0:14]          # slice as required
# OE#, CE#, WE# driven by PLD decode nets (not shown)

power_and_bypass(sram)

# ----------------------------------------------------------------------
# Cart flash example (data + lower address)
# ----------------------------------------------------------------------
flash = Part('Memory_Flash', 'SST39SF040-55-4C-TU', footprint='Package_SO:TSOP-I-32_8x14mm_P0.5mm')

flash['DQ[0:7]'] += cpu_d
flash['A[0:13]'] += cpu_a[0:13]
# higher address bits, OE#, WE#, CE# from PLD / MAP logic

power_and_bypass(flash)

# ----------------------------------------------------------------------
# MCU-M data pins (hi-Z when not serving soft $7Fxx)
# ----------------------------------------------------------------------
mcu_m = Part('MCU_Module', 'AVR128DB28', footprint='Package_DIP:DIP-28_W7.62mm')

# Example: map a subset of PORTA/PORTD to the CPU data bus
mcu_m['PA[0:7]'] += cpu_d          # or whichever pins are used
# RDY open-drain, SEL_SOFT*, etc.

power_and_bypass(mcu_m)

# ----------------------------------------------------------------------
# Optional: hierarchical sub-circuit example
# ----------------------------------------------------------------------
@subcircuit
def peripheral_block(data_bus, addr_bus, controls):
    """Example of a reusable block that attaches to the shared bus."""
    # instantiate chips, connect to the passed-in buses
    pass

# ----------------------------------------------------------------------
# Generate netlist for KiCad
# ----------------------------------------------------------------------
generate_netlist(file_='retr01_bus.net')   # or the KiCad-specific export
# generate_xml() / generate_svg() etc. also available

ERC()   # run SKiDL’s electrical rule check
```

### Key SKiDL points illustrated

| Construct              | Purpose                                      |
|------------------------|----------------------------------------------|
| `Bus('CPU_D', 8)`      | Creates eight related nets as one object     |
| `cpu['D[0:7]'] += cpu_d` | Connects a whole bus in one statement      |
| `cpu_a[0:14]`          | Slice / index individual members             |
| `@subcircuit`          | Hierarchical modules that accept buses       |
| `power_and_bypass()`   | Reusable helper for decoupling               |
| `generate_netlist()`   | Produces a file KiCad Pcbnew can import      |

---

## 5. Workflow with KiCad

1. Write / maintain the SKiDL description (can be split across several `.py` files).
2. Run the script → netlist (and optional ERC).
3. In Pcbnew: **File → Import → Netlist**. Footprints appear.
4. Place the chips so data pins face a common corridor, then route the bus as parallel traces.
5. When the Python changes, re-generate the netlist and re-import. Unchanged footprints keep their placement and routing (KiCad’s netlist merge options or external helpers such as skidl-updater help).

---

## 6. Extra tips for this style of design

- Keep the three-way hi-Z rule explicit in comments or even in small assertion helpers.
- Attach net classes or series resistors to individual bus members or the whole bus.
- Generate the same netlist for documentation, BOM scripts, or future simulation.
- Version-control the Python sources; diffs are readable.
- Start with a minimal set of chips (CPU + one RAM + MCU-M data pins) and grow the description.

---

## 7. References (quick)

- SKiDL documentation: https://devbisme.github.io/skidl/
- Bus class & hierarchy examples in the SKiDL docs and GitHub repository.
- Classic multi-chip 6502 / Z80 layout practice: cluster bus participants, route parallel corridors, solid ground under the bus.

This combination (clean bus discipline + SKiDL description + careful placement) keeps both the schematic description and the PCB copper manageable even when many chips share the same CPU bus.
