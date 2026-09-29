# Staged PCB bring-up guide (Tiers A through H)

Feasibility analysis, hardware layout rules, and staged validation strategy for bringing up Retr01 on a single physical motherboard PCB revision.

---

## 1. Role of solderless breadboards during bring-up

Solderless breadboards remain a valuable intermediate tool before and during PCB assembly:

1. **Individual IC sanity testing:** Before inserting rare or expensive ICs (W65C02S, programmed ATF22V10s, AT27C256R PROM) into a freshly soldered board, individual pin truth tables and programming verification can be performed on a breadboard using an Arduino test rig as detailed in [`bench-testing-guide.md`](bench-testing-guide.md).
2. **Optional early video exploration (Tiers A, B, and C):** Builders may choose to prototype the video generation stages on a breadboard first to observe raw sync signals or test custom JEDEC equations before ordering copper.
3. **High-speed breadboard limitations:** When breadboarding Tiers A, B, or C, builders must account for the physical parasitic effects detailed in [`bench-testing-guide.md`](bench-testing-guide.md):
   - Contact clip capacitance (2 to 5 pF per row) detuning the 21.477 MHz Pierce crystal oscillator network.
   - Power rail inductance causing ATF22V10 ground bounce and false counter clocking during simultaneous output switching.
   - Resistor row capacitance introducing low-pass filtering on the R-2R DAC, resulting in color smear.
   - Capacitive crosstalk on the multiplexed field SRAM write enable (`/WE`) line during Tier C bus handoffs.

Moving to a printed circuit board eliminates these breadboard contact parasitics, but introduces the reality of fixed copper routing. On any initial board revision (Rev 1.0), minor bodge wires (cut traces and wire jumpers) remain a normal debugging recourse for resolving schematic errata.

---

## 2. Feasibility analysis for a single unified PCB

Designing a single printed circuit board that serves every stage from Tier A through Tier H is fully feasible and follows standard embedded hardware engineering practice. Fabricating separate intermediate boards for each tier introduces layout divergence, extra turnaround delay, and unnecessary fabrication cost.

### Core feasibility principles

1. **Superset topology:** Tier H is the architectural superset of all motherboard hardware. Every trace, bus, and passive required by earlier tiers exists in the Tier H schematic.
2. **Socketed active components:** All counted DIP/SPDIP ICs mount in dual-wipe or turned-pin sockets. Subsystems are brought online by inserting chips into their respective sockets stage by stage, without rework.
3. **Convergence of Tiers A and B on copper:** On a breadboard, Tier A omits the Compositor PLD purely to minimize loose jumper wiring. On a PCB, the Compositor is already routed in copper between Beam X/Y and the Color PROM. Programming the Compositor with pass-through equations validates Tiers A and B simultaneously on the same board.
4. **Fail-closed unpopulated behavior:** When an unpopulated socket leaves a net floating, properly placed pull-up or pull-down resistors keep active-low control lines (`/OE`, `/WE`, `/CE`, `RESB`, `RDY`) in a known, safe, inactive state.
5. **Domain isolation via jumper headers:** 2-pin 2.54 mm headers with removable shorting jumpers placed on key power traces permit testing individual blocks in total isolation before tying them to shared buses.

---

## 3. Realistic PCB layout, routing, and placement guidelines

Designing a board that functions reliably at Retr01 clock frequencies (21.477 MHz master clock, 5.369 MHz pixel clock, 8.000 MHz CPU clock) relies on standard digital layout rules rather than ultra-high-speed transmission line engineering.

### Trace length and propagation delay reality

Signal propagation velocity in standard FR4 fiberglass is approximately 150 mm per nanosecond (6 inches per nanosecond).

- At 5.369 MHz (DOT clock), one clock period is 186 nanoseconds.
- At 8.000 MHz (PHI2 clock), one clock period is 125 nanoseconds.
- At 21.477 MHz (master oscillator), one clock period is 46 nanoseconds.

A trace length variation of 50 mm (2 inches) across parallel data lines introduces approximately 0.33 nanoseconds of propagation skew. In an 8-bit bus with 125 ns cycle times, 0.33 ns represents less than 0.3% of the clock period. Consequently, serpentine trace length matching is unnecessary for Retr01. Direct, point-to-point routing with minimal vias satisfies all timing constraints.

