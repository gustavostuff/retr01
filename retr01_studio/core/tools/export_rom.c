#include "retr01_studio/cart.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    R01Project p;
    char err[512];
    const char *proj;
    const char *stem;

    proj = argc > 1 ? argv[1] : R01_DEFAULT_PROJECT;
    stem = argc > 2 ? argv[2] : R01_DEFAULT_CART_STEM;
    if (r01_project_load_json(&p, proj, err, sizeof(err)) != 0) {
        fprintf(stderr, "load failed: %s\n", err);
        return 1;
    }
    if (r01_export_bundle(&p, stem, err, sizeof(err)) != 0) {
        fprintf(stderr, "export failed: %s\n", err);
        return 1;
    }
    printf("exported %s from %s\n", stem, proj);
    return 0;
}
