#include "r01_custom_logic_scan.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
    if (strncmp(p, "R01_GAME_MODE_PLATFORMER", 24) == 0) {
        *out = 1;
        return n + 24;
    }
    if (strncmp(p, "R01_GAME_MODE_TOPDOWN", 21) == 0) {
        *out = 0;
        return n + 21;
    }
    if (strncmp(p, "R01_PLAT_GRAVITY_DEFAULT", 24) == 0) {
        *out = 0; /* 0 = engine default from r01_play_physics.h */
        return n + 24;
    }
    if (strncmp(p, "R01_PLAT_JUMP_DEFAULT", 21) == 0) {
        *out = 0;
        return n + 21;
    }
    if (strncmp(p, "R01_PLAT_METER_DEFAULT", 22) == 0) {
        *out = 0;
        return n + 22;
    }
    return 0;
}

static int ident_char(int c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int skip_ident(const char **pp) {
    const char *p = *pp;
    if (!p || !(isalpha((unsigned char)*p) || *p == '_')) {
        return 0;
    }
    p++;
    while (ident_char(*p)) {
        p++;
    }
    *pp = p;
    return 1;
}

static const char *skip_ws(const char *p) {
    while (p && *p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

/* Strip line comments, block comments, and string/char literals so calls can span lines. */
static char *load_stripped(const char *path) {
    FILE *f;
    long sz;
    char *raw;
    char *out;
    size_t n;
    size_t i;
    size_t o;
    int in_line = 0;
    int in_block = 0;
    int in_str = 0;
    int in_chr = 0;
    if (!path) {
        return NULL;
    }
    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0 || sz > 4 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    n = (size_t)sz;
    raw = (char *)malloc(n + 1u);
    out = (char *)malloc(n + 1u);
    if (!raw || !out) {
        free(raw);
        free(out);
        fclose(f);
        return NULL;
    }
    if (fread(raw, 1, n, f) != n) {
        free(raw);
        free(out);
        fclose(f);
        return NULL;
    }
    fclose(f);
    raw[n] = '\0';
    o = 0;
    for (i = 0; i < n; i++) {
        char c = raw[i];
        if (in_line) {
            if (c == '\n') {
                in_line = 0;
                out[o++] = ' ';
            }
            continue;
        }
        if (in_block) {
            if (c == '*' && i + 1 < n && raw[i + 1] == '/') {
                in_block = 0;
                i++;
                out[o++] = ' ';
            }
            continue;
        }
        if (in_str) {
            if (c == '\\' && i + 1 < n) {
                i++;
            } else if (c == '"') {
                in_str = 0;
            }
            out[o++] = ' ';
            continue;
        }
        if (in_chr) {
            if (c == '\\' && i + 1 < n) {
                i++;
            } else if (c == '\'') {
                in_chr = 0;
            }
            out[o++] = ' ';
            continue;
        }
        if (c == '/' && i + 1 < n && raw[i + 1] == '/') {
            in_line = 1;
            i++;
            continue;
        }
        if (c == '/' && i + 1 < n && raw[i + 1] == '*') {
            in_block = 1;
            i++;
            continue;
        }
        if (c == '"') {
            in_str = 1;
            out[o++] = ' ';
            continue;
        }
        if (c == '\'') {
            in_chr = 1;
            out[o++] = ' ';
            continue;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    free(raw);
    return out;
}

static const char *find_ident(const char *buf, const char *from, const char *name) {
    size_t n;
    const char *p;
    if (!buf || !from || !name) {
        return NULL;
    }
    n = strlen(name);
    p = from;
    while ((p = strstr(p, name)) != NULL) {
        if ((p == buf || !ident_char((unsigned char)p[-1])) && !ident_char((unsigned char)p[n])) {
            return p;
        }
        p += n;
    }
    return NULL;
}

static const char *call_args(const char *ident_at, const char *name) {
    const char *p;
    if (!ident_at || !name) {
        return NULL;
    }
    p = skip_ws(ident_at + strlen(name));
    if (*p != '(') {
        return NULL;
    }
    return p;
}

static int parse_first_ident(const char **pp) {
    const char *p = skip_ws(*pp);
    if (!skip_ident(&p)) {
        return 0;
    }
    *pp = p;
    return 1;
}

static int parse_ctx_two_ints(const char *args, int *out_a, int *out_b) {
    const char *p = args;
    int n0, n1;
    if (!p || p[0] != '(') {
        return -1;
    }
    p++;
    if (!parse_first_ident(&p)) {
        return -1;
    }
    p = skip_ws(p);
    if (*p != ',') {
        return -1;
    }
    p++;
    n0 = parse_int_or_known_token(p, out_a);
    if (n0 <= 0) {
        return -1;
    }
    p += n0;
    p = skip_ws(p);
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
    if (!parse_first_ident(&p)) {
        return -1;
    }
    p = skip_ws(p);
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

static int parse_ctx_only(const char *args) {
    const char *p = args;
    if (!p || p[0] != '(') {
        return -1;
    }
    p++;
    if (!parse_first_ident(&p)) {
        return -1;
    }
    p = skip_ws(p);
    if (*p != ')') {
        return -1;
    }
    return 0;
}

int r01_custom_logic_scan_deadzone(const char *path, int *out_dx, int *out_dy) {
    char *buf;
    const char *p;
    int found = 0;
    int dx = 0;
    int dy = 0;
    if (!path || !out_dx || !out_dy) {
        return -1;
    }
    buf = load_stripped(path);
    if (!buf) {
        return -1;
    }
    p = buf;
    while (*p) {
        const char *dis = find_ident(buf, p, "r01_camera_disable_deadzone");
        const char *set = find_ident(buf, p, "r01_camera_set_deadzone");
        const char *next;
        int is_disable;
        if (!dis && !set) {
            break;
        }
        if (dis && (!set || dis < set)) {
            next = dis;
            is_disable = 1;
        } else {
            next = set;
            is_disable = 0;
        }
        {
            const char *args = call_args(next, is_disable ? "r01_camera_disable_deadzone" : "r01_camera_set_deadzone");
            if (is_disable) {
                if (args && parse_ctx_only(args) == 0) {
                    dx = 0;
                    dy = 0;
                    found = 1;
                }
            } else {
                int sx = 0;
                int sy = 0;
                if (args && parse_ctx_two_ints(args, &sx, &sy) == 0) {
                    dx = sx;
                    dy = sy;
                    found = 1;
                }
            }
        }
        p = next + 1;
    }
    free(buf);
    if (!found) {
        return -1;
    }
    *out_dx = dx;
    *out_dy = dy;
    return 0;
}

static int scan_last_two_ints(const char *path, const char *call, int *out_a, int *out_b) {
    char *buf;
    const char *p;
    int found = 0;
    int a = 0;
    int b = 0;
    if (!path || !call || !out_a || !out_b) {
        return -1;
    }
    buf = load_stripped(path);
    if (!buf) {
        return -1;
    }
    p = buf;
    while ((p = find_ident(buf, p, call)) != NULL) {
        const char *args = call_args(p, call);
        int sa = 0;
        int sb = 0;
        if (args && parse_ctx_two_ints(args, &sa, &sb) == 0) {
            a = sa;
            b = sb;
            found = 1;
        }
        p += strlen(call);
    }
    free(buf);
    if (!found) {
        return -1;
    }
    *out_a = a;
    *out_b = b;
    return 0;
}

static int scan_ctx_one_named(const char *path, const char *call, int *out_v) {
    char *buf;
    const char *p;
    int found = 0;
    int v = 0;
    if (!path || !call || !out_v) {
        return -1;
    }
    buf = load_stripped(path);
    if (!buf) {
        return -1;
    }
    p = buf;
    while ((p = find_ident(buf, p, call)) != NULL) {
        const char *args = call_args(p, call);
        int n = 0;
        if (args && parse_ctx_one_int(args, &n) == 0) {
            v = n;
            found = 1;
        }
        p += strlen(call);
    }
    free(buf);
    if (!found) {
        return -1;
    }
    *out_v = v;
    return 0;
}

int r01_custom_logic_scan_bg0_wrap(const char *path, int *out_wrap_x, int *out_wrap_y) {
    return scan_last_two_ints(path, "r01_bg0_set_wrap", out_wrap_x, out_wrap_y);
}

int r01_custom_logic_scan_bg0_clip_bg1(const char *path, int *out_enable) {
    return scan_ctx_one_named(path, "r01_bg0_set_clip_to_bg1", out_enable);
}

int r01_custom_logic_scan_game_mode(const char *path, int *out_mode) {
    return scan_ctx_one_named(path, "r01_game_set_mode", out_mode);
}

int r01_custom_logic_scan_plat_gravity(const char *path, int *out_gravity) {
    return scan_ctx_one_named(path, "r01_platformer_set_gravity", out_gravity);
}

int r01_custom_logic_scan_plat_jump(const char *path, int *out_jump) {
    return scan_ctx_one_named(path, "r01_platformer_set_jump", out_jump);
}

int r01_custom_logic_scan_plat_meter(const char *path, int *out_meter) {
    return scan_ctx_one_named(path, "r01_platformer_set_meter", out_meter);
}

int r01_custom_logic_scan_plat_crouch(const char *path, int *out_state) {
    return scan_ctx_one_named(path, "r01_player_anim_set_crouch_state", out_state);
}

int r01_custom_logic_scan_player_idle(const char *path, int *out_state) {
    return scan_ctx_one_named(path, "r01_player_anim_set_idle_state", out_state);
}

int r01_custom_logic_scan_player_walk(const char *path, int *out_state) {
    return scan_ctx_one_named(path, "r01_player_anim_set_walk_all", out_state);
}

int r01_custom_logic_scan_player_jump(const char *path, int *out_state) {
    return scan_ctx_one_named(path, "r01_player_anim_set_jump_state", out_state);
}

int r01_custom_logic_scan_solid_patterns(const char *path, uint8_t *out_banks, uint8_t *out_tiles,
                                         int max_count, int *out_count) {
    char *buf;
    const char *p;
    int n = 0;
    if (!path || !out_banks || !out_tiles || !out_count || max_count < 1) {
        return -1;
    }
    if (max_count > R01_CUSTOM_SOLID_PAT_MAX) {
        max_count = R01_CUSTOM_SOLID_PAT_MAX;
    }
    buf = load_stripped(path);
    if (!buf) {
        return -1;
    }
    p = buf;
    while ((p = find_ident(buf, p, "r01_solid_pattern_add")) != NULL) {
        const char *args = call_args(p, "r01_solid_pattern_add");
        int bank = 0;
        int tile = 0;
        int i;
        p += strlen("r01_solid_pattern_add");
        if (!args || parse_ctx_two_ints(args, &bank, &tile) != 0) {
            continue;
        }
        if (bank < 0 || bank > 15 || tile < 0 || tile > 255) {
            continue;
        }
        for (i = 0; i < n; i++) {
            if ((int)out_banks[i] == bank && (int)out_tiles[i] == tile) {
                break;
            }
        }
        if (i < n) {
            continue;
        }
        if (n >= max_count) {
            continue;
        }
        out_banks[n] = (uint8_t)bank;
        out_tiles[n] = (uint8_t)tile;
        n++;
    }
    free(buf);
    *out_count = n;
    return n > 0 ? 0 : -1;
}

int r01_custom_logic_scan_bgm_play(const char *path, int *out_track) {
    return scan_ctx_one_named(path, "r01_bgm_play", out_track);
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
