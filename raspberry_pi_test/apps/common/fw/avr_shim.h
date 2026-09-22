#ifndef R01_FW_AVR_SHIM_H
#define R01_FW_AVR_SHIM_H

/*
 * Thin IO shim: real AVR-Dx SFRs under __AVR__, host stubs otherwise
 * so soft_fexx / mailbox logic can compile on the desktop.
 */

#include <stdint.h>

#ifdef __AVR__
#include <avr/io.h>
#include <avr/interrupt.h>
#else

typedef struct {
    uint8_t DIR;
    uint8_t OUT;
    uint8_t IN;
} R01HostPort;

extern R01HostPort PORTA, PORTC, PORTD, PORTF;
extern volatile uint8_t VPORTA_IN;
extern volatile uint8_t VPORTC_IN;
extern volatile uint8_t VPORTD_IN;
extern volatile uint8_t VPORTF_IN;
extern volatile uint8_t PORTMUX_SPIROUTEA;
extern volatile uint8_t PORTMUX_TCAROUTEA;
extern volatile uint8_t CLKCTRL_OSCHFCTRLA;
extern volatile uint8_t CLKCTRL_MCLKCTRLA;
extern volatile uint8_t SPI1_CTRLA;
extern volatile uint8_t SPI1_CTRLB;
extern volatile uint8_t SPI1_DATA;
extern volatile uint8_t SPI1_INTFLAGS;
extern volatile uint8_t TCA0_SINGLE_CTRLA;
extern volatile uint8_t TCA0_SINGLE_CTRLB;
extern volatile uint16_t TCA0_SINGLE_PER;
extern volatile uint16_t TCA0_SINGLE_CMP1;
extern volatile uint8_t TWI0_CTRLA;
extern volatile uint8_t USART2_CTRLB;

#ifndef SPI_ENABLE_bm
#define SPI_ENABLE_bm 0x01u
#endif
#ifndef SPI_MASTER_bm
#define SPI_MASTER_bm 0x10u
#endif
#ifndef SPI_SSD_bm
#define SPI_SSD_bm 0x04u
#endif
#ifndef SPI_PRESC_DIV16_gc
#define SPI_PRESC_DIV16_gc 0x02u
#endif
#ifndef SPI_IF_bm
#define SPI_IF_bm 0x80u
#endif
#ifndef PORTMUX_SPI1_ALT1_gc
#define PORTMUX_SPI1_ALT1_gc 0x04u
#endif
#ifndef PORTMUX_TCA0_PORTF_gc
#define PORTMUX_TCA0_PORTF_gc 0x05u
#endif
#ifndef TCA_SINGLE_ENABLE_bm
#define TCA_SINGLE_ENABLE_bm 0x01u
#endif
#ifndef TCA_SINGLE_CLKSEL_DIV64_gc
#define TCA_SINGLE_CLKSEL_DIV64_gc 0x06u
#endif
#ifndef TCA_SINGLE_WGMODE_SINGLESLOPE_gc
#define TCA_SINGLE_WGMODE_SINGLESLOPE_gc 0x03u
#endif
#ifndef TCA_SINGLE_CMP1EN_bm
#define TCA_SINGLE_CMP1EN_bm 0x20u
#endif
#ifndef USART_ODME_bm
#define USART_ODME_bm 0x08u
#endif
#ifndef CLKCTRL_FREQSEL_24M_gc
#define CLKCTRL_FREQSEL_24M_gc 0x09u
#endif
#ifndef CLKCTRL_CLKSEL_OSCHF_gc
#define CLKCTRL_CLKSEL_OSCHF_gc 0x00u
#endif

#define VPORTA_IN VPORTA_IN
#define VPORTC_IN VPORTC_IN
#define VPORTD_IN VPORTD_IN
#define VPORTF_IN VPORTF_IN

#define sei() ((void)0)
#define cli() ((void)0)

#endif /* !__AVR__ */

#endif
