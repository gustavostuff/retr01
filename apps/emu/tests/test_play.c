#include "retr01_emu/machine.h"
#include "retr01_emu/cart.h"
#include "retr01_emu/play.h"
#include "retr01_emu/types.h"
#include "retr01_emu/video.h"
#include "r01_play_camera.h"

#include <stdio.h>
#include <string.h>

static const uint8_t *cart_screen_payload(const R01eCart *c, int col, int row) {
    R01eWorldView wv;
    const uint8_t *dir;
    int si;

    if (r01e_cart_world(c, 0, &wv) != 0) {
        return NULL;
    }
    dir = r01e_cart_ptr(c, wv.base + wv.off_screen_dir, (size_t)wv.screen_count * 12u);
    if (!dir) {
        return NULL;
    }
    for (si = 0; si < wv.screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        if (R01E_CELL_COL(e[0]) == col && R01E_CELL_ROW(e[0]) == row) {
            uint32_t poff = (uint32_t)e[4] | ((uint32_t)e[5] << 8) | ((uint32_t)e[6] << 16);
            return r01e_cart_ptr(c, wv.base + poff, R01E_SCREEN_PAYLOAD);
        }
    }
    return NULL;
}

/* VRAM slots must match cart for the current cam origin (render vs collision sync). */
static int vram_matches_cart(const R01eMachine *m) {
    int dx, dy;

    for (dy = 0; dy < 2; dy++) {
        for (dx = 0; dx < 2; dx++) {
            int col = m->video.cam_origin_col + dx;
            int row = m->video.cam_origin_row + dy;
            int slot = dy * 2 + dx;
            const uint8_t *pay = cart_screen_payload(&m->cart, col, row);
            const uint8_t *vram = m->video.vram + (size_t)slot * R01E_VRAM_SLOT_BYTES;
            if (!pay) {
                if (m->video.slot_present[slot]) {
                    fprintf(stderr, "FAIL slot %d present but cart missing (%d,%d)\n", slot, col, row);
                    return 0;
                }
                continue;
            }
            if (!m->video.slot_present[slot]) {
                fprintf(stderr, "FAIL slot %d empty for cart screen (%d,%d)\n", slot, col, row);
                return 0;
            }
            if (memcmp(vram, pay, R01E_SCREEN_PAYLOAD) != 0) {
                fprintf(stderr, "FAIL slot %d VRAM!=cart screen (%d,%d) (boot stream clobber?)\n", slot, col,
                        row);
                return 0;
            }
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    const char *path;
    if (argc < 2 || !argv[1] || !argv[1][0]) {
        fprintf(stderr, "skip: provide cart path argv\n");
        return 77;
    }
    path = argv[1];
    R01eMachine m;
    char err[256];
    int spawn_x;
    int spawn_y;
    int expect_x;
    int expect_y;
    int expect_cam_x;
    int expect_cam_y;
    int f;

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }
    if (r01e_machine_apu_tracker_start_cart(&m) != 0 || m.apu_bytecode_len < 1) {
        fprintf(stderr, "FAIL cart BGM stream empty\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (m.apu_ins[0] != 2 || m.apu_ins[1] != 0 || m.apu_ins[2] != 3) {
        fprintf(stderr, "FAIL cart BGM ins %u,%u,%u (expected piano, guitar, flute)\n", m.apu_ins[0],
                m.apu_ins[1], m.apu_ins[2]);
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (r01e_play_start(&m) != 1) {
        fprintf(stderr, "FAIL play start\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (!vram_matches_cart(&m)) {
        r01e_machine_shutdown(&m);
        return 1;
    }
    spawn_x = expect_x = m.play.player_x;
    spawn_y = expect_y = m.play.player_y;
    {
        expect_cam_x = m.play.cam_x;
        expect_cam_y = m.play.cam_y;
        r01_play_camera_update(&expect_cam_x, &expect_cam_y, spawn_x, spawn_y, m.play.player_w, m.play.player_h,
                               R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, m.play.cam_deadzone_x, m.play.cam_deadzone_y,
                               R01_PLAY_CAM_AXIS_BOTH);
        if (m.play.cam_x != expect_cam_x || m.play.cam_y != expect_cam_y) {
            fprintf(stderr, "FAIL camera at spawn: got %d,%d expected %d,%d\n", m.play.cam_x, m.play.cam_y,
                    expect_cam_x, expect_cam_y);
            r01e_machine_shutdown(&m);
            return 1;
        }
    }

    {
        int cam_x = 0;
        int cam_y = 0;
        int px = R01E_SCREEN_PX_W / 2;
        int py = R01E_SCREEN_PX_H / 2;
        int prev;
        int i;
        int moved = 0;
        r01_play_camera_snap(&cam_x, &cam_y, px, py, R01E_PLAY_PLAYER_W, R01E_PLAY_PLAYER_H, R01E_SCREEN_PX_W,
                             R01E_SCREEN_PX_H, 32, 70, R01_PLAY_CAM_AXIS_BOTH);
        prev = cam_x;
        for (i = 0; i < 24; i++) {
            int d;
            px += 2;
            r01_play_camera_update(&cam_x, &cam_y, px, py, R01E_PLAY_PLAYER_W, R01E_PLAY_PLAYER_H, R01E_SCREEN_PX_W,
                                   R01E_SCREEN_PX_H, 32, 70, R01_PLAY_CAM_AXIS_BOTH);
            d = cam_x - prev;
            if (d != 0 && d != 2) {
                fprintf(stderr, "FAIL 2px deadzone engage hitch dcam=%d at step %d\n", d, i);
                r01e_machine_shutdown(&m);
                return 1;
            }
            if (moved && d != 2) {
                fprintf(stderr, "FAIL 2px deadzone follow dcam=%d at step %d\n", d, i);
                r01e_machine_shutdown(&m);
                return 1;
            }
            if (d == 2) {
                moved = 1;
            }
            prev = cam_x;
        }
        if (!moved) {
            fprintf(stderr, "FAIL 2px run never left deadzone\n");
            r01e_machine_shutdown(&m);
            return 1;
        }
    }

    /* Leaving the dead zone scrolls the camera. */
    if (!r01e_cart_is_c_prg(&m.cart)) {
        m.play.player_x = spawn_x + 8;
        m.play.player_y = spawn_y;
        m.io.pad0 = 0;
        r01e_play_tick(&m);
        {
            expect_cam_x = m.play.cam_x;
            expect_cam_y = m.play.cam_y;
            r01_play_camera_update(&expect_cam_x, &expect_cam_y, m.play.player_x, m.play.player_y, m.play.player_w,
                                   m.play.player_h, R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, m.play.cam_deadzone_x,
                                   m.play.cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
            if (m.play.cam_x != expect_cam_x || m.play.cam_y != expect_cam_y) {
                fprintf(stderr, "FAIL camera follow after deadzone exit: got %d,%d expected %d,%d\n", m.play.cam_x,
                        m.play.cam_y, expect_cam_x, expect_cam_y);
                r01e_machine_shutdown(&m);
                return 1;
            }
        }
    } else if (!m.ram[0x02E8]) {
        fprintf(stderr, "FAIL C PRG sys ready\n");
        r01e_machine_shutdown(&m);
        return 1;
    }

    printf("ok play spawn=%d,%d cam=%d\n", spawn_x, spawn_y, m.play.cam_x);

    /*
     * L-map junction: (3,2) present, (3,1) missing. Follow cam may peek into the
     * hole (BG0 show-through is fine). Player must stay on-screen for OAM.
     */
    if (!r01e_cart_is_c_prg(&m.cart) && r01e_cart_has_screen(&m.cart, 0, 3, 2) &&
        !r01e_cart_has_screen(&m.cart, 0, 3, 1)) {
        int vx, vy;
        m.play.player_x = 3 * R01E_SCREEN_PX_W + 20;
        m.play.player_y = 2 * R01E_SCREEN_PX_H + 40;
        m.play.cam_x = 2 * R01E_SCREEN_PX_W + 64;
        m.play.cam_y = 1 * R01E_SCREEN_PX_H + 60;
        m.io.pad0 = 0;
        r01e_play_tick(&m);
        vx = m.play.player_x - m.play.cam_x;
        vy = m.play.player_y - m.play.cam_y;
        if (r01e_oam_tile_off_screen(vx, vy)) {
            fprintf(stderr, "FAIL L-map player off-screen: player=%d,%d cam=%d,%d oam=%d,%d\n",
                    m.play.player_x, m.play.player_y, m.play.cam_x, m.play.cam_y, vx, vy);
            r01e_machine_shutdown(&m);
            return 1;
        }
        printf("ok L-map player on-screen cam=%d,%d oam=%d,%d\n", m.play.cam_x, m.play.cam_y, vx, vy);
    }

    /*
     * Regression: unfinished PRG boot MAP stream must not clobber Host Play VRAM
     * after play_start (collision stays on cart; render would show start-screen
     * tiles until the next origin reload).
     */
    for (f = 0; f < 45; f++) {
        r01e_machine_set_pad(&m, 0, R01E_PAD_RIGHT);
        if (r01e_machine_frame(&m) == 0) {
            fprintf(stderr, "FAIL frame %d\n", f);
            r01e_machine_shutdown(&m);
            return 1;
        }
    }
    if (!vram_matches_cart(&m)) {
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (r01e_cart_is_c_prg(&m.cart)) {
        uint64_t used;
        if (m.play.player_x == spawn_x && m.play.player_y == spawn_y) {
            fprintf(stderr, "FAIL C PRG pad did not move player from %d,%d\n", spawn_x, spawn_y);
            r01e_machine_shutdown(&m);
            return 1;
        }
        used = m.prof_last_active + m.prof_last_vblank;
        printf("ok C PRG moved player %d,%d -> %d,%d cpu active=%llu vblank=%llu used=%llu/%llu\n", spawn_x,
               spawn_y, m.play.player_x, m.play.player_y, (unsigned long long)m.prof_last_active,
               (unsigned long long)m.prof_last_vblank, (unsigned long long)used,
               (unsigned long long)R01E_CPU_BUDGET_CYCLES);
        if (used > 22000ull) {
            fprintf(stderr, "FAIL C PRG CPU %llu cycles (budget %llu) after walk\n", (unsigned long long)used,
                    (unsigned long long)R01E_CPU_BUDGET_CYCLES);
            r01e_machine_shutdown(&m);
            return 1;
        }
        {
            int origin0 = m.video.cam_origin_col;
            int crossed = 0;
            for (f = 0; f < 200; f++) {
                r01e_machine_set_pad(&m, 0, R01E_PAD_RIGHT);
                if (r01e_machine_frame(&m) == 0) {
                    fprintf(stderr, "FAIL seam frame %d\n", f);
                    r01e_machine_shutdown(&m);
                    return 1;
                }
                if (m.video.cam_origin_col != origin0) {
                    used = m.prof_last_active + m.prof_last_vblank;
                    printf("ok C PRG seam origin %d->%d cpu active=%llu vblank=%llu used=%llu/%llu\n",
                           origin0, m.video.cam_origin_col, (unsigned long long)m.prof_last_active,
                           (unsigned long long)m.prof_last_vblank, (unsigned long long)used,
                           (unsigned long long)R01E_CPU_BUDGET_CYCLES);
                    if (used > 28000ull) {
                        fprintf(stderr, "FAIL C PRG seam CPU %llu (MAP copy too expensive)\n",
                                (unsigned long long)used);
                        r01e_machine_shutdown(&m);
                        return 1;
                    }
                    crossed = 1;
                    break;
                }
            }
            if (!crossed) {
                fprintf(stderr, "FAIL C PRG camera did not cross screen origin (player=%d,%d cam=%d,%d)\n",
                        m.play.player_x, m.play.player_y, m.play.cam_x, m.play.cam_y);
                r01e_machine_shutdown(&m);
                return 1;
            }
        }
    }
    if (!vram_matches_cart(&m)) {
        r01e_machine_shutdown(&m);
        return 1;
    }
    printf("ok VRAM stays synced to cart after walk origin=%d,%d\n", m.video.cam_origin_col,
           m.video.cam_origin_row);

    r01e_machine_shutdown(&m);
    return 0;
}
