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
