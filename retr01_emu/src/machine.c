#include "retr01_emu/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Run Phase 1 PRG until pal+MAP stream has filled VRAM slot 0 (or give up). */
static void catchup_prg_boot(R01eMachine *m) {
    int i;
    int saw = 0;

    for (i = 0; i < 300000; i++) {
        (void)r01e_machine_step_insn(m);
        if (!saw) {
            int t;
            for (t = 0; t < 32; t++) {
                if (m->video.vram[t] != 0) {
                    saw = 1;
                    break;
                }
            }
        } else if (m->cpu.pc >= 0x8000u && m->cpu.pc < 0x8100u && i > 1000) {
            /* Stream done: back in low PRG (pad / VBlank hang). */
            break;
        }
    }
    if (saw) {
        m->video.slot_present[0] = 1;
    }
}

int r01e_machine_init(R01eMachine *m, const char *cart_path, char *err, size_t err_cap) {
    if (!m || !cart_path) {
        if (err && err_cap) {
            snprintf(err, err_cap, "bad args");
        }
        return -1;
    }
    memset(m, 0, sizeof(*m));
    if (r01e_cart_load_path(&m->cart, cart_path, err, err_cap) != 0) {
        return -1;
    }
    m->dot_num = R01E_DOT_HZ;
    m->dot_den = R01E_CPU_HZ;
    r01e_machine_reset(m);
    return 0;
}

void r01e_machine_shutdown(R01eMachine *m) {
    if (!m) {
        return;
    }
    r01e_cart_free(&m->cart);
    memset(m, 0, sizeof(*m));
}

void r01e_machine_reset(R01eMachine *m) {
    if (!m) {
        return;
    }
    memset(m->ram, 0, sizeof(m->ram));
    r01e_io_reset(&m->io);
    r01e_video_reset(&m->video);
    r01e_play_reset(&m->play);
    m->nmi_pending = 0;
    m->dot_acc = 0;
    m->prof_waiting = 0;
    m->prof_last_active = 0;
    m->prof_last_vblank = 0;
    m->prof_acc_active = 0;
    m->prof_acc_vblank = 0;
    m->prof_acc_idle = 0;
    m->prof_last_idle = 0;
    r01e_cpu_reset(&m->cpu, m);
    if (r01e_video_softboot_enabled()) {
        (void)r01e_video_boot_world(m, 0);
    } else {
        (void)r01e_video_prepare_world(m, 0);
        catchup_prg_boot(m);
        /* Boot stream is not steady-state frame work — drop it from the chart. */
        m->prof_acc_active = 0;
        m->prof_acc_vblank = 0;
        m->prof_acc_idle = 0;
        m->prof_last_active = 0;
        m->prof_last_vblank = 0;
        m->prof_last_idle = 0;
    }
    (void)r01e_play_start(m);
    r01e_video_render_frame(m);
}

uint8_t r01e_mem_read(R01eMachine *m, uint16_t addr) {
    const uint8_t *prg;
    uint32_t prg_off;

    if (addr < 0x8000u) {
        return m->ram[addr];
    }
    if (addr >= 0xFE00u && addr <= 0xFEFFu) {
        return r01e_io_read(m, addr);
    }
    prg = r01e_cart_prg(&m->cart);
    if (!prg) {
        return 0xFF;
    }
    prg_off = (uint32_t)(addr - 0x8000u);
    if (prg_off >= m->cart.len_prg) {
        return 0xFF;
    }
    return prg[prg_off];
}

void r01e_mem_write(R01eMachine *m, uint16_t addr, uint8_t v) {
    if (addr < 0x8000u) {
        m->ram[addr] = v;
        return;
    }
    if (addr >= 0xFE00u && addr <= 0xFEFFu) {
        r01e_io_write(m, addr, v);
    }
}

