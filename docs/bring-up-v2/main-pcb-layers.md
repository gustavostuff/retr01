# Main PCB: layer assignment

Which nets live on which copper layer of the motherboard under `apps/sim/tier-h/kicad/main-pcb/`.
Placement zones: [main-pcb-placement/README.md](main-pcb-placement/README.md). Stackup summary: [`docs/general/hardware.md`](../general/hardware.md).

Cart and pad PCBs are 2-layer and are outside this doc.

---

## Stackup

| Layer | Copper |
|-------|--------|
| 1 | Clock-rate digital, +5V, GND fill |
| 2 | Solid GND plane (no signal traces) |
| 3 | Solid GND plane (no signal traces) |
| 4 | Analog, slow digital, I/O, GND fill |

GND is one net on all four layers. Planes and fills are not cut into a digital region and an analog region. Isolation comes from part placement, from which outer layer a net uses, and from the analog keepout on layer 4.

+5V is a routed net on layer 1 at 0.8 mm to 1.2 mm. It is not an inner plane.

Layer 2 returns layer 1. Layer 3 returns layer 4. Stitching vias tie GND on all four layers every 10 mm to 15 mm and next to each IC ground pin. KiCad zone fill drops orphan copper.

---

## Target split

The routing target is about half the signal traces on layer 1 and the other half on layer 4.

Clock-rate buses dominate pin count (CPU A/D, cart stubs, VRAM mux, field AD, color index, scroll). Those stay on layer 1, so layer 1 may still carry more copper than layer 4. That is acceptable.

Every net in the layer 4 tables below belongs on layer 4. Leaving a safe net on layer 1 is the exception, used only when a layer 4 path would cut the analog keepout or would be much longer.

---

## How a net picks a layer

| Test | Layer |
|------|-------|
| +5V | 1 |
| Digital square clock (PHI2, DOT, U04 buffer, U74, 157 pin S) | 1 |
| Parallel bus that changes on PHI2 or DOT (A, D, AD, mux, index, field, scroll, SPI clock/data) | 1 |
| Analog (crystal Pierce, DAC guns, AD724, composite, audio after PWM) | 4 |
| Reset, ready, selects, handshake, I2C, pads, arcade, LEDs, line/frame strobes, MAP high bits | 4 |

A via may pass through layers 2 and 3. Signal copper does not stop on layers 2 or 3.

---

## Analog keepout (layer 4)

Layer 4 analog copper sits in Zone 2:

- Y1 / Y2 Pierce loops at U04 (crystal pins, R23, R24, C21, C22, C25, C26)
- Y3 and FSC into U725 FIN (C23, C24)
- DAC gun nodes after R1-R8, plus R9-R11, into J2 pins 1-3
- U725 RIN / GIN / BIN / COMP and J9
- HSYNC / VSYNC / CSYNC into J2 pins 4-6 (short, in this corner)

Digital copper on layer 4 stays out of that island. No parallel run beside those nets. A short crossing at 90 degrees is the fallback when a crossing cannot move.

AUDIO_PWM uses layer 4 in the Zone 5 to J8 corridor. That run stays off the Zone 2 island.

---

## Layer 1 (clock-rate digital and power)