### Recommended trace widths and board stackup

Standard PCB fabrication specifications:
- **Digital signal traces:** 0.25 mm (10 mil) width with 0.25 mm spacing. This fits between 2.54 mm DIP socket pins while maintaining high fabrication yield.
- **Power traces (2-layer board):** 0.8 mm to 1.2 mm (30 to 50 mil) for primary VCC distribution buses, bordered by ground copper pours on top and bottom layers.
- **Unified ground plane rule:** A single, unbroken ground plane serves all digital and analog components. Ground planes are never physically split into digital and analog copper sections. Noise isolation is achieved through component placement and physical zoning rather than cutting copper.
- **2-layer ground pour strategy:** Ground zones fill both top (F.Cu) and bottom (B.Cu) layers. To prevent slicing the bottom ground plane into disconnected strips, signal routing runs predominantly on the top layer, using the bottom layer only for short jumper links.
- **Stitching vias:** Ground stitching vias connect top and bottom ground fills liberally (every 10 to 15 mm and adjacent to IC ground pins) to maintain low-impedance return paths. Unconnected copper islands are set to be removed in KiCad zone fill properties.
- **4-layer board recommendation:** Utilizing a 4-layer stackup (Layer 1: Signals, Layer 2: Solid Ground Plane, Layer 3: +5.0 V Power Plane, Layer 4: Signals) provides unbroken low-impedance power and ground returns. This suppresses rail inductance, simplifies routing, and eliminates ground bounce risks without significant cost penalty.

### Component placement topology

Component placement follows the natural left-to-right signal flow of the console architecture:

1. **Oscillator cluster:** Y2 (21.477 MHz crystal), 74HCU04 inverter, 1 Mohm feedback resistor, and C1/C2 load capacitors reside within a tight 20 mm cluster. The ground pins of the load capacitors return directly to the 74HCU04 ground pin before reaching the board ground plane.
2. **Video pipeline flow:** Components align sequentially:
   `Beam X / Beam Y PLDs -> Compositor PLD -> Color PROM -> R-2R Resistor Ladder -> J2 Video Header`
   The R-2R ladder resistors mount immediately adjacent to the PROM data output pins to minimize analog trace capacitance.
3. **CPU and memory cluster:** W65C02S sits adjacent to System RAM (U3) and VRAM (U6). The three 74HC157 address multiplexers bridge the physical span between the CPU address bus, Beam counters, and VRAM.
4. **Decoupling proximity:** Every IC socket features a 0.1 uF ceramic bypass capacitor placed within 5 mm of its VCC pin.

---

## 4. Hardware layout provisions for single-board staged bring-up

To allow one PCB to support progressive bring-up, the PCB layout incorporates specific isolation jumpers, tap headers, and pull resistors:

### Power domain isolation jumpers

Power rails split from the main +5.0 V input jack (J1) through 2-pin 2.54 mm headers with standard shorting shunts:

| Jumper refdes | Rail domain | Downstream components | Role during bring-up |
| --- | --- | --- | --- |
| **JP_PWR1** | VCC_CORE | Clocks, Beam PLDs, Compositor, Color PROM, DAC | Allows video subsystem testing without powering processors |
| **JP_PWR2** | VCC_AVR | MCU-M, MCU-S1, MCU-S2 | Isolates microcontrollers for independent current and programming checks |
| **JP_PWR3** | VCC_CPU | W65C02S, System RAM, VRAM, 74HC157 muxes | Keeps CPU and main memory unpowered during early SPI lab stages |
| **JP_PWR4** | VCC_AUDIO | Audio op-amps, filter passives | Keeps audio circuitry unpowered until Tier H |

Removing a shunt allows measuring supply current to that domain with a multimeter in series, or keeping a domain completely unpowered while debugging a short circuit.

### Logic analyzer tap headers

To avoid clipping onto fragile IC pins on the board, the layout provides two 2.54 mm male pin headers dedicated to logic analyzer connection:

