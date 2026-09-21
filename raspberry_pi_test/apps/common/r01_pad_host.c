#include "r01_pad_host.h"

#include <stdio.h>
#include <string.h>

#define R01_PAD_SLOTS 2
#define R01_PAD_AXIS_DEAD 8000
#define R01_PAD_MENU_BTN_N 3

typedef struct R01PadHost {
    SDL_GameController *pad[R01_PAD_SLOTS];
    int ready;
    int menu_open;
    int menu_sel;
    uint8_t prev_guide;
    uint8_t prev_up;
    uint8_t prev_down;
    uint8_t prev_ok;
    uint8_t prev_back;
} R01PadHost;

static R01PadHost g_pad;

static int load_db_file(const char *path) {
    int n;
    if (!path || !path[0]) {
        return -1;
    }
    n = SDL_GameControllerAddMappingsFromFile(path);
    if (n >= 0) {
        fprintf(stderr, "r01_pad_host: loaded %d mappings from %s\n", n, path);
        return n;
    }
    return -1;
}

static void load_community_db(void) {
    const char *env;
#ifdef R01_REPO_ROOT
    (void)load_db_file(R01_REPO_ROOT "/apps/common/gamecontrollerdb.txt");
#endif
    env = SDL_getenv("SDL_GAMECONTROLLERCONFIG_FILE");
    if (env && env[0]) {
        (void)load_db_file(env);
    }
    (void)load_db_file("gamecontrollerdb.txt");
    {
        char *base = SDL_GetBasePath();
        if (base) {
            char path[1024];
            snprintf(path, sizeof(path), "%sgamecontrollerdb.txt", base);
            (void)load_db_file(path);
            SDL_free(base);
        }
    }
}

static SDL_JoystickID pad_instance(SDL_GameController *c) {
    SDL_Joystick *js;
    if (!c) {
        return -1;
    }
    js = SDL_GameControllerGetJoystick(c);
    return js ? SDL_JoystickInstanceID(js) : -1;
}

static int slot_of_instance(SDL_JoystickID id) {
    int i;
    for (i = 0; i < R01_PAD_SLOTS; i++) {
        if (g_pad.pad[i] && pad_instance(g_pad.pad[i]) == id) {
            return i;
        }
    }
    return -1;
}

static void open_index(int idx) {
    SDL_GameController *c;
    SDL_JoystickID id;
    int slot;
    if (idx < 0 || !SDL_IsGameController(idx)) {
        return;
    }
    c = SDL_GameControllerOpen(idx);
    if (!c) {
        fprintf(stderr, "r01_pad_host: open %d failed (%s)\n", idx, SDL_GetError());
        return;
    }
    id = pad_instance(c);
    if (id >= 0 && slot_of_instance(id) >= 0) {
        SDL_GameControllerClose(c);
        return;
    }
    for (slot = 0; slot < R01_PAD_SLOTS; slot++) {
        if (!g_pad.pad[slot]) {
            g_pad.pad[slot] = c;
            printf("retr01: pad P%d %s\n", slot + 1, SDL_GameControllerName(c) ? SDL_GameControllerName(c) : "controller");
            return;
        }
    }
    SDL_GameControllerClose(c);
}

static void close_instance(SDL_JoystickID id) {
    int slot = slot_of_instance(id);
    if (slot < 0) {
        return;
    }
    printf("retr01: pad P%d removed\n", slot + 1);
    SDL_GameControllerClose(g_pad.pad[slot]);
    g_pad.pad[slot] = NULL;
}

static void open_connected(void) {
    int i;
    int n = SDL_NumJoysticks();
    for (i = 0; i < n; i++) {
        open_index(i);
    }
}

static int btn(SDL_GameController *c, SDL_GameControllerButton b) {
    return c && SDL_GameControllerGetButton(c, b);
}

static int axis_neg(SDL_GameController *c, SDL_GameControllerAxis a) {
    return c && SDL_GameControllerGetAxis(c, a) < -R01_PAD_AXIS_DEAD;
}

static int axis_pos(SDL_GameController *c, SDL_GameControllerAxis a) {
    return c && SDL_GameControllerGetAxis(c, a) > R01_PAD_AXIS_DEAD;
}

