# PCB component placement and floor planning guide

Functional zoning, physical placement topology, and bus-flow routing guidelines for the Retr01 motherboard PCB layout.

---

## 1. Architectural rationale for functional zoning

Grouping parts by package size (all DIP-28 memories in one column, all logic ICs along one edge) looks tidy and still creates routing bottlenecks:

1. **Long bus crossings:** Unrelated memory chips in one cluster force four independent buses (CPU system, multiplexed VRAM, compositor color index, and S1 blitter) to cross repeatedly.
2. **Antenna trace loops:** Video timing PLDs far from the Color PROM and DAC stretch the 6-bit color index bus across the board under the cartridge connector. That run radiates and picks up digital noise.
3. **Crosstalk into controller and audio lines:** High-speed address buses next to gamepad lines or audio filtering degrade those analog nets.

Components sit by **electrical signal flow** in five functional zones around the central cartridge connector. Traces stay short and layer changes stay few.

---

## 2. Board form factor: 160 mm x 100 mm (Standard Eurocard 3U)

The physical motherboard outline is specified at **160 mm width by 100 mm height**, conforming to the international **Standard Eurocard 3U format** (standardized under DIN 41494 and IEC 60297).

### Rationale for the 160 mm x 100 mm standard size

1. **Off-the-shelf enclosure compatibility:** 160 mm x 100 mm is a common Eurocard size. Desktop cases from Hammond, Fischer Elektronik, and similar vendors include card guides for that outline. A custom chassis is not required.
2. **Component density and breathing room:** Accommodates the counted motherboard BOM (18 through-hole DIP/SPDIP ICs plus the direct-mount SOIC-16 surface-mount AD724 composite encoder, 36-pin cartridge edge connector, discrete jacks, and ~65 passives) at a balanced ~45% packing density. This ensures that 0.1 uF ceramic bypass capacitors sit immediately adjacent to IC power pins (< 5 mm), sockets maintain physical clearance for test clips during bring-up, and M3 corner mounting holes retain full 5 mm clearance from copper traces.
3. **Geometry for 3-column physical bus topology:** The 1.6:1 aspect ratio provides the necessary horizontal span for a balanced 3-column layout:
   - Left column (~45 mm): System RAM, VRAM, and multiplexers.
   - Center column (~70 mm): 52 mm CPU package, 65 mm cartridge slot, and central MCU-M.
   - Right column (~45 mm): Self-contained video engine, Color PROM, and DAC.
4. **Fabrication efficiency:** Standard PCB prototyping pools at commercial fabricators support 160 mm x 100 mm boards under standard tier pricing and lead times.

---

## 3. Master PCB floor plan

The physical layout arranges connectors along the edges for ergonomics, with internal functional islands organized to minimize trace lengths between communicating chips:

```
+----------------------------------------------------------------------------------+
| [REAR I/O]   Power (J1)    Audio (J8)     Composite (J9)                         |
+-----------------------+--------------------------------+-------------------------+
| SYSTEM RAM AND RESET  | W65C02S CPU (U1, DIP-40)       | VIDEO ENGINE & DAC      |
| (Top-Left)            | (Top-Center, Horizontal)       | (Top-Right)             |
|                       |                                |                         |
|   * System RAM (U3)   |   * Sits DIRECTLY above J36    |   * Y1 + Y2 crystals    |
|   * MCP130 Supervisor |   * Address and data drop      |   * U04 / U74 clocks    |
|                       |     into cart. Bus left to U3  |   * Beam X/Y, Comp      |
|                       |                                |   * Color PROM, DAC, J2 |
+-----------------------+--------------------------------+-------------------------+
|                 CARTRIDGE SLOT (J36, Center-Horizontal)                          |
|                 (Directly below the CPU, middle spine)                           |
+-------------------------+-----------------------------+--------------------------+
| VRAM & 74HC157 MUXES    | MCU-M (CENTRAL DISPATCHER)  | S1 SPRITE ENGINE         |
| (Middle-Left)           | (Middle-Center)             | (Middle-Right)           |
|                         |                             |                          |
|   * VRAM (U6)           |   * MCU-M (AVR128DB28)      |  * MCU-S1 (Blitter)      |
|   * 3x 74HC157 Muxes    |   * Soft I/O bus interface  |  * 74HC573 Latch         |
|   * Scroll Latch (574)  |   * Central SPI routing hub |  * Field SRAM (U41)      |
+-------------------------+-----------------------------+--------------------------+
| CONTROLLER & AUDIO       | [OPEN / EXPANSION AREA]                               |
| (Bottom-Left)            | (Bottom-Right)                                        |
|                          |                                                       |
|   * MCU-S2 (AVR128)      |   * Ground return, test points, mounting              |
|   * J3, J4 TRS Jacks     |                                                       |
|   * Arcade header J5     |                                                       |
+-----------------------+----------------------------------------------------------+
```

