# Bench testing guide for Retr01 ICs

Methods for measuring frequencies and validating individual IC pin behavior on solderless breadboards before assembling complete subsystem tiers.

---

## 1. Frequency and pulse measurement via Arduino

When laboratory oscilloscopes are unavailable, an ATmega328P Arduino (Uno or Nano) connected via USB to a Linux workstation functions as a hardware frequency and period counter.

### HSYNC (15.74 kHz) and VSYNC (60 Hz) measurement

The ATmega328P Timer1 Input Capture Unit on digital pin 8 measures pulse intervals with 62.5 nanosecond resolution at 16 MHz.

#### Arduino Input Capture sketch

```cpp
// Frequency and period counter on Arduino Pin D8 (ICP1)
volatile uint16_t t_start = 0;
volatile uint16_t t_period = 0;
volatile bool new_sample = false;

ISR(TIMER1_CAPT_vect) {
  uint16_t now = ICR1;
  t_period = now - t_start;
  t_start = now;
  new_sample = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(8, INPUT);

  TCCR1A = 0;
  TCCR1B = bit(CS10) | bit(ICES1);
  TIMSK1 = bit(ICIE1);
}

void loop() {
  if (new_sample) {
    new_sample = false;
    float freq = 16000000.0 / t_period;
    float period_us = t_period * 0.0625;
    Serial.print("Freq: ");
    Serial.print(freq, 2);
    Serial.print(" Hz | Period: ");
    Serial.print(period_us, 2);
    Serial.println(" us");
    delay(250);
  }
}
```

#### Terminal monitoring on Linux

The probe from the test pin goes to Arduino pin 8, with a shared ground. Serial is 115200 baud:

```bash
screen /dev/ttyUSB0 115200
```

Target readings:
- **Beam X HSYNC:** 15.746 kHz (+-50 Hz), period approximately 63.5 microseconds.
- **Beam Y VSYNC:** 60.098 Hz (+-0.5 Hz), period approximately 16.64 milliseconds.

---

### DOT clock (5.369318 MHz) measurement

Digital pin 5 (T1) connects to the Timer1 external clock input. The timer counts external edges directly up to approximately 6.5 MHz. Gating the counter for 100 milliseconds produces a frequency reading.

#### Arduino clock counter sketch

```cpp
// High-speed pulse counter on Arduino Pin D5 (T1)
void setup() {
  Serial.begin(115200);
  pinMode(5, INPUT);
}

void loop() {
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TCCR1B = bit(CS12) | bit(CS11) | bit(CS10);

  delay(100);

  TCCR1B = 0;
  uint16_t counts = TCNT1;
  uint32_t freq = (uint32_t)counts * 10;

  Serial.print("DOT Clock: ");
  Serial.print(freq);
  Serial.println(" Hz");
  delay(300);
}
```

Target reading on the 74HC74 Q2 output (DOT clock): approximately 5369318 Hz.

---

### Logic analyzer alternative (Linux pulseview)

An 8-channel 24 MHz USB logic analyzer connects directly to Linux via the sigrok toolchain:

```bash
sudo pacman -S pulseview sigrok-cli
```

Connecting analyzer channels to DOT, HSYNC, CSYNC, and VSYNC provides visual multi-channel traces and automatic duty cycle and frequency decodes.

---

## 2. Testing individual IC pin truth tables

Testing chips individually with static logic levels confirms proper programming and functionality before placing them on the active video bus.

### General bench test setup

This setup applies to all static and combinatorial IC tests:

**Power**
- Regulated +5.0 V supply (clean, well decoupled)
- Common ground between all components

**Input drivers**
- 8-position DIP switch module with 10 kohm pull-down resistors to ground
- Closing a switch applies +5 V (logic high)
- Opening the switch pulls the pin to ground (logic low)

**Output indicators**
- Low-current LEDs (red or green preferred) with 1 kohm series resistors to ground
- Wiring: IC pin --- 1k resistor --- LED anode --- LED cathode --- GND
- Alternative: digital multimeter set to DC voltage
- When a pin is high (+5 V) the LED glows. When low (0 V) it goes dark

