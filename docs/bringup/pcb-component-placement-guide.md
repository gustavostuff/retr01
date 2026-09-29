# PCB component placement and floor planning guide

Functional zoning, physical placement topology, and bus-flow routing guidelines for the Retr01 motherboard PCB layout.

---

## 1. Architectural rationale for functional zoning

Preliminary PCB drafts frequently group components by physical package dimensions (such as placing all DIP-28 memory packages in a single vertical column, or aligning all logic ICs on one edge). While visually tidy, package-shape grouping creates severe routing bottlenecks:

1. **Long bus crossings:** Grouping unrelated memory chips forces lines from four independent memory buses (CPU system bus, multiplexed VRAM bus, compositor color index bus, and S1 blitter bus) to cross each other repeatedly across the board.
2. **Antenna trace loops:** Separating the video timing PLDs from the Color PROM and DAC forces the high-speed 6-bit color index bus to travel across the entire board under the cartridge connector, radiating electromagnetic interference and increasing susceptibility to digital noise.
3. **Crosstalk into controller and audio lines:** Routing high-speed address buses near sensitive gamepad lines or audio filtering networks degrades signal fidelity.

To achieve clean, short traces with minimal layer transitions, components are arranged by **electrical signal flow** into five dedicated functional zones surrounding the central cartridge connector.

---

## 2. Board form factor: 160 mm x 100 mm (Standard Eurocard 3U)

The physical motherboard outline is specified at **160 mm width by 100 mm height**, conforming to the international **Standard Eurocard 3U format** (standardized under DIN 41494 and IEC 60297).

### Rationale for the 160 mm x 100 mm standard size

