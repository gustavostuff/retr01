#include "retr01_studio/paths.h"
#include "retr01_studio/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int r01_repo_root(char *out, size_t cap) {
    const char *env;

    if (!out || cap < 2) {
        return -1;
    }
    env = getenv("R01_ROOT");
    if (env && env[0]) {
        snprintf(out, cap, "%s", env);
        return 0;
    }
#ifdef R01_REPO_ROOT
    snprintf(out, cap, "%s", R01_REPO_ROOT);
    return 0;
#endif
    if (getcwd(out, cap)) {
        return 0;
    }
    return -1;
}

int r01_path_resolve(const char *rel, char *out, size_t out_cap) {
    char root[R01_PATH_MAX];

    if (!rel || !out || out_cap < 2) {
        return -1;
    }
    if (rel[0] == '/') {
        snprintf(out, out_cap, "%s", rel);
        return 0;
    }
    if (r01_repo_root(root, sizeof(root)) != 0) {
        snprintf(out, out_cap, "%s", rel);
        return 0;
    }
    snprintf(out, out_cap, "%s/%s", root, rel);
    return 0;
}

int r01_export_stem(const char *project_path, const char *fallback_name, char *out, size_t out_cap) {
    char tmp[R01_PATH_MAX];
    char name[R01_PATH_MAX];
    const char *slash;
    const char *base;
    char *dot;
    size_t n;

    if (!out || out_cap < 2) {
        return -1;
    }
    out[0] = '\0';
    if (project_path && project_path[0]) {
        slash = strrchr(project_path, '/');
        base = slash ? slash + 1 : project_path;
        snprintf(name, sizeof(name), "%s", base);
        dot = strrchr(name, '.');
        if (dot && (strcmp(dot, ".r01proj") == 0 || strcmp(dot, ".json") == 0)) {
            *dot = '\0';
        }
        if (!name[0]) {
            snprintf(name, sizeof(name), "%s", fallback_name && fallback_name[0] ? fallback_name : "project");
        }
        if (slash) {
            n = (size_t)(slash - project_path);
            if (n >= sizeof(tmp)) {
                n = sizeof(tmp) - 1;
            }
            memcpy(tmp, project_path, n);
            tmp[n] = '\0';
            if (snprintf(out, out_cap, "%s/%s", tmp, name) >= (int)out_cap) {
                return -1;
            }
        } else {
            if (snprintf(out, out_cap, "%s", name) >= (int)out_cap) {
                return -1;
            }
        }
        return 0;
    }
    {
        const char *stem = fallback_name && fallback_name[0] ? fallback_name : "project";
        char rel[R01_PATH_MAX];
        if (snprintf(rel, sizeof(rel), "%s/%s", R01_OUTPUT_DIR, stem) >= (int)sizeof(rel)) {
            return -1;
        }
        return r01_path_resolve(rel, out, out_cap);
    }
}
