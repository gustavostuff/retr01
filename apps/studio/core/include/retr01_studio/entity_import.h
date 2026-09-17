#ifndef retr01_STUDIO_ENTITY_IMPORT_H
#define retr01_STUDIO_ENTITY_IMPORT_H

#include "retr01_studio/types.h"

#include <stddef.h>

typedef struct R01EntityImportFrame {
    const uint8_t *rgba; /* w * h * 4 */
    int w;
    int h;
    int delay; /* display frames, min 1 */
} R01EntityImportFrame;

typedef struct R01EntityImportState {
    char name[R01_ENTITY_NAME_MAX];
    int frame_count;
    R01EntityImportFrame frames[R01_ENTITY_FRAMES_MAX];
} R01EntityImportState;

typedef struct R01EntityImport {
    char name[R01_ENTITY_NAME_MAX];
    int state_count;
    R01EntityImportState states[R01_ENTITY_STATES_MAX];
} R01EntityImport;

typedef struct R01AsepriteImportResult {
    int generated;
    int unchanged; /* 1 = file listing matched the saved snapshot */
} R01AsepriteImportResult;

typedef struct R01AsepriteListing {
    char files[R01_ASEPRITE_LISTING_MAX][R01_ASEPRITE_REL_MAX];
    int count;
} R01AsepriteListing;

/* Append a new entity type from RGBA frames. Returns type index or -1. */
int r01_world_import_entity_frames(R01Project *p, R01World *w, const R01EntityImport *in, char *err_buf,
                                   size_t err_cap);

int r01_aseprite_listing_scan(const char *dir, R01AsepriteListing *out, char *err_buf, size_t err_cap);
int r01_aseprite_listing_equal(const R01AsepriteListing *a, const R01Project *p);

/*
 * Manual import from dirname(project_path)/aseprite_entities/.
 * Does not run on project load. Returns 0 or -1.
 */
int r01_project_import_aseprite_entities(R01Project *p, const char *project_path, R01AsepriteImportResult *out,
                                         char *err_buf, size_t err_cap);

#endif
