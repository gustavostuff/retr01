#include "r01_custom_logic_scan.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Parse 0/1, R01_BG0_*_ON/OFF, or R01_CAM_DEADZONE_*_DEFAULT after optional whitespace.
 * Returns chars consumed, or 0. */
static int parse_int_or_known_token(const char *p, int *out) {
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
    if (strncmp(p, "R01_CAM_DEADZONE_X_DEFAULT", 26) == 0) {
        *out = 32;
        return n + 26;
    }
    if (strncmp(p, "R01_CAM_DEADZONE_Y_DEFAULT", 26) == 0) {
        *out = 30;
        return n + 26;
    }
    if (strncmp(p, "R01_CAM_DEADZONE_OFF", 19) == 0) {
        *out = 0;
        return n + 19;
    }
    return 0;
}

static int parse_ctx_two_ints(const char *args, int *out_a, int *out_b) {
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
    n0 = parse_int_or_known_token(p, out_a);
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
    n1 = parse_int_or_known_token(p, out_b);
    if (n1 <= 0) {
        return -1;
    }
    return 0;
}

static int parse_ctx_one_int(const char *args, int *out_v) {
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
    n = parse_int_or_known_token(p, out_v);
    if (n <= 0) {
        return -1;
    }
    return 0;
}

static int line_call_is_active(const char *line, const char *call, int in_block_comment) {
    const char *p;
    const char *slash;
    const char *block;
    const char *trim;
    if (!line || !call || in_block_comment) {
        return 0;
    }
    trim = line;
    while (*trim && isspace((unsigned char)*trim)) {
        trim++;
    }
    /* Block-comment body lines: " * ..." */
    if (trim[0] == '*' && trim[1] != '/') {
        return 0;
    }
    p = strstr(line, call);
    if (!p) {
        return 0;
    }
    slash = strstr(line, "//");
    if (slash && slash < p) {
        return 0;
    }
    block = strstr(line, "/*");
    if (block && block < p) {
        return 0;
    }
    return 1;
}

/* Advance in-block flag across a line (non-nested block comments). */
static void scan_update_block_comment(const char *line, int *in_block) {
    const char *p = line;
    if (!line || !in_block) {
        return;
    }
    while (*p) {
        if (!*in_block && p[0] == '/' && p[1] == '*') {
            *in_block = 1;
            p += 2;
            continue;
        }
        if (*in_block && p[0] == '*' && p[1] == '/') {
            *in_block = 0;
            p += 2;
            continue;
        }
        p++;
    }
}

static int parse_ctx_only(const char *args) {
    const char *p = args;
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
    if (*p != ')') {
        return -1;
    }
    return 0;
}

int r01_custom_logic_scan_deadzone(const char *path, int *out_dx, int *out_dy) {
    FILE *f;
    char line[512];
    int found = 0;
    int dx = 0;
    int dy = 0;
    int in_block = 0;
    if (!path || !out_dx || !out_dy) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *args;
        int line_in_block = in_block;
        scan_update_block_comment(line, &in_block);
        if (line_call_is_active(line, "r01_camera_disable_deadzone", line_in_block)) {
            args = strchr(strstr(line, "r01_camera_disable_deadzone"), '(');
            if (args && parse_ctx_only(args) == 0) {
                dx = 0;
                dy = 0;
                found = 1;
            }
            continue;
        }
        if (line_call_is_active(line, "r01_camera_set_deadzone", line_in_block)) {
            int sx = 0;
            int sy = 0;
            args = strchr(strstr(line, "r01_camera_set_deadzone"), '(');
            if (args && parse_ctx_two_ints(args, &sx, &sy) == 0) {
                dx = sx;
                dy = sy;
                found = 1;
            }
        }
    }
    fclose(f);
    if (!found) {
        return -1;
    }
    *out_dx = dx;
    *out_dy = dy;
    return 0;
}

int r01_custom_logic_scan_bg0_wrap(const char *path, int *out_wrap_x, int *out_wrap_y) {
    FILE *f;
    char line[512];
    int found = 0;
    int wx = 0;
    int wy = 0;
    int in_block = 0;
    if (!path || !out_wrap_x || !out_wrap_y) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *args;
        int sx = 0;
        int sy = 0;
        int line_in_block = in_block;
        scan_update_block_comment(line, &in_block);
        if (!line_call_is_active(line, "r01_bg0_set_wrap", line_in_block)) {
            continue;
        }
        args = strchr(strstr(line, "r01_bg0_set_wrap"), '(');
        if (args && parse_ctx_two_ints(args, &sx, &sy) == 0) {
            wx = sx;
            wy = sy;
            found = 1;
        }
    }
    fclose(f);
    if (!found) {
        return -1;
    }
    *out_wrap_x = wx;
    *out_wrap_y = wy;
    return 0;
}

int r01_custom_logic_scan_bg0_clip_bg1(const char *path, int *out_enable) {
    FILE *f;
    char line[512];
    int found = 0;
    int en = 0;
    int in_block = 0;
    if (!path || !out_enable) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        const char *args;
        int v = 0;
        int line_in_block = in_block;
        scan_update_block_comment(line, &in_block);
        if (!line_call_is_active(line, "r01_bg0_set_clip_to_bg1", line_in_block)) {
            continue;
        }
        args = strchr(strstr(line, "r01_bg0_set_clip_to_bg1"), '(');
        if (args && parse_ctx_one_int(args, &v) == 0) {
            en = v;
            found = 1;
        }
    }
    fclose(f);
    if (!found) {
        return -1;
    }
    *out_enable = en;
    return 0;
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
