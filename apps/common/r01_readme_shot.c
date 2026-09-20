#include "r01_readme_shot.h"

#if R01_README_SHOT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef R01_REPO_ROOT
#define R01_REPO_ROOT "."
#endif

static uint32_t crc32_bytes(const uint8_t *p, size_t n) {
    static uint32_t tbl[256];
    static int ready;
    uint32_t c = 0xffffffffu;
    size_t i;
    if (!ready) {
        uint32_t k, b, v;
        for (k = 0; k < 256; k++) {
            v = k;
            for (b = 0; b < 8; b++) {
                v = (v & 1u) ? (0xEDB88320u ^ (v >> 1)) : (v >> 1);
            }
            tbl[k] = v;
        }
        ready = 1;
    }
    for (i = 0; i < n; i++) {
        c = tbl[(c ^ p[i]) & 0xffu] ^ (c >> 8);
    }
    return c ^ 0xffffffffu;
}

static uint32_t adler32_bytes(const uint8_t *p, size_t n) {
    uint32_t a = 1, b = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        a += p[i];
        if (a >= 65521u) {
            a -= 65521u;
        }
        b += a;
        if (b >= 65521u) {
            b %= 65521u;
        }
    }
    return (b << 16) | a;
}

static void be32(uint8_t *d, uint32_t v) {
    d[0] = (uint8_t)(v >> 24);
    d[1] = (uint8_t)(v >> 16);
    d[2] = (uint8_t)(v >> 8);
    d[3] = (uint8_t)v;
}

static int write_chunk(FILE *f, const char type[4], const uint8_t *data, uint32_t len) {
    uint8_t hdr[8];
    uint8_t crcbuf[4];
    uint32_t crc;
    uint8_t *tmp;
    be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    if (fwrite(hdr, 1, 8, f) != 8) {
        return -1;
    }
    tmp = (uint8_t *)malloc(4u + len);
    if (!tmp) {
        return -1;
    }
    memcpy(tmp, type, 4);
    if (len && data) {
        memcpy(tmp + 4, data, len);
    }
    crc = crc32_bytes(tmp, 4u + len);
    free(tmp);
    if (len && data && fwrite(data, 1, len, f) != len) {
        return -1;
    }
    be32(crcbuf, crc);
    return fwrite(crcbuf, 1, 4, f) == 4 ? 0 : -1;
}

static int write_png_rgb(const char *path, int w, int h, const uint8_t *rgb, int stride) {
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    FILE *f;
    uint8_t ihdr[13];
    uint8_t *raw = NULL;
    uint8_t *zlib = NULL;
    int y;
    size_t raw_n, z_n, off;
    uint32_t adler;
    int rc = -1;
    if (!path || !rgb || w < 1 || h < 1 || stride < w * 3) {
        return -1;
    }
    raw_n = (size_t)h * (size_t)(1 + w * 3);
    raw = (uint8_t *)malloc(raw_n);
    if (!raw) {
        return -1;
    }
    for (y = 0; y < h; y++) {
        size_t row = (size_t)y * (size_t)(1 + w * 3);
        raw[row] = 0;
        memcpy(raw + row + 1, rgb + (size_t)y * (size_t)stride, (size_t)w * 3u);
    }
    adler = adler32_bytes(raw, raw_n);
    /* zlib header + stored blocks + adler */
    z_n = 2u + ((raw_n + 65534u) / 65535u) * 5u + raw_n + 4u;
    zlib = (uint8_t *)malloc(z_n);
    if (!zlib) {
        free(raw);
        return -1;
    }
    zlib[0] = 0x78;
    zlib[1] = 0x01;
    off = 2;
    {
        size_t left = raw_n;
        const uint8_t *src = raw;
        while (left > 0) {
            uint32_t chunk = left > 65535u ? 65535u : (uint32_t)left;
            zlib[off++] = (left - chunk == 0) ? 0x01u : 0x00u;
            zlib[off++] = (uint8_t)(chunk & 0xffu);
            zlib[off++] = (uint8_t)((chunk >> 8) & 0xffu);
            zlib[off++] = (uint8_t)((~chunk) & 0xffu);
            zlib[off++] = (uint8_t)(((~chunk) >> 8) & 0xffu);
            memcpy(zlib + off, src, chunk);
            off += chunk;
            src += chunk;
            left -= chunk;
        }
    }
    be32(zlib + off, adler);
    off += 4;
    f = fopen(path, "wb");
    if (!f) {
        free(raw);
        free(zlib);
        return -1;
    }
    if (fwrite(sig, 1, 8, f) != 8) {
        goto done;
    }
    memset(ihdr, 0, sizeof(ihdr));
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;
    ihdr[9] = 2; /* RGB */
    if (write_chunk(f, "IHDR", ihdr, 13) != 0) {
        goto done;
    }
    if (write_chunk(f, "IDAT", zlib, (uint32_t)off) != 0) {
        goto done;
    }
    if (write_chunk(f, "IEND", NULL, 0) != 0) {
        goto done;
    }
    rc = 0;
done:
    fclose(f);
    free(raw);
    free(zlib);
    return rc;
}

