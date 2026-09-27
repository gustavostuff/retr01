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
Connect the probe from the test pin to Arduino pin 8, and connect common ground. Open the serial terminal at 115200 baud:

```bash
screen /dev/ttyUSB0 115200
```

Target readings:
- **Beam X HSYNC:** 15.746 kHz (+-50 Hz), period approximately 63.5 microseconds.
- **Beam Y VSYNC:** 60.098 Hz (+-0.5 Hz), period approximately 16.64 milliseconds.

---

### DOT clock (5.369318 MHz) measurement
Digital pin 5 (T1) connects to the Timer1 external clock input. The timer counts external edges directly up to approximately 6.5 MHz. Gating the counter for 100 milliseconds produces frequency readings in Hertz.

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

Target reading on the Si5351A output or canned oscillator: approximately 5369318 Hz.

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
- Power rails: Regulated +5.0 V and clean ground.
- Input drivers: 8-position DIP switch module with 10 k ohm pull-down resistors to ground. Closing a switch applies +5 V (logic high). Opening the switch pulls the pin to ground (logic low).
- Output indicators: Low-current LEDs with 1 k ohm series resistors to ground, or a digital multimeter set to DC voltage.

---

### Testing AT27C256R Color PROM
Verifies that programmed memory addresses output the expected packed R3G3B2 color bytes.

1. Ground CE# (pin 20) and OE# (pin 22).
2. Connect VCC (pin 28) to +5 V and GND (pin 14) to ground.
3. Tie upper address lines A[14:6] to ground.
4. Connect address lines A[5:0] (pins 10, 9, 8, 7, 6, 5) to DIP switches.
5. Connect LEDs or probe DQ[7:0] (pins 11, 12, 13, 15, 16, 17, 18, 19).
6. Set address switches to test indices:
   - Index 0 (all switches low): Output matches master palette color 0.
   - Index 1 (A0 high, rest low): Output matches master palette color 1.
7. Raising CE# or OE# to 5 V should place DQ[7:0] into high impedance (LEDs turn off).

---

### Testing 74HC573 transparent latch
Verifies transparent pass-through and edge latching behavior.

1. Connect VCC (pin 20) to +5 V and GND (pin 10) to ground.
2. Ground OE# (pin 1).
3. Connect inputs D[7:0] (pins 2 to 9) to DIP switches.
4. Connect outputs Q[7:0] (pins 19 down to 12) to LEDs.
5. Connect LE (latch enable, pin 11) to a separate control switch.
6. Test transparent mode: Set LE high. Toggle DIP switches on D[7:0]. Outputs Q[7:0] mirror inputs immediately.
7. Test latch mode: With a specific pattern on D[7:0], switch LE low. Change switches on D[7:0]. Outputs Q[7:0] remain frozen on the latched pattern.

---

### Testing 74HC74 dual flip-flop
Verifies toggle and frequency divider operation.

1. Connect VCC (pin 14) to +5 V and GND (pin 7) to ground.
2. Tie /CLR (pin 1) and /PRE (pin 4) to +5 V (inactive).
3. Tie /Q1 (pin 6) back into D1 (pin 2).
4. Connect Q1 (pin 5) to an indicator LED.
5. Pulse CLK1 (pin 3) from ground to +5 V using a debounced switch or slow Arduino pulse pin.
6. The LED on Q1 toggles state on each rising clock edge, confirming divide-by-2 operation.
7. Connecting Q1 to CLK2 and looping /Q2 to D2 verifies divide-by-4 operation on Q2.

---

### Testing ATF22V10 PLD logic and state counters
Verifies that compiled equations produce correct outputs.

1. Connect VCC (pin 24) to +5 V and GND (pin 12) to ground.
2. Connect inputs (pins 2 to 11) to DIP switches.
3. For registered designs (such as Beam X counter):
   - Connect a slow clock source to CLK (pin 1), such as an Arduino toggling a pin at 10 Hz, or a manual pushbutton.
   - Connect outputs to LEDs.
   - Verify that output count lines advance in binary sequence on clock edges and that reset pins clear the state.
4. For combinatorial decodes (such as Compositor layer priorities):
   - Set input switches to specific priority states.
   - Verify that index output lines switch according to the design truth table.

---

### Automated testing via Arduino GPIO rig
An Arduino can run automated truth table checks by driving chip inputs and reading outputs:

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

This automated vector testing verifies chip responses against expected values across the full address or pattern range in seconds.
