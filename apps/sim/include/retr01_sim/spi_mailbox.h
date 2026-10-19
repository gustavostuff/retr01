#ifndef retr01_SIM_SPI_MAILBOX_H
#define retr01_SIM_SPI_MAILBOX_H

struct R01sBoard;

/* Behavioral SPI mailbox (shared opcodes with hw/firmware/common/r01_spi_mailbox.h). */
void r01s_spi_mailbox_flush(struct R01sBoard *b);
void r01s_spi_mailbox_on_vblank(struct R01sBoard *b);

#endif
