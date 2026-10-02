#include "r01a_ui.h"

#include <string.h>

static void hist_capture(const R01aUi *ui, R01aHistSnap *s) {
    int i;
    s->part_n = 0;
    for (i = 0; i < ui->chip_count && s->part_n < R01A_BOARD_MAX_CHIPS; i++) {
        const NsEntity *e = ui->chips[i];
        R01aHistPart *p;
        if (!e || !e->refdes) {
            continue;
        }
        p = &s->parts[s->part_n++];
        memset(p, 0, sizeof(*p));
        strncpy(p->ref, e->refdes, R01A_HIST_REF - 1);
        p->x = e->board_x;
        p->y = e->board_y;
        p->orient = (int)e->orient;
        if (e->visual == NS_ENTITY_VIS_PASSIVE) {
            const NsPassive *pas = (const NsPassive *)e;
            p->px = pas->pivot_x;
            p->py = pas->pivot_y;
        }
        if (e->visual == NS_ENTITY_VIS_DISPLAY) {
            p->scale_2x = ns_video_sink_scale_2x((const NsVideoSink *)e);
        }
    }
    s->trace_n = ui->trace_n;
    if (s->trace_n > R01A_TRACE_MAX) {
        s->trace_n = R01A_TRACE_MAX;
    }
    if (s->trace_n > 0) {
        memcpy(s->traces, ui->traces, sizeof(ui->traces[0]) * (size_t)s->trace_n);
    }
}

static void hist_restore(R01aUi *ui, const R01aHistSnap *s) {
    int i;
    for (i = 0; i < s->part_n; i++) {
        const R01aHistPart *p = &s->parts[i];
        NsEntity *e = ui_ent(ui, p->ref);
        if (!e) {
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_PASSIVE) {
            NsPassive *pas = (NsPassive *)e;
            pas->pivot_x = p->px;
            pas->pivot_y = p->py;
            e->board_x = p->x;
            e->board_y = p->y;
            ns_passive_set_orient(pas, (NsPkgOrient)p->orient);
            e->board_x = p->x;
            e->board_y = p->y;
            pas->pivot_x = p->px;
            pas->pivot_y = p->py;
        } else {
            ns_entity_set_orient(e, (NsPkgOrient)p->orient);
            ns_entity_place(e, p->x, p->y);
        }
        if (e->visual == NS_ENTITY_VIS_DISPLAY) {
            NsVideoSink *sink = (NsVideoSink *)e;
            if (ns_video_sink_scale_2x(sink) != p->scale_2x) {
                ns_video_sink_set_scale_2x(sink, p->scale_2x);
            }
        }
    }
    ui->trace_n = s->trace_n;
    if (ui->trace_n > 0) {
        memcpy(ui->traces, s->traces, sizeof(ui->traces[0]) * (size_t)ui->trace_n);
    }
    arm_cancel(ui);
}

void hist_init(R01aUi *ui) {
    memset(&ui->hist, 0, sizeof(ui->hist));
    hist_capture(ui, &ui->hist.snap[0]);
    ui->hist.n = 1;
    ui->hist.cur = 0;
}

void hist_after(R01aUi *ui) {
    R01aHistory *h = &ui->hist;
    if (h->cur + 1 < h->n) {
        h->n = h->cur + 1;
    }
    if (h->n >= R01A_HIST_MAX) {
        memmove(&h->snap[0], &h->snap[1], sizeof(h->snap[0]) * (R01A_HIST_MAX - 1));
        h->n = R01A_HIST_MAX - 1;
        h->cur = h->n - 1;
    }
    hist_capture(ui, &h->snap[h->n]);
    h->cur = h->n;
    h->n++;
}

int hist_undo(R01aUi *ui) {
    if (ui->hist.cur <= 0) {
        return 0;
    }
    ui->hist.cur--;
    hist_restore(ui, &ui->hist.snap[ui->hist.cur]);
    return 1;
}

int hist_redo(R01aUi *ui) {
    if (ui->hist.cur + 1 >= ui->hist.n) {
        return 0;
    }
    ui->hist.cur++;
    hist_restore(ui, &ui->hist.snap[ui->hist.cur]);
    return 1;
}
