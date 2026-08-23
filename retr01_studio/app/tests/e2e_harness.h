#ifndef RETR01_E2E_HARNESS_H
#define RETR01_E2E_HARNESS_H

#include "app_shell.h"

#include <stdint.h>

#ifndef R01_E2E_GOLDEN_DIR
#define R01_E2E_GOLDEN_DIR "goldens"
#endif
#ifndef R01_E2E_OUT_DIR
#define R01_E2E_OUT_DIR "out"
#endif

/* 1 if UPDATE_GOLDENS=1 in environment. */
int e2e_update_goldens(void);

/* 1 if E2E_WATCH=1 — show a real window and slow-step through actions. */
int e2e_watch(void);

/* Present + optional watch delay. Call after UI mutations when watching. */
void e2e_pump(AppShell *app);

void e2e_reset_project(AppShell *app);
void e2e_set_workdir(AppShell *app, const char *stem_path);

void e2e_key(AppShell *app, SDL_Keycode sym, Uint16 mod);
void e2e_click(AppShell *app, int lx, int ly);
void e2e_ctrl_click(AppShell *app, int lx, int ly);
void e2e_drag(AppShell *app, int x0, int y0, int x1, int y1);
void e2e_wheel(AppShell *app, int lx, int ly, int wheel_y);

/* Scroll left column to bottom (Constraints visible). */
void e2e_scroll_left_bottom(AppShell *app);

/* Capture canvas; compare to goldens/<name>.bmp (or write if UPDATE_GOLDENS). */
int e2e_assert_golden(AppShell *app, const char *name);

/* Save capture to out/ for debugging. */
int e2e_dump(AppShell *app, const char *name);

#endif