---

## 4. Why this floor plan works (Physics and ergonomics)

The physical zoning succeeds because it aligns component placement directly with electromagnetic physics, trace parasitics, and user interaction mechanics.

### Noise isolation and return path containment

In mixed-signal systems containing high-speed digital buses alongside sensitive analog outputs, noise coupling occurs primarily through shared ground return paths.

1. **Digital return current isolation:** High-frequency currents returning through the ground plane flow directly beneath their respective signal traces. In this layout, CPU bus switching stays on the left and top-center, and S1 blitter traffic stays in the middle-right (Zone 4). Those return paths stay off the analog video DAC (Zone 2) and the audio jack run (Zone 5 up to J8).
2. **Analog video quiet zone:** By placing the video engine, Color PROM, and R-2R ladder in the upper-right corner directly next to the RGBS header (J2), the entire analog video signal path spans less than 20 mm of total copper. No high-speed digital buses cut through or run parallel to these analog nodes, preventing visual sparkling, pixel noise, and 60 Hz hum on CRT monitors.
3. **Audio line protection:** The PWM audio line from MCU-S2 is filtered locally in Zone 5 before traveling up to RCA jack J8. Keeping controller lines and audio generation on the opposite end of the board from CPU memory buses prevents digital bus activity from leaking into the sound output as high-pitched whine.

---

### Short high-frequency lines and minimal trace inductance

Every millimeter of PCB trace adds approximately 1 nH of parasitic inductance and 0.1 pF of stray capacitance. Long traces carrying fast digital edges cause signal overshoot, ringing, and crosstalk.

1. **Clock crystal island:** Y1 (8.000 MHz), Y2 (21.47727 MHz), U04, feedback resistors, and load capacitors sit in one compact cluster (< 20 mm analog loop). That keeps both Pierce oscillators at the same chip.
2. **Color index bus length:** The Compositor sits next to the Color PROM. The 6-bit index bus stays under 25 mm. Short copper keeps inductance low without series damping on that bus.
3. **Self-contained blitter loop (24 MHz):** MCU-S1, the 74HC573 address latch, and Field SRAM (U41) form an isolated triangle in Zone 4. The multiplexed AD[7:0] bus, which operates during high-speed VBlank bursts, stays entirely within this local zone.
4. **Direct cartridge bus drop:** Placing the W65C02S CPU immediately above the central cartridge connector (J36) allows CPU address lines A[13:0] and data lines D[7:0] to descend straight down into the connector pins. This minimizes stub lengths and avoids routing dense parallel buses around board obstacles.

---

### Visual appeal, cable segregation, and mechanical balance

The floor plan also sets cable paths and mechanical load:

1. **Cable segregation (front vs rear):** Stationary cables (5 V DC barrel, RGBS, composite RCA, audio line out) attach along the rear (top) edge. Player cables (gamepads and arcade controls) plug into the front (bottom) edge. Cables stay off the board during play.
2. **Mechanical balance for cartridge insertion:** Cartridges impose vertical insertion and extraction forces. Placing the heavy 36-pin edge connector dead-center horizontally distributes mechanical strain symmetrically across all four PCB corner mounting holes, preventing board flexing and solder joint fatigue.
3. **Structured bus highways and retro aesthetics:** By grouping communicating chips in parallel rows (such as CPU and System RAM, or the three 74HC157 multiplexers), traces run in uniform, parallel ribbons with minimal vias. This produces an orderly, clean visual appearance reminiscent of classic arcade and console printed circuit boards rather than an unstructured prototype layout.

