#ifndef retr01_STUDIO_ENTITY_IMPORT_H
#define retr01_STUDIO_ENTITY_IMPORT_H

#include "retr01_studio/types.h"

#include <stddef.h>

#define R01_SHA1_HEX_LEN 40
#define R01_ASEPRITE_META_JSON "meta.json"
#define R01_ASEPRITE_FOLDER_FILES_MAX (R01_ENTITY_STATES_MAX + 4)

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
    int unchanged; /* 1 = listing + per-file checksums matched, nothing imported */
} R01AsepriteImportResult;

typedef struct R01AsepriteListing {
    char files[R01_ASEPRITE_LISTING_MAX][R01_ASEPRITE_REL_MAX];
    int count;
} R01AsepriteListing;

typedef struct R01AsepriteFileHash {
    char name[R01_ASEPRITE_REL_MAX]; /* filename (idle.ase) or state key (idle) */
    char sha1[R01_SHA1_HEX_LEN + 1];
} R01AsepriteFileHash;

typedef struct R01AsepriteFolderMeta {
    R01AsepriteFileHash files[R01_ASEPRITE_FOLDER_FILES_MAX];
    int count;
} R01AsepriteFolderMeta;

/* Append a new entity type from RGBA frames. Returns type index or -1. */
int r01_world_import_entity_frames(R01Project *p, R01World *w, const R01EntityImport *in, char *err_buf,
                                   size_t err_cap);

/* Overwrite type_idx in place. Preserves instances and the player mark. */
int r01_world_import_entity_frames_replace(R01Project *p, R01World *w, int type_idx, const R01EntityImport *in,
                                           char *err_buf, size_t err_cap);

int r01_aseprite_listing_scan(const char *dir, R01AsepriteListing *out, char *err_buf, size_t err_cap);
int r01_aseprite_listing_equal(const R01AsepriteListing *a, const R01Project *p);

int r01_sha1_file(const char *path, char out_hex[R01_SHA1_HEX_LEN + 1], char *err_buf, size_t err_cap);
int r01_aseprite_folder_meta_scan(const char *folder_dir, R01AsepriteFolderMeta *out, char *err_buf,
                                  size_t err_cap);
int r01_aseprite_folder_meta_load(const char *json_path, R01AsepriteFolderMeta *out, char *err_buf,
                                  size_t err_cap);
int r01_aseprite_folder_meta_save(const char *json_path, const R01AsepriteFolderMeta *meta, char *err_buf,
                                  size_t err_cap);
int r01_aseprite_folder_meta_equal(const R01AsepriteFolderMeta *a, const R01AsepriteFolderMeta *b);

/*
 * Manual import from dirname(project_path)/aseprite_entities/.
 * Does not run on project load. Returns 0 or -1.
 * Writes meta.json after a successful import: per-state sha1.
 * Re-imports a folder when any Aseprite file's checksum differs.
 * Frame delay comes from each Aseprite file's duration (milliseconds).
 */
int r01_project_import_aseprite_entities(R01Project *p, const char *project_path, R01AsepriteImportResult *out,
                                         char *err_buf, size_t err_cap);

#endif
