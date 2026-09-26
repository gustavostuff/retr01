#include "retr01_emu/play.h"

#include "retr01_emu/cart.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/video.h"
#include "r01_play_camera.h"
#include "r01_play_sys.h"

#include <string.h>

static void play_apply_vid_flags(R01eMachine *m) {
    R01eVideo *vid;
    uint8_t f;

    if (!m || !m->ram[R01_SYS_READY]) {
        return;
    }
    vid = &m->video;
    f = m->ram[R01_SYS_VID_FLAGS];
    vid->bg0_wrap_x = (f & R01E_CART_WHDR_FLAG_BG0_WRAP_X) != 0;
    vid->bg0_wrap_y = (f & R01E_CART_WHDR_FLAG_BG0_WRAP_Y) != 0;
    vid->bg0_clip_bg1 = (f & R01E_CART_WHDR_FLAG_BG0_CLIP_BG1) != 0;
}

static void play_follow_c_sys(R01eMachine *m) {
    if (!m || !m->ram[0x02E8]) {
        return;
    }
    m->play.player_x = (int)m->ram[0x02E0] | ((int)m->ram[0x02E1] << 8);
    m->play.player_y = (int)m->ram[0x02E2] | ((int)m->ram[0x02E3] << 8);
    m->play.cam_x = (int)m->ram[0x02E4] | ((int)m->ram[0x02E5] << 8);
    m->play.cam_y = (int)m->ram[0x02E6] | ((int)m->ram[0x02E7] << 8);
}

static void play_copy_cam_deadzone(R01eMachine *m) {
    R01eWorldView wv;
    int dx;
    int dy;

    if (!m) {
        return;
    }
    m->play.cam_deadzone_x = R01_PLAY_CAM_DEADZONE_X_DEFAULT;
    m->play.cam_deadzone_y = R01_PLAY_CAM_DEADZONE_Y_DEFAULT;
    if (r01e_cart_is_c_prg(&m->cart)) {
        return;
    }
    if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) != 0) {
        return;
    }
    dx = (int)wv.cam_deadzone_x;
    dy = (int)wv.cam_deadzone_y;
    if (dx <= R01E_SCREEN_PX_W && dy <= R01E_SCREEN_PX_H && (dx > 0 || dy > 0)) {
        m->play.cam_deadzone_x = dx;
        m->play.cam_deadzone_y = dy;
    }
}

void r01e_play_sync_video(R01eMachine *m) {
    R01ePlay *pl;
    R01eVideo *vid;
    int ox, oy;
    int origin_changed;

    if (!m || !m->play.enabled) {
        return;
    }
    pl = &m->play;
    vid = &m->video;
    ox = pl->cam_x / R01E_SCREEN_PX_W;
    oy = pl->cam_y / R01E_SCREEN_PX_H;
    origin_changed = (ox != vid->cam_origin_col || oy != vid->cam_origin_row);
    vid->cam_x = pl->cam_x;
    vid->cam_y = pl->cam_y;
    vid->cam_origin_col = ox;
    vid->cam_origin_row = oy;
    /* Packed PRG owns $7F02/$7F03. Host refreshes the 2x2 window only. */
    if (origin_changed) {
        (void)r01e_video_fill_origin_slots(m);
    }
    r01e_video_update_bg0_scroll(m);
}

void r01e_play_reset(R01ePlay *play) {
    if (!play) {
        return;
    }
    memset(play, 0, sizeof(*play));
    play->player_w = R01E_PLAY_PLAYER_W;
    play->player_h = R01E_PLAY_PLAYER_H;
}

int r01e_play_start(R01eMachine *m) {
    if (!m) {
        return 0;
    }
    r01e_play_reset(&m->play);
    play_copy_cam_deadzone(m);
    if (!r01e_cart_is_c_prg(&m->cart)) {
        return 0;
    }
    m->play.enabled = 1;
    if (m->ram[R01_SYS_READY]) {
        play_follow_c_sys(m);
        play_apply_vid_flags(m);
        r01e_play_sync_video(m);
        (void)r01e_video_sync_camera(m);
    }
    return 1;
}

void r01e_play_tick(R01eMachine *m) {
    if (!m || !m->play.enabled) {
        return;
    }
    if (!r01e_cart_is_c_prg(&m->cart)) {
        return;
    }
    if (m->ram[R01_SYS_READY]) {
        play_follow_c_sys(m);
        play_apply_vid_flags(m);
        r01e_play_sync_video(m);
    }
}

void r01e_play_player_rgb(const R01eMachine *m, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!m || !r || !g || !b) {
        return;
    }
    r01e_video_kit_rgb(m->io.pal[R01E_PAL_PLAYER] & 63, r, g, b);
}

void r01e_play_draw(R01eMachine *m) {
    /* Player is drawn via OAM composite in r01e_video_render_frame. */
    (void)m;
}

void r01e_play_post_event(R01ePlay *play, R01eEvent evt) {
    (void)play;
    (void)evt;
}
