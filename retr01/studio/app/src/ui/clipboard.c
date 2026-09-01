#include "ui/ui.h"
#include "ui/internal.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(R01_HAS_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
} PngMemReader;

static void png_mem_read(png_structp png, png_bytep out, png_size_t need) {
    PngMemReader *r = (PngMemReader *)png_get_io_ptr(png);
    if (!r || r->pos + need > r->len) {
        png_error(png, "png mem underrun");
        return;
    }
    memcpy(out, r->data + r->pos, need);
    r->pos += need;
}

static int decode_png_rgba_mem(const uint8_t *png_bytes, size_t png_len, uint8_t **out_px, int *out_w,
                               int *out_h) {
    png_structp png;
    png_infop info;
    PngMemReader reader;
    png_bytep *rows = NULL;
    uint8_t *px = NULL;
    int w, h, y;
    if (!png_bytes || png_len < 8 || !out_px || !out_w || !out_h) {
        return -1;
    }
    if (png_sig_cmp(png_bytes, 0, 8) != 0) {
        return -1;
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        free(rows);
        free(px);
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }
    reader.data = png_bytes;
    reader.len = png_len;
    reader.pos = 0;
    png_set_read_fn(png, &reader, png_mem_read);
    png_read_info(png, info);
    w = (int)png_get_image_width(png, info);
    h = (int)png_get_image_height(png, info);
    if (w < 1 || h < 1 || w > 4096 || h > 4096) {
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }
    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    px = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
    rows = (png_bytep *)malloc((size_t)h * sizeof(png_bytep));
    if (!px || !rows) {
        free(rows);
        free(px);
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }
    for (y = 0; y < h; y++) {
        rows[y] = px + (size_t)y * (size_t)w * 4u;
    }
    png_read_image(png, rows);
    png_read_end(png, NULL);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    *out_px = px;
    *out_w = w;
    *out_h = h;
    return 0;
}

static int read_cmd_stdout(char *const argv[], uint8_t **out, size_t *out_len) {
    int pipefd[2];
    pid_t pid;
    uint8_t *buf = NULL;
    size_t cap = 0, len = 0;
    ssize_t n;
    int status;
    if (!argv || !argv[0] || !out || !out_len) {
        return -1;
    }
    *out = NULL;
    *out_len = 0;
#if !(defined(__linux__) || defined(__APPLE__))
    (void)pipefd;
    (void)pid;
    (void)buf;
    (void)cap;
    (void)len;
    (void)n;
    (void)status;
    return -1;
#else
    if (pipe(pipefd) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        int nullfd;
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) {
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    close(pipefd[1]);
    for (;;) {
        uint8_t chunk[4096];
        n = read(pipefd[0], chunk, sizeof(chunk));
        if (n < 0) {
            free(buf);
            close(pipefd[0]);
            waitpid(pid, &status, 0);
            return -1;
        }
        if (n == 0) {
            break;
        }
        if (len + (size_t)n + 1u > cap) {
            size_t ncap = cap ? cap * 2u : 16384u;
            uint8_t *nb;
            while (ncap < len + (size_t)n + 1u) {
                ncap *= 2u;
            }
            nb = (uint8_t *)realloc(buf, ncap);
            if (!nb) {
                free(buf);
                close(pipefd[0]);
                waitpid(pid, &status, 0);
                return -1;
            }
            buf = nb;
            cap = ncap;
        }
        memcpy(buf + len, chunk, (size_t)n);
        len += (size_t)n;
    }
    close(pipefd[0]);
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0 || len < 8) {
        free(buf);
        return -1;
    }
    *out = buf;
    *out_len = len;
    return 0;
#endif
}

#if defined(R01_HAS_X11)
static int clipboard_png_x11(uint8_t **out, size_t *out_len) {
    Display *dpy;
    Window w;
    Atom clipboard, png, prop, incr;
    XEvent ev;
    int i;
    Atom type = None;
    int format = 0;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char *data = NULL;
    size_t total;

    if (!out || !out_len) {
        return -1;
    }
    *out = NULL;
    *out_len = 0;
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        return -1;
    }
    w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
    clipboard = XInternAtom(dpy, "CLIPBOARD", False);
    png = XInternAtom(dpy, "image/png", False);
    prop = XInternAtom(dpy, "R01_CLIPBOARD_PNG", False);
    incr = XInternAtom(dpy, "INCR", False);
    XSelectInput(dpy, w, PropertyChangeMask);
    XConvertSelection(dpy, clipboard, png, prop, w, CurrentTime);
    XFlush(dpy);

    for (i = 0; i < 100; i++) {
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == SelectionNotify) {
                if (ev.xselection.property == None) {
                    XDestroyWindow(dpy, w);
                    XCloseDisplay(dpy);
                    return -1;
                }
                goto got_notify;
            }
        }
        usleep(10000);
    }
    XDestroyWindow(dpy, w);
    XCloseDisplay(dpy);
    return -1;

