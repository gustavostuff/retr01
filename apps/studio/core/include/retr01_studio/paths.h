#ifndef retr01_STUDIO_PATHS_H
#define retr01_STUDIO_PATHS_H

#include <stddef.h>

/* Resolve a repo-relative path (e.g. output/foo) to an absolute path. */
int r01_path_resolve(const char *rel, char *out, size_t out_cap);

/* Export stem: dirname(project)/basename_without_ext, else output/<name>. */
int r01_export_stem(const char *project_path, const char *fallback_name, char *out, size_t out_cap);

#endif
