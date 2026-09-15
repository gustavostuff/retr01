#include "ui/undo/undo.h"
#include "ui/ui.h"
#include "ui/internal.h"

#include <stdio.h>
#include <string.h>

static void cmd_destroy(UiUndoCmd *cmd) {
    if (!cmd) {
        return;
    }
    if (cmd->vt && cmd->vt->destroy && cmd->data) {
        cmd->vt->destroy(cmd->data);
    }
    cmd->vt = NULL;
    cmd->data = NULL;
    cmd->label[0] = '\0';
}

void ui_undo_init(UiUndoStack *st) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
}

void ui_undo_clear(UiUndoStack *st) {
    int i;
    if (!st) {
        return;
    }
    for (i = 0; i < st->count; i++) {
        cmd_destroy(&st->cmds[i]);
    }
    st->count = 0;
    st->cursor = 0;
}

void ui_undo_shutdown(UiUndoStack *st) {
    ui_undo_clear(st);
}

int ui_undo_push(UiUndoStack *st, const UiUndoVTable *vt, void *data, const char *label) {
    int i;
    if (!st || !vt || !data) {
        if (vt && vt->destroy && data) {
            vt->destroy(data);
        }
        return -1;
    }
    /* Drop redo tail. */
    for (i = st->cursor; i < st->count; i++) {
        cmd_destroy(&st->cmds[i]);
    }
    st->count = st->cursor;
    if (st->count >= UI_UNDO_DEPTH_MAX) {
        cmd_destroy(&st->cmds[0]);
        memmove(&st->cmds[0], &st->cmds[1], (size_t)(UI_UNDO_DEPTH_MAX - 1) * sizeof(st->cmds[0]));
        st->count = UI_UNDO_DEPTH_MAX - 1;
        st->cursor = st->count;
    }
    st->cmds[st->count].vt = vt;
    st->cmds[st->count].data = data;
    st->cmds[st->count].label[0] = '\0';
    if (label && label[0]) {
        strncpy(st->cmds[st->count].label, label, UI_UNDO_LABEL_MAX - 1);
        st->cmds[st->count].label[UI_UNDO_LABEL_MAX - 1] = '\0';
    }
    st->count++;
    st->cursor = st->count;
    return 0;
}

int ui_undo_can_undo(const UiUndoStack *st) {
    return st && st->cursor > 0;
}

int ui_undo_can_redo(const UiUndoStack *st) {
    return st && st->cursor < st->count;
}

int ui_undo_undo(UiState *ui) {
    UiUndoCmd *cmd;
    if (!ui || !ui_undo_can_undo(&ui->undo)) {
        return 0;
    }
    ui->undo.cursor--;
    cmd = &ui->undo.cmds[ui->undo.cursor];
    if (cmd->vt && cmd->vt->undo) {
        cmd->vt->undo(ui, cmd->data);
    }
    if (cmd->label[0]) {
        char msg[48];
        snprintf(msg, sizeof(msg), "undo %s", cmd->label);
        ui_toast(ui, msg, 0);
    } else {
        ui_toast(ui, "undo", 0);
    }
    return 1;
}

int ui_undo_redo(UiState *ui) {
    UiUndoCmd *cmd;
    if (!ui || !ui_undo_can_redo(&ui->undo)) {
        return 0;
    }
    cmd = &ui->undo.cmds[ui->undo.cursor];
    if (cmd->vt && cmd->vt->redo) {
        cmd->vt->redo(ui, cmd->data);
    }
    ui->undo.cursor++;
    if (cmd->label[0]) {
        char msg[48];
        snprintf(msg, sizeof(msg), "redo %s", cmd->label);
        ui_toast(ui, msg, 0);
    } else {
        ui_toast(ui, "redo", 0);
    }
    return 1;
}