---

## 5. Detailed zone specifications

### Zone 1: System RAM and reset (Top-Left)

Houses system memory and the MCP130 reset supervisor.

**Components:**
- System RAM AS6C62256 (U3, DIP-28)
- MCP130 Reset Supervisor (U130, TO-92)
- Bulk filter electrolytic capacitor E1 (220 uF near J1)
- Decoupling capacitors: C2 (for U3), C17 (for U130)

**Placement and routing rules:**
- U3 (System RAM) sits in the top-left area to the left of the CPU.
- The 16-bit address bus (A[15:0]) and 8-bit data bus (D[7:0]) run directly into U3 from the left side of the CPU.
- MCP130 mounts immediately adjacent to CPU pin 40 (`RESB`), with a dedicated 10 kohm pull-up resistor (R30) tied to +5.0 V.
- Buffered PHI2 from the Zone 2 clock island reaches CPU pin 37 through series 33 ohm R12 at the buffer output.

---

### Central CPU: W65C02S (Top-Center, directly above J36)

Serves as the central bus master bridging system RAM and cartridge ROM.

**Components:**
- W65C02S CPU (U1, DIP-40)
- Decoupling capacitor C1 (for U1)

**Placement and routing rules:**
- U1 mounts horizontally in the Top-Center of the board, positioned directly above the cartridge connector J36.
- Address pins A0-A11 (pins 9-20) and A12-A15 (pins 22-25) plus data pins D7-D0 (pins 26-33) face toward the cartridge connector. A0-A13 and D[7:0] drop into the top pin row of J36. Pin 21 is VSS, not an address pin.
- The address and data bus branches westward to feed System RAM U3 in Zone 1.
- Placing the CPU directly above J36 eliminates dog-leg bends and long stub traces on the primary memory bus.

---

### Zone 2: Video engine and analog DAC (Top-Right)

Generates pixel timing, compositor layering, color lookup, and analog video signals.

**Components:**
- Y2 (21.47727 MHz crystal, HC-49/US)
- Y1 (8.000 MHz crystal, HC-49/US)
- 74HCU04 unbuffered inverter (U04, both Pierce loops and clock buffers)
- 74HC74 dual D-type flip-flop (U74, divide-by-4 dot clock)
- Beam X PLD (UPLDX, ATF22V10)
- Beam Y PLD (UPLDY, ATF22V10)
- Compositor PLD (UPLDV, ATF22V10)
- Color PROM AT27C256R (U24, DIP-28)
- Discrete R-2R resistor ladder network (R1 through R11)
- RGBS video output header J2
- Optional composite video encoder: AD724 (U725, SOIC-16), Y3 (3.579545 MHz crystal), RCA jack J9
- Decoupling capacitors: C8 (UPLDX), C9 (UPLDY), C10 (UPLDV), C16 (U24), C18 (U725), C19 (U04), C20 (U74)
- Crystal load capacitors: C22, C23 (Y2), C24, C25 (Y3), C26, C27 (Y1)

**Placement and routing rules:**
- Arranged in a strict sequential line:
  `Oscillator -> Beam X/Y -> Compositor -> Color PROM -> DAC Resistors -> J2 Video Header`
- Y1, Y2, U04, feedback resistors, and load capacitors C22/C23/C26/C27 form one clock island (loop area under 20 mm). Analog Pierce loops stay at U04. Buffered PHI2 and DOT leave that island as digital clocks.
- The 6-bit color index bus runs directly from Compositor outputs to Color PROM address inputs A[5:0] with trace lengths under 25 mm.
- Resistors R1 through R8 mount immediately adjacent to PROM data output pins DQ[7:0].
- The analog video header J2 sits on the top board edge directly adjacent to the DAC termination resistors R9, R10, and R11.
- No digital address or CPU buses pass through the Zone 2 analog corner.

---

### Central Spine: Cartridge connector (Center)

Provides the physical docking slot for game cartridges and flash memory.

**Components:**
- Cartridge edge connector J36 (36-pin female slot)

