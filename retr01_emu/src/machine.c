#include "retr01_emu/machine.h"

#include <stdio.h>
#include <string.h>

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
    m->dot_acc = 0;
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
    r01e_ppu_reset(&m->ppu);
    m->nmi_pending = 0;
    m->irq_line = 0;
    m->dot_acc = 0;
    r01e_cpu_reset(&m->cpu, m);
    /* Preload world 0 so FB is ready before stub STA $FE30 (also reloads). */
    (void)r01e_ppu_boot_world(m, 0);
    r01e_ppu_render_frame(m);
}

uint8_t r01e_mem_read(R01eMachine *m, uint16_t addr) {
    const uint8_t *prg;
    uint32_t prg_off;
    if (addr < 0x8000u) {
        return m->ram[addr];
    }
    if (addr >= 0xFE00u && addr <= 0xFEFFu) {
        return r01e_ppu_read(m, addr);
    }
    /* PRG: cart image maps linearly into $8000-$FFFF with I/O hole. */
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
        r01e_ppu_write(m, addr, v);
    }
    /* PRG writes ignored (ROM). */
}

static void advance_dots(R01eMachine *m, int cpu_cycles) {
    /* dots ≈ cycles * DOT_HZ / CPU_HZ */
    m->dot_acc += (uint64_t)cpu_cycles * m->dot_num;
    while (m->dot_acc >= m->dot_den) {
        m->dot_acc -= m->dot_den;
        r01e_ppu_dot(m);
    }
}

int r01e_machine_step_insn(R01eMachine *m) {
    int cyc;
    if (!m) {
        return 0;
    }
    cyc = r01e_cpu_step(&m->cpu, m);
    advance_dots(m, cyc);
    return cyc;
}

int r01e_machine_frame(R01eMachine *m) {
    int start_frame;
    int guard = 0;
    if (!m) {
        return 0;
    }
    start_frame = m->ppu.frame;
    while (m->ppu.frame == start_frame && guard < 2000000) {
        (void)r01e_machine_step_insn(m);
        guard++;
    }
    /* Ensure FB is current even if beam wrap already rendered. */
    if (!m->ppu.chr_loaded) {
        (void)r01e_ppu_boot_world(m, (int)m->ppu.world);
    }
    r01e_ppu_render_frame(m);
    return guard;
}

void r01e_machine_set_pad(R01eMachine *m, int player, uint8_t bits) {
    if (!m) {
        return;
    }
    if (player == 0) {
        m->ppu.pad0 = bits;
    } else {
        m->ppu.pad1 = bits;
    }
}