static uint8_t *nn_scale_rgb(const uint8_t *src, int w, int h, int stride, int scale, int *out_w, int *out_h) {
    uint8_t *dst;
    int x, y, sx, sy, dw, dh;
    if (scale < 1) {
        scale = 1;
    }
    dw = w * scale;
    dh = h * scale;
    dst = (uint8_t *)malloc((size_t)dw * (size_t)dh * 3u);
    if (!dst) {
        return NULL;
    }
    for (y = 0; y < h; y++) {
        for (sy = 0; sy < scale; sy++) {
            uint8_t *drow = dst + ((size_t)(y * scale + sy) * (size_t)dw * 3u);
            for (x = 0; x < w; x++) {
                const uint8_t *s = src + (size_t)y * (size_t)stride + (size_t)x * 3u;
                for (sx = 0; sx < scale; sx++) {
                    memcpy(drow, s, 3);
                    drow += 3;
                }
            }
        }
    }
    *out_w = dw;
    *out_h = dh;
    return dst;
}

static int readme_dir_path(char *dir, size_t cap, char *path, size_t path_cap, const char *file_name) {
    if (snprintf(dir, cap, "%s/img/readme", R01_REPO_ROOT) >= (int)cap) {
        return -1;
    }
    (void)mkdir(dir, 0755);
    if (snprintf(path, path_cap, "%s/%s", dir, file_name) >= (int)path_cap) {
        return -1;
    }
    return 0;
}

int r01_readme_shot_save_rgb(const uint8_t *rgb, int w, int h, int stride, int scale, const char *file_name) {
    char dir[1024];
    char path[1088];
    uint8_t *scaled = NULL;
    const uint8_t *write_px;
    int out_w, out_h, write_stride, rc;
    if (!rgb || !file_name || !file_name[0] || w < 1 || h < 1 || stride < w * 3) {
        fprintf(stderr, "readme shot: bad rgb args\n");
        return -1;
    }
    if (scale < 1) {
        scale = 1;
    }
    write_px = rgb;
    out_w = w;
    out_h = h;
    write_stride = stride;
    if (scale > 1) {
        scaled = nn_scale_rgb(rgb, w, h, stride, scale, &out_w, &out_h);
        if (!scaled) {
            fprintf(stderr, "readme shot: scale alloc failed\n");
            return -1;
        }
        write_px = scaled;
        write_stride = out_w * 3;
    }
    if (readme_dir_path(dir, sizeof(dir), path, sizeof(path), file_name) != 0) {
        free(scaled);
        return -1;
    }
    rc = write_png_rgb(path, out_w, out_h, write_px, write_stride);
    free(scaled);
    if (rc == 0) {
        fprintf(stderr, "readme shot: %s (%dx%d)\n", path, out_w, out_h);
    } else {
        fprintf(stderr, "readme shot failed: %s\n", path);
    }
    return rc;
}

int r01_readme_shot_save_renderer(SDL_Renderer *ren, SDL_Window *win, const char *file_name) {
    uint8_t *px = NULL;
    int ow = 0, oh = 0, lw = 0, lh = 0, ww = 0;
    int stride, scale = 1, rc;
    if (!ren || !file_name || !file_name[0]) {
        return -1;
    }
    if (SDL_GetRendererOutputSize(ren, &ow, &oh) != 0 || ow < 1 || oh < 1) {
        fprintf(stderr, "readme shot: output size (%s)\n", SDL_GetError());
        return -1;
    }
    stride = ow * 3;
    px = (uint8_t *)malloc((size_t)stride * (size_t)oh);
    if (!px) {
        return -1;
    }
    if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_RGB24, px, stride) != 0) {
        fprintf(stderr, "readme shot: ReadPixels (%s)\n", SDL_GetError());
        free(px);
        return -1;
    }
    SDL_RenderGetLogicalSize(ren, &lw, &lh);
    if (win) {
        int dummy = 0;
        SDL_GetWindowSize(win, &ww, &dummy);
    }
    if (lw > 0 && ww > lw && ow == lw) {
        scale = ww / lw;
        if (scale < 1) {
            scale = 1;
        }
    }
    rc = r01_readme_shot_save_rgb(px, ow, oh, stride, scale, file_name);
    free(px);
    return rc;
}

#endif /* R01_README_SHOT */