**Placement and routing rules:**
- J36 mounts horizontally across the middle of the motherboard directly below the W65C02S CPU.
- CPU address lines A0-A13 and data lines D[7:0] drop straight down from U1 into the upper pin row of J36.
- Compositor mapping lines (`CART_A14` through `CART_A18`, `CART_OE#`, `CART_WE#`) enter J36 from Zone 2 on the right.
- Series damping resistors (R14 through R23) sit directly in line between the motherboard buses and J36 pins.

---

### Zone 3: VRAM and address multiplexing (Middle-Left)

Manages video tile memory and the half-cycle PHI2 bus interleave.

**Components:**
- 3x 74HC157 quad 2:1 multiplexers (U7A, U7B, U7C, DIP-16)
- Interleaved VRAM AS6C62256 (U6, DIP-28)
- 74HC574 Scroll X register (U574, DIP-20)
- Decoupling capacitors: C3 (for U6), C11 (for U7A), C12 (for U7B), C13 (for U7C), C15 (for U574)

**Placement and routing rules:**
- The three 74HC157 multiplexers sit directly adjacent to VRAM U6.
- Input A of each multiplexer connects to CPU address lines A[11:0] descending from Zone 1.
- Input B of each multiplexer connects to Beam counter lines arriving from Zone 2.
- Multiplexer outputs Y connect directly to VRAM address pins with shortest possible trace lengths.
- Strobe pin G on each 74HC157 must not float. G high forces the Y outputs low. The strobe comes from the PLD phase decode, not a hard tie to ground.

---

### Central dispatcher: MCU-M (Middle-Center)

Soft `$7Fxx` and SPI hub between CPU, S1, and S2.

**Components:**
- MCU-M (UM, AVR128DB28, SPDIP-28)
- Decoupling capacitor C5 (for UM)

**Placement and routing rules:**
- UM sits at the junction of the CPU data bus, Zone 3 decode, and SPI to S1/S2.
- Bypass C5 is within 5 mm of pin 20 (VDD). Pins 15 and 21 are GND.

---

### Zone 4: S1 sprite rasterizer (Middle-Right)

Builds sprite fields in VBlank and renders background scanline slices.

**Components:**
- MCU-S1 video assist microcontroller (US1, AVR128DB28, SPDIP-28)
- 74HC573 transparent address latch (U573, DIP-20)
- Field SRAM AS6C62256 (U41, DIP-28)
- Decoupling capacitors: C4 (for U41), C6 (for US1), C14 (for U573)

**Placement and routing rules:**
- US1, U573, and U41 form a compact triangular cluster in the middle-right area of the board.
- The multiplexed address and data bus (AD[7:0]) connects US1 pins directly to U573 inputs and U41 data pins.
- S1 write enable (`/WE`) runs with a dedicated short trace to U41 pin 27, held high by local pull-up resistor.
- SPI lines (MOSI, MISO, SCK, `/SS_S1`) run to MCU-M in the middle-center dispatcher.

---

### Zone 5: Controller and audio I/O (Bottom-Left)

Interfaces with external gamepads, arcade controls, and audio output.

**Components:**
- MCU-S2 peripheral microcontroller (US2, AVR128DB28, SPDIP-28)
- 2x 3.5 mm TRS controller jacks (J3, J4)
- Arcade control header J5 (2x10)
- Cabinet power/reset header J7 (2x2)
- RCA audio output jack J8 (top rear edge)
- PWM from US2 pin PF1 runs to J8. The locked BOM has no extra audio filter capacitors.
- Decoupling capacitor: C7 (for US2)

**Placement and routing rules:**
- US2 sits in the lower-left corner immediately behind controller jacks J3 and J4.
- Open-drain UART controller data lines connect to J3 and J4 through short traces with local 4.7 kohm pull-up resistors.
- Audio PWM output from US2 pin PF1 routes up to the rear audio jack J8.

---

## 6. Connector placement and edge mapping

All mechanical interfaces are positioned along the board perimeter according to ergonomic function:

