# Main PCB: capacitors

Bulk **E1**, bypass **C1-C20**, and crystal loads **C22-C27**. There is no **C21**.

Values match [`docs/passive_bom.md`](../../passive_bom.md) and `retr01_kicad/tier_h_map.py`. Zones: [README.md](README.md). Crystal loops: [crystals.md](crystals.md).

Cart flash/EEPROM and pad ATtiny85 each have one 100 nF on **those** boards, not here.

---

## Bulk E1

| Ref | Value | Zone | + | - |
|-----|-------|------|---|---|
| E1 | 220 uF | Z1 at J1 | `+5V` | `GND` |

**E1** sits next to [J1](connectors.md).

---

## Bypass C1-C20 (100 nF)

One per IC, within 5 mm of VCC. Pin 1 to the IC power pin, pin 2 to `GND`.

| Cap | IC | Power pin | Zone |
|-----|-----|-----------|------|
| C1 | U1 | 8 | CPU |
| C2 | U3 | 28 | Z1 |
| C3 | U6 | 28 | Z3 |
| C4 | U41 | 28 | Z4 |
| C5 | UM | 20 | Hub |
| C6 | US1 | 20 | Z4 |
| C7 | US2 | 20 | Z5 |
| C8 | UPLDX | 24 | Z2 |
| C9 | UPLDY | 24 | Z2 |
| C10 | UPLDV | 24 | Z2 |
| C11 | U7A | 16 | Z3 |
| C12 | U7B | 16 | Z3 |
| C13 | U7C | 16 | Z3 |
| C14 | U573 | 20 | Z4 |
| C15 | U574 | 20 | Z3 |
| C16 | U24 | 28 | Z2 |
| C17 | U130 | 2 | Z1 |
| C18 | U725 | 4 and 14 | Z2 |
| C19 | U04 | 14 | Z2 clock island |
| C20 | U74 | 14 | Z2 clock island |

IC pin tables: [ics.md](ics.md).

---

## Crystal loads C22-C27 (~18-20 pF, SKiDL value 22 pF)

Sit at the crystal, not across the board. Pin 1 to the crystal pin, pin 2 to `GND`. Island layout: [crystals.md](crystals.md).

| Cap | Crystal pin | Zone |
|-----|-------------|------|
| C22 | Y2 pin 1 | Z2 |
| C23 | Y2 pin 2 | Z2 |
| C24 | Y3 pin 1 | Z2 |
| C25 | Y3 pin 2 | Z2 |
| C26 | Y1 pin 1 | Z2 |
| C27 | Y1 pin 2 | Z2 |