got_notify:
    if (XGetWindowProperty(dpy, w, prop, 0, 0, False, AnyPropertyType, &type, &format, &nitems, &bytes_after,
                            &data) != Success) {
        XDestroyWindow(dpy, w);
        XCloseDisplay(dpy);
        return -1;
    }
    if (data) {
        XFree(data);
        data = NULL;
    }
    if (type == incr || bytes_after == 0 || bytes_after > 8u * 1024u * 1024u) {
        XDeleteProperty(dpy, w, prop);
        XDestroyWindow(dpy, w);
        XCloseDisplay(dpy);
        return -1;
    }
    total = (size_t)bytes_after;
    if (XGetWindowProperty(dpy, w, prop, 0, (long)((total + 3u) / 4u), False, AnyPropertyType, &type, &format,
                            &nitems, &bytes_after, &data) != Success ||
        !data || nitems == 0) {
        if (data) {
            XFree(data);
        }
        XDeleteProperty(dpy, w, prop);
        XDestroyWindow(dpy, w);
        XCloseDisplay(dpy);
        return -1;
    }
    {
        size_t nbytes = (size_t)nitems * (size_t)(format / 8);
        uint8_t *copy;
        if (nbytes < 8 || png_sig_cmp(data, 0, 8) != 0) {
            XFree(data);
            XDeleteProperty(dpy, w, prop);
            XDestroyWindow(dpy, w);
            XCloseDisplay(dpy);
            return -1;
        }
        copy = (uint8_t *)malloc(nbytes);
        if (!copy) {
            XFree(data);
            XDeleteProperty(dpy, w, prop);
            XDestroyWindow(dpy, w);
            XCloseDisplay(dpy);
            return -1;
        }
        memcpy(copy, data, nbytes);
        XFree(data);
        XDeleteProperty(dpy, w, prop);
        XDestroyWindow(dpy, w);
        XCloseDisplay(dpy);
        *out = copy;
        *out_len = nbytes;
        return 0;
    }
}
#endif

static int clipboard_png_bytes(uint8_t **out, size_t *out_len) {
    char *wl_argv[] = {"wl-paste", "-t", "image/png", NULL};
    char *xclip_argv[] = {"xclip", "-selection", "clipboard", "-t", "image/png", "-o", NULL};
    if (!out || !out_len) {
        return -1;
    }
    *out = NULL;
    *out_len = 0;
    if (getenv("WAYLAND_DISPLAY")) {
        if (read_cmd_stdout(wl_argv, out, out_len) == 0) {
            return 0;
        }
    }
    if (read_cmd_stdout(xclip_argv, out, out_len) == 0) {
        return 0;
    }
#if defined(R01_HAS_X11)
    if (clipboard_png_x11(out, out_len) == 0) {
        return 0;
    }
#endif
    if (!getenv("WAYLAND_DISPLAY")) {
        if (read_cmd_stdout(wl_argv, out, out_len) == 0) {
            return 0;
        }
    }
    return -1;
}

static void fill_target_rgb(const R01Project *p, int row, int pal, int spr, uint8_t target_rgb[4][3]) {
    int c;
    for (c = 0; c < 4; c++) {
        uint8_t cr, cg, cb;
        uint8_t master;
        if (spr) {
            master = p->global_pal_spr[row][pal & 3].idx[c];
        } else {
            master = p->global_pal_bg[row][pal & 3].idx[c];
        }
        r01_kit_rgb(master, &cr, &cg, &cb);
        target_rgb[c][0] = cr;
        target_rgb[c][1] = cg;
        target_rgb[c][2] = cb;
    }
}

int ui_paste_clipboard_png_tile(UiState *ui, uint8_t chr[R01_TILE_BYTES], int pal, int spr_plane) {
    uint8_t *png = NULL;
    size_t png_len = 0;
    uint8_t *rgba = NULL;
    int w = 0, h = 0;
    uint8_t targets[4][3];
    const R01World *world;
    int row;
    if (!ui || !ui->project || !chr) {
        return -1;
    }
    if (clipboard_png_bytes(&png, &png_len) != 0) {
        ui_toast(ui, "no PNG on clipboard", 1);
        return -1;
    }
    if (decode_png_rgba_mem(png, png_len, &rgba, &w, &h) != 0) {
        free(png);
        ui_toast(ui, "clipboard PNG decode failed", 1);
        return -1;
    }
    free(png);
    world = r01_project_active_world_const(ui->project);
    row = world ? world->default_pal_row : 0;
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    fill_target_rgb(ui->project, row, pal, spr_plane, targets);
    r01_tile_from_rgba_brightness(chr, rgba, w, h, 0, 0, (const uint8_t (*)[3])targets);
    free(rgba);
    if (w > 8 || h > 8) {
        ui_toast(ui, "pasted top-left 8x8", 0);
    } else {
        ui_toast(ui, "pasted PNG", 0);
    }
    return 0;
}