| Connector | Label | Location on PCB | Purpose |
| --- | --- | --- | --- |
| **J1** | DC_IN | Top-Left rear edge | 5.0 V regulated DC barrel jack (center positive) |
| **J8** | AUDIO | Top-Center rear edge | RCA mono/stereo audio line out |
| **J9** | COMPOSITE | Top-Center rear edge | RCA composite video out (yellow) |
| **J2** | RGBS_HDR | Top-Right rear edge | 6-pin 0.1 inch header for raw RGBS video |
| **J36** | CART_EDGE | Center board spine | 36-pin 2.54 mm edge connector for game carts |
| **J3** | TRS_P1 | Bottom-Left front edge | 3.5 mm TRS jack for Player 1 gamepad |
| **J4** | TRS_P2 | Bottom-Left front edge | 3.5 mm TRS jack for Player 2 gamepad |
| **J5** | ARCADE | Bottom-Left edge near J4 | 2x10 pin header. Odd pins are Player 1, even pins are Player 2, pins 17-20 are GND |
| **J7** | CAB_PWR | Bottom edge near J5 | 2x2 `+5V`/`GND` over `RESET_N`/`GND` |

---

## 7. Power routing and ground distribution

1. **Power entry:** +5.0 V enters at J1 (Top-Left) into bulk electrolytic capacitor E1 (220 uF to 470 uF). Cab power and reset also land on J7.
2. **Domain distribution:** Main VCC splits through four 2-pin isolation headers (JP_PWR1 through JP_PWR4) to allow progressive bring-up as specified in [`staged-pcb-bringup-guide.md`](staged-pcb-bringup-guide.md).
3. **Decoupling proximity:** Every IC socket has a 0.1 uF low-ESR ceramic capacitor connected within 5 mm of its VCC pin according to the complete assignment in Section 8.
4. **Unified ground:** Layers 2 and 3 are solid GND planes. Layers 1 and 4 carry their signals plus a GND fill, so all four layers have GND copper. The pours are not split into digital and analog regions. Net assignment is in `docs/general/hardware.md`.
5. **Via stitching:** Vias tie GND on all four layers every 10 mm to 15 mm and immediately adjacent to IC ground pins. Orphan copper is removed in the zone fill.

---

## 8. Capacitor assignment and placement reference

The motherboard houses 20 decoupling sites (19 counted ICs plus the MCP130 supervisor), 6 crystal load capacitors, and 1 bulk entry electrolytic capacitor. Cart flash, cart EEPROM, and pad ATtiny85 each have their own 100 nF on those boards, not in the C1-C20 motherboard set.

