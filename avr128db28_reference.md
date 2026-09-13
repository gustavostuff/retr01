# AVR128DB28 THT Reference Guide

This document serves as a comprehensive, verified hardware reference for the **Microchip AVR128DB28** microcontroller in the 28-pin SPDIP (.300" Skinny Plastic Dual In-Line) package, based on the official Microchip DS40002247 datasheet. It is structured to support hardware design and software simulation in `lib/netlist_sim/`.

---

## 1. Core Specifications & Memory

*   **CPU:** 8-bit AVR CPU with hardware multiplier
*   **Maximum Clock Speed:** 24 MHz (standard operating range across 1.8V - 5.5V; up to 32 MHz achievable in overclocked configurations)
*   **Operating Voltage:** 1.8V – 5.5V
*   **Flash Memory:** 128 KB
*   **SRAM:** 16 KB
*   **EEPROM:** 512 Bytes
*   **I/O Pins Exposed in 28-Pin Package:** 22 GPIOs (PA0–PA7, PC0–PC3, PD1–PD7, PF0–PF1, PF6)

---

## 2. Verified 28-Pin SPDIP Pinout

> [!WARNING]
> **Critical Differences from Generic AVRs / Larger Packages:**
> 1. **No PD0:** In the 28-pin package, **PD0 is omitted**. Port D only contains pins **PD1 through PD7**. It cannot be used as a full contiguous 8-bit parallel bus without pairing with another port bit (e.g. PC0 or PA7).
> 2. **DAC0 Output:** DAC0 is routed to **PD6 (Pin 12)**, not PA6.
> 3. **High-Frequency Crystal (XTALHF):** Located on **PA0 (Pin 22)** and **PA1 (Pin 23)**.
> 4. **Low-Frequency Crystal (XTAL32K):** Located on **PF0 (Pin 16)** and **PF1 (Pin 17)**.
> 5. **UPDI & RESET are Separate Pins:** Pin 19 is dedicated **UPDI**; Pin 18 is **PF6 / RESET**.
> 6. **Multi-Voltage I/O (MVIO):** Pin 6 is **VDDIO2**, allowing Port C (PC0–PC3) to run on an independent voltage level (e.g., 3.3V while the core runs at 5V).

### Pin Assignment Table (1 to 28)

| Pin # | Name | Direction / Type | Primary & Multiplexed Functions | Notes |
|:-----:|:----:|:----------------:|:--------------------------------|:------|
| **1** | **PA7** | I/O (Digital/Analog) | CLKOUT, SPI0 SS alt, TCA0 WO3 alt, CCL LUT0 OUT | General I/O |
| **2** | **PC0** | I/O (MVIO / Port C) | USART1 TxD, SPI1 MOSI, TCA0 WO0 alt, CCL IN0 | Powered by VDDIO2 |
| **3** | **PC1** | I/O (MVIO / Port C) | USART1 RxD, SPI1 MISO, TCA0 WO1 alt, CCL IN1 | Powered by VDDIO2 |
| **4** | **PC2** | I/O (MVIO / Port C) | USART1 XCK, SPI1 SCK, TWI0 SCL alt, EVOUTC | Powered by VDDIO2 |
| **5** | **PC3** | I/O (MVIO / Port C) | USART1 XDIR, SPI1 SS, TWI0 SDA alt | Powered by VDDIO2 |
| **6** | **VDDIO2** | Power (Supply) | Dedicated MVIO supply for Port C (1.62V - 5.5V) | Decouple with 100nF to GND |
| **7** | **PD1** | I/O (Digital/Analog) | AIN1, OP0 INP, ZCD0 IN | *Notice: PD0 does not exist in 28-pin!* |
| **8** | **PD2** | I/O (Digital/Analog) | AIN2, OP0 OUT, TCA0 WO2 alt, EVOUTD | Port D starts at PD1 |
| **9** | **PD3** | I/O (Digital/Analog) | AIN3, OP0 INN, TCA0 WO3 alt | Analog/Digital |
| **10** | **PD4** | I/O (Digital/Analog) | AIN4, OP1 INP, TCA0 WO4 alt | Analog/Digital |
| **11** | **PD5** | I/O (Digital/Analog) | AIN5, OP1 OUT, TCA0 WO5 alt | Analog/Digital |
| **12** | **PD6** | I/O (Digital/Analog) | **DAC0 OUT**, AIN6, OP1 INN alt | **Hardware 10-bit DAC output!** |
| **13** | **PD7** | I/O (Digital/Analog) | AIN7, VREFA (External Ref), OP1 INN, EVOUTD alt | Analog reference / GPIO |
| **14** | **AVDD** | Power (Supply) | Analog VDD (Internally connected to VDD) | Decouple close to pin |
| **15** | **AGND / GND** | Power (Ground) | Analog Ground (Internally connected to GND) | Ground plane |
| **16** | **PF0** | I/O (Digital/Analog) | **XTAL32K1** (32.768 kHz), USART2 TxD | RTC / low-power oscillator |
| **17** | **PF1** | I/O (Digital/Analog) | **XTAL32K2** (32.768 kHz), USART2 RxD | RTC / low-power oscillator |
| **18** | **PF6** | Input | **RESET** (External Reset), GPIO In | Active low reset with internal pullup |
| **19** | **UPDI** | I/O | **UPDI** (Unified Program and Debug Interface) | Single-wire programming interface |
| **20** | **VDD** | Power (Supply) | Main digital power supply (1.8V - 5.5V) | Decouple with 100nF close to pin |
| **21** | **GND** | Power (Ground) | Main digital ground | System Ground |
| **22** | **PA0** | I/O (Digital/Analog) | **XTALHF1** (Ext HF crystal), EXTCLK, USART0 TxD | High-Speed Clock Input |
| **23** | **PA1** | I/O (Digital/Analog) | **XTALHF2** (Ext HF crystal), USART0 RxD | High-Speed Clock Output |
| **24** | **PA2** | I/O (Digital/Analog) | USART0 XCK, TWI0 SCL, EVOUTA, TCA0 WO2 | Analog / Digital |
| **25** | **PA3** | I/O (Digital/Analog) | USART0 XDIR, TWI0 SDA, TCA0 WO3 | Analog / Digital |
| **26** | **PA4** | I/O (Digital/Analog) | SPI0 MOSI, USART0 TxD alt, TCA0 WO4, TCD0 WOA | High-speed SPI master bus |
| **27** | **PA5** | I/O (Digital/Analog) | SPI0 MISO, USART0 RxD alt, TCA0 WO5, TCD0 WOB | High-speed SPI master bus |
| **28** | **PA6** | I/O (Digital/Analog) | SPI0 SCK, USART0 XCK alt, TCD0 WOC | High-speed SPI master bus |

---

## 3. Package Dimensions (for `lib/netlist_sim` & Breadboarding)

*   **Package Type:** 28-Lead Skinny Plastic Dual In-Line Package (SPDIP)
*   **Body Width (E1):** Nominal 0.285" (~7.24 mm), 300 mil class
*   **Row-to-Row Spacing (eB):** 0.300" nominal (7.62 mm)
*   **Overall Length (D):** Nominal 1.365" (~34.67 mm)
*   **Pitch (e):** Standard 0.100" (2.54 mm)
*   **Sim Model Constants:**
    *   `dip_pins`: 28
    *   `len_mm`: 35
    *   `wid_mm`: 8

---

## 4. Key Considerations for Retr01 Hardware Simulation

1. **Parallel Bus Workaround:** Since Port D is only 7 pins (PD1–PD7) on the 28-pin package, driving an 8-bit parallel color DAC requires either:
   - Using 7 bits of color (e.g. 3-3-1 RGB or 2-3-2) on PD1–PD7.
   - Borrowing one pin (e.g. PA7 or PC0) to complete 8 bits.
   - Or upgrading to the 32-pin/48-pin package if a true monolithic 8-bit single-cycle port write (`PORTx.OUT = v;`) is mandatory.
2. **Audio Output:** DAC0 output is on **PD6**. Internal Op-Amp OP0 or OP1 can be chained to buffer DAC0 directly out to line/speaker.
3. **Inter-MCU Link:** Port C (PC0–PC3) is ideal for MSPI or synchronous serial between MCU1 and MCU2, with native MVIO support if one runs at 3.3V and the other at 5V.
