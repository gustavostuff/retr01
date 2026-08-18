#include "retr01/chr_pack.h"
#include "retr01/palette.h"
#include "retr01/project.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifndef RETR01_PALETTE_V01_PATH
#define RETR01_PALETTE_V01_PATH "retr01_world_studio/retr01_palette_v_01.txt"
#endif

int main(int argc, char **argv)
{
    retr01_project_t proj;
    const char *in_path;
    const char *out_path;
    int unique;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s project.r01proj out.retr01\n", argv[0]);
        return 1;
    }

    in_path = argv[1];
    out_path = argv[2];

    memset(&proj, 0, sizeof(proj));
    if (retr01_project_load(&proj, in_path) != 0) {
        fprintf(stderr, "failed to load project: %s (%s)\n", in_path, strerror(errno));
        return 1;
    }

    retr01_palette_load_v01(RETR01_PALETTE_V01_PATH, &proj.palette);

    unique = retr01_project_pack(&proj);
    if (unique < 0) {
        fprintf(stderr, "Generate/pack failed (unique tiles > 256?)\n");
        retr01_project_free(&proj);
        return 1;
    }

    if (retr01_project_export_retr01(&proj, out_path) != 0) {
        fprintf(stderr, "failed to write cart: %s\n", out_path);
        retr01_project_free(&proj);
        return 1;
    }

    printf("wrote %s  screens=%d  unique_tiles=%d  start=world %d (%d,%d)\n", out_path,
           proj.screen_count, proj.chr_used[proj.active_bank], proj.build_start_world,
           proj.build_start_col, proj.build_start_row);

    retr01_project_free(&proj);
    return 0;
}