1. **Off-the-shelf enclosure compatibility:** As a globally recognized standard format, numerous manufacturers (such as Hammond Manufacturing, Fischer Elektronik, and standard electronic project box suppliers) produce aluminum extruded and molded plastic desktop cases with integrated internal card guide slots designed specifically for 160 mm x 100 mm PCBs. This enables housing the system in durable, professional enclosures without requiring custom 3D-printed chassis rails.
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
+---------------------------------------------------------------------------+
| [REAR I/O]   Power (J1)    Audio (J8)     Composite (J9)    RGBS Video(J2)|
+-----------------------+-----------------------------+---------------------+
| SYSTEM RAM & CLOCK    | W65C02S CPU (U1, DIP-40)    | VIDEO ENGINE & DAC  |
| (Top-Left)            | (Top-Center, Horizontal)    | (Top-Right)         |
|                       |                             |                     |
|   * System RAM (U3)   |   * Sits DIRECTLY above J36 |   * 21.48 MHz Osc   |
|   * PHI2 Clock (Y1)   |   * Address & Data drop     |   * Beam X & Y PLDs |
|   * MCP130 Supervisor |     straight down into cart |   * Compositor PLD  |
|                       |   * Bus branches left to U3 |   * Color PROM, DAC |
+-----------------------+-----------------------------+---------------------+
|                 CARTRIDGE SLOT (J36, Center-Horizontal)                   |
|                 (Directly below the CPU, middle spine)                    |
+-----------------------+-----------------------------+---------------------+
| VRAM & 74HC157 MUXES  | MCU-M (CENTRAL DISPATCHER)  | S1 SPRITE ENGINE    |
| (Middle-Left)         | (Middle-Center)             | (Middle-Right)      |
|                       |                             |                     |
|   * VRAM (U6)         |   * MCU-M (AVR128DB28)      |  * MCU-S1 (Blitter) |
|   * 3x 74HC157 Muxes  |   * Soft I/O bus interface  |  * 74HC573 Latch    |
|   * Scroll Latch (574)|   * Central SPI routing hub |  * Field SRAM (U41) |
+-----------------------+-----------------------------+---------------------+
| CONTROLLER & AUDIO    | [OPEN / EXPANSION AREA]                           |
| (Bottom-Left)         | (Bottom-Right)                                    |
|                       |                                                   |
|   * MCU-S2 (AVR128)   |   * Ground return, test points, mounting          |
|   * J3, J4 TRS Jacks  |                                                   |
|   * Arcade Header J5  |                                                   |
+-----------------------+---------------------------------------------------+
```

---

## 4. Why this floor plan works (Physics and ergonomics)

The physical zoning succeeds because it aligns component placement directly with electromagnetic physics, trace parasitics, and user interaction mechanics.

### Noise isolation and return path containment

In mixed-signal systems containing high-speed digital buses alongside sensitive analog outputs, noise coupling occurs primarily through shared ground return paths.

1. **Digital return current isolation:** High-frequency currents returning through the ground plane flow directly beneath their respective signal traces. In this layout, CPU bus switching (Zone 1) and S1 blitter traffic (Zone 4) remain strictly confined to the left side and lower right of the board. Their ground return currents never pass beneath the analog video DAC (Zone 2) or audio filtering network (Zone 5).
2. **Analog video quiet zone:** By placing the video engine, Color PROM, and R-2R ladder in the upper-right corner directly next to the RGBS header (J2), the entire analog video signal path spans less than 20 mm of total copper. No high-speed digital buses cut through or run parallel to these analog nodes, preventing visual sparkling, pixel noise, and 60 Hz hum on CRT monitors.
3. **Audio line protection:** The PWM audio line from MCU-S2 is filtered locally in Zone 5 before traveling up to RCA jack J8. Keeping controller lines and audio generation on the opposite end of the board from CPU memory buses prevents digital bus activity from leaking into the sound output as high-pitched whine.

---

### Short high-frequency lines and minimal trace inductance

Every millimeter of PCB trace adds approximately 1 nH of parasitic inductance and 0.1 pF of stray capacitance. Long traces carrying fast digital edges cause signal overshoot, ringing, and crosstalk.

1. **Master crystal stability (21.48 MHz):** The 21.477 MHz crystal Y2, 74HCU04 inverter, and feedback network sit in an ultra-compact cluster (< 20 mm total loop). This prevents RF emissions and ensures reliable oscillator startup without capacitive detuning.
2. **Elimination of the color index bus antenna:** In the preliminary prototype layout, the 6-bit color index bus traveled roughly 180 mm across the entire board from the PLDs to the PROM. In this floor plan, placing the Compositor directly adjacent to the Color PROM shrinks that bus to under 25 mm. This eliminates trace inductance, preventing signal ringing without needing series damping resistors.
3. **Self-contained blitter loop (24 MHz):** MCU-S1, the 74HC573 address latch, and Field SRAM (U41) form an isolated triangle in Zone 4. The multiplexed AD[7:0] bus, which operates during high-speed VBlank bursts, stays entirely within this local zone.
4. **Direct cartridge bus drop:** Placing the W65C02S CPU immediately above the central cartridge connector (J36) allows CPU address lines A[13:0] and data lines D[7:0] to descend straight down into the connector pins. This minimizes stub lengths and avoids routing dense parallel buses around board obstacles.

---

### Visual appeal, cable segregation, and mechanical balance

Beyond electrical performance, the floor plan optimizes physical aesthetics, cable management, and mechanical durability:

1. **Clean cable segregation (Front vs. Rear):** All stationary, heavy cables (5V DC power barrel, RGBS monitor cable, composite RCA, audio line out) attach exclusively along the rear (top) edge. Conversely, dynamic player cables (gamepads and arcade controls) plug into the front (bottom) edge. This prevents cables from draping across the board or tangling with controllers during play.
2. **Mechanical balance for cartridge insertion:** Cartridges impose vertical insertion and extraction forces. Placing the heavy 36-pin edge connector dead-center horizontally distributes mechanical strain symmetrically across all four PCB corner mounting holes, preventing board flexing and solder joint fatigue.
3. **Structured bus highways and retro aesthetics:** By grouping communicating chips in parallel rows (such as CPU and System RAM, or the three 74HC157 multiplexers), traces run in uniform, parallel ribbons with minimal vias. This produces an orderly, clean visual appearance reminiscent of classic arcade and console printed circuit boards rather than an unstructured prototype layout.

---

## 5. Detailed zone specifications

### Zone 1: System RAM and clock generation (Top-Left)

Houses system memory and master CPU clock generation.

**Components:**
- System RAM AS6C62256 (U3, DIP-28)
- PHI2 Clock source Y1 (8.000 MHz canned oscillator or discrete circuit)
- MCP130 Reset Supervisor (TO-92)
- Reset tactile pushbutton (SW_RST)
- Decoupling capacitors C2, C17

**Placement and routing rules:**
- U3 (System RAM) sits in the top-left area to the left of the CPU.
- The 16-bit address bus (A[15:0]) and 8-bit data bus (D[7:0]) run directly into U3 from the left side of the CPU.
- MCP130 mounts immediately adjacent to CPU pin 40 (`RESB`), with a dedicated 10 kohm pull-up resistor (R30) tied to +5.0 V.
- Y1 (PHI2) sits near CPU pin 37 (`PHI2_IN`) with series 33 ohm damping resistor R12 placed directly at the oscillator output.

---

### Central CPU: W65C02S (Top-Center, directly above J36)

Serves as the central bus master bridging system RAM and cartridge ROM.

**Components:**
- W65C02S CPU (U1, DIP-40)
- Decoupling capacitor C1

**Placement and routing rules:**
- U1 mounts horizontally in the Top-Center of the board, positioned directly above the cartridge connector J36.
- Address pins A[13:0] (pins 9-23) and Data pins D[7:0] (pins 26-33) face toward the cartridge connector and drop straight down into the top pin row of J36.
- The address and data bus branches westward to feed System RAM U3 in Zone 1.
- Placing the CPU directly above J36 eliminates dog-leg bends and long stub traces on the primary memory bus.

---

### Zone 2: Video engine and analog DAC (Top-Right)

Generates pixel timing, compositor layering, color lookup, and analog video signals.

**Components:**
- Y2 (21.47727 MHz crystal, HC-49/US)
- 74HCU04 unbuffered inverter (oscillator feedback stage)
- 74HC74 dual D-type flip-flop (divide-by-4 dot clock divider)
- Beam X PLD (UPLDX, ATF22V10)
- Beam Y PLD (UPLDY, ATF22V10)
- Compositor PLD (UPLDV, ATF22V10)
- Color PROM AT27C256R (U24, DIP-28)
- Discrete R-2R resistor ladder network (R1 through R11)
- RGBS video output header J2
- Optional composite video encoder: AD724 (U725, SOIC-16), Y3 (3.579545 MHz crystal), RCA jack J9

**Placement and routing rules:**
- Arranged in a strict sequential line:
  `Oscillator -> Beam X/Y -> Compositor -> Color PROM -> DAC Resistors -> J2 Video Header`
- Y2, the 74HCU04, feedback resistor, and load capacitors C22/C23 form a tight cluster with trace loop area under 20 mm.
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
- CPU address lines A[13:0] and data lines D[7:0] drop straight down from U1 into the upper pin row of J36.
- Compositor mapping lines (`CART_A14` through `CART_A18`, `CART_OE#`, `CART_WE#`) enter J36 from Zone 2 on the right.
- Series damping resistors (R14 through R23) sit directly in line between the motherboard buses and J36 pins.

