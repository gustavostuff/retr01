#ifndef retr01_STUDIO_UI_UNDO_H
#define retr01_STUDIO_UI_UNDO_H

#include <stddef.h>

struct UiState;

#define UI_UNDO_DEPTH_MAX 64
#define UI_UNDO_LABEL_MAX 32

typedef struct UiUndoCmd UiUndoCmd;

typedef struct UiUndoVTable {
    void (*undo)(struct UiState *ui, void *data);
    void (*redo)(struct UiState *ui, void *data);
    void (*destroy)(void *data);
} UiUndoVTable;

struct UiUndoCmd {
    const UiUndoVTable *vt;
    void *data;
    char label[UI_UNDO_LABEL_MAX];
};

typedef struct UiUndoStack {
    UiUndoCmd cmds[UI_UNDO_DEPTH_MAX];
    int count;  /* entries 0..count-1 live */
    int cursor; /* next push index; undo applies cmds[cursor-1] */
} UiUndoStack;

void ui_undo_init(UiUndoStack *st);
void ui_undo_clear(UiUndoStack *st);
void ui_undo_shutdown(UiUndoStack *st);

/* Takes ownership of data on success (calls destroy on replace/clear). Returns 0 ok, -1 fail. */
int ui_undo_push(UiUndoStack *st, const UiUndoVTable *vt, void *data, const char *label);

int ui_undo_can_undo(const UiUndoStack *st);
int ui_undo_can_redo(const UiUndoStack *st);
int ui_undo_undo(struct UiState *ui);
int ui_undo_redo(struct UiState *ui);

#endif