static uint8_t bits_from_pad(SDL_GameController *c) {
    uint8_t b = 0;
    if (!c || !SDL_GameControllerGetAttached(c)) {
        return 0;
    }
    if (btn(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || axis_pos(c, SDL_CONTROLLER_AXIS_LEFTX)) {
        b |= R01_PAD_RIGHT;
    }
    if (btn(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || axis_neg(c, SDL_CONTROLLER_AXIS_LEFTX)) {
        b |= R01_PAD_LEFT;
    }
    if (btn(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || axis_pos(c, SDL_CONTROLLER_AXIS_LEFTY)) {
        b |= R01_PAD_DOWN;
    }
    if (btn(c, SDL_CONTROLLER_BUTTON_DPAD_UP) || axis_neg(c, SDL_CONTROLLER_AXIS_LEFTY)) {
        b |= R01_PAD_UP;
    }
    if ((b & (R01_PAD_UP | R01_PAD_DOWN)) == (R01_PAD_UP | R01_PAD_DOWN)) {
        b = (uint8_t)(b & (uint8_t)~(R01_PAD_UP | R01_PAD_DOWN));
    }
    if ((b & (R01_PAD_LEFT | R01_PAD_RIGHT)) == (R01_PAD_LEFT | R01_PAD_RIGHT)) {
        b = (uint8_t)(b & (uint8_t)~(R01_PAD_LEFT | R01_PAD_RIGHT));
    }
    /* South/North = face Y (jump). East/West = face X. */
    if (btn(c, SDL_CONTROLLER_BUTTON_A) || btn(c, SDL_CONTROLLER_BUTTON_Y)) {
        b |= R01_PAD_Y;
    }
    if (btn(c, SDL_CONTROLLER_BUTTON_B) || btn(c, SDL_CONTROLLER_BUTTON_X)) {
        b |= R01_PAD_X;
    }
    if (btn(c, SDL_CONTROLLER_BUTTON_START) || btn(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        b |= R01_PAD_START;
    }
    if (btn(c, SDL_CONTROLLER_BUTTON_BACK) || btn(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
        b |= R01_PAD_COIN;
    }
    return b;
}

static uint8_t any_guide(void) {
    int i;
    for (i = 0; i < R01_PAD_SLOTS; i++) {
        if (btn(g_pad.pad[i], SDL_CONTROLLER_BUTTON_GUIDE)) {
            return 1;
        }
    }
    return 0;
}

static uint8_t any_up(void) {
    int i;
    for (i = 0; i < R01_PAD_SLOTS; i++) {
        if (btn(g_pad.pad[i], SDL_CONTROLLER_BUTTON_DPAD_UP) || axis_neg(g_pad.pad[i], SDL_CONTROLLER_AXIS_LEFTY)) {
            return 1;
        }
    }
    return 0;
}

static uint8_t any_down(void) {
    int i;
    for (i = 0; i < R01_PAD_SLOTS; i++) {
        if (btn(g_pad.pad[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN) || axis_pos(g_pad.pad[i], SDL_CONTROLLER_AXIS_LEFTY)) {
            return 1;
        }
    }
    return 0;
}

static uint8_t any_ok(void) {
    int i;
    for (i = 0; i < R01_PAD_SLOTS; i++) {
        if (btn(g_pad.pad[i], SDL_CONTROLLER_BUTTON_A) || btn(g_pad.pad[i], SDL_CONTROLLER_BUTTON_START)) {
            return 1;
        }
    }
    return 0;
}

static uint8_t any_back(void) {
    int i;
    for (i = 0; i < R01_PAD_SLOTS; i++) {
        if (btn(g_pad.pad[i], SDL_CONTROLLER_BUTTON_B)) {
            return 1;
        }
    }
    return 0;
}

static int confirm_sel(void) {
    int act = g_pad.menu_sel + 1;
    if (act != R01_PAD_MENU_SCALE) {
        g_pad.menu_open = 0;
    }
    return act;
}

static int menu_px(int vh) {
    return (vh >= 180) ? 2 : 1;
}

static void menu_geom(int ox, int oy, int vw, int vh, SDL_Rect *bounds, SDL_Rect btn[R01_PAD_MENU_BTN_N]) {
    int px = menu_px(vh);
    int bw = 58 * px;
    int bh = 13 * px;
    int gap = 2 * px;
    int i;
    int total_h = R01_PAD_MENU_BTN_N * bh + (R01_PAD_MENU_BTN_N - 1) * gap;
    int x = ox + (vw - bw) / 2;
    int y = oy + (vh - total_h) / 2;
    if (px > 1) {
        x &= ~1;
        y &= ~1;
    }
    for (i = 0; i < R01_PAD_MENU_BTN_N; i++) {
        btn[i].x = x;
        btn[i].y = y + i * (bh + gap);
        btn[i].w = bw;
        btn[i].h = bh;
    }
    if (bounds) {
        bounds->x = x;
        bounds->y = y;
        bounds->w = bw;
        bounds->h = total_h;
    }
}

/* 5x7 uppercase / digits used by the overlay (MSB = left). */
static const uint8_t GLYPH_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t GLYPH_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
static const uint8_t GLYPH_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
static const uint8_t GLYPH_I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t GLYPH_Q[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
static const uint8_t GLYPH_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
static const uint8_t GLYPH_S[7] = {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E};
static const uint8_t GLYPH_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
static const uint8_t GLYPH_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
static const uint8_t GLYPH_X[7] = {0x11, 0x0A, 0x04, 0x04, 0x04, 0x0A, 0x11};

static const uint8_t *glyph_for(char ch) {
    switch (ch) {
    case '1':
        return GLYPH_1;
    case '2':
        return GLYPH_2;
    case 'E':
        return GLYPH_E;
    case 'I':
        return GLYPH_I;
    case 'Q':
        return GLYPH_Q;
    case 'R':
        return GLYPH_R;
    case 'S':
        return GLYPH_S;
    case 'T':
        return GLYPH_T;
    case 'U':
        return GLYPH_U;
    case 'X':
        return GLYPH_X;
    default:
        return NULL;
    }
}

static void blit_glyph(SDL_Renderer *ren, int x, int y, const uint8_t rows[7], int px) {
    int row, col;
    SDL_Rect d;
    for (row = 0; row < 7; row++) {
        for (col = 0; col < 5; col++) {
            if (rows[row] & (1u << (4 - col))) {
                d.x = x + col * px;
                d.y = y + row * px;
                d.w = px;
                d.h = px;
                SDL_RenderFillRect(ren, &d);
            }
        }
    }
}

static void draw_label(SDL_Renderer *ren, const SDL_Rect *box, const char *text, int px, Uint8 r, Uint8 g, Uint8 b) {
    int gw;
    int gh;
    int n;
    int w;
    int x;
    int y;
    int i;
    if (px < 1) {
        px = 1;
    }
    gw = 6 * px;
    gh = 7 * px;
    n = (int)strlen(text);
    w = n > 0 ? (n * gw - px) : 0;
    x = box->x + (box->w - w) / 2;
    y = box->y + (box->h - gh) / 2;
    if (px > 1) {
        x &= ~1;
        y &= ~1;
    }
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    for (i = 0; i < n; i++) {
        const uint8_t *gl = glyph_for(text[i]);
        if (gl) {
            blit_glyph(ren, x + i * gw, y, gl, px);
        }
    }
}

void r01_pad_host_preinit(void) {
#ifdef SDL_HINT_GAMECONTROLLERCONFIG_FILE
#ifdef R01_REPO_ROOT
    SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE, R01_REPO_ROOT "/apps/common/gamecontrollerdb.txt");
#endif
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
}

int r01_pad_host_init(void) {
    if (g_pad.ready) {
        return 0;
    }
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            fprintf(stderr, "r01_pad_host: GAMECONTROLLER (%s)\n", SDL_GetError());
            return -1;
        }
    }
    load_community_db();
    SDL_GameControllerEventState(SDL_ENABLE);
    open_connected();
    g_pad.ready = 1;
    g_pad.menu_sel = 0;
    return 0;
}

void r01_pad_host_shutdown(void) {
    int i;
    for (i = 0; i < R01_PAD_SLOTS; i++) {
        if (g_pad.pad[i]) {
            SDL_GameControllerClose(g_pad.pad[i]);
            g_pad.pad[i] = NULL;
        }
    }
    g_pad.ready = 0;
    g_pad.menu_open = 0;
}

void r01_pad_host_event(const SDL_Event *e) {
    if (!e || !g_pad.ready) {
        return;
    }
    if (e->type == SDL_CONTROLLERDEVICEADDED) {
        open_index(e->cdevice.which);
    } else if (e->type == SDL_CONTROLLERDEVICEREMOVED) {
        close_instance((SDL_JoystickID)e->cdevice.which);
    }
}

uint8_t r01_pad_host_bits(int player) {
    if (player < 0 || player >= R01_PAD_SLOTS || g_pad.menu_open) {
        return 0;
    }
    return bits_from_pad(g_pad.pad[player]);
}

int r01_pad_host_menu_open(void) {
    return g_pad.menu_open;
}

void r01_pad_host_menu_set_open(int open) {
    g_pad.menu_open = open ? 1 : 0;
    if (open) {
        g_pad.menu_sel = 0;
    }
}

int r01_pad_host_tick(void) {
    uint8_t guide = any_guide();
    uint8_t up = any_up();
    uint8_t down = any_down();
    uint8_t ok = any_ok();
    uint8_t back = any_back();
    int act = R01_PAD_MENU_NONE;

    if (guide && !g_pad.prev_guide) {
        g_pad.menu_open = !g_pad.menu_open;
        if (g_pad.menu_open) {
            g_pad.menu_sel = 0;
        }
    }
    if (g_pad.menu_open) {
        if (up && !g_pad.prev_up) {
            g_pad.menu_sel = (g_pad.menu_sel + R01_PAD_MENU_BTN_N - 1) % R01_PAD_MENU_BTN_N;
        }
        if (down && !g_pad.prev_down) {
            g_pad.menu_sel = (g_pad.menu_sel + 1) % R01_PAD_MENU_BTN_N;
        }
        if (ok && !g_pad.prev_ok) {
            act = confirm_sel();
        } else if (back && !g_pad.prev_back) {
            g_pad.menu_open = 0;
        }
    }
    g_pad.prev_guide = guide;
    g_pad.prev_up = up;
    g_pad.prev_down = down;
    g_pad.prev_ok = ok;
    g_pad.prev_back = back;
    return act;
}

int r01_pad_host_menu_keydown(int key, int repeat) {
    if (!g_pad.menu_open) {
        return -1;
    }
    if (repeat) {
        return R01_PAD_MENU_NONE;
    }
    if (key == SDLK_UP || key == SDLK_w) {
        g_pad.menu_sel = (g_pad.menu_sel + R01_PAD_MENU_BTN_N - 1) % R01_PAD_MENU_BTN_N;
        return R01_PAD_MENU_NONE;
    }
    if (key == SDLK_DOWN || key == SDLK_s) {
        g_pad.menu_sel = (g_pad.menu_sel + 1) % R01_PAD_MENU_BTN_N;
        return R01_PAD_MENU_NONE;
    }
    if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_h) {
        return confirm_sel();
    }
    if (key == SDLK_ESCAPE) {
        g_pad.menu_open = 0;
        return R01_PAD_MENU_NONE;
    }
    return R01_PAD_MENU_NONE;
}

int r01_pad_host_menu_click(int x, int y, int ox, int oy, int vw, int vh) {
    SDL_Rect bounds;
    SDL_Rect btn[R01_PAD_MENU_BTN_N];
    int i;
    if (!g_pad.menu_open) {
        return -1;
    }
    menu_geom(ox, oy, vw, vh, &bounds, btn);
    for (i = 0; i < R01_PAD_MENU_BTN_N; i++) {
        if (x >= btn[i].x && x < btn[i].x + btn[i].w && y >= btn[i].y && y < btn[i].y + btn[i].h) {
            g_pad.menu_sel = i;
            return confirm_sel();
        }
    }
    g_pad.menu_open = 0;
    return R01_PAD_MENU_NONE;
}

void r01_pad_host_draw_menu(SDL_Renderer *ren, int ox, int oy, int vw, int vh, int scale_x) {
    SDL_Rect bounds;
    SDL_Rect btn[R01_PAD_MENU_BTN_N];
    SDL_BlendMode old = SDL_BLENDMODE_NONE;
    int i;
    const char *labels[R01_PAD_MENU_BTN_N];
    if (!ren || !g_pad.menu_open) {
        return;
    }
    if (scale_x < 1) {
        scale_x = 1;
    }
    labels[0] = "RESET";
    labels[1] = "QUIT";
    labels[2] = (scale_x >= 2) ? "2X" : "1X";
    menu_geom(ox, oy, vw, vh, &bounds, btn);

    SDL_GetRenderDrawBlendMode(ren, &old);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (i = 0; i < R01_PAD_MENU_BTN_N; i++) {
        int sel = (i == g_pad.menu_sel);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 128);
        SDL_RenderFillRect(ren, &btn[i]);
        if (sel) {
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDrawRect(ren, &btn[i]);
        }
        draw_label(ren, &btn[i], labels[i], menu_px(vh), 255, 255, 255);
    }
    SDL_SetRenderDrawBlendMode(ren, old);
}
