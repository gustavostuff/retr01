#ifndef RETR01_STUDIO_JSON_IO_H
#define RETR01_STUDIO_JSON_IO_H

#include "retr01_studio/types.h"

#include <stddef.h>

/* 0 = ok, -1 = error (message in err_buf if non-NULL). */
int r01_project_save_json(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_project_load_json(R01Project *p, const char *path, char *err_buf, size_t err_cap);

#endif
