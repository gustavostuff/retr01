#include "retr01_emu/machine.h"

#include "r01_apu_cart.h"
#include "r01_bgm_fd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Run Phase 1 PRG until the boot MAP stream has written a full screen into slot 0
 * (or give up). PC still in $8000-$80FF is not "done": the stream lives there.
 * Stopping mid-copy lets the CPU keep writing the start screen over Play's
 * 2x2 after play_start, so collision (cart) and render (VRAM) diverge.
 */
static void catchup_prg_boot(R01eMachine *m) {
    int i;
    int saw = 0;

    for (i = 0; i < 500000; i++) {
        (void)r01e_machine_step_insn(m);
        if (m->ram[0x02E8]) {
            break;
        }
        if (!saw) {
            int t;
            for (t = 0; t < 32; t++) {
                if (m->video.vram[t] != 0) {
                    saw = 1;
                    break;
                }
            }
        } else if (m->io.vram_addr >= (uint16_t)R01E_SCREEN_PAYLOAD) {
            /* Tiles+attrs (480 B) streamed from $0000; pals finish before MAP. */
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
    {
        size_t n = strlen(cart_path);
        if (n >= sizeof(m->cart_path)) {
            n = sizeof(m->cart_path) - 1u;
        }
        memcpy(m->cart_path, cart_path, n);
        m->cart_path[n] = '\0';
    }
    m->dot_num = R01E_DOT_HZ;
    m->dot_den = R01E_CPU_HZ;
    r01e_machine_reset(m);
    return 0;
}

int r01e_machine_init_mem(R01eMachine *m, const uint8_t *img, size_t len, char *err, size_t err_cap) {
    if (!m || !img) {
        if (err && err_cap) {
            snprintf(err, err_cap, "bad args");
        }
        return -1;
    }
    memset(m, 0, sizeof(*m));
    if (r01e_cart_load_mem(&m->cart, img, len, err, err_cap) != 0) {
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
    m->apu_tracker_on = 0;
    m->apu_bytecode_len = 0;
    m->apu_sfx_len = 0;
    m->apu_sfx_prev_pad = 0;
    memset(m->apu_ins, 0, sizeof(m->apu_ins));
    r01_apu_tracker_init(&m->apu_tracker);
    r01e_cpu_reset(&m->cpu, m);
    if (r01e_video_softboot_enabled()) {
        (void)r01e_video_boot_world(m, 0);
    } else {
        (void)r01e_video_prepare_world(m, 0);
        catchup_prg_boot(m);
        /* Fill the 2x2 window from cart; PRG only streams slot 0. */
        (void)r01e_video_sync_camera(m);
        /* Boot stream is not steady-state frame work -- drop it from the chart. */
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

    /* RAM $0000-$7EFF; soft I/O $7F00-$7FFF; PRG $8000+ (no FE hole). */
    if (addr < 0x7F00u) {
        return m->ram[addr];
    }
    if (addr <= 0x7FFFu) {
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
    if (addr < 0x7F00u) {
        m->ram[addr] = v;
        return;
    }
    if (addr <= 0x7FFFu) {
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
    /* Attribute by beam position at instruction start; busy vs idle after $7F01 poll. */
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
    /* Packed PRG publishes OAM / scroll. Play follow copies $02E0 in r01e_io_dot. */
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
    /* Host stages; CPU sees latched pads after VBlank enter. */
    if (player == 0) {
        m->io.pad0_host = bits;
    } else {
        m->io.pad1_host = bits;
    }
}

void r01e_machine_apu_tracker_stop(R01eMachine *m) {
    if (!m) {
        return;
    }
    m->apu_tracker_on = 0;
    m->apu_bytecode_len = 0;
    m->apu_sfx_len = 0;
    m->apu_sfx_prev_pad = 0;
    memset(m->apu_ins, 0, sizeof(m->apu_ins));
    r01_apu_tracker_init(&m->apu_tracker);
}

int r01e_machine_apu_sfx(R01eMachine *m, uint8_t id) {
    int n;
    if (!m || !m->apu_tracker_on) {
        return -1;
    }
    n = r01_apu_sfx_encode(id, m->apu_sfx, (unsigned)sizeof(m->apu_sfx));
    if (n < 0) {
        return -1;
    }
    m->apu_sfx_len = (uint16_t)n;
    r01_apu_tracker_trigger_sfx(&m->apu_tracker, m->apu_sfx, m->apu_sfx_len);
    return 0;
}

static int load_bgm_cells(const char *path, char cells[R01_BGM_FD_STEPS_MAX][R01_BGM_FD_CH][R01_BGM_FD_TOKEN],
                          int *out_steps) {
    FILE *f;
    long sz;
    int steps;
    int t, ch;
    unsigned char *buf = NULL;
    size_t need;
    if (!path || !path[0] || !out_steps) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    need = (size_t)R01_BGM_FD_CH * (size_t)R01_BGM_FD_TOKEN;
    if (sz < (long)need || ((size_t)sz % need) != 0) {
        fclose(f);
        return -1;
    }
    steps = (int)((size_t)sz / need);
    if (steps < 1) {
        fclose(f);
        return -1;
    }
    if (steps > R01_BGM_FD_STEPS_MAX) {
        steps = R01_BGM_FD_STEPS_MAX;
    }
    buf = (unsigned char *)malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    memset(cells, 0, sizeof(char) * (size_t)R01_BGM_FD_STEPS_MAX * R01_BGM_FD_CH * R01_BGM_FD_TOKEN);
    for (t = 0; t < steps; t++) {
        for (ch = 0; ch < R01_BGM_FD_CH; ch++) {
            size_t off = ((size_t)t * (size_t)R01_BGM_FD_CH + (size_t)ch) * (size_t)R01_BGM_FD_TOKEN;
            char tok[R01_BGM_FD_TOKEN];
            int i;
            memcpy(tok, buf + off, (size_t)R01_BGM_FD_TOKEN);
            tok[R01_BGM_FD_TOKEN - 1] = '\0';
            for (i = 0; i < R01_BGM_FD_TOKEN; i++) {
                if (tok[i] == '\0') {
                    break;
                }
            }
            if (i == 0) {
                snprintf(cells[t][ch], R01_BGM_FD_TOKEN, "--");
            } else {
                snprintf(cells[t][ch], R01_BGM_FD_TOKEN, "%s", tok);
            }
        }
    }
    free(buf);
    *out_steps = steps;
    return 0;
}

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int r01e_machine_apu_tracker_start_cart(R01eMachine *m) {
    const uint8_t *prg;
    const uint8_t *blob;
    uint8_t boot;
    uint16_t off;
    uint16_t len;
    uint32_t prg_len;
    uint16_t payload_min = R01_PRG_BGM_HDR;
    if (!m) {
        return -1;
    }
    r01e_machine_apu_tracker_stop(m);
    m->apu_tracker_on = 1;
    prg = r01e_cart_prg(&m->cart);
    prg_len = m->cart.len_prg;
    if (!prg || prg_len < R01E_PRG_BYTES) {
        return 0;
    }
    blob = r01e_cart_ptr(&m->cart, m->cart.off_bgm, m->cart.len_bgm);
    if (!blob || m->cart.len_bgm < R01_PRG_BGM_HDR) {
        return 0;
    }
    if (blob[0] != R01_PRG_BGM_MAGIC0 || blob[1] != R01_PRG_BGM_MAGIC1) {
        return 0;
    }
    if (blob[3] == R01_PRG_BGM_INS_VER) {
        payload_min = (uint16_t)R01_PRG_BGM_HDR_V1;
    }
    boot = prg[R01_PRG_BGM_BOOT_OFF];
    if (boot < 1u || boot > R01_PRG_BGM_TRACKS) {
        int t;
        int tc = (int)blob[2];
        boot = 0;
        if (tc < 0) {
            tc = 0;
        }
        if (tc > (int)R01_PRG_BGM_TRACKS) {
            tc = (int)R01_PRG_BGM_TRACKS;
        }
        for (t = 0; t < tc; t++) {
            uint16_t tlen = rd_u16_le(blob + 20u + (unsigned)t * 2u);
            if (tlen > 0u) {
                boot = (uint8_t)(t + 1);
                break;
            }
        }
    }
    if (boot < 1u || boot > R01_PRG_BGM_TRACKS) {
        return 0;
    }
    if (blob[3] == R01_PRG_BGM_INS_VER) {
        const uint8_t *ip = blob + R01_PRG_BGM_HDR + (unsigned)(boot - 1u) * R01_PRG_BGM_INS_CH;
        unsigned ch;
        for (ch = 0; ch < R01_PRG_BGM_INS_CH && ch < (unsigned)sizeof(m->apu_ins); ch++) {
            uint8_t v = ip[ch];
            if (v > R01_PRG_BGM_INS_MAX) {
                v = 0;
            }
            m->apu_ins[ch] = v;
        }
    }
    off = rd_u16_le(blob + 4u + (unsigned)(boot - 1u) * 2u);
    len = rd_u16_le(blob + 20u + (unsigned)(boot - 1u) * 2u);
    if (off < payload_min || len < 1u) {
        return 0;
    }
    if ((unsigned)off + (unsigned)len > m->cart.len_bgm) {
        return 0;
    }
    if (len > R01E_APU_BYTECODE_MAX) {
        len = (uint16_t)R01E_APU_BYTECODE_MAX;
    }
    memcpy(m->apu_bytecode, blob + off, (size_t)len);
    m->apu_bytecode_len = len;
    r01_apu_tracker_set_bgm(&m->apu_tracker, m->apu_bytecode, m->apu_bytecode_len);
    (void)r01_apu_tracker_nmi(&m->apu_tracker, m->io.apu);
    return 0;
}

int r01e_machine_apu_tracker_start(R01eMachine *m, const char *bgm_bin_path) {
    char cells[R01_BGM_FD_STEPS_MAX][R01_BGM_FD_CH][R01_BGM_FD_TOKEN];
    int steps = 0;
    int n;
    if (!m) {
        return -1;
    }
    r01e_machine_apu_tracker_stop(m);
    if (!bgm_bin_path || load_bgm_cells(bgm_bin_path, cells, &steps) != 0) {
        /* Builtin one-note demo bytecode: FD ch0 C4, FE 01, FA */
        static const uint8_t demo[] = {R01_APU_FD_OP, 0x01u, 0xC4u, R01_APU_CTRL_FE, 0x01u, R01_APU_CTRL_FA};
        memcpy(m->apu_bytecode, demo, sizeof(demo));
        m->apu_bytecode_len = (uint16_t)sizeof(demo);
    } else {
        n = r01_bgm_fd_encode_cells((const char(*)[R01_BGM_FD_CH][R01_BGM_FD_TOKEN])cells, steps,
                                    m->apu_bytecode, R01E_APU_BYTECODE_MAX);
        if (n < 0) {
            return -1;
        }
        m->apu_bytecode_len = (uint16_t)n;
    }
    r01_apu_tracker_set_bgm(&m->apu_tracker, m->apu_bytecode, m->apu_bytecode_len);
    m->apu_tracker_on = 1;
    (void)r01_apu_tracker_nmi(&m->apu_tracker, m->io.apu);
    return 0;
}