static void advance_dots(R01eMachine *m, int cpu_cycles) {
    m->dot_acc += (uint64_t)cpu_cycles * m->dot_num;
    while (m->dot_acc >= m->dot_den) {
        m->dot_acc -= m->dot_den;
        r01e_io_dot(m);
    }
}

int r01e_machine_step_insn(R01eMachine *m) {
    int cyc;
    int in_vblank;
    int waiting;

    if (!m) {
        return 0;
    }
    /* Attribute by beam position at instruction start; busy vs idle after $FE01 poll. */
    in_vblank = (m->io.dot_y >= R01E_VISIBLE_H);
    /* NMI / IRQ work is never "waiting" even if we interrupted a status spin. */
    if (m->nmi_pending) {
        m->prof_waiting = 0;
    }
    cyc = r01e_cpu_step(&m->cpu, m);
    waiting = m->prof_waiting;
    if (waiting) {
        m->prof_acc_idle += (uint64_t)cyc;
    } else if (in_vblank) {
        m->prof_acc_vblank += (uint64_t)cyc;
    } else {
        m->prof_acc_active += (uint64_t)cyc;
    }
    advance_dots(m, cyc);
    return cyc;
}

int r01e_machine_frame(R01eMachine *m) {
    int start_frame;
    int guard = 0;

    if (!m) {
        return 0;
    }
    start_frame = m->io.frame;
    while (m->io.frame == start_frame && guard < 2000000) {
        (void)r01e_machine_step_insn(m);
        guard++;
    }
    m->prof_last_active = m->prof_acc_active;
    m->prof_last_vblank = m->prof_acc_vblank;
    m->prof_last_idle = m->prof_acc_idle;
    m->prof_acc_active = 0;
    m->prof_acc_vblank = 0;
    m->prof_acc_idle = 0;
    /* #region agent log */
    {
        static int dbg_n;
        if (dbg_n < 40) {
            FILE *df = fopen("/home/g/Repos/retr01/.cursor/debug-7de916.log", "a");
            if (df) {
                uint64_t busy = m->prof_last_active + m->prof_last_vblank;
                uint64_t all = busy + m->prof_last_idle;
                fprintf(df,
                        "{\"sessionId\":\"7de916\",\"runId\":\"busy-metric\",\"hypothesisId\":\"F,G,H\","
                        "\"location\":\"machine.c:frame\",\"message\":\"busy vs idle frame\","
                        "\"data\":{\"n\":%d,\"busy_a\":%llu,\"busy_v\":%llu,\"busy\":%llu,"
                        "\"idle\":%llu,\"all\":%llu,\"budget\":%llu,\"pct_budget\":%.2f,"
                        "\"waiting\":%d,\"insns\":%d},"
                        "\"timestamp\":%d}\n",
                        dbg_n, (unsigned long long)m->prof_last_active,
                        (unsigned long long)m->prof_last_vblank, (unsigned long long)busy,
                        (unsigned long long)m->prof_last_idle, (unsigned long long)all,
                        (unsigned long long)R01E_CPU_BUDGET_CYCLES,
                        R01E_CPU_BUDGET_CYCLES ? (100.0 * (double)busy / (double)R01E_CPU_BUDGET_CYCLES)
                                               : 0.0,
                        m->prof_waiting, guard, dbg_n);
                fclose(df);
            }
            dbg_n++;
        }
    }
    /* #endregion */
    r01e_play_tick(m);
    if (!m->video.chr_loaded) {
        if (r01e_video_softboot_enabled()) {
            (void)r01e_video_boot_world(m, (int)m->io.world);
        } else {
            (void)r01e_video_prepare_world(m, (int)m->io.world);
        }
    }
    r01e_video_render_frame(m);
    return guard;
}

void r01e_machine_set_pad(R01eMachine *m, int player, uint8_t bits) {
    if (!m) {
        return;
    }
    if (player == 0) {
        m->io.pad0 = bits;
    } else {
        m->io.pad1 = bits;
    }
}
