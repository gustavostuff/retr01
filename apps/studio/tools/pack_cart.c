#include "retr01_studio/cart.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stem_from_json(const char *json, char *out, size_t cap) {
    char *dot;
    snprintf(out, cap, "%s", json ? json : "");
    dot = strrchr(out, '.');
    if (dot && strcmp(dot, ".r01proj") == 0) {
        *dot = '\0';
    }
}

int main(int argc, char **argv) {
    R01Project *p;
    char err[512];
    char stem[512];
    const char *json;
    const char *out_stem;

    if (argc < 2 || !argv[1] || !argv[1][0]) {
        fprintf(stderr, "usage: r01_pack_cart project.r01proj [out_stem]\n");
        return 1;
    }
    json = argv[1];
    if (argc > 2 && argv[2] && argv[2][0]) {
        out_stem = argv[2];
    } else {
        stem_from_json(json, stem, sizeof(stem));
        out_stem = stem;
    }

    p = (R01Project *)calloc(1, sizeof(R01Project));
    if (!p) {
        fprintf(stderr, "oom\n");
        return 1;
    }
    if (r01_project_load_json(p, json, err, sizeof(err)) != 0) {
        fprintf(stderr, "%s\n", err);
        free(p);
        return 1;
    }
    if (r01_export_bundle(p, out_stem, err, sizeof(err)) != 0) {
        fprintf(stderr, "%s\n", err);
        free(p);
        return 1;
    }
    printf("wrote %s.retr01\n", out_stem);
    free(p);
    return 0;
}
