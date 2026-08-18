#ifndef RETR01_PROJECT_H
#define RETR01_PROJECT_H

#include "retr01/palette.h"
#include "retr01/screen.h"
#include "retr01/types.h"

#define RETR01_PROJECT_MAX_SCREENS 64
#define RETR01_PROJECT_TITLE_MAX 128
#define RETR01_PROJECT_PATH_MAX 512
#define RETR01_CHR_BANK_BYTES (512 * 16)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct retr01_project_screen {
    char id[32];
    retr01_screen_t screen;
} retr01_project_screen_t;

typedef struct retr01_project {
    int format_version;
    char title[RETR01_PROJECT_TITLE_MAX];
    char path[RETR01_PROJECT_PATH_MAX];
    retr01_master_palette_t palette;
    char master_palette_source[32];

    int active_world;
    int active_bank;
    int active_bg_palette;
    int active_chr_tile;

    uint8_t chr_banks[4][RETR01_CHR_BANK_BYTES];
    int chr_used[4];

    retr01_project_screen_t screens[RETR01_PROJECT_MAX_SCREENS];
    int screen_count;
    int active_screen;

    int build_start_world;
    int build_start_col;
    int build_start_row;
    char build_output_name[64];
} retr01_project_t;

void retr01_project_init_default(retr01_project_t *proj, const char *palette_v01_path);
int retr01_project_load(retr01_project_t *proj, const char *path);
int retr01_project_save(const retr01_project_t *proj, const char *path);
int retr01_project_export_retr01(const retr01_project_t *proj, const char *out_path);

retr01_project_screen_t *retr01_project_active_screen(retr01_project_t *proj);
retr01_project_screen_t *retr01_project_ensure_screen(retr01_project_t *proj, int world,
                                                      uint8_t col, uint8_t row);

#ifdef __cplusplus
}
#endif

#endif
