#include "retr01_sim/spi_mailbox.h"

#include "avr128db28_m.h"
#include "avr128db28_s1.h"
#include "avr128db28_s2.h"
#include "r01_spi_mailbox.h"
#include "retr01_sim/board.h"
#include "retr01_sim/bus.h"

void r01s_spi_mailbox_flush(R01sBoard *b) {
    R01sAvr128db28M *m;
    R01sEntity *me;
    R01sEntity *s1e;
    R01sEntity *s2e;
    const uint8_t *oam;
    const uint8_t *apu;
    unsigned i;

    if (!b) {
        return;
    }
    m = &b->mcu_m;
    me = r01s_avr128db28_m_entity(m);
    s1e = r01s_avr128db28_s1_entity(&b->mcu_s1);
    s2e = r01s_avr128db28_s2_entity(&b->mcu_s2);

    if (me) {
        r01s_entity_drive(me, "/SS_S1", R01S_LVL_H);
        r01s_entity_drive(me, "/SS_S2", R01S_LVL_H);
    }

    if (r01s_avr128db28_m_soft_oam_dirty(m)) {
        oam = r01s_avr128db28_m_soft_oam(m);
        if (me) {
            r01s_entity_drive(me, "/SS_S1", R01S_LVL_L);
        }
        if (s1e) {
            r01s_entity_drive(s1e, "/SS_S1", R01S_LVL_L);
        }
        (void)R01_MB_OP_OAM_BULK;
        for (i = 0; i < R01_OAM_BYTES; i++) {
            r01s_avr128db28_s1_oam_poke(&b->mcu_s1, (uint16_t)i, oam[i]);
        }
        r01s_avr128db28_m_soft_clear_oam_dirty(m);
        if (me) {
            r01s_entity_drive(me, "/SS_S1", R01S_LVL_H);
        }
        if (s1e) {
            r01s_entity_drive(s1e, "/SS_S1", R01S_LVL_H);
        }
    }

    if (r01s_avr128db28_m_soft_apu_dirty(m)) {
        apu = r01s_avr128db28_m_soft_apu(m);
        if (me) {
            r01s_entity_drive(me, "/SS_S2", R01S_LVL_L);
        }
        if (s2e) {
            r01s_entity_drive(s2e, "/SS_S2", R01S_LVL_L);
        }
        (void)R01_MB_OP_APU_REG;
        for (i = 0; i < R01_APU_REGS; i++) {
            r01s_avr128db28_s2_poke(&b->mcu_s2, i, apu[i]);
        }
        r01s_avr128db28_m_soft_clear_apu_dirty(m);
        if (me) {
            r01s_entity_drive(me, "/SS_S2", R01S_LVL_H);
        }
        if (s2e) {
            r01s_entity_drive(s2e, "/SS_S2", R01S_LVL_H);
        }
    }
}

void r01s_spi_mailbox_on_vblank(R01sBoard *b) {
    if (!b) {
        return;
    }
    r01s_avr128db28_m_soft_request_vbl_mark(&b->mcu_m);
    (void)R01_MB_OP_VBL_MARK;
    r01s_spi_mailbox_flush(b);
    b->mcu_m.soft_vbl_mark_pending = 0;
}
