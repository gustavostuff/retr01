#ifndef retr01_EMU_MACHINE_H
#define retr01_EMU_MACHINE_H

#include "retr01_emu/cart.h"
#include "retr01_emu/cpu.h"
#include "retr01_emu/io.h"
#include "retr01_emu/play.h"
#include "retr01_emu/types.h"
#include "retr01_emu/video.h"

#include <stdint.h>

typedef struct R01eMachine {
    R01eCart cart;
    R01eCpu cpu;
    R01eIo io;
    R01eVideo video;
    R01ePlay play;
    uint8_t ram[0x8000];
    uint64_t dot_num;
    uint64_t dot_den;
    uint64_t dot_acc;
    int nmi_pending;
    /* 1 while spinning on $FE01 with VBlank clear (not counted as busy work). */
    int prof_waiting;
    /* Last completed CRT frame: busy CPU cycles (excludes PPUSTATUS wait) in active vs VBlank. */
    uint64_t prof_last_active;
    uint64_t prof_last_vblank;
    uint64_t prof_acc_active;
    uint64_t prof_acc_vblank;
    /* Full frame cycle totals (including wait) -- debug / sanity only. */
    uint64_t prof_acc_idle;
    uint64_t prof_last_idle;

    uint8_t cart_save[R01E_CARTEE_BYTES];
    uint8_t machine_eeprom[R01E_MEEPROM_BYTES];
} R01eMachine;

int r01e_machine_init(R01eMachine *m, const char *cart_path, char *err, size_t err_cap);
void r01e_machine_shutdown(R01eMachine *m);
void r01e_machine_reset(R01eMachine *m);

uint8_t r01e_mem_read(R01eMachine *m, uint16_t addr);
void r01e_mem_write(R01eMachine *m, uint16_t addr, uint8_t v);

int r01e_machine_frame(R01eMachine *m);
int r01e_machine_step_insn(R01eMachine *m);

void r01e_machine_set_pad(R01eMachine *m, int player, uint8_t bits);

#endif
