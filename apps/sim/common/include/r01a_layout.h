#ifndef R01A_LAYOUT_H
#define R01A_LAYOUT_H

struct R01aBoard;

int r01a_layout_save(const char *path, const struct R01aBoard *board, int pan_x, int pan_y, int zoom,
                    int air_always);
int r01a_layout_load(const char *path, struct R01aBoard *board, int *pan_x, int *pan_y, int *zoom,
                    int *air_always);

#endif