---

### Zone 3: VRAM and address multiplexing (Middle-Left)

Manages video tile memory and the half-cycle PHI2 bus interleave.

**Components:**
- 3x 74HC157 quad 2:1 multiplexers (U7A, U7B, U7C, DIP-16)
- Interleaved VRAM AS6C62256 (U6, DIP-28)
- 74HC574 Scroll X register (U574, DIP-20)
- MCU-M master microcontroller (UM, AVR128DB28, SPDIP-28)

**Placement and routing rules:**
- The three 74HC157 multiplexers sit directly adjacent to VRAM U6.
- Input A of each multiplexer connects to CPU address lines A[11:0] descending from Zone 1.
- Input B of each multiplexer connects to Beam counter lines arriving from Zone 2.
- Multiplexer outputs Y connect directly to VRAM address pins with shortest possible trace lengths.
- Strobe pins (G) of all three 74HC157 chips tie solidly to ground.
- MCU-M sits at the central junction between Zone 1 (CPU data bus), Zone 3 (soft register decode), and the SPI bus going to S1 and S2.

---

### Zone 4: S1 sprite rasterizer (Middle-Right)

Builds sprite fields in VBlank and renders background scanline slices.

**Components:**
- MCU-S1 video assist microcontroller (US1, AVR128DB28, SPDIP-28)
- 74HC573 transparent address latch (U573, DIP-20)
- Field SRAM AS6C62256 (U41, DIP-28)

