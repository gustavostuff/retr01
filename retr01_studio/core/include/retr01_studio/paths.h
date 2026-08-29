#ifndef retr01_STUDIO_PATHS_H
#define retr01_STUDIO_PATHS_H

#include <stddef.h>

/* Resolve a repo-relative path (e.g. output/test) to an absolute path. */
int r01_path_resolve(const char *rel, char *out, size_t out_cap);

#endif
