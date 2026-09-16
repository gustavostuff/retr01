#include "ui/modals/project_io_internal.h"

#include "retr01_studio/paths.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef R01_STUDIO_PROJECTS_DIR
#define R01_STUDIO_PROJECTS_DIR "apps/studio/projects"
#endif

int project_io_path_is_dir(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int project_io_path_is_file(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int ends_with_ci(const char *path, const char *suffix) {
    size_t pl, sl;
    const char *p;
    if (!path || !suffix) {
        return 0;
    }
    pl = strlen(path);
    sl = strlen(suffix);
    if (pl < sl) {
        return 0;
    }
    p = path + pl - sl;
    while (*suffix) {
        char a = *p++;
        char b = *suffix++;
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

void project_io_default_projects_dir(char *out, size_t cap) {
    if (r01_path_resolve(R01_STUDIO_PROJECTS_DIR, out, cap) != 0) {
        snprintf(out, cap, "%s", R01_STUDIO_PROJECTS_DIR);
    }
}

void project_io_sanitize_name(const char *in, char *out, size_t cap) {
    size_t w = 0;
    if (!out || cap < 2) {
        return;
    }
    out[0] = '\0';
    if (!in) {
        return;
    }
    while (*in && w + 1 < cap) {
        unsigned char c = (unsigned char)*in++;
        if (isalnum(c) || c == '-' || c == '_') {
            out[w++] = (char)c;
        } else if (c == ' ' || c == '.') {
            out[w++] = '_';
        }
    }
    out[w] = '\0';
    while (w > 0 && out[w - 1] == '_') {
        out[--w] = '\0';
    }
}

int project_io_mkdir_p(const char *path) {
    char tmp[R01_PATH_MAX];
    size_t len;
    size_t i;
    if (!path || !path[0]) {
        return -1;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) {
        return -1;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (tmp[0] && !project_io_path_is_dir(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            tmp[i] = '/';
        }
    }
    if (!project_io_path_is_dir(tmp)) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

static int cmp_entry(const void *a, const void *b) {
    const char *sa = (const char *)a;
    const char *sb = (const char *)b;
    int da = (sa[0] == '/');
    int db = (sb[0] == '/');
    if (da != db) {
        return db - da;
    }
    return strcasecmp(sa + da, sb + db);
}

void project_io_refresh_listing(UiState *ui) {
    UiProjectIo *pio;
    DIR *d;
    struct dirent *ent;
    char path[R01_PATH_MAX];
    int n = 0;

    if (!ui) {
        return;
    }
    pio = &ui->project_io;
    pio->entry_count = 0;
    pio->sel = -1;
    pio->scroll = 0;
    if (!pio->dir[0] || !project_io_path_is_dir(pio->dir)) {
        return;
    }
    d = opendir(pio->dir);
    if (!d) {
        return;
    }
    while ((ent = readdir(d)) != NULL && n < UI_PROJ_IO_MAX_ENTRIES) {
        const char *name = ent->d_name;
        int is_dir;
        if (!name || strcmp(name, ".") == 0) {
            continue;
        }
        if (strcmp(name, "..") == 0) {
            snprintf(pio->entries[n], sizeof(pio->entries[n]), "/..");
            pio->entry_is_dir[n] = 1;
            n++;
            continue;
        }
        if (name[0] == '.') {
            continue;
        }
        if (snprintf(path, sizeof(path), "%s/%s", pio->dir, name) >= (int)sizeof(path)) {
            continue;
        }
        is_dir = project_io_path_is_dir(path);
        if (!is_dir && !ends_with_ci(name, ".r01proj")) {
            continue;
        }
        if (is_dir) {
            size_t nlen = strlen(name);
            if (nlen + 2u > sizeof(pio->entries[n])) {
                continue;
            }
            pio->entries[n][0] = '/';
            memcpy(pio->entries[n] + 1, name, nlen + 1u);
        } else {
            snprintf(pio->entries[n], sizeof(pio->entries[n]), "%s", name);
        }
        pio->entry_is_dir[n] = is_dir;
        n++;
    }
    closedir(d);
    qsort(pio->entries, (size_t)n, sizeof(pio->entries[0]), cmp_entry);
    for (int i = 0; i < n; i++) {
        pio->entry_is_dir[i] = (pio->entries[i][0] == '/');
    }
    pio->entry_count = n;
}

void project_io_set_dir(UiState *ui, const char *dir) {
    char resolved[R01_PATH_MAX];
    if (!ui || !dir) {
        return;
    }
    if (realpath(dir, resolved)) {
        snprintf(ui->project_io.dir, sizeof(ui->project_io.dir), "%s", resolved);
    } else {
        snprintf(ui->project_io.dir, sizeof(ui->project_io.dir), "%s", dir);
    }
    project_io_refresh_listing(ui);
}

void project_io_parent_dir(char *path) {
    char *slash;
    size_t n;
    if (!path || !path[0]) {
        return;
    }
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/') {
        path[--n] = '\0';
    }
    slash = strrchr(path, '/');
    if (!slash) {
        return;
    }
    if (slash == path) {
        path[1] = '\0';
        return;
    }
    *slash = '\0';
}

int project_io_find_proj_in_dir(const char *dir, char *out, size_t out_cap) {
    DIR *d;
    struct dirent *ent;
    char preferred[R01_PATH_MAX];
    const char *base;
    if (!dir || !out || out_cap < 2) {
        return -1;
    }
    base = strrchr(dir, '/');
    base = base ? base + 1 : dir;
    if (snprintf(preferred, sizeof(preferred), "%s/%s.r01proj", dir, base) < (int)sizeof(preferred) &&
        project_io_path_is_file(preferred)) {
        snprintf(out, out_cap, "%s", preferred);
        return 0;
    }
    d = opendir(dir);
    if (!d) {
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        if (n > 8 && strcmp(ent->d_name + n - 8, ".r01proj") == 0) {
            snprintf(out, out_cap, "%s/%s", dir, ent->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

int project_io_selection_is_openable(const UiState *ui, char *out_path, size_t out_cap) {
    const UiProjectIo *pio;
    const char *ent;
    char path[R01_PATH_MAX];
    if (!ui || ui->project_io.sel < 0 || ui->project_io.sel >= ui->project_io.entry_count) {
        return 0;
    }
    pio = &ui->project_io;
    ent = pio->entries[pio->sel];
    if (pio->entry_is_dir[pio->sel]) {
        if (strcmp(ent, "/..") == 0) {
            return 0;
        }
        if (snprintf(path, sizeof(path), "%s/%s", pio->dir, ent + 1) >= (int)sizeof(path)) {
            return 0;
        }
        return project_io_find_proj_in_dir(path, out_path, out_cap) == 0;
    }
    if (snprintf(path, sizeof(path), "%s/%s", pio->dir, ent) >= (int)sizeof(path)) {
        return 0;
    }
    if (!ends_with_ci(path, ".r01proj") || !project_io_path_is_file(path)) {
        return 0;
    }
    snprintf(out_path, out_cap, "%s", path);
    return 1;
}

void project_io_enter_selected_dir(UiState *ui) {
    UiProjectIo *pio = &ui->project_io;
    char next[R01_PATH_MAX];
    const char *ent;
    if (pio->sel < 0 || pio->sel >= pio->entry_count || !pio->entry_is_dir[pio->sel]) {
        return;
    }
    ent = pio->entries[pio->sel];
    if (strcmp(ent, "/..") == 0) {
        snprintf(next, sizeof(next), "%s", pio->dir);
        project_io_parent_dir(next);
        project_io_set_dir(ui, next);
        return;
    }
    if (snprintf(next, sizeof(next), "%s/%s", pio->dir, ent + 1) >= (int)sizeof(next)) {
        return;
    }
    project_io_set_dir(ui, next);
}