These nets stay on layer 1 for their full run, except a short underpass (see [Underpasses](#underpasses)).

### Power

| Net | Notes |
|-----|-------|
| `+5V` | 0.8 mm to 1.2 mm from J1 / E1. Same width to J7, J36, J3/J4 tips. Not on layer 4. |

IC VCC / VDD / VDDIO2 / AVDD / APOS / DPOS pins tie to this net with short layer 1 stubs and local 100 nF (C1-C20).

### Clocks (digital squares)

| Net | Path |
|-----|------|
| U04 DOT buffer (pins 3-4) | Into U74 CLK |
| U74 Q /CLK chain | 21 MHz divide down to DOT |
| **DOT** | U74 pin 9 through **R13** to UPLDX CLK |
| U04 PHI2 buffer (pins 9-8) | Into **R12** |
| **PHI2** | R12 to U1 pin 37, and to 157 pin S (phase select) |

Pierce nodes on U04 pins 1-2 and 5-6 are analog. Those stay on layer 4.

Clocks stay short, toward the middle of the board, off the perimeter.

### CPU, RAM, cart (PHI2)

| Net | Path |
|-----|------|
| U1 A0-A15 | U3, mux I0, J36 A0-A13 |
| U1 D0-D7 | U3 DQ, U574 D, UM CPU_D, J36 D |
| U1 RWB | Decode / PLDs |
| U3 CE# / OE# / WE# | System RAM decode |
| J36 `CART_OE#` | Through **R14** |
| J36 `CART_WE#` | Through **R15** |

### VRAM and mux (PHI2 and beam)

| Net | Path |
|-----|------|
| U7A/B/C I0 | CPU A0-A11 |
| U7A/B/C I1 | Beam address |
| U7A/B/C Y | U6 A0-A11 |
| U7 S (pin 1) | PHI2 |
| U6 DQ | CPU D (PHI2-high window) |
| U6 CE# / OE# / WE# | `SEL_VRAM` and write decode |
| U7 G# | Compositor enable (must not float) |

### Video digital (DOT)

| Net | Path |
|-----|------|
| Beam X / Y counter bits | UPLDX, UPLDY, compositor, mux I1 |
| Color index A0-A5 | UPLDV to U24 A0-A5 (under 25 mm) |
| U24 O0-O7 | Into **R1-R8** (digital until the resistor pad) |
| U574 Q0-Q7 | Scroll X into beam / compositor |

### S1 field cluster (Zone 4)

| Net | Path |
|-----|------|
| US1 AD0-AD7 | U573 D and U41 DQ |
| US1 A8-A14 | U41 A8-A14 |
| U573 Q0-Q7 | U41 A0-A7 |
| ALE | U573 LE |
| `/WE` | U41 WE# |
| U41 CE# / OE# | PLD field window |
| SPI MOSI / MISO / SCK | UM to US1 and US2 |

The S1 triangle stays local. Layer 1 keeps the AD burst on this island.

---

## Layer 4 (analog and every safe digital net)

### Analog (must)

| Net | Path |
|-----|------|
| Y2 Pierce | U04 pins 1-2, **R24**, **C21**, **C22** |
| Y1 Pierce | U04 pins 5-6, **R23**, **C25**, **C26** |
| Y3 / FSC | U725 FIN, **C23**, **C24** |
| Red / Green / Blue guns | After **R1-R8**, through **R9-R11**, to J2 pins 1-3 and U725 RIN/GIN/BIN |
| U725 COMP | J9 center |
| AUDIO_PWM | US2 PF1 to J8, Zone 5 corridor only |

U24 O0-O7 arrive on layer 1. The analog net begins at the R1-R8 pad on the gun side.

### Reset, ready, supervisor

| Net | Path |
|-----|------|
| `RESB` / `RESET_N` | U130 RESET#, U1 pin 40, **R22**, J7 pin 3 |
| `CPU_RDY` | UM OD, U1 RDY, **R21** |

### Handshake, decode, MAP high bits

| Net | Path |
|-----|------|
| `VBL` | Beam Y to UM PD3 (and US1 VBL if wired) |
| `S1_RDY` | US1 PF6 to UM PF6 |
| IRQB / NMI / HBLANK / VBLANK | Beam PLDs to CPU / MCU |
| HSYNC / VSYNC / CSYNC | Beam to J2 pins 5 / 6 / 4 and U725, inside the analog corner, short |
| `SEL_SOFT0` / `1` / `2` | Compositor to UM |
| `CPU_A_SAMPLE` | To UM PD4 |
| `/SS_S1` / `/SS_S2` | UM to US1 / US2 (selects only, not MOSI/MISO/SCK) |
| `LE_7F02` / `$7F03` / `$7F04` | Compositor load strobes |
| CART A14-A18 | UPLDV MAP to J36 B13-B17 |

MAP A14-A18 and the soft SELs route around the analog keepout, not through the DAC or Pierce loop.

### I2C, program, pads, arcade, LEDs

| Net | Path |
|-----|------|
| SDA / SCL | UM PA2-3, **R16** / **R17**, **R19** / **R20**, J36 A3/B3 |
| Cart program | **J10** DATA to UM PC1. **CART_ARM** is compositor MAP latch D7. AVR pin 19 stays off this header |
| `PAD_DATA` | US2 PF0, **R18**, J3/J4 ring |
| J5 P1 / P2 bits | US2 ports to arcade header |
| Heartbeat / power LEDs | Series resistors to LED anodes |

---

## Underpasses

Layer 1 congestion may drop a layer 1 net to layer 4 for a short hop, then via back up.

Rules for a hop:

- A few millimeters, not a second digital highway
- GND vias next to both ends so the return stays under the hop
- No hop through the analog keepout
- No hop of `+5V`
- No hop of PHI2, DOT, the U04 digital buffers, or the U74 chain
- Prefer hops on a single stub (one pin escape under a DIP) over hops of a whole bus

A whole bus that cannot finish on layer 1 is a placement problem first. The parts or the ribbon move before that bus occupies layer 4.

---

## Geometry

| Item | Value |
|------|-------|
| Digital signal width / space | 0.25 mm / 0.25 mm |
| +5V width | 0.8 mm to 1.2 mm |
| Analog video / Pierce | Short, local to Zone 2. 45-degree bends. Extra clearance from layer 1 clocks. |
| GND stitch | Via every 10 mm to 15 mm, and at each IC GND pin |
| Bypass | 100 nF within 5 mm of each VCC pin, short via into the GND plane |

---

## KiCad net class Layer4

Motherboard projects under `apps/sim/tier-h/kicad/main-pcb/v_01/` carry a net class named **Layer4**. Nets from the layer 4 tables above match that class through `netclass_patterns` (and through SKiDL on the next `export_netlist.sh` run). Layer 1 nets stay in **Default**.

Pcbnew colors Layer4 air wires from the class `pcb_color` (orange at `rgb(220, 143, 50)`). That color is a starting point. Board Setup, Net Classes, Layer4 changes it. Track and via sizes on Layer4 match Default.

Pattern source: `apps/sim/tier-h/skidl/retr01_kicad/layer4_nets.py`.

---

## Related

- [`docs/general/hardware.md`](../general/hardware.md): stackup summary and connector pinout
- [main-pcb-placement/README.md](main-pcb-placement/README.md): zones and part list
- [main-pcb-placement/ics.md](main-pcb-placement/ics.md): IC pin destinations
- [main-pcb-placement/resistors.md](main-pcb-placement/resistors.md): R1-R24
- [main-pcb-placement/crystals.md](main-pcb-placement/crystals.md): Pierce and divider
- [`docs/bringup/pcb-component-placement-guide.md`](../bringup/pcb-component-placement-guide.md): zone rationale
- [`docs/bringup/staged-pcb-bringup-guide.md`](../bringup/staged-pcb-bringup-guide.md): fab widths
