#include "atmega328p.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

#include <stdio.h>

static void write_reg(R01sEntity *e, unsigned reg, uint8_t v) {
    int i;
    for (i = 0; i < 5; i++) {
        char n[4];
        snprintf(n, sizeof(n), "A%d", i);
        r01s_entity_drive(e, n, (reg & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
    r01s_entity_drive(e, "CE#", R01S_LVL_L);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
    r01s_entity_drive(e, "WE#", R01S_LVL_L);
    r01s_bus_write(e, "DQ", 8, v);
    r01s_entity_eval(e);
    r01s_entity_drive(e, "WE#", R01S_LVL_H);
    r01s_entity_eval(e);
    r01s_entity_drive(e, "CE#", R01S_LVL_H);
}

static uint8_t read_reg(R01sEntity *e, unsigned reg) {
    int i;
    uint8_t v;
    for (i = 0; i < 5; i++) {
        char n[4];
        snprintf(n, sizeof(n), "A%d", i);
        r01s_entity_drive(e, n, (reg & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
    r01s_entity_drive(e, "CE#", R01S_LVL_L);
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "WE#", R01S_LVL_H);
    r01s_entity_eval(e);
    v = (uint8_t)r01s_bus_read(e, "DQ", 8);
    r01s_entity_drive(e, "CE#", R01S_LVL_H);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
    r01s_entity_eval(e);
    return v;
}

int main(void) {
    R01sAtmega328p chip;
    R01sEntity *e;
    int i;

    r01s_atmega328p_init(&chip, "U328");
    e = r01s_atmega328p_entity(&chip);
    expect_true(e->dip_pins == 28, "28-pin package");

    write_reg(e, 1, 0x10);
    write_reg(e, 2, 0x00);
    write_reg(e, 0, 0x8F); /* enable + vol 8 */
    expect_true(read_reg(e, 0) == 0x8F, "readback FE40");
    expect_true(r01s_atmega328p_enabled(&chip), "enabled");
    expect_true(r01s_atmega328p_period(&chip) == 0x10, "period 16");

    for (i = 0; i < 64; i++) {
        r01s_entity_tick(e);
    }
    expect_true(r01s_atmega328p_pwm_edges(&chip) > 0, "PWM edges");
    expect_true(r01s_atmega328p_pwm_hi_samples(&chip) > 0, "PWM hi samples");
    expect_true(r01s_entity_sense(e, "PWM") == R01S_LVL_H || r01s_entity_sense(e, "PWM") == R01S_LVL_L,
                "PWM driven");

    write_reg(e, 0, 0x00);
    {
        uint32_t edges = r01s_atmega328p_pwm_edges(&chip);
        for (i = 0; i < 32; i++) {
            r01s_entity_tick(e);
        }
        expect_true(r01s_entity_sense(e, "PWM") == R01S_LVL_L, "PWM idle when disabled");
        expect_true(r01s_atmega328p_pwm_edges(&chip) == edges ||
                        r01s_atmega328p_pwm_edges(&chip) == edges + 1,
                    "no free-run after disable");
    }

    /* 8-voice mix + math wave draw. */
    r01s_atmega328p_voice_set(&chip, 0, R01S_APU_WAVE_PULSE, 1, 8, 2, 32);
    r01s_atmega328p_voice_set(&chip, 2, R01S_APU_WAVE_TRIANGLE, 1, 15, 0, 48);
    r01s_atmega328p_voice_set(&chip, 3, R01S_APU_WAVE_NOISE, 1, 8, 0, 16);
    for (i = 0; i < 128; i++) {
        r01s_entity_tick(e);
    }
    expect_true(r01s_atmega328p_voice(&chip, 0)->sample != 0 || r01s_atmega328p_voice(&chip, 2)->sample != 0,
                "voices produce samples");
    /* Fixed-time WAVE draw: shorter period => denser cycles (pitch readable). */
    {
        int z32 = 0;
        int z16 = 0;
        int x;
        r01s_atmega328p_voice_set(&chip, 0, R01S_APU_WAVE_PULSE, 1, 8, 2, 32);
        for (x = 1; x < 64; x++) {
            if (r01s_apu_voice_wave_y(r01s_atmega328p_voice(&chip, 0), x - 1, 64) *
                    r01s_apu_voice_wave_y(r01s_atmega328p_voice(&chip, 0), x, 64) <
                0) {
                z32++;
            }
        }
        r01s_atmega328p_voice_set(&chip, 0, R01S_APU_WAVE_PULSE, 1, 8, 2, 16);
        for (x = 1; x < 64; x++) {
            if (r01s_apu_voice_wave_y(r01s_atmega328p_voice(&chip, 0), x - 1, 64) *
                    r01s_apu_voice_wave_y(r01s_atmega328p_voice(&chip, 0), x, 64) <
                0) {
                z16++;
            }
        }
        expect_true(z16 > z32, "higher pitch more WAVE zero-crossings");
        r01s_atmega328p_voice_set(&chip, 0, R01S_APU_WAVE_PULSE, 1, 8, 2, 32);
    }
    expect_true(r01s_apu_voice_wave_y(r01s_atmega328p_voice(&chip, 0), 0, 64) !=
                    r01s_apu_voice_wave_y(r01s_atmega328p_voice(&chip, 0), 40, 64) ||
                r01s_apu_voice_wave_y(r01s_atmega328p_voice(&chip, 0), 0, 64) != 0,
                "pulse math draw varies");
    {
        uint8_t scope[32];
        int n = r01s_atmega328p_scope_copy(&chip, scope, 32);
        expect_true(n > 0, "analog scope has samples");
    }

    /* Silent WAVE viz sequencer. */
    r01s_atmega328p_viz_start(&chip, 1000, NULL);
    expect_true(r01s_atmega328p_viz_active(&chip), "viz active");
    expect_true(r01s_atmega328p_viz_step(&chip) == 0, "viz step 0");
    expect_true(r01s_atmega328p_voice(&chip, 0)->enable, "viz B1 on at step 0");
    /* Advance one grid step via 60 Hz NMI ticks (dt per frame capped at 100 ms). */
    {
        int frames = r01_bgm_fd_frames_per_step();
        int ms_nmi = 1000 / (R01_BGM_FD_NMI_HZ * R01S_APU_VIZ_TEMPO_SCALE);
        int need_ms = frames * (ms_nmi < 1 ? 1 : ms_nmi);
        uint32_t t = 1000;
        int left = need_ms < 1 ? 1 : need_ms;
        while (left > 0) {
            int chunk = left > 100 ? 100 : left;
            t += (uint32_t)chunk;
            left -= chunk;
            r01s_atmega328p_viz_frame(&chip, t);
        }
    }
    expect_true(r01s_atmega328p_viz_step(&chip) == 1, "viz advanced");
    r01s_atmega328p_viz_stop(&chip);
    expect_true(!r01s_atmega328p_viz_active(&chip), "viz stopped");

    return test_done("test_atmega328p");
}
