#include "r01_custom_logic_scan.h"

#include <stdio.h>
#include <string.h>

int r01_custom_logic_scan_deadzone(const char *path, int *out_dx, int *out_dy) {
    FILE *f;
    char line[512];
    if (!path || !out_dx || !out_dy) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "r01_camera_set_deadzone");
        const char *args;
        if (!p) {
            continue;
        }
        args = strchr(p, '(');
        if (args) {
            int dx = 0;
            int dy = 0;
            if (sscanf(args, "(ctx, %d, %d)", &dx, &dy) == 2 || sscanf(args, "(ctx,%d,%d)", &dx, &dy) == 2) {
                *out_dx = dx;
                *out_dy = dy;
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
}

int r01_custom_logic_path_for_project(const char *proj_path, char *out, size_t out_cap) {
    const char *slash;
    size_t dir_len;
    if (!proj_path || !out || out_cap < 20) {
        return -1;
    }
    slash = strrchr(proj_path, '/');
    if (!slash) {
        slash = strrchr(proj_path, '\\');
    }
    if (!slash) {
        snprintf(out, out_cap, "C/custom_logic.c");
        return 0;
    }
    dir_len = (size_t)(slash - proj_path);
    if (dir_len + strlen("/C/custom_logic.c") + 1 > out_cap) {
        return -1;
    }
    memcpy(out, proj_path, dir_len);
    snprintf(out + dir_len, out_cap - dir_len, "/C/custom_logic.c");
    return 0;
}
