#ifndef R01_README_SHOT_H
#define R01_README_SHOT_H

/* Personal README F12 shots (Gus). Not a product feature.
 * Set to 0, or delete this pair + the F12 hooks in Studio/Emu, to remove. */
#ifndef R01_README_SHOT
#define R01_README_SHOT 1
#endif

#if R01_README_SHOT
#include <SDL.h>
#include <stdint.h>

/* Writes repo img/readme/<file_name>. scale is integer nearest-neighbor (1 = as-is). */
int r01_readme_shot_save_rgb(const uint8_t *rgb, int w, int h, int stride, int scale, const char *file_name);
int r01_readme_shot_save_renderer(SDL_Renderer *ren, SDL_Window *win, const char *file_name);

#endif /* R01_README_SHOT */

#endif
