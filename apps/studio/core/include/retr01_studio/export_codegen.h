#ifndef retr01_STUDIO_EXPORT_CODEGEN_H
#define retr01_STUDIO_EXPORT_CODEGEN_H

#include "retr01_studio/types.h"

/* Write data/ bins, include/ id headers (overwrite OK), and game_logic.c (created once). */
int r01_export_codegen(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap);
/* Compile game_logic.c with llvm-mos to retr01.prg beside path_stem. */
int r01_export_compile_prg(const char *path_stem, char *err_buf, size_t err_cap);

#endif
