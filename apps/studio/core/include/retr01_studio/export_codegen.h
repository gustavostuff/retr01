#ifndef retr01_STUDIO_EXPORT_CODEGEN_H
#define retr01_STUDIO_EXPORT_CODEGEN_H

#include "retr01_studio/types.h"

/* Write output/C, output/ASM, output/data under the directory containing path_stem. */
int r01_export_codegen(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap);

#endif