**Clock and control signals**
- Slow clock source for sequential tests: Arduino toggling a pin at ~10 Hz, or a manual debounced pushbutton
- Manual switches for control signals (CE#, OE#, LE, CLK, etc.) held high or low via jumpers

---

### Testing AT27C256R Color PROM

This PROM stores 64 packed R3G3B2 color bytes. The software source of truth (`apps/common/r01_kit_palette.c`) holds the full 8-bit RGB triples. The burn tool packs those triples into single bytes for the hardware PROM.

When CE# and OE# are both low, DQ[7:0] output the byte stored at the selected address. Higher address lines A[14:6] are grounded to keep access within the first 64 bytes.

**Setup**

1. Ground CE# (pin 20) and OE# (pin 22).
2. Connect VCC (pin 28) to +5 V and GND (pin 14) to ground.
3. Tie upper address lines A[14:6] to ground.
4. Connect address lines A[5:0] (pins 10, 9, 8, 7, 6, 5) to DIP switches.
5. Connect LEDs or probe DQ[7:0] (pins 11, 12, 13, 15, 16, 17, 18, 19) to LEDs via 1 kohm series resistors, or to a meter set to DC voltage.

**Test procedure**

1. Set address switches to index 0 (all switches open / low).
2. Read the output: must match the packed R3G3B2 value of palette color 0 (black = 0x00).
3. Set address switches to index 1 (A0 closed / high, rest open / low).
4. Read the output: must match the packed R3G3B2 value of palette color 1.
5. Check a few more distinctive entries (e.g., index 16, 32, 48, 63) against the expected values in `docs/general/palette/global_system_palette.md`.
6. Raise CE# or OE# to +5 V. The outputs must go high-impedance (LEDs turn off, meter reads very high resistance or open circuit).
7. Lower CE# and OE# back to ground. Outputs must drive again.

If all checks pass, the PROM is programmed correctly.

---

### Testing 74HC573 transparent latch

A 74HC573 is a set of 8 memory cells with two modes of operation:

**Transparent mode (LE high)**
- The outputs Q[7:0] follow the inputs D[7:0] in real time, as if directly wired.
- Any change on the D pins appears immediately on the Q pins (with only a few nanoseconds propagation delay).

**Latched mode (LE low)**
- The outputs freeze on the value present on the D pins at the instant LE transitioned from high to low.
- Further changes to the D pins have no effect. The Q pins hold the latched value.
- Only when LE returns high do the Q pins wake up and follow the D pins again.

The OE# (Output Enable, active-low) pin is a master switch. When OE# is low, outputs drive normally. When OE# is high, outputs go high-impedance.

**Setup**

1. Power
   - Pin 20 (VCC) to +5 V
   - Pin 10 (GND) to ground

2. Enable outputs
   - Pin 1 (OE#) to ground

3. Inputs
   - Pins 2-9 (D[0:7]) to DIP switches

4. Outputs
   - Pins 19 down to 12 (Q[0:7]) to LEDs with 1 kohm series resistors

5. Control
   - Pin 11 (LE) to a manual switch that can toggle between +5 V and ground

**Test procedure**

Test 1 - Transparent mode:
1. Set LE high (+5 V).
2. Toggle the DIP switches on D[7:0] to various patterns.
3. Verify the LEDs on Q[7:0] follow immediately and perfectly, matching each switch change.

Test 2 - Latched mode:
1. While LE is high, set a known pattern on the DIP switches (e.g., binary 10101010). Confirm the LEDs show the same pattern.
2. Switch LE low (to ground).
3. Change the DIP switches to a different pattern (e.g., binary 01010101).
4. Verify the LEDs remain frozen on the old pattern (10101010). No switch changes should affect them.
5. Switch LE back high (+5 V).
6. Verify the LEDs instantly jump to match the current DIP switch pattern (01010101).

Test 3 - Output disable:
1. With LE high and a pattern on D[7:0], raise OE# to +5 V.
2. Outputs must go high-impedance (LEDs turn off).
3. Lower OE# back to ground.
4. Outputs must drive again and show the pattern on D[7:0].

If all three behaviors work correctly, the chip is good.

---

### Testing 74HC74 dual flip-flop

A 74HC74 contains two independent D flip-flops, each with:
- D (data input)
- CLK (clock input)
- /PRE (preset, active-low) to force the Q output high
- /CLR (clear, active-low) to force the Q output low
- Q and /Q outputs

When /CLR and /PRE are both inactive (high), a rising edge on CLK latches the D input into Q and inverts it into /Q. When D and /Q are looped back together, the flip-flop becomes a divide-by-2 frequency divider.

**Setup**

1. Power
   - Pin 14 (VCC) to +5 V
   - Pin 7 (GND) to ground

2. Control (make inactive)
   - Pin 1 (/CLR) to +5 V
   - Pin 4 (/PRE) to +5 V

3. Flip-flop 1 configuration
   - Pin 6 (/Q1) looped back to pin 2 (D1)
   - Pin 5 (Q1) to an LED with 1 kohm series resistor

4. Flip-flop 2 configuration (for divide-by-4 test)
   - Pin 5 (Q1) connected to pin 9 (CLK2)
   - Pin 11 (/Q2) looped back to pin 12 (D2)
   - Pin 9 (Q2) to an LED with 1 kohm series resistor

5. Clock input
   - Pin 3 (CLK1) driven by a slow pulse source (Arduino pin toggling at ~10 Hz, or a debounced manual switch)

**Test procedure**

Test 1 - Divide-by-2 on flip-flop 1:
1. Pulse CLK1 from 0 V to +5 V, back to 0 V (one complete clock cycle).
2. Observe the LED on Q1. It must toggle state on each rising edge of CLK1.
3. After two pulses the LED is back to its original state (one complete 50% duty cycle).
4. If the LED toggles smoothly on each pulse, divide-by-2 is confirmed.

Test 2 - Divide-by-4 via cascaded flip-flops:
1. With the same slow clock applied to CLK1, observe the LED on Q2.
2. It must toggle at half the rate of Q1 (once every two toggles of Q1).
3. After four clock pulses at CLK1, Q2 is back to its original state (one complete divide-by-4 cycle).

Test 3 - Asynchronous reset:
1. With CLK1 idle and Q1 high (LED on), pulse /CLR from +5 V to ground and back.
2. The LED must turn off (Q1 forces low) immediately, without waiting for a clock edge.
3. Return /CLR to +5 V. Behavior must be normal again (no forced low).

If all behaviors work, the flip-flops are good.

---

### Testing ATF22V10 PLD logic and state counters

A programmable logic device (PLD) executes Boolean equations on its inputs to produce outputs. It can be purely combinatorial (outputs depend only on current inputs) or registered (outputs depend on inputs and previous clock states).

The ATF22V10 in Retr01 is used for tasks like beam counters (registered) and compositor layer priorities (combinatorial).

**Setup**

1. Power
   - Pin 24 (VCC) to +5 V
   - Pin 12 (GND) to ground

2. Inputs
   - Pins 2-11 (input pins) to DIP switches

3. Outputs
   - PLD output pins to LEDs with 1 kohm series resistors

4. Clock (if registered)
   - CLK pin to a slow clock source (Arduino toggling at ~10 Hz, or manual pushbutton)

5. Reset (if registered)
   - Any reset input pins to a manual switch (high = inactive, ground = active) or tied inactive

**Test procedure for combinatorial designs (e.g., priority decoder)**

1. Set input switches to a known state that should produce a specific output pattern.
2. Read the output LEDs. They must match the expected result for that input.
3. Change inputs to other known states and verify outputs match the truth table (defined in the design .pld file and documentation).
4. Test edge cases: all inputs low, all inputs high, alternating patterns.

**Test procedure for registered designs (e.g., beam counter)**

1. Activate reset (drive reset pin low or ground it, depending on the design). Verify all output LEDs are in their reset state (usually all off).
2. Release reset (return reset pin to high or leave it ungrounded).
3. Apply a slow clock pulse (one rising edge). Check that the counter output advances by one in binary.
4. Apply more clock pulses and verify the output counts upward (0, 1, 2, 3, ...).
5. When the count reaches its maximum (e.g., 255 for an 8-bit counter), the next pulse should wrap to 0 (or follow the design's overflow behavior).
6. Verify that activating reset again returns the output to its initial state.

If all checks pass, the PLD is programmed correctly.

---

### Automated testing via Arduino GPIO rig

An Arduino can run automated truth table checks by driving chip inputs through a range of test values and recording the outputs. This is the exhaustive, logged version of manual DIP-switch and LED testing.

#### What the sketch does

- Port D (pins 0-7) is configured as outputs that drive the IC's input pins (address lines, data inputs, control signals, etc.).
- Port B (pins 8-13) is configured as inputs that read the IC's output pins.
- The loop applies every value from 0 to 63 (covering 6 bits, which covers common test cases: lower address bits of the PROM, small combinatorial PLD decodes, etc.).
- For each test value the sketch:
  1. Writes the pattern to Port D
  2. Waits 5 microseconds for the chip to settle
  3. Reads the lower 6 bits of Port B
  4. Prints "Applied: X | Read: Y" over serial
- A host script or manual inspection compares each printed pair against the expected truth table. If every applied vector produces the correct result, the IC passes.

#### Limitations

- This sketch only exercises combinatorial or static-latched behavior. Sequential circuits (counters, registered PLDs, flip-flops) need explicit clock pulses and longer vector sequences that must be added to the sketch.
- Pins 0 and 1 are also the hardware UART. On a real Uno/Nano, avoid driving them while using Serial, or move the stimulus to a different port pair.
- There is no automatic pass/fail check. The serial log must be verified by hand or by a later script (e.g., a Python wrapper that compares against a reference file).

#### The sketch

```cpp
// Automated 8-bit bus stimulus and readback
void setup() {
  Serial.begin(115200);

  // Set Port D (pins 0-7) as stimulus outputs
  DDRD = 0xFF;

  // Set Port B (pins 8-13) as test inputs
  DDRB = 0x00;
  PORTB = 0x00; // No pullups
}

void loop() {
  for (uint8_t test_val = 0; test_val < 64; test_val++) {
    PORTD = test_val;
    delayMicroseconds(5);
    uint8_t result = PINB & 0x3F;

    Serial.print("Applied: ");
    Serial.print(test_val);
    Serial.print(" | Read: ");
    Serial.println(result);
    delay(200);
  }
}
```

#### Practical wiring notes

- Arduino Port D pins (0-7) connect to the IC inputs under test (address, data, control, and similar).
- IC outputs connect to Arduino Port B pins 8-13.
- Arduino and IC share a common ground.
- The IC is powered from a clean +5 V regulated supply.
- Pins that stay in a fixed state (CE#, OE#, LE, CLK, and similar) are jumpered to +5 V (high) or ground (low). Extra sketch-driven pins use more Arduino DDR/PORT bits.

This automated rig is simply the exhaustive, logged version of the manual DIP-switch and LED tests described in the sections above.

---

## 3. Solderless breadboard layout rules and high-speed risks (Tiers A, B, and C)

Solderless breadboards introduce parasitic resistance, capacitance, and inductance that do not exist on a finished printed circuit board. At DC and low switching rates these parasitics are negligible. At Retr01 master clock frequencies (21.477 MHz) and video pixel rates (5.369 MHz), breadboard parasitics directly cause oscillator failure, false clock triggering, color smearing, and memory corruption.

### Electrical parasitics baseline of breadboard strips

Standard solderless breadboards exhibit physical limitations across their internal spring contact clips:
- **Contact row capacitance:** Each 5-hole tie-point strip presents 2 to 5 pF of capacitance to adjacent rows and to the metal backing plate underneath.
- **Contact resistance:** Spring clips introduce 10 to 50 mohm of series resistance per connection, increasing when dirty or fatigued.
- **Power rail inductance:** Long internal power bus strips and jumper wires introduce 10 to 30 nH of inductance per connection strip.
- **Ground loop area:** Without a continuous copper ground plane, return currents follow circuitous jumper paths, forming large magnetic loop antennas that pick up and radiate switching noise.

---

### Risk 1: Pierce oscillator detuning and startup failure (Tier A)

Retr01 generates the master color clock using a discrete Pierce crystal oscillator circuit (74HCU04 inverter, Y2 21.47727 MHz crystal, 1 Mohm feedback resistor, and two load capacitors C1/C2).

**Failure mechanism:**
- The 2 to 5 pF parasitic capacitance of adjacent breadboard tie-point rows adds directly in parallel with the external crystal load capacitors (nominal 18 to 22 pF).
- This extra parasitic capacitance alters the total capacitive load seen by the crystal, pulling the resonant frequency off-target or shifting the phase angle across the 74HCU04 inverter.
- If total loop gain drops below unity or phase shift departs from 180 degrees, the oscillator fails to start completely or produces unstable, intermittent oscillation.

**Mitigation rules:**
- Y2, the 74HCU04 inverter (pins 1 and 2), the 1 Mohm feedback resistor, and C1/C2 reside on immediately adjacent breadboard rows.
- Component leads are trimmed as short as practical (under 10 mm) before insertion into the breadboard.
- The grounded ends of C1, C2, and 74HCU04 pin 7 (GND) connect to a single common tie-point row before connecting to the main ground rail.
- In high-capacitance breadboards, C1 and C2 are downsized slightly (such as 15 or 18 pF) to compensate for breadboard contact capacitance.

---

### Risk 2: ATF22V10 ground bounce and false clocking (Tiers A and B)

The Beam X, Beam Y, and Compositor functions are implemented in ATF22V10C programmable logic devices running at 5.369 MHz.

**Failure mechanism:**
- High-speed CMOS PLDs (-7 ns and -10 ns speed grades) feature output transition times under 3 ns.
- When multiple outputs switch simultaneously (such as the 6-bit index bus or multiple counter MSBs), the rapid change in transient current (dI/dt) across the parasitic inductance of the breadboard ground rail induces a sharp voltage spike:
  `V_bounce = L_rail * (dI / dt)`
- This ground bounce momentarily shifts the internal chip 0 V reference relative to external logic levels. A momentary ground bounce spike exceeding 0.8 V on an active-low reset or clock input can falsely clock internal state registers or cause Beam counters to skip lines.

**Mitigation rules:**
- A 0.1 uF low-ESR ceramic capacitor mounts directly across Pin 24 (VCC) and Pin 12 (GND) of each ATF22V10, straddling the IC package with shortest possible leads.
- Power and ground distribution rails bridge at both ends of each breadboard slab with short 22 AWG solid jumpers to form a closed low-impedance loop rather than a single open stub.
- A 10 uF to 47 uF electrolytic or tantalum bulk decoupling capacitor connects at the power entry point of each breadboard.

---

### Risk 3: R-2R DAC bandwidth limitation and RGBS signal degradation (Tiers A and B)

The color PROM drives a discrete R-2R resistor ladder DAC to generate analog Red, Green, and Blue voltages for the RGBS video connector.

**Failure mechanism:**
- Solderless breadboard row-to-row capacitance (2 to 5 pF per row) forms an unintentional low-pass RC filter with the R-2R ladder resistors (1 kohm and 2 kohm network).
- This low-pass filtering limits the analog bandwidth of the DAC, rounding off the sharp transitions of the 5.369 MHz dot clock pixels. On an RGBS monitor, this manifests as horizontal color smear, blurred character edges, and chromatic bleeding between adjacent pixels.
- In addition, running video signals (R, G, B, CSYNC) through long loose jumper wires without dedicated ground returns introduces inductive ringing and 60 Hz hum, resulting in horizontal sync jitter or image tearing.

**Mitigation rules:**
- R-2R ladder resistors sit immediately adjacent to the AT27C256R PROM data output pins (DQ[7:0]).
- Resistor leads are trimmed short without daisy-chaining jumpers between ladder nodes.
- A dedicated ground wire runs twisted alongside each video output line (R, G, B, and CSYNC) directly to the video output connector pins.
- Cable runs from the breadboard to the monitor or capture card stay under 1 meter during lab testing.

---

### Risk 4: Tier C field SRAM write pulse glitching and bus contention (Tier C)

Tier C introduces MCU-S1, a 74HC573 transparent latch, and field SRAM (e.g. AS6C62256) where MCU-S1 fills sprite patterns during VBlank and hardware video logic reads SRAM during active scan.

**Failure mechanism:**
- Solderless breadboards feature high capacitive coupling between adjacent jumper wires running in parallel.
- A fast edge on an adjacent clock or counter line can capacitively inject a narrow negative spike (runt pulse) into the SRAM active-low write enable (/WE) or chip enable (/CE) line.
- If /WE glitches low while the address bus is in transition, the SRAM executes an unintentional write cycle, corrupting random sprite data.
- Multiplexed address and data lines (AD[7:0]) driven alternately by MCU-S1 and the video multiplexers can suffer bus contention if latch enable (LE) and output enable (/OE) signals overlap on breadboard lines with asymmetric propagation delays.

**Mitigation rules:**
- The SRAM /WE control wire runs physically separated from high-frequency clock signals (DOT, 21.48 MHz, and counter toggle lines).
- A 4.7 kohm to 10 kohm pull-up resistor connects directly at the SRAM /WE pin (pin 27 on DIP-28) to +5 V to hold the line inactive during bus transitions.
- MCU-S1 firmware maintains explicit non-overlapping guard intervals between releasing the bus and asserting /WE or /OE.
- Local 0.1 uF decoupling capacitors mount directly at the VCC pins of the SRAM and 74HC573 latch.

---

## 4. LED testing reference

All ICs under test (AT27C256R, 74HC573, 74HC74, ATF22V10, etc.) use 5 V logic.

- A high output is approximately +5 V
- A low output is approximately 0 V

### LED color choice

Any common LED works:
- **Red or green (preferred):** forward voltage ~1.8-2.2 V, bright with standard 1 kohm resistor
- **Yellow:** also fine, similar voltage to red
- **Blue or white:** forward voltage ~2.8-3.3 V, slightly dimmer with same resistor, but usable

Low-current LEDs (2-5 mA) are ideal. They are clearly visible and put minimal load on the IC pin.

### Correct wiring

LED lights when the pin is HIGH:

```
IC output pin --- 1k ohm resistor --- LED anode (+) --- LED cathode (-) --- GND
```

The resistor must be in series with the LED. Without it the LED burns out almost instantly and risks damaging the IC pin.

1 kohm is the recommended value and is safely conservative:
- Approximate current for red LED: (5 V - 2 V) / 1000 ohm = 3 mA
- Approximate current for green LED: (5 V - 2 V) / 1000 ohm = 3 mA

### Incorrect wiring to avoid

Do not connect both an LED and a separate pull-down resistor independently to ground from the same pin. This either shorts the LED or leaves it without a proper current path. The resistor and LED must be in series between the pin and ground.

---

## 5. Wiring checklist for breadboard tests

Before applying power to any test setup:

- [ ] All power (VCC) connections are correct (to +5 V only).
- [ ] All ground (GND) connections are made and are common across all components.
- [ ] Power and ground rails are bridged at both ends of each breadboard slab.
- [ ] 0.1 uF ceramic bypass capacitors are installed across VCC and GND directly at each IC package.
- [ ] Bulk 10 uF to 47 uF capacitor is installed at the main power entry point.
- [ ] Crystal oscillator components (Y2, 74HCU04, Rf, C1/C2) have trimmed leads under 10 mm and sit on adjacent tie-point rows.
- [ ] LED series resistors are 1 kohm and are in series between each output pin and the LED.
- [ ] DIP switch pull-down resistors are 10 kohm and pull to ground.
- [ ] Clock signals are connected to the correct pins and are driven by a verified source.
- [ ] Control signals (CE#, OE#, LE, /CLR, /PRE, etc.) are wired to ground or +5 V as required by the test, or are driven by a switch.
- [ ] SRAM /WE line has a dedicated 4.7 kohm to 10 kohm pull-up to +5 V and is routed away from clock lines (Tier C).
- [ ] Video lines (R, G, B, CSYNC) have twisted ground returns running to the display connector.
- [ ] No two signal pins are shorted together (check for accidental wire touching).
- [ ] Power supply is turned off while wiring, and turned on only after all connections are verified.

---

## 6. Common troubleshooting

**LEDs don't light at all**
- Verify +5 V is actually present at VCC pins (use a meter).
- Confirm ground is common and solid.
- LED polarity: longer leg (anode) goes to the resistor. Shorter leg (cathode) goes to ground.
- If using the automated Arduino rig, check serial output is present and reasonable values are being applied.

**LEDs light but flicker or dim unexpectedly**
- Verify power supply is clean and well-decoupled (add capacitors near IC VCC pins if not already done).
- Check for accidental shorts or touching wires.
- Confirm DIP switch contacts are clean and making solid connections.

**PROM outputs don't match expected values**
- Verify CE# and OE# are both low (to ground).
- Confirm address inputs A[5:0] match the intended index (check DIP switch state).
- Check that the PROM was burned with the correct image (use the automated Arduino test to log all 64 values and compare against the reference palette).

**Latch outputs don't freeze when LE goes low**
- Ensure LE pin is actually driven to ground (use a meter to confirm 0 V).
- Verify the chip is powered and the 5 V supply is stable.

**PLD outputs don't follow expected equations**
- Review the .pld source file and equations to confirm what the outputs should be for the current inputs.
- Verify all inputs are wired and are at the intended logic level (use a meter or LED on a control line to confirm).
- If registered, ensure the clock is actually pulsing (use a meter or LED on the clock line to confirm).

---

## 7. Troubleshooting the automated Arduino rig

**No serial output**
- Verify the Arduino is connected and recognized by the operating system (check /dev/ttyUSB* or dmesg).
- Confirm the baud rate in the sketch and terminal match (115200).
- Try reflashing the Arduino with the sketch.

**Outputs always read as zero or always read as all ones (0x3F)**
- Check that Port B pins (8-13) are actually wired to the IC output pins.
- Verify the IC is powered and the 5 V supply is present.
- Use a meter to confirm the output pins are at the expected logic level.

**Results don't match the expected truth table**
- Double-check the wiring of Port D outputs to the IC input pins (order matters).
- Verify any fixed control signals (CE#, OE#, LE, etc.) are at the correct logic level.
- If testing a PROM, confirm the burn image matches the reference palette.
- If testing a PLD, review the equations in the .pld source file against the applied inputs and expected outputs.

