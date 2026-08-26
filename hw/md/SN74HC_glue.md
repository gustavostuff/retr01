# SN74HC glue -- DIP-14 gates (v0)

**PDFs:**  
[`../SN74HC00_nand.pdf`](../SN74HC00_nand.pdf),  
[`../SN74HC04_inverter.pdf`](../SN74HC04_inverter.pdf),  
[`../SN74HC08_and.pdf`](../SN74HC08_and.pdf),  
[`../SN74HC14_schmitt.pdf`](../SN74HC14_schmitt.pdf),  
[`../SN74HC32_or.pdf`](../SN74HC32_or.pdf),  
[`../SN74HC86_xor.pdf`](../SN74HC86_xor.pdf).

**Package:** 14-pin PDIP (N). **VCC** pin 14, **GND** pin 7 on all of these.  
**Qty (v0):** **10** total -- HC14 x1, HC00 x2, HC04 x2, HC08 x2, HC32 x2, HC86 x1.  
**Qty (v1):** **0** -- equations move into ATF22V10.

Family: 74HC, 2-6 V, retr01 at **5 V**. Propagation delays typically teens of ns.

## retr01 role

Small combinatorial glue: reset conditioning, enable combining, polarity fixups, XOR for compare helpers, Schmitt clocks/reset (HC14). Exact nets are schematic-level; sim instantiates gates from the netlist.

---

## Shared DIP-14 pin pattern (quad 2-input)

Used by **HC00, HC08, HC32, HC86**:

```text
         +-----\/-----+
     1A  | 1       14 | VCC
     1B  | 2       13 | 4B
     1Y  | 3       12 | 4A
     2A  | 4       11 | 4Y
     2B  | 5       10 | 3B
     2Y  | 6        9 | 3A
    GND  | 7        8 | 3Y
         +------------+
```

### Truth (per gate)

| Part | Y = |
|------|-----|
| **HC00** NAND | `!(A & B)` |
| **HC08** AND | `A & B` |
| **HC32** OR | `A \| B` |
| **HC86** XOR | `A ^ B` |

---

## Hex inverter / Schmitt (different pinout)

**HC04** (inverter) and **HC14** (Schmitt inverter): six channels, Y = !A (HC14 has hysteresis).

```text
         +-----\/-----+
     1A  | 1       14 | VCC
     1Y  | 2       13 | 6A
     2A  | 3       12 | 6Y
     2Y  | 4       11 | 5A
     3A  | 5       10 | 5Y
     3Y  | 6        9 | 4A
    GND  | 7        8 | 4Y
         +------------+
```

| Part | Note |
|------|------|
| **HC04** | Standard CMOS invert |
| **HC14** | Schmitt trigger -- clean up slow edges / reset / crystal buffer duties |

---

## Expected I/O (unit)

Drive A (and B); Y matches Boolean above within `tpd`. Unused inputs must be tied (not floating) in hardware and in sim.

## Communication

Scattered between CPU decode, reset, beam wrap, IRQ polarity. v1 deletes these packages once PLD equations cover the same nets.

## Sim notes

One tiny module type per gate family (NAND2, AND2, OR2, XOR2, INV, SCHMITT_INV) with instance count from the netlist. No need for full TI PDF switching tables until timing closure.

## Package dimensions

| | |
|--|--|
| Outline | PDIP-14, 300 mil (HC00/04/08/14/32/86) |
| Body (nom.) | **19 x 6 mm** (length x width; ~19.3 x 6.35) |
| Sim @ 4 px/mm | **76 x 24 px** horizontal (default) |
| Reference | [`packages_dip.md`](packages_dip.md) |
