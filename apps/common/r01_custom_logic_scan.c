#include "r01_custom_logic_scan.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Parse 0/1 or R01_BG0_*_ON/OFF after optional whitespace. Returns chars consumed, or 0. */
static int parse_bg0_flag_token(const char *p, int *out) {
    int n = 0;
    while (*p && isspace((unsigned char)*p)) {
        p++;
        n++;
    }
    if (sscanf(p, "%d", out) == 1) {
        int digits = 0;
        const char *q = p;
        if (*q == '+' || *q == '-') {
            q++;
        }
        while (*q && isdigit((unsigned char)*q)) {
            q++;
            digits++;
        }
        if (digits < 1) {
            return 0;
        }
        return n + (int)(q - p);
    }
    if (strncmp(p, "R01_BG0_WRAP_ON", 15) == 0) {
        *out = 1;
        return n + 15;
    }
    if (strncmp(p, "R01_BG0_WRAP_OFF", 16) == 0) {
        *out = 0;
        return n + 16;
    }
    if (strncmp(p, "R01_BG0_CLIP_ON", 15) == 0) {
        *out = 1;
        return n + 15;
    }
    if (strncmp(p, "R01_BG0_CLIP_OFF", 16) == 0) {
        *out = 0;
        return n + 16;
    }
    return 0;
}

static int parse_bg0_wrap_args(const char *args, int *out_wx, int *out_wy) {
    const char *p = args;
    int n0, n1;
    if (!p || p[0] != '(') {
        return -1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (strncmp(p, "ctx", 3) != 0) {
        return -1;
    }
    p += 3;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != ',') {
        return -1;
    }
    p++;
    n0 = parse_bg0_flag_token(p, out_wx);
    if (n0 <= 0) {
        return -1;
    }
    p += n0;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != ',') {
        return -1;
    }
    p++;
    n1 = parse_bg0_flag_token(p, out_wy);
    if (n1 <= 0) {
        return -1;
    }
    return 0;
}

static int parse_bg0_clip_args(const char *args, int *out_enable) {
    const char *p = args;
    int n;
    if (!p || p[0] != '(') {
        return -1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (strncmp(p, "ctx", 3) != 0) {
        return -1;
    }
    p += 3;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != ',') {
        return -1;
    }
    p++;
    n = parse_bg0_flag_token(p, out_enable);
    if (n <= 0) {
        return -1;
    }
    return 0;
}

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

int r01_custom_logic_scan_bg0_wrap(const char *path, int *out_wrap_x, int *out_wrap_y) {
    FILE *f;
    char line[512];
    if (!path || !out_wrap_x || !out_wrap_y) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "r01_bg0_set_wrap");
        const char *args;
        int wx = 0;
        int wy = 0;
        if (!p) {
            continue;
        }
        args = strchr(p, '(');
        if (args && parse_bg0_wrap_args(args, &wx, &wy) == 0) {
            *out_wrap_x = wx;
            *out_wrap_y = wy;
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}

int r01_custom_logic_scan_bg0_clip_bg1(const char *path, int *out_enable) {
    FILE *f;
    char line[512];
    if (!path || !out_enable) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "r01_bg0_set_clip_to_bg1");
        const char *args;
        int en = 0;
        if (!p) {
            continue;
        }
        args = strchr(p, '(');
        if (args && parse_bg0_clip_args(args, &en) == 0) {
            *out_enable = en;
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return -1;
}

int r01_custom_logic_scan_bgm_play(const char *path, int *out_track) {
    FILE *f;
    char line[512];
    if (!path || !out_track) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "r01_bgm_play");
        const char *args;
        if (!p) {
            continue;
        }
        args = strchr(p, '(');
        if (args) {
            int track = 0;
            if (sscanf(args, "(ctx, %d)", &track) == 1 || sscanf(args, "(ctx,%d)", &track) == 1) {
                if (track >= 1) {
                    *out_track = track;
                    fclose(f);
                    return 0;
                }
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

int r01_custom_logic_path_for_output(const char *output_root, char *out, size_t out_cap) {
    if (!output_root || !out || out_cap < 24) {
        return -1;
    }
    if (snprintf(out, out_cap, "%s/C/custom_logic.c", output_root) >= (int)out_cap) {
        return -1;
    }
    return 0;
}

int r01_bgm_track_bin_path(const char *output_root, int track_1based, char *out, size_t out_cap) {
    if (!output_root || !out || out_cap < 32 || track_1based < 1) {
        return -1;
    }
    if (snprintf(out, out_cap, "%s/data/bgm_track%d.bin", output_root, track_1based) >= (int)out_cap) {
        return -1;
    }
    return 0;
}