**J_TAP_SPI (6-pin header):**
- Pin 1: SCK
- Pin 2: MOSI
- Pin 3: MISO
- Pin 4: `/SS_S1`
- Pin 5: `/SS_S2`
- Pin 6: GND

**J_TAP_TIMING (8-pin header):**
- Pin 1: DOT (5.369 MHz)
- Pin 2: PHI2 (8.000 MHz)
- Pin 3: HSYNC
- Pin 4: VSYNC
- Pin 5: CSYNC
- Pin 6: VBLANK
- Pin 7: `S1_RDY`
- Pin 8: GND

### Bus control and injection jumpers

To support intermediate tier testing where an upstream driver does not yet exist:

| Jumper refdes | Function | Default configuration | Staged test configuration |
| --- | --- | --- | --- |
| **JP_RESB** | CPU Reset Source | 1-2: MCP130 supervisor output | 2-3: Manual pushbutton / test clip to GND |
| **JP_RDY** | CPU RDY Pull | 1-2: 4.7 kohm pull-up to +5V tied to MCU-M | Open: External override / scope observation |
| **JP_MUX_SEL** | VRAM 74HC157 Select | 1-2: Tied to PHI2 clock | 2-3: Tied to static LOW (beam only) or HIGH (CPU only) |
| **JP_CART_OE** | Cartridge Output Enable | 1-2: Compositor PLD `/CART_OE` output | Open: Forced high via board pull-up (cart bus isolated) |

---

## 5. Staged bring-up sequence on the single PCB

The board is populated and tested in six discrete stages:

```
[Phase 0: Bare PCB Power & Continuity]
                   |
[Phase 1: Video Core (Tiers A-C on PCB)]
                   |
[Phase 2: Tier D - MCU-M and OAM SPI]
                   |
[Phase 3: Tier E - PHI2 Clock & Interleaved VRAM]
                   |
[Phase 4: Tier F - W65C02S & Soft $7Fxx]
                   |
[Phase 5: Tier G - Cartridge Interface & MAP]
                   |
[Phase 6: Tier H - MCU-S2, Pads, & Audio]
```

### Phase 0: Bare PCB power and continuity verification

Before populating any sockets or passives:
1. Measure resistance between +5.0 V and GND planes. Resistance must read open circuit or greater than 1 Mohm.
2. Solder power barrel jack J1, main filter capacitor E1, and power switch SW1.
3. Apply +5.0 V and measure voltage across every IC socket VCC/GND pin pair before soldering sockets.
4. Solder all DIP sockets, bypass capacitors, resistor arrays, and discrete passives.

---

### Phase 1: Video core verification (Tiers A-C on PCB)

Before inserting CPU or microcontrollers, the video generation path is confirmed on the PCB ground plane:
- **Populate:**
  - Discrete Pierce oscillator components (Y2 21.477 MHz, 74HCU04, 74HC74).
  - Sockets: UPLDX (Beam X), UPLDY (Beam Y), UPLDV (Compositor), U24 (Color PROM), US1 (MCU-S1), U41 (Field SRAM), U573 (ALE latch).
  - Resistor DAC (R1-R11) and RGBS video header J2.
- **Leave unpopulated:**
  - Sockets: U1 (W65C02S), U3 (System RAM), U6 (VRAM), U7A-C (HC157), UM (MCU-M), US2 (MCU-S2), AD724.
- **Verification:**
  - Power up with JP_PWR1 and JP_PWR2 installed.
  - Connect monitor to J2 RGBS header.
  - Verify crisp color bars and S1 test sprite patterns.
  - Confirm absence of breadboard ringing or color smearing thanks to the solid PCB ground plane.

---

### Phase 2: Tier D (MCU-M and OAM SPI path)

Proves SPI mailbox transfer between MCU-M and MCU-S1 on the PCB:
- **Populate:**
  - Socket UM (AVR128DB28 for MCU-M) and bypass cap C5.
- **Leave unpopulated:**
  - Sockets: U1 (CPU), U3 (System RAM), U6 (VRAM), U7A-C (HC157), US2 (MCU-S2).
- **Verification:**
  - Clip 8-channel logic analyzer to `J_TAP_SPI`.
  - Flash MCU-M with test firmware that sends moving OAM coordinates to S1 during early VBlank.
  - Verify sprites move across the screen on the RGBS monitor.
  - Confirm that SPI activity ceases well before active display begins.