**Placement and routing rules:**
- US1, U573, and U41 form a compact triangular cluster in the middle-right area of the board.
- The multiplexed address and data bus (AD[7:0]) connects US1 pins directly to U573 inputs and U41 data pins.
- S1 write enable (`/WE`) runs with a dedicated short trace to U41 pin 27, held high by local pull-up resistor.
- SPI lines (MOSI, MISO, SCK, `/SS_S1`) run leftward to MCU-M in Zone 3.

---

### Zone 5: Controller and audio I/O (Bottom-Left)

Interfaces with external gamepads, arcade controls, and audio output.

**Components:**
- MCU-S2 peripheral microcontroller (US2, AVR128DB28, SPDIP-28)
- 2x 3.5 mm TRS controller jacks (J3, J4)
- 2x10 arcade control pin header (J5)
- Passive audio low-pass filter components (resistors, film capacitors)
- RCA audio output jack J8 (routed to top rear edge)

**Placement and routing rules:**
- US2 sits in the lower-left corner immediately behind controller jacks J3 and J4.
- Open-drain UART controller data lines connect to J3 and J4 through short traces with local 4.7 kohm pull-up resistors.
- Audio PWM output from US2 pin PF1 feeds through a passive RC filter network in Zone 5 before routing up to the rear audio jack J8.

---

## 6. Connector placement and edge mapping

All mechanical interfaces are positioned along the board perimeter according to ergonomic function:

| Connector | Label | Location on PCB | Purpose |
| --- | --- | --- | --- |
| **J1** | DC_IN | Top-Left rear edge | 5.0 V regulated DC barrel jack (center positive) |
| **SW1** | POWER | Top-Left edge near J1 | Main system power slide switch |
| **J8** | AUDIO | Top-Center rear edge | RCA mono/stereo audio line out |
| **J9** | COMPOSITE | Top-Center rear edge | RCA composite video out (yellow) |
| **J2** | RGBS_HDR | Top-Right rear edge | 6-pin 0.1 inch header for raw RGBS video |
| **J36** | CART_EDGE | Center board spine | 36-pin 2.54 mm edge connector for game carts |
| **J3** | TRS_P1 | Bottom-Left front edge | 3.5 mm TRS jack for Player 1 gamepad |
| **J4** | TRS_P2 | Bottom-Left front edge | 3.5 mm TRS jack for Player 2 gamepad |
| **J5** | ARCADE | Bottom-Left edge near J4 | 2x10 dual-row pin header for arcade cabinet controls |

---

## 7. Power routing and ground distribution

1. **Power entry:** +5.0 V enters at J1 (Top-Left) through power switch SW1 into bulk electrolytic capacitor E1 (220 uF to 470 uF).
2. **Domain distribution:** Main VCC splits through four 2-pin isolation headers (JP_PWR1 through JP_PWR4) to allow progressive bring-up as specified in [`staged-pcb-bringup-guide.md`](staged-pcb-bringup-guide.md).
3. **Decoupling proximity:** Every IC socket has a 0.1 uF low-ESR ceramic capacitor connected within 5 mm of its VCC pin.
4. **Unified ground plane:** Both top and bottom copper layers are flooded with GND pours. The bottom layer is preserved as the primary continuous ground return by routing the majority of signal traces on the top layer.
5. **Via stitching:** Ground stitching vias are placed every 10 mm to 15 mm across the board and immediately adjacent to IC ground pins.