| Cap | Value | Type | Assigned IC or Net | Package | Power Pin | Ground Pin | Board Location | Proximity Requirement |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | 100 nF | Ceramic | U1 (W65C02S CPU) | DIP-40 | Pin 8 (VDD) | Pin 21 (VSS) | Central CPU (Top-Center) | Mount within 5 mm of pin 8 |
| C2 | 100 nF | Ceramic | U3 (System RAM) | DIP-28 | Pin 28 (VCC) | Pin 14 (VSS) | Zone 1 (Top-Left) | Mount within 5 mm of pin 28 |
| C3 | 100 nF | Ceramic | U6 (VRAM) | DIP-28 | Pin 28 (VCC) | Pin 14 (VSS) | Zone 3 (Middle-Left) | Mount within 5 mm of pin 28 |
| C4 | 100 nF | Ceramic | U41 (Field SRAM) | DIP-28 | Pin 28 (VCC) | Pin 14 (VSS) | Zone 4 (Middle-Right) | Mount within 5 mm of pin 28 |
| C5 | 100 nF | Ceramic | UM (MCU-M) | SPDIP-28 | Pin 20 (VDD) | Pins 15, 21 (GND) | Middle-Center | Mount within 5 mm of pin 20 |
| C6 | 100 nF | Ceramic | US1 (MCU-S1) | SPDIP-28 | Pin 20 (VDD) | Pins 15, 21 (GND) | Zone 4 (Middle-Right) | Mount within 5 mm of pin 20 |
| C7 | 100 nF | Ceramic | US2 (MCU-S2) | SPDIP-28 | Pin 20 (VDD) | Pins 15, 21 (GND) | Zone 5 (Bottom-Left) | Mount within 5 mm of pin 20 |
| C8 | 100 nF | Ceramic | UPLDX (Beam X) | DIP-24 | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) | Mount within 5 mm of pin 24 |
| C9 | 100 nF | Ceramic | UPLDY (Beam Y) | DIP-24 | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) | Mount within 5 mm of pin 24 |
| C10 | 100 nF | Ceramic | UPLDV (Compositor) | DIP-24 | Pin 24 (VCC) | Pin 12 (GND) | Zone 2 (Top-Right) | Mount within 5 mm of pin 24 |
| C11 | 100 nF | Ceramic | U7A (74HC157 Mux A) | DIP-16 | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) | Mount within 5 mm of pin 16 |
| C12 | 100 nF | Ceramic | U7B (74HC157 Mux B) | DIP-16 | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) | Mount within 5 mm of pin 16 |
| C13 | 100 nF | Ceramic | U7C (74HC157 Mux C) | DIP-16 | Pin 16 (VCC) | Pin 8 (GND) | Zone 3 (Middle-Left) | Mount within 5 mm of pin 16 |
| C14 | 100 nF | Ceramic | U573 (Address Latch) | DIP-20 | Pin 20 (VCC) | Pin 10 (GND) | Zone 4 (Middle-Right) | Mount within 5 mm of pin 20 |
| C15 | 100 nF | Ceramic | U574 (Scroll X Latch) | DIP-20 | Pin 20 (VCC) | Pin 10 (GND) | Zone 3 (Middle-Left) | Mount within 5 mm of pin 20 |
| C16 | 100 nF | Ceramic | U24 (Color PROM) | DIP-28 | Pin 28 (VCC) | Pin 14 (GND) | Zone 2 (Top-Right) | Mount within 5 mm of pin 28 |
| C17 | 100 nF | Ceramic | U130 (MCP130 Supervisor) | TO-92 | Pin 2 (VDD) | Pin 3 (VSS) | Zone 1 (Top-Left) | Mount within 5 mm of pin 2 |
| C18 | 100 nF | Ceramic | U725 (AD724 Composite) | SOIC-16 | Pin 4 (APOS), pin 14 (DPOS) | Pin 2 (AGND), pin 13 (DGND) | Zone 2 (Top-Right) | Mount adjacent to pins 4 and 14 |
| C19 | 100 nF | Ceramic | U04 (74HCU04) | DIP-14 | Pin 14 (VCC) | Pin 7 (GND) | Zone 2 clock island | Mount within 5 mm of pin 14 |
| C20 | 100 nF | Ceramic | U74 (74HC74) | DIP-14 | Pin 14 (VCC) | Pin 7 (GND) | Zone 2 clock island | Mount within 5 mm of pin 14 |
| C22 | 22 pF | Ceramic | Y2 (21.47727 MHz) | Discrete | Crystal pin 1 | GND | Zone 2 clock island | Tight loop with Y2 and U04 |
| C23 | 22 pF | Ceramic | Y2 (21.47727 MHz) | Discrete | Crystal pin 2 | GND | Zone 2 clock island | Tight loop with Y2 and U04 |
| C24 | 22 pF | Ceramic | Y3 (3.579545 MHz FSC) | Discrete | Crystal pin 1 | GND | Zone 2 (Top-Right) | Tight loop with Y3 and U725 |
| C25 | 22 pF | Ceramic | Y3 (3.579545 MHz FSC) | Discrete | Crystal pin 2 | GND | Zone 2 (Top-Right) | Tight loop with Y3 and U725 |
| C26 | 22 pF | Ceramic | Y1 (8.000 MHz) | Discrete | Crystal pin 1 | GND | Zone 2 clock island | Tight loop with Y1 and U04 |
| C27 | 22 pF | Ceramic | Y1 (8.000 MHz) | Discrete | Crystal pin 2 | GND | Zone 2 clock island | Tight loop with Y1 and U04 |
| E1 | 220 uF | Electrolytic | Power Entry Rail | Radial Can | +5V Rail | GND | Zone 1 (Top-Left) | Mount adjacent to J1 / SW1 |

