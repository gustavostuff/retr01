# Main PCB placement

Floor-plan checklist for the motherboard under `apps/sim/tier-h/kicad/main-pcb/`.
Board outline is **170 x 170 mm** Mini-ITX. Skidl netlist is `apps/sim/tier-h/skidl/retr01_prelim.net`.

Design sources: [`docs/general/hardware.md`](../../general/hardware.md), floor plan [`docs/bringup/pcb-component-placement-guide.md`](../../bringup/pcb-component-placement-guide.md), passive values [`docs/passive_bom.md`](../../passive_bom.md), DIP pin numbers in `apps/sim/tier-h/skidl/retr01_kicad/pinmap.py`.

Pin-level R/C wiring: [`docs/misc/kicad-schematic-tier-h-passives.md`](../../misc/kicad-schematic-tier-h-passives.md).

---

## Docs in this folder

| Doc | What it covers |
|-----|----------------|
| [main-pcb-layers.md](../main-pcb-layers.md) | Layer 1 vs layer 4 assignment, analog keepout, underpasses |
| [connectors.md](connectors.md) | J1, J2, J3/J4, J5, J7, J8, J9, J36 |
| [ics.md](ics.md) | CPU, RAM, PLDs, MCUs, clock logic, encoder, glue |
| [resistors.md](resistors.md) | R1-R24 (DAC, series 33 ohm, pull-ups, Pierce 1M) |
| [capacitors.md](capacitors.md) | E1 bulk, C1-C20 bypass, C21-C26 crystal loads |
| [crystals.md](crystals.md) | Y1, Y2, Y3 and the clock-island cluster |

---

## Quick map

```
TOP (rear I/O)     J1 power | J8 audio | J9 composite | J2 RGBS
----------------------------------------------------------------
 Zone1 SYSTEM RAM  |  CPU U1 (above cart)  |  Zone2 VIDEO / DAC
                   |                       |
-------------------+------ J36 CART -------+--------------------
 Zone3 VRAM+MUX    |  MCU-M (UM)           |  Zone4 S1 + field
 Zone5 CTRL+AUDIO  |  (open / mounting)    |
----------------------------------------------------------------
BOTTOM (front)     J3/J4 pads | J5 arcade | J7 cab
```

| Zone | Name | Parts |
|------|------|-------|
| **Z1** | Top-left | U3, U130, E1, C2, C17, **R22** |
| **CPU** | Top-center | U1, C1, **R21**, directly above J36 |
| **Z2** | Top-right | Clock island (Y1-Y3, R12/R13/R23/R24, C19/C20/C21-C26), PLDs, U24, DAC R1-R11, J2, U725, J9 |
| **CART** | Center | J36, cart series **R14-R17** |
| **Z3** | Middle-left | U6, U7A/B/C, U574 and their Cs |
| **HUB** | Middle-center | UM, C5, I2C series **R16/R17**, pull-ups **R19/R20** |
| **Z4** | Middle-right | US1, U573, U41 and Cs |
| **Z5** | Bottom-left | US2, J3, J4, J5, J7, C7, pad pull-up **R18** |

---

## Main PCB passive roll-up

54 through-hole passives on this board. Cart flash, cart EEPROM, and the pad ATtiny85 each carry their own 100 nF. Cabinet **47 ohm** series parts sit on the harness. Canvas rails use the **5V** and **GND** symbols.

| Class | Refs | Qty | Doc |
|-------|------|----:|-----|
| Crystals | Y1, Y2, Y3 | 3 | [crystals.md](crystals.md) |
| Bulk | E1 | 1 | [capacitors.md](capacitors.md) |
| Bypass 100 nF | C1-C20 | 20 | [capacitors.md](capacitors.md) |
| Crystal loads | C21-C26 | 6 | [capacitors.md](capacitors.md), [crystals.md](crystals.md) |
| DAC | R1-R11 | 11 | [resistors.md](resistors.md) |
| Series 33 ohm | R12-R17 | 6 | [resistors.md](resistors.md) |
| Pull-ups | R18-R22 | 5 | [resistors.md](resistors.md) |
| Pierce feedback 1M | R23, R24 | 2 | [resistors.md](resistors.md), [crystals.md](crystals.md) |

---

## Placement sequence

1. Board outline and mounting holes
2. J36 at center, then U1 above it
3. J1 / E1 at top-left. J2 / J8 / J9 on the top edge. J3-J5 / J7 at bottom-left
4. Zone 2 clock island, then PLDs, U24, DAC, encoder
5. Zone 1 U3 and reset
6. Zone 3 U6, three HC157, U574
7. UM hub
8. Zone 4 US1, U573, U41
9. Zone 5 US2
10. All passives: 100 nF bypass C1-C20, crystal loads C21-C26, E1, DAC R1-R11, series R12-R17, pull-ups R18-R22, feedback R23-R24

Ratsnest refresh from `retr01_prelim.net` follows. Short nets route first: bypass, clocks, color index, field AD, cart drop.

---

## Related

- [`docs/passive_bom.md`](../../passive_bom.md): values and counts
- [`docs/misc/kicad-schematic-tier-h-passives.md`](../../misc/kicad-schematic-tier-h-passives.md): pin-level R/C wiring
- [`docs/bring-up-v2/main-pcb-layers.md`](../main-pcb-layers.md): which nets sit on layer 1 vs layer 4
- [`docs/bringup/pcb-component-placement-guide.md`](../../bringup/pcb-component-placement-guide.md): zone rationale
- [`docs/bringup/pcb-netlist-verification-guide.md`](../../bringup/pcb-netlist-verification-guide.md): PCB vs JSON check
- [`docs/bringup/tier-h-skidl-export.md`](../../bringup/tier-h-skidl-export.md): `.net` export
- [`temp/motherboard-placement-notes.md`](../../../temp/motherboard-placement-notes.md): routing and parasitics notes
