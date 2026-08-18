#ifndef RETR01_CART_H
#define RETR01_CART_H

#include "retr01/types.h"

#ifdef __cplusplus
extern "C" {
#endif

void retr01_cart_init(retr01_cart_t *cart);
void retr01_cart_free(retr01_cart_t *cart);

int retr01_cart_load_file(const char *path, retr01_cart_t *out);
int retr01_cart_write_file(const char *path, const retr01_cart_t *cart);

#ifdef __cplusplus
}
#endif

#endif
