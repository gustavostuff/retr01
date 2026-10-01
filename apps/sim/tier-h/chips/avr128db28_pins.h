#ifndef retr01_SIM_AVR128DB28_PINS_H
#define retr01_SIM_AVR128DB28_PINS_H

/*
 * AVR128DB28-I/SP physical SPDIP-28 numbers (docs/ic_behavior/AVR128DB28.md).
 * Sim entities use these indices so Tier H JSON matches KiCad DIP28 pads.
 */

#define R01S_AVR_PA0 22
#define R01S_AVR_PA1 23
#define R01S_AVR_PA2 24
#define R01S_AVR_PA3 25
#define R01S_AVR_PA4 26
#define R01S_AVR_PA5 27
#define R01S_AVR_PA6 28
#define R01S_AVR_PA7 1
#define R01S_AVR_PC0 2
#define R01S_AVR_PC1 3
#define R01S_AVR_PC2 4
#define R01S_AVR_PC3 5
#define R01S_AVR_VDDIO2 6
#define R01S_AVR_PD1 7
#define R01S_AVR_PD2 8
#define R01S_AVR_PD3 9
#define R01S_AVR_PD4 10
#define R01S_AVR_PD5 11
#define R01S_AVR_PD6 12
#define R01S_AVR_PD7 13
#define R01S_AVR_AVDD 14
#define R01S_AVR_GND1 15
#define R01S_AVR_PF0 16
#define R01S_AVR_PF1 17
#define R01S_AVR_PF6 18
#define R01S_AVR_UPDI 19
#define R01S_AVR_VDD 20
#define R01S_AVR_GND2 21

/* Sim-only (no PCB pad; omitted from Skidl export). */
#define R01S_AVR_SIM_CLK 127
#define R01S_AVR_SIM_RUN 128

#endif