---

### Phase 3: Tier E (PHI2 clock and interleaved VRAM)

Proves 8.000 MHz memory multiplexing without the CPU:
- **Populate:**
  - Y1 (8.000 MHz clock source) and 33 ohm series resistor R12.
  - Sockets: U7A, U7B, U7C (74HC157 multiplexers), U6 (VRAM AS6C62256).
- **Leave unpopulated:**
  - Sockets: U1 (CPU), U3 (System RAM), US2 (MCU-S2).
- **Verification:**
  - Verify island 1 (static beam fetch): configure JP_MUX_SEL to static LOW. Confirm steady BG1 tiles display from pre-burned VRAM.
  - Verify island 2 (interleave): configure JP_MUX_SEL to follow PHI2. Connect MCU-M test pins or an external writer to the unpopulated CPU socket data/address pins to perform PHI2-high write bursts.
  - Confirm zero sparkle or bus fights on the display.

---

### Phase 4: Tier F (W65C02S and soft $7Fxx)

Brings the game CPU and system memory online:
- **Populate:**
  - Sockets: U1 (W65C02S), U3 (System RAM AS6C62256), U574 (Scroll latch), MCP130 reset supervisor.
  - Install JP_PWR3 shunt.
- **Cartridge slot status:**
  - Cartridge slot remains empty. PRG code runs from a pre-loaded image in System RAM or a test PROM plugged into the cart footprint.
- **Verification:**
  - Power up and observe MCP130 holding `RESB` low for 350 ms before crisp release.
  - Verify W65C02S executes reset vector and initiates soft I/O read/write cycles to MCU-M at `$7Fxx`.
  - Measure `CPU_RDY` on an oscilloscope to confirm zero hold cycles on common register accesses.
  - Verify CPU-driven sprite movement and smooth background scrolling on display.

---

### Phase 5: Tier G (Cartridge interface and MAP)

Validates the physical cartridge edge connector and memory mapping:
- **Populate:**
  - Solder cartridge edge connector J36.
  - Insert physical cartridge containing SST39SF040 Flash and optional 24C64 EEPROM.
- **Verification:**
  - Remove temporary Tier F PRG mapping.
  - Power up and boot PRG directly from the physical cartridge.
  - Verify MAP streaming registers (`$7F90-$7F93`) stream tile data from cartridge Flash into VRAM.
  - Verify that cartridge `/OE` is asserted only during valid PRG and MAP access cycles.

---

### Phase 6: Tier H (MCU-S2, controllers, and audio)

Brings the remaining peripherals online to complete the console:
- **Populate:**
  - Socket US2 (AVR128DB28 for MCU-S2).
  - Controller TRS jacks J3 and J4, pull-up resistor R26.
  - Audio output filter passives, RCA audio jack J8.
  - Optional: solder AD724 SOIC-16, Y3 crystal (3.579 MHz), and RCA composite jack J9.
  - Install JP_PWR4 shunt.
- **Verification:**
  - Connect game controllers to J3 and J4. Verify button inputs register in software during VBlank.
  - Verify PWM audio output on J8 produces clean sound without digital buzz.
  - If AD724 is populated, verify composite video output on J9.

---

## 6. Summary of PCB design requirements checklist

When designing the Tier H KiCad schematic and board layout, include:

1. **IC Sockets:** Use DIP sockets for all 19 counted motherboard ICs.
2. **Four Power Jumpers:** JP_PWR1 (Core/Video), JP_PWR2 (AVRs), JP_PWR3 (CPU/Memory), JP_PWR4 (Audio).
3. **Dedicated Tap Headers:** 6-pin SPI header and 8-pin timing header on standard 2.54 mm pitch.
4. **Pull-Up Resistors:** Ensure all active-low control pins (`/OE`, `/WE`, `/CE`, `RESB`, `RDY`) have dedicated pull-ups directly at the socket pins so unpopulated sockets do not leave lines floating.
5. **Ground Loops:** Place at least four through-hole ground test loops around the board perimeter for logic analyzer and oscilloscope ground leads.
