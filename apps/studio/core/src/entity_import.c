#include "retr01_studio/entity_import.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <dirent.h>
#include <errno.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define IMPORT_ALPHA_CUTOFF 128

static void set_err(char *err_buf, size_t err_cap, const char *msg) {
    if (err_buf && err_cap > 0) {
        snprintf(err_buf, err_cap, "%s", msg ? msg : "error");
    }
}

static int ends_with_ci(const char *path, const char *suffix) {
    size_t pl, sl;
    const char *p;
    if (!path || !suffix) {
        return 0;
    }
    pl = strlen(path);
    sl = strlen(suffix);
    if (pl < sl) {
        return 0;
    }
    p = path + pl - sl;
    while (*suffix) {
        unsigned char a = (unsigned char)*p++;
        unsigned char b = (unsigned char)*suffix++;
        if (a >= 'A' && a <= 'Z') {
            a = (unsigned char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (unsigned char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int is_ase_name(const char *name) {
    return ends_with_ci(name, ".ase") || ends_with_ci(name, ".aseprite");
}

typedef struct Sha1Ctx {
    uint32_t h[5];
    uint64_t nbits;
    uint8_t buf[64];
    size_t buf_n;
} Sha1Ctx;

static uint32_t sha1_rol(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

static void sha1_block(Sha1Ctx *ctx, const uint8_t *p) {
    uint32_t w[80];
    uint32_t a, b, c, d, e;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) |
               (uint32_t)p[i * 4 + 3];
    }
    for (i = 16; i < 80; i++) {
        w[i] = sha1_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    a = ctx->h[0];
    b = ctx->h[1];
    c = ctx->h[2];
    d = ctx->h[3];
    e = ctx->h[4];
    for (i = 0; i < 80; i++) {
        uint32_t f, k, t;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        t = sha1_rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = sha1_rol(b, 30);
        b = a;
        a = t;
    }
    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
}

static void sha1_init(Sha1Ctx *ctx) {
    ctx->h[0] = 0x67452301u;
    ctx->h[1] = 0xEFCDAB89u;
    ctx->h[2] = 0x98BADCFEu;
    ctx->h[3] = 0x10325476u;
    ctx->h[4] = 0xC3D2E1F0u;
    ctx->nbits = 0;
    ctx->buf_n = 0;
}

static void sha1_update(Sha1Ctx *ctx, const uint8_t *data, size_t n) {
    while (n > 0) {
        size_t take = 64u - ctx->buf_n;
        if (take > n) {
            take = n;
        }
        memcpy(ctx->buf + ctx->buf_n, data, take);
        ctx->buf_n += take;
        data += take;
        n -= take;
        if (ctx->buf_n == 64u) {
            sha1_block(ctx, ctx->buf);
            ctx->nbits += 512u;
            ctx->buf_n = 0;
        }
    }
}

static void sha1_final(Sha1Ctx *ctx, uint8_t out[20]) {
    int i;
    ctx->nbits += (uint64_t)ctx->buf_n * 8u;
    ctx->buf[ctx->buf_n++] = 0x80u;
    if (ctx->buf_n > 56u) {
        while (ctx->buf_n < 64u) {
            ctx->buf[ctx->buf_n++] = 0;
        }
        sha1_block(ctx, ctx->buf);
        ctx->buf_n = 0;
    }
    while (ctx->buf_n < 56u) {
        ctx->buf[ctx->buf_n++] = 0;
    }
    ctx->buf[56] = (uint8_t)(ctx->nbits >> 56);
    ctx->buf[57] = (uint8_t)(ctx->nbits >> 48);
    ctx->buf[58] = (uint8_t)(ctx->nbits >> 40);
    ctx->buf[59] = (uint8_t)(ctx->nbits >> 32);
    ctx->buf[60] = (uint8_t)(ctx->nbits >> 24);
    ctx->buf[61] = (uint8_t)(ctx->nbits >> 16);
    ctx->buf[62] = (uint8_t)(ctx->nbits >> 8);
    ctx->buf[63] = (uint8_t)ctx->nbits;
    sha1_block(ctx, ctx->buf);
    for (i = 0; i < 5; i++) {
        out[i * 4] = (uint8_t)(ctx->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)ctx->h[i];
    }
}

static void sha1_hex(const uint8_t digest[20], char out[R01_SHA1_HEX_LEN + 1]) {
    static const char *hex = "0123456789abcdef";
    int i;
    for (i = 0; i < 20; i++) {
        out[i * 2] = hex[(digest[i] >> 4) & 0x0f];
        out[i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    out[R01_SHA1_HEX_LEN] = '\0';
}

int r01_sha1_file(const char *path, char out_hex[R01_SHA1_HEX_LEN + 1], char *err_buf, size_t err_cap) {
    FILE *f;
    Sha1Ctx ctx;
    uint8_t buf[4096];
    uint8_t digest[20];
    size_t n;
    if (!path || !out_hex) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    out_hex[0] = '\0';
    f = fopen(path, "rb");
    if (!f) {
        set_err(err_buf, err_cap, "cannot read file for sha1");
        return -1;
    }
    sha1_init(&ctx);
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha1_update(&ctx, buf, n);
    }
    if (ferror(f)) {
        fclose(f);
        set_err(err_buf, err_cap, "sha1 read failed");
        return -1;
    }
    fclose(f);
    sha1_final(&ctx, digest);
    sha1_hex(digest, out_hex);
    return 0;
}

static int path_is_dir(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void stem_from_filename(const char *name, char *out, size_t cap) {
    const char *dot;
    size_t n;
    if (!out || cap < 2) {
        return;
    }
    out[0] = '\0';
    if (!name) {
        return;
    }
    dot = strrchr(name, '.');
    n = dot ? (size_t)(dot - name) : strlen(name);
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(out, name, n);
    out[n] = '\0';
}

static int state_slot_hint(const char *slug) {
    if (!slug || !slug[0]) {
        return -1;
    }
    if (strcmp(slug, "idle") == 0 || strcmp(slug, "stand") == 0) {
        return 0;
    }
    if (strcmp(slug, "walk") == 0 || strcmp(slug, "walking") == 0 || strcmp(slug, "run") == 0 ||
        strcmp(slug, "running") == 0) {
        return 1;
    }
    if (strcmp(slug, "hurt") == 0) {
        return 2;
    }
    if (strcmp(slug, "jump") == 0 || strcmp(slug, "crouch") == 0 || strcmp(slug, "crouching") == 0) {
        return 3;
    }
    return -1;
}

static int catalog_index_for_slug(const R01Project *p, const char *slug) {
    int i;
    char have[R01_ENTITY_NAME_MAX];
    if (!p || !slug) {
        return -1;
    }
    for (i = 0; i < p->entity_count; i++) {
        r01_id_slugify(have, sizeof(have), r01_entity_display_name(&p->entities[i]));
        if (strcmp(have, slug) == 0) {
            return i;
        }
    }
    return -1;
}

static int listing_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int r01_aseprite_listing_equal(const R01AsepriteListing *a, const R01Project *p) {
    int i;
    if (!a || !p) {
        return 0;
    }
    if (a->count != p->aseprite_entities_file_count) {
        return 0;
    }
    for (i = 0; i < a->count; i++) {
        if (strcmp(a->files[i], p->aseprite_entities_files[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

int r01_aseprite_listing_scan(const char *dir, R01AsepriteListing *out, char *err_buf, size_t err_cap) {
    DIR *d;
    struct dirent *ent;
    if (!dir || !out) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (!path_is_dir(dir)) {
        set_err(err_buf, err_cap, "aseprite_entities/ not found");
        return -1;
    }
    d = opendir(dir);
    if (!d) {
        set_err(err_buf, err_cap, "cannot read aseprite_entities/");
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        char sub[R01_PATH_MAX];
        DIR *sd;
        struct dirent *se;
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (snprintf(sub, sizeof(sub), "%s/%s", dir, ent->d_name) >= (int)sizeof(sub)) {
            continue;
        }
        if (!path_is_dir(sub)) {
            continue;
        }
        sd = opendir(sub);
        if (!sd) {
            continue;
        }
        while ((se = readdir(sd)) != NULL) {
            char rel[R01_ASEPRITE_REL_MAX];
            if (se->d_name[0] == '.') {
                continue;
            }
            if (!is_ase_name(se->d_name)) {
                continue;
            }
            if (snprintf(rel, sizeof(rel), "%s/%s", ent->d_name, se->d_name) >= (int)sizeof(rel)) {
                closedir(sd);
                closedir(d);
                set_err(err_buf, err_cap, "aseprite path too long");
                return -1;
            }
            if (out->count >= R01_ASEPRITE_LISTING_MAX) {
                closedir(sd);
                closedir(d);
                set_err(err_buf, err_cap, "aseprite_entities/ too many files");
                return -1;
            }
            snprintf(out->files[out->count], R01_ASEPRITE_REL_MAX, "%s", rel);
            out->count++;
        }
        closedir(sd);
    }
    closedir(d);
    if (out->count > 1) {
        qsort(out->files, (size_t)out->count, R01_ASEPRITE_REL_MAX, listing_cmp);
    }
    return 0;
}

static int meta_file_cmp(const void *a, const void *b) {
    return strcmp(((const R01AsepriteFileHash *)a)->name, ((const R01AsepriteFileHash *)b)->name);
}

static void meta_sort(R01AsepriteFolderMeta *m) {
    if (m && m->count > 1) {
        qsort(m->files, (size_t)m->count, sizeof(m->files[0]), meta_file_cmp);
    }
}

static int sha1_hex_ok(const char *s) {
    int i;
    if (!s || strlen(s) != (size_t)R01_SHA1_HEX_LEN) {
        return 0;
    }
    for (i = 0; i < R01_SHA1_HEX_LEN; i++) {
        char c = s[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            continue;
        }
        return 0;
    }
    return 1;
}

static void sha1_hex_lower(char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        if (*s >= 'A' && *s <= 'F') {
            *s = (char)(*s - 'A' + 'a');
        }
        s++;
    }
}

static const char *skip_ws(const char *p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

static int ms_to_delay(int ms) {
    int d;
    if (ms < 1) {
        return 1;
    }
    d = (ms * 60) / 1000;
    if (d < 1) {
        d = 1;
    }
    if (d > 255) {
        d = 255;
    }
    return d;
}

static void meta_stem(const char *name, char *out, size_t cap) {
    stem_from_filename(name, out, cap);
    if (out && !out[0] && name) {
        snprintf(out, cap, "%s", name);
    }
}

static int meta_entry_match(const R01AsepriteFileHash *a, const R01AsepriteFileHash *b) {
    char sa[R01_ENTITY_NAME_MAX];
    char sb[R01_ENTITY_NAME_MAX];
    if (!a || !b) {
        return 0;
    }
    if (strcmp(a->name, b->name) == 0) {
        return 1;
    }
    meta_stem(a->name, sa, sizeof(sa));
    meta_stem(b->name, sb, sizeof(sb));
    return sa[0] && strcmp(sa, sb) == 0;
}

static int meta_hashed_count(const R01AsepriteFolderMeta *m) {
    int i;
    int n = 0;
    if (!m) {
        return 0;
    }
    for (i = 0; i < m->count; i++) {
        if (m->files[i].sha1[0]) {
            n++;
        }
    }
    return n;
}

int r01_aseprite_folder_meta_equal(const R01AsepriteFolderMeta *a, const R01AsepriteFolderMeta *b) {
    int i;
    int hashed;
    if (!a || !b) {
        return 0;
    }
    hashed = meta_hashed_count(a);
    if (hashed < 1 || hashed != meta_hashed_count(b)) {
        return 0;
    }
    for (i = 0; i < a->count; i++) {
        int j;
        int found = 0;
        if (!a->files[i].sha1[0]) {
            continue;
        }
        for (j = 0; j < b->count; j++) {
            if (!b->files[j].sha1[0]) {
                continue;
            }
            if (meta_entry_match(&a->files[i], &b->files[j])) {
                if (strcmp(a->files[i].sha1, b->files[j].sha1) != 0) {
                    return 0;
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    return 1;
}

int r01_aseprite_folder_meta_scan(const char *folder_dir, R01AsepriteFolderMeta *out, char *err_buf,
                                  size_t err_cap) {
    DIR *d;
    struct dirent *ent;
    if (!folder_dir || !out) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (!path_is_dir(folder_dir)) {
        set_err(err_buf, err_cap, "entity folder not found");
        return -1;
    }
    d = opendir(folder_dir);
    if (!d) {
        set_err(err_buf, err_cap, "cannot read entity folder");
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        char path[R01_PATH_MAX];
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (!is_ase_name(ent->d_name)) {
            continue;
        }
        if (out->count >= R01_ASEPRITE_FOLDER_FILES_MAX) {
            closedir(d);
            set_err(err_buf, err_cap, "too many aseprite files");
            return -1;
        }
        if (snprintf(path, sizeof(path), "%s/%s", folder_dir, ent->d_name) >= (int)sizeof(path)) {
            closedir(d);
            set_err(err_buf, err_cap, "aseprite path too long");
            return -1;
        }
        snprintf(out->files[out->count].name, R01_ASEPRITE_REL_MAX, "%s", ent->d_name);
        if (r01_sha1_file(path, out->files[out->count].sha1, err_buf, err_cap) != 0) {
            closedir(d);
            return -1;
        }
        out->count++;
    }
    closedir(d);
    meta_sort(out);
    return 0;
}

int r01_aseprite_folder_meta_save(const char *json_path, const R01AsepriteFolderMeta *meta, char *err_buf,
                                  size_t err_cap) {
    FILE *f;
    int i;
    int wrote = 0;
    if (!json_path || !meta) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    f = fopen(json_path, "wb");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write meta.json");
        return -1;
    }
    fprintf(f, "{\n");
    for (i = 0; i < meta->count; i++) {
        char key[R01_ENTITY_NAME_MAX];
        if (!meta->files[i].sha1[0]) {
            continue;
        }
        meta_stem(meta->files[i].name, key, sizeof(key));
        if (!key[0]) {
            continue;
        }
        fprintf(f, "%s  \"%s\": \"%s\"", wrote ? ",\n" : "", key, meta->files[i].sha1);
        wrote = 1;
    }
    fprintf(f, "%s}\n", wrote ? "\n" : "");
    if (fclose(f) != 0) {
        set_err(err_buf, err_cap, "cannot write meta.json");
        return -1;
    }
    return 0;
}

static int parse_state_object(const char *p, R01AsepriteFileHash *out, const char **end_out) {
    int depth = 0;
    int in_str = 0;
    const char *end;
    const char *q;
    if (!p || *p != '{' || !out) {
        return -1;
    }
    for (end = p; *end; end++) {
        if (in_str) {
            if (*end == '\\' && end[1]) {
                end++;
                continue;
            }
            if (*end == '\"') {
                in_str = 0;
            }
            continue;
        }
        if (*end == '\"') {
            in_str = 1;
            continue;
        }
        if (*end == '{') {
            depth++;
        } else if (*end == '}') {
            depth--;
            if (depth == 0) {
                break;
            }
        }
    }
    if (*end != '}') {
        return -1;
    }
    q = p + 1;
    while (q < end) {
        const char *k_end;
        size_t kn;
        q = skip_ws(q);
        if (q >= end || *q == '}') {
            break;
        }
        if (*q != '\"') {
            return -1;
        }
        q++;
        k_end = q;
        while (k_end < end && *k_end != '\"') {
            k_end++;
        }
        if (k_end >= end) {
            return -1;
        }
        kn = (size_t)(k_end - q);
        q = skip_ws(k_end + 1);
        if (*q != ':') {
            return -1;
        }
        q = skip_ws(q + 1);
        if (kn == 4 && strncmp(k_end - 4, "sha1", 4) == 0 && *q == '\"') {
            const char *s_end;
            size_t sn;
            q++;
            s_end = q;
            while (s_end < end && *s_end != '\"') {
                s_end++;
            }
            sn = (size_t)(s_end - q);
            if (sn != (size_t)R01_SHA1_HEX_LEN || s_end >= end) {
                return -1;
            }
            memcpy(out->sha1, q, sn);
            out->sha1[sn] = '\0';
            if (!sha1_hex_ok(out->sha1)) {
                return -1;
            }
            sha1_hex_lower(out->sha1);
            q = s_end + 1;
        } else {
            /* Skip unknown values. */
            if (*q == '\"') {
                q++;
                while (q < end && *q != '\"') {
                    if (*q == '\\' && q[1]) {
                        q++;
                    }
                    q++;
                }
                if (q < end) {
                    q++;
                }
            } else if (*q == '{') {
                int d = 0;
                do {
                    if (*q == '{') {
                        d++;
                    } else if (*q == '}') {
                        d--;
                    }
                    q++;
                } while (q <= end && d > 0);
            } else if (*q == '[') {
                int d = 0;
                do {
                    if (*q == '[') {
                        d++;
                    } else if (*q == ']') {
                        d--;
                    }
                    q++;
                } while (q <= end && d > 0);
            } else {
                while (q < end && *q != ',' && *q != '}') {
                    q++;
                }
            }
        }
        q = skip_ws(q);
        if (*q == ',') {
            q++;
        }
    }
    if (end_out) {
        *end_out = end + 1;
    }
    return 0;
}

int r01_aseprite_folder_meta_load(const char *json_path, R01AsepriteFolderMeta *out, char *err_buf,
                                  size_t err_cap) {
    FILE *f;
    long sz;
    char *buf;
    const char *p;
    if (!json_path || !out) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    memset(out, 0, sizeof(*out));
    f = fopen(json_path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[sz] = '\0';
    p = buf;
    while (*p && *p != '{') {
        p++;
    }
    if (*p != '{') {
        free(buf);
        return -1;
    }
    p++;
    while (*p) {
        char name[R01_ASEPRITE_REL_MAX];
        size_t n;
        const char *end;
        p = skip_ws(p);
        if (*p == ',') {
            p++;
            p = skip_ws(p);
        }
        if (*p == '}') {
            break;
        }
        if (*p != '\"') {
            free(buf);
            return -1;
        }
        p++;
        end = strchr(p, '\"');
        if (!end) {
            free(buf);
            return -1;
        }
        n = (size_t)(end - p);
        if (n < 1 || n >= sizeof(name)) {
            free(buf);
            return -1;
        }
        memcpy(name, p, n);
        name[n] = '\0';
        p = skip_ws(end + 1);
        if (*p != ':') {
            free(buf);
            return -1;
        }
        p = skip_ws(p + 1);
        if (out->count >= R01_ASEPRITE_FOLDER_FILES_MAX) {
            free(buf);
            set_err(err_buf, err_cap, "too many aseprite files");
            return -1;
        }
        memset(&out->files[out->count], 0, sizeof(out->files[0]));
        snprintf(out->files[out->count].name, R01_ASEPRITE_REL_MAX, "%s", name);
        if (*p == '\"') {
            char sha1[R01_SHA1_HEX_LEN + 1];
            p++;
            end = strchr(p, '\"');
            if (!end) {
                free(buf);
                return -1;
            }
            n = (size_t)(end - p);
            if (n != (size_t)R01_SHA1_HEX_LEN) {
                free(buf);
                return -1;
            }
            memcpy(sha1, p, n);
            sha1[n] = '\0';
            if (!sha1_hex_ok(sha1)) {
                free(buf);
                return -1;
            }
            sha1_hex_lower(sha1);
            snprintf(out->files[out->count].sha1, sizeof(out->files[0].sha1), "%s", sha1);
            p = end + 1;
        } else if (*p == '{') {
            const char *obj_end = NULL;
            if (parse_state_object(p, &out->files[out->count], &obj_end) != 0) {
                free(buf);
                return -1;
            }
            p = obj_end;
        } else {
            free(buf);
            return -1;
        }
        if (!out->files[out->count].sha1[0]) {
            continue;
        }
        out->count++;
    }
    free(buf);
    meta_sort(out);
    return 0;
}

static int folder_meta_path(const char *root, const char *folder, char *out, size_t cap) {
    if (!root || !folder || !out) {
        return -1;
    }
    return snprintf(out, cap, "%s/%s/%s", root, folder, R01_ASEPRITE_META_JSON) >= (int)cap ? -1 : 0;
}

static int folder_dir_path(const char *root, const char *folder, char *out, size_t cap) {
    if (!root || !folder || !out) {
        return -1;
    }
    return snprintf(out, cap, "%s/%s", root, folder) >= (int)cap ? -1 : 0;
}

static int store_listing_without_folders(R01Project *p, const R01AsepriteListing *disk, const char *const *skip,
                                         int skip_n) {
    int i;
    if (!p || !disk) {
        return -1;
    }
    p->aseprite_entities_file_count = 0;
    for (i = 0; i < disk->count; i++) {
        char folder[R01_ENTITY_NAME_MAX];
        const char *slash = strchr(disk->files[i], '/');
        int n;
        int k;
        int drop = 0;
        if (!slash) {
            continue;
        }
        n = (int)(slash - disk->files[i]);
        if (n >= (int)sizeof(folder)) {
            n = (int)sizeof(folder) - 1;
        }
        memcpy(folder, disk->files[i], (size_t)n);
        folder[n] = '\0';
        for (k = 0; k < skip_n; k++) {
            if (skip[k] && strcmp(skip[k], folder) == 0) {
                drop = 1;
                break;
            }
        }
        if (drop) {
            continue;
        }
        if (p->aseprite_entities_file_count >= R01_ASEPRITE_LISTING_MAX) {
            break;
        }
        snprintf(p->aseprite_entities_files[p->aseprite_entities_file_count], R01_ASEPRITE_REL_MAX, "%s",
                 disk->files[i]);
        p->aseprite_entities_file_count++;
    }
    return 0;
}

static int spr_pal0_rgb(const R01Project *p, const R01World *w, uint8_t rgb[4][3]) {
    int row;
    int i;
    if (!p || !w || !rgb) {
        return -1;
    }
    row = w->default_pal_row;
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    for (i = 0; i < R01_PAL_COLORS; i++) {
        r01_kit_rgb(p->global_pal_spr[row][0].idx[i], &rgb[i][0], &rgb[i][1], &rgb[i][2]);
    }
    return 0;
}

static int map_opaque_to_pal(uint8_t rgb[4][3], uint8_t r, uint8_t g, uint8_t b, char *err_buf,
                             size_t err_cap) {
    int i;
    if (r01_kit_exact_master(r, g, b) < 0) {
        if (err_buf && err_cap > 0) {
            snprintf(err_buf, err_cap, "not a kit color %02X%02X%02X", r, g, b);
        }
        return -1;
    }
    for (i = 1; i < R01_PAL_COLORS; i++) {
        if (rgb[i][0] == r && rgb[i][1] == g && rgb[i][2] == b) {
            return i;
        }
    }
    set_err(err_buf, err_cap, "color not in sprite pal 0");
    return -1;
}

static int quad_to_tile(uint8_t out16[R01_TILE_BYTES], const uint8_t *rgba, int img_w, int img_h, int ox, int oy,
                        uint8_t pal_rgb[4][3], int *out_blank, char *err_buf, size_t err_cap) {
    int sx, sy;
    int opaque_n = 0;
    uint8_t opaque[3][3];
    int live = 0;
    memset(out16, 0, R01_TILE_BYTES);
    *out_blank = 1;
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            int ix = ox + sx;
            int iy = oy + sy;
            const uint8_t *px;
            uint8_t col;
            if (ix < 0 || iy < 0 || ix >= img_w || iy >= img_h) {
                continue;
            }
            px = rgba + ((size_t)iy * (size_t)img_w + (size_t)ix) * 4u;
            if (px[3] < IMPORT_ALPHA_CUTOFF) {
                continue;
            }
            {
                int mapped = map_opaque_to_pal(pal_rgb, px[0], px[1], px[2], err_buf, err_cap);
                if (mapped < 1) {
                    return -1;
                }
                col = (uint8_t)mapped;
            }
            if (opaque_n < 3) {
                int k;
                int seen = 0;
                for (k = 0; k < opaque_n; k++) {
                    if (opaque[k][0] == px[0] && opaque[k][1] == px[1] && opaque[k][2] == px[2]) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen) {
                    opaque[opaque_n][0] = px[0];
                    opaque[opaque_n][1] = px[1];
                    opaque[opaque_n][2] = px[2];
                    opaque_n++;
                }
            } else {
                int k;
                int seen = 0;
                for (k = 0; k < opaque_n; k++) {
                    if (opaque[k][0] == px[0] && opaque[k][1] == px[1] && opaque[k][2] == px[2]) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen) {
                    set_err(err_buf, err_cap, "quad has more than 3 opaque colors");
                    return -1;
                }
            }
            r01_tile_set_pixel(out16, sx, sy, col);
            live = 1;
        }
    }
    *out_blank = !live;
    if (opaque_n > 3) {
        set_err(err_buf, err_cap, "quad has more than 3 opaque colors");
        return -1;
    }
    return 0;
}

static int find_existing_spr_pattern(const R01Project *p, const R01World *w, const uint8_t tile[R01_TILE_BYTES],
                                     int *bank, int *tile_id) {
    int b;
    int id;
    (void)w;
    if (!p || !tile || !bank || !tile_id) {
        return 0;
    }
    for (b = 0; b < R01_SPR_BANKS; b++) {
        for (id = 0; id < p->chr_banks[b].tile_count; id++) {
            const uint8_t *have = r01_chr_spr_tile(p, b, id);
            if (have && memcmp(have, tile, R01_TILE_BYTES) == 0) {
                *bank = b;
                *tile_id = id;
                return 1;
            }
        }
    }
    return 0;
}

static int dedupe_chr(R01Project *p, R01World *w, const uint8_t tile[R01_TILE_BYTES], int *bank, int *tile_id) {
    int b;
    int id;
    if (find_existing_spr_pattern(p, w, tile, bank, tile_id)) {
        return 0;
    }
    b = r01_chr_find_spr_bank_space(p);
    if (b < 0) {
        return -1;
    }
    id = r01_chr_alloc_spr_tile(p, b);
    if (id < 0 || r01_chr_write_spr_tile(p, b, id, tile) != 0) {
        return -1;
    }
    *bank = b;
    *tile_id = id;
    return 0;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static const R01EntityState *saved_state_by_name(const R01EntityType *prev, const char *name) {
    int si;
    if (!prev || !name || !name[0]) {
        return NULL;
    }
    for (si = 0; si < prev->state_count && si < R01_ENTITY_STATES_MAX; si++) {
        if (strcmp(prev->states[si].name, name) == 0) {
            return &prev->states[si];
        }
    }
    return NULL;
}

/* Keep Studio origin, hitbox, and compose sprite positions on states that
 * still exist under the same name. Parts match by bank+tile first, then leftover
 * old slots in order, so origin/hitbox stay aligned with the sprites. */
static void restore_frame_part_pos(R01EntityFrame *fr, const R01EntityFrame *old) {
    int used[R01_ENTITY_PARTS_MAX];
    int taken[R01_ENTITY_PARTS_MAX];
    int pi;
    int pass;
    if (!fr || !old) {
        return;
    }
    memset(used, 0, sizeof(used));
    memset(taken, 0, sizeof(taken));
    for (pass = 0; pass < 2; pass++) {
        for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
            R01EntityPart *pt;
            int oi;
            int found = -1;
            if (taken[pi]) {
                continue;
            }
            pt = &fr->parts[pi];
            for (oi = 0; oi < old->part_count && oi < R01_ENTITY_PARTS_MAX; oi++) {
                const R01EntityPart *op = &old->parts[oi];
                if (used[oi]) {
                    continue;
                }
                if (pass == 0 && (op->bank != pt->bank || op->tile_id != pt->tile_id)) {
                    continue;
                }
                found = oi;
                break;
            }
            if (found < 0) {
                continue;
            }
            used[found] = 1;
            taken[pi] = 1;
            pt->dx = clamp_int(old->parts[found].dx, 0, R01_ENTITY_COMPOSE_PX - 8);
            pt->dy = clamp_int(old->parts[found].dy, 0, R01_ENTITY_COMPOSE_PX - 8);
        }
    }
}

static void restore_entity_guides(R01EntityType *e, const R01EntityType *prev) {
    int si;
    if (!e || !prev) {
        return;
    }
    for (si = 0; si < e->state_count && si < R01_ENTITY_STATES_MAX; si++) {
        R01EntityState *st = &e->states[si];
        const R01EntityState *old = saved_state_by_name(prev, st->name);
        int fi;
        if (!old) {
            continue;
        }
        st->hitbox_x = old->hitbox_x;
        st->hitbox_y = old->hitbox_y;
        st->hitbox_w = old->hitbox_w;
        st->hitbox_h = old->hitbox_h;
        r01_entity_state_clamp_hitbox(st);
        for (fi = 0; fi < st->frame_count && fi < old->frame_count && fi < R01_ENTITY_FRAMES_MAX; fi++) {
            st->frames[fi].origin_x = clamp_int(old->frames[fi].origin_x, 0, R01_ENTITY_COMPOSE_PX);
            st->frames[fi].origin_y = clamp_int(old->frames[fi].origin_y, 0, R01_ENTITY_COMPOSE_PX);
            restore_frame_part_pos(&st->frames[fi], &old->frames[fi]);
        }
    }
}

static int frame_parts_aabb(const R01EntityFrame *fr, int *min_x, int *min_y, int *max_x, int *max_y) {
    int pi;
    int have = 0;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!fr) {
        return 0;
    }
    for (pi = 0; pi < fr->part_count; pi++) {
        const R01EntityPart *pt = &fr->parts[pi];
        int px0 = pt->dx;
        int py0 = pt->dy;
        int px1 = pt->dx + 8;
        int py1 = pt->dy + 8;
        if (!have) {
            x0 = px0;
            y0 = py0;
            x1 = px1;
            y1 = py1;
            have = 1;
        } else {
            if (px0 < x0) {
                x0 = px0;
            }
            if (py0 < y0) {
                y0 = py0;
            }
            if (px1 > x1) {
                x1 = px1;
            }
            if (py1 > y1) {
                y1 = py1;
            }
        }
    }
    if (!have) {
        return 0;
    }
    if (min_x) {
        *min_x = x0;
    }
    if (min_y) {
        *min_y = y0;
    }
    if (max_x) {
        *max_x = x1;
    }
    if (max_y) {
        *max_y = y1;
    }
    return 1;
}

/* Place the sprite group in the middle of the 32x32 compose grid, then put origin
 * on that group's AABB. Play uses origin-relative coords, so the compose
 * translation does not change draw. */
static void frame_import_center_compose(R01EntityFrame *fr) {
    int min_x, min_y, max_x, max_y;
    int off_x, off_y;
    int pi;
    if (!frame_parts_aabb(fr, &min_x, &min_y, &max_x, &max_y)) {
        return;
    }
    off_x = (R01_ENTITY_COMPOSE_PX - (max_x - min_x)) / 2 - min_x;
    off_y = (R01_ENTITY_COMPOSE_PX - (max_y - min_y)) / 2 - min_y;
    for (pi = 0; pi < fr->part_count; pi++) {
        fr->parts[pi].dx = clamp_int(fr->parts[pi].dx + off_x, 0, R01_ENTITY_COMPOSE_PX - 8);
        fr->parts[pi].dy = clamp_int(fr->parts[pi].dy + off_y, 0, R01_ENTITY_COMPOSE_PX - 8);
    }
    if (!frame_parts_aabb(fr, &min_x, &min_y, &max_x, &max_y)) {
        return;
    }
    fr->origin_x = clamp_int((min_x + max_x) / 2, 0, R01_ENTITY_COMPOSE_PX);
    fr->origin_y = clamp_int((min_y + max_y) / 2, 0, R01_ENTITY_COMPOSE_PX);
}

static void state_import_center_hitbox(R01EntityState *st) {
    int min_x, min_y, max_x, max_y;
    int hw = R01_ENTITY_HITBOX_W;
    int hh = R01_ENTITY_HITBOX_H;
    int fi;
    if (!st) {
        return;
    }
    for (fi = 0; fi < st->frame_count; fi++) {
        if (frame_parts_aabb(&st->frames[fi], &min_x, &min_y, &max_x, &max_y)) {
            st->hitbox_w = hw;
            st->hitbox_h = hh;
            st->hitbox_x = clamp_int((min_x + max_x - hw) / 2, 0, R01_ENTITY_COMPOSE_PX - hw);
            st->hitbox_y = clamp_int((min_y + max_y - hh) / 2, 0, R01_ENTITY_COMPOSE_PX - hh);
            return;
        }
    }
}

static int validate_entity_import(R01Project *p, R01World *w, const R01EntityImport *in, uint8_t pal_rgb[4][3],
                                  char *err_buf, size_t err_cap) {
    int si;
    if (!p || !w || !in || in->state_count < 1 || in->state_count > R01_ENTITY_STATES_MAX) {
        set_err(err_buf, err_cap, "bad entity import");
        return -1;
    }
    if (spr_pal0_rgb(p, w, pal_rgb) != 0) {
        set_err(err_buf, err_cap, "bad sprite pal 0");
        return -1;
    }
    for (si = 0; si < in->state_count; si++) {
        int fi;
        const R01EntityImportState *st = &in->states[si];
        if (st->frame_count < 1 || st->frame_count > R01_ENTITY_FRAMES_MAX) {
            set_err(err_buf, err_cap, "bad frame count");
            return -1;
        }
        for (fi = 0; fi < st->frame_count; fi++) {
            const R01EntityImportFrame *fr = &st->frames[fi];
            char msg[160];
            if (!fr->rgba || fr->w < 8 || fr->h < 8) {
                snprintf(msg, sizeof(msg), "%s %s f%d: bad frame size %dx%d", in->name, st->name, fi, fr->w,
                         fr->h);
                set_err(err_buf, err_cap, msg);
                return -1;
            }
            if ((fr->w % 8) != 0 || (fr->h % 8) != 0) {
                snprintf(msg, sizeof(msg), "%s %s f%d: %dx%d must be a multiple of 8", in->name, st->name, fi,
                         fr->w, fr->h);
                set_err(err_buf, err_cap, msg);
                return -1;
            }
            if (fr->w > R01_ENTITY_COMPOSE_PX || fr->h > R01_ENTITY_COMPOSE_PX) {
                snprintf(msg, sizeof(msg), "%s %s f%d: %dx%d exceeds 32x32", in->name, st->name, fi, fr->w,
                         fr->h);
                set_err(err_buf, err_cap, msg);
                return -1;
            }
        }
    }
    return 0;
}

static int fill_entity_from_import(R01Project *p, R01World *w, R01EntityType *e, const R01EntityImport *in,
                                   uint8_t pal_rgb[4][3], char *err_buf, size_t err_cap) {
    int si;
    char qerr[128];

    r01_entity_type_init(e);
    snprintf(e->name, sizeof(e->name), "%s", in->name[0] ? in->name : "Entity");
    e->state_count = 0;
    for (si = 0; si < in->state_count; si++) {
        const R01EntityImportState *src = &in->states[si];
        R01EntityState *st;
        int fi;
        r01_entity_ensure_state(e, si);
        st = r01_entity_state(e, si);
        if (!st) {
            set_err(err_buf, err_cap, "bad state");
            return -1;
        }
        snprintf(st->name, sizeof(st->name), "%s", src->name[0] ? src->name : r01_entity_default_state_name(si));
        st->frame_count = 0;
        for (fi = 0; fi < src->frame_count; fi++) {
            const R01EntityImportFrame *fr = &src->frames[fi];
            R01EntityFrame *dst;
            int col, row;
            int cols;
            int rows;
            int delay = fr->delay < 1 ? 1 : fr->delay;
            if (delay > 255) {
                delay = 255;
            }
            r01_entity_ensure_frame(e, si, fi);
            dst = r01_entity_frame(e, si, fi);
            if (!dst) {
                set_err(err_buf, err_cap, "bad frame");
                return -1;
            }
            dst->part_count = 0;
            dst->delay = delay;
            cols = fr->w / 8;
            rows = fr->h / 8;
            for (row = 0; row < rows; row++) {
                for (col = 0; col < cols; col++) {
                    uint8_t tile[R01_TILE_BYTES];
                    int blank = 0;
                    R01EntityPart part;
                    int bank = 0;
                    int tile_id = 0;
                    qerr[0] = '\0';
                    if (quad_to_tile(tile, fr->rgba, fr->w, fr->h, col * 8, row * 8, pal_rgb, &blank, qerr,
                                     sizeof(qerr)) != 0) {
                        char msg[160];
                        snprintf(msg, sizeof(msg), "%s %s f%d tile %d,%d: %s", e->name, st->name, fi, col, row,
                                 qerr[0] ? qerr : "quad error");
                        set_err(err_buf, err_cap, msg);
                        return -1;
                    }
                    if (blank) {
                        continue;
                    }
                    if (dst->part_count >= R01_ENTITY_PARTS_MAX) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "%s %s f%d has more than 6 sprites", e->name, st->name, fi);
                        set_err(err_buf, err_cap, msg);
                        return -1;
                    }
                    if (dedupe_chr(p, w, tile, &bank, &tile_id) != 0) {
                        set_err(err_buf, err_cap, "sprite CHR full");
                        return -1;
                    }
                    memset(&part, 0, sizeof(part));
                    part.bank = bank;
                    part.tile_id = tile_id;
                    part.pal = 0;
                    part.dx = col * 8;
                    part.dy = row * 8;
                    if (r01_entity_frame_add_part(dst, &part) < 0) {
                        set_err(err_buf, err_cap, "could not add sprite");
                        return -1;
                    }
                }
            }
            if (dst->part_count < 1) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%s %s f%d is empty", e->name, st->name, fi);
                set_err(err_buf, err_cap, msg);
                return -1;
            }
            frame_import_center_compose(dst);
        }
        state_import_center_hitbox(st);
    }
    return 0;
}

int r01_world_import_entity_frames(R01Project *p, R01World *w, const R01EntityImport *in, char *err_buf,
                                   size_t err_cap) {
    uint8_t pal_rgb[4][3];
    int idx;
    R01EntityType *e;

    if (validate_entity_import(p, w, in, pal_rgb, err_buf, err_cap) != 0) {
        return -1;
    }
    idx = r01_world_entity_add(p);
    if (idx < 0) {
        set_err(err_buf, err_cap, "entity catalog full");
        return -1;
    }
    e = r01_world_entity(p, idx);
    if (fill_entity_from_import(p, w, e, in, pal_rgb, err_buf, err_cap) != 0) {
        r01_world_entity_remove(p, idx);
        return -1;
    }
    return idx;
}

int r01_world_import_entity_frames_replace(R01Project *p, R01World *w, int type_idx, const R01EntityImport *in,
                                           char *err_buf, size_t err_cap) {
    uint8_t pal_rgb[4][3];
    R01EntityType *e;
    R01EntityType saved;

    if (!w || type_idx < 0 || type_idx >= p->entity_count) {
        set_err(err_buf, err_cap, "bad entity replace");
        return -1;
    }
    if (validate_entity_import(p, w, in, pal_rgb, err_buf, err_cap) != 0) {
        return -1;
    }
    e = r01_world_entity(p, type_idx);
    saved = *e;
    if (fill_entity_from_import(p, w, e, in, pal_rgb, err_buf, err_cap) != 0) {
        *e = saved;
        return -1;
    }
    restore_entity_guides(e, &saved);
    if (r01_world_player_entity(p) == type_idx) {
        if (r01_project_set_player_entity(p, w, type_idx) != 0) {
            *e = saved;
            set_err(err_buf, err_cap, "could not move player sprites");
            return -1;
        }
    }
    return type_idx;
}

static int load_png_rgba(const char *path, uint8_t **out_px, int *out_w, int *out_h) {
    FILE *fp;
    png_structp png;
    png_infop info;
    png_byte header[8];
    png_bytep *rows = NULL;
    uint8_t *px = NULL;
    int w, h, y;
    if (!path || !out_px || !out_w || !out_h) {
        return -1;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }
    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8) != 0) {
        fclose(fp);
        return -1;
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        free(rows);
        free(px);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);
    w = (int)png_get_image_width(png, info);
    h = (int)png_get_image_height(png, info);
    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    if (png_get_channels(png, info) == 3) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
        png_read_update_info(png, info);
    }
    if (png_get_channels(png, info) != 4 || png_get_rowbytes(png, info) != (png_size_t)w * 4u) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }
    px = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
    rows = (png_bytep *)malloc((size_t)h * sizeof(png_bytep));
    if (!px || !rows) {
        free(rows);
        free(px);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }
    for (y = 0; y < h; y++) {
        rows[y] = px + (size_t)y * (size_t)w * 4u;
    }
    png_read_image(png, rows);
    png_read_end(png, NULL);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    *out_px = px;
    *out_w = w;
    *out_h = h;
    return 0;
}

static int find_aseprite(char *out, size_t cap) {
    const char *env = getenv("ASEPRITE");
    const char *path;
    if (!out || cap < 2) {
        return -1;
    }
    if (env && env[0] && access(env, X_OK) == 0) {
        snprintf(out, cap, "%s", env);
        return 0;
    }
    path = getenv("PATH");
    if (!path) {
        return -1;
    }
    while (*path) {
        const char *sep = strchr(path, ':');
        size_t n = sep ? (size_t)(sep - path) : strlen(path);
        if (n > 0 && n + 10u <= cap) {
            memcpy(out, path, n);
            memcpy(out + n, "/aseprite", 10);
            if (access(out, X_OK) == 0) {
                return 0;
            }
        }
        if (!sep) {
            break;
        }
        path = sep + 1;
    }
    return -1;
}

static int run_argv(char *const argv[]) {
    pid_t pid;
    int st;
    if (!argv || !argv[0]) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    if (waitpid(pid, &st, 0) < 0) {
        return -1;
    }
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        return -1;
    }
    return 0;
}

static int read_file(const char *path, char **out, size_t *out_len) {
    FILE *f;
    long sz;
    char *buf;
    if (!path || !out) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[sz] = '\0';
    *out = buf;
    if (out_len) {
        *out_len = (size_t)sz;
    }
    return 0;
}

typedef struct AseFrameMeta {
    int x, y, w, h, duration_ms;
} AseFrameMeta;

static const char *json_arr_end(const char *start) {
    int depth = 0;
    int in_str = 0;
    const char *p;
    if (!start || *start != '[') {
        return NULL;
    }
    for (p = start; *p; p++) {
        if (in_str) {
            if (*p == '\\' && p[1]) {
                p++;
                continue;
            }
            if (*p == '\"') {
                in_str = 0;
            }
            continue;
        }
        if (*p == '\"') {
            in_str = 1;
            continue;
        }
        if (*p == '[') {
            depth++;
        } else if (*p == ']') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static const char *json_obj_end(const char *start) {
    int depth = 0;
    int in_str = 0;
    const char *p;
    if (!start || *start != '{') {
        return NULL;
    }
    for (p = start; *p; p++) {
        if (in_str) {
            if (*p == '\\' && p[1]) {
                p++;
                continue;
            }
            if (*p == '\"') {
                in_str = 0;
            }
            continue;
        }
        if (*p == '\"') {
            in_str = 1;
            continue;
        }
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static int json_int_key(const char *p, const char *key, int *out) {
    const char *k = p ? strstr(p, key) : NULL;
    char *end;
    long v;
    if (!k || !out) {
        return 0;
    }
    k += strlen(key);
    while (*k == ' ' || *k == '\t' || *k == ':' || *k == '\"') {
        k++;
    }
    v = strtol(k, &end, 10);
    if (end == k) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int parse_ase_frames(const char *json, AseFrameMeta *out, int cap, int *n) {
    const char *sec;
    const char *arr;
    const char *arr_end;
    const char *obj;
    int count = 0;
    if (!json || !out || !n || cap < 1) {
        return -1;
    }
    *n = 0;
    sec = strstr(json, "\"frames\"");
    arr = sec ? strchr(sec, '[') : NULL;
    arr_end = arr ? json_arr_end(arr) : NULL;
    if (!arr || !arr_end) {
        return -1;
    }
    obj = strchr(arr, '{');
    while (obj && obj < arr_end && count < cap) {
        const char *end = json_obj_end(obj);
        char *slice;
        size_t len;
        const char *fr;
        if (!end) {
            break;
        }
        len = (size_t)(end - obj + 1);
        slice = (char *)malloc(len + 1u);
        if (!slice) {
            return -1;
        }
        memcpy(slice, obj, len);
        slice[len] = '\0';
        out[count].duration_ms = 100;
        json_int_key(slice, "\"duration\"", &out[count].duration_ms);
        fr = strstr(slice, "\"frame\"");
        if (fr) {
            json_int_key(fr, "\"x\"", &out[count].x);
            json_int_key(fr, "\"y\"", &out[count].y);
            json_int_key(fr, "\"w\"", &out[count].w);
            json_int_key(fr, "\"h\"", &out[count].h);
        }
        free(slice);
        if (out[count].w < 1 || out[count].h < 1) {
            return -1;
        }
        count++;
        obj = strchr(end + 1, '{');
    }
    *n = count;
    return count > 0 ? 0 : -1;
}

static void blit_rgba(uint8_t *dst, int dw, int dh, const uint8_t *src, int sw, int sh, int sx, int sy) {
    int y;
    for (y = 0; y < dh; y++) {
        int iy = sy + y;
        if (iy < 0 || iy >= sh || sx < 0 || sx + dw > sw) {
            memset(dst + (size_t)y * (size_t)dw * 4u, 0, (size_t)dw * 4u);
            continue;
        }
        memcpy(dst + (size_t)y * (size_t)dw * 4u, src + ((size_t)iy * (size_t)sw + (size_t)sx) * 4u,
               (size_t)dw * 4u);
    }
}

static int mkdir_p(const char *path) {
    char tmp[R01_PATH_MAX];
    size_t i;
    size_t n;
    if (!path || !path[0]) {
        return -1;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    n = strlen(tmp);
    for (i = 1; i < n; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void rm_tree(const char *path) {
    DIR *d;
    struct dirent *ent;
    if (!path || !path[0]) {
        return;
    }
    d = opendir(path);
    if (!d) {
        unlink(path);
        return;
    }
    while ((ent = readdir(d)) != NULL) {
        char child[R01_PATH_MAX];
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
            continue;
        }
        if (snprintf(child, sizeof(child), "%s/%s", path, ent->d_name) >= (int)sizeof(child)) {
            continue;
        }
        if (path_is_dir(child)) {
            rm_tree(child);
        } else {
            unlink(child);
        }
    }
    closedir(d);
    rmdir(path);
}

static int export_ase_state(const char *aseprite, const char *ase_path, const char *work,
                            R01EntityImportState *st, char *err_buf, size_t err_cap) {
    char sheet[R01_PATH_MAX];
    char json_path[R01_PATH_MAX];
    char *argv[16];
    char *json = NULL;
    uint8_t *sheet_px = NULL;
    int sheet_w = 0, sheet_h = 0;
    AseFrameMeta meta[R01_ENTITY_FRAMES_MAX + 4];
    int n = 0;
    int i;
    uint8_t *owned[R01_ENTITY_FRAMES_MAX];
    int owned_n = 0;

    memset(owned, 0, sizeof(owned));
    if (snprintf(sheet, sizeof(sheet), "%s/sheet.png", work) >= (int)sizeof(sheet) ||
        snprintf(json_path, sizeof(json_path), "%s/sheet.json", work) >= (int)sizeof(json_path)) {
        set_err(err_buf, err_cap, "temp path too long");
        return -1;
    }
    argv[0] = (char *)aseprite;
    argv[1] = "-b";
    argv[2] = "--all-layers";
    argv[3] = "--ignore-empty";
    argv[4] = "--format";
    argv[5] = "json-array";
    argv[6] = "--sheet-type";
    argv[7] = "horizontal";
    argv[8] = "--sheet";
    argv[9] = sheet;
    argv[10] = "--data";
    argv[11] = json_path;
    argv[12] = (char *)ase_path;
    argv[13] = NULL;
    if (run_argv(argv) != 0) {
        set_err(err_buf, err_cap, "aseprite export failed");
        return -1;
    }
    if (read_file(json_path, &json, NULL) != 0 || parse_ase_frames(json, meta, (int)(sizeof(meta) / sizeof(meta[0])),
                                                                 &n) != 0) {
        free(json);
        set_err(err_buf, err_cap, "aseprite json missing frames");
        return -1;
    }
    free(json);
    if (n > R01_ENTITY_FRAMES_MAX) {
        set_err(err_buf, err_cap, "more than 4 frames");
        return -1;
    }
    if (load_png_rgba(sheet, &sheet_px, &sheet_w, &sheet_h) != 0) {
        set_err(err_buf, err_cap, "aseprite sheet png failed");
        return -1;
    }
    st->frame_count = n;
    for (i = 0; i < n; i++) {
        uint8_t *frame;
        int fw = meta[i].w;
        int fh = meta[i].h;
        frame = (uint8_t *)malloc((size_t)fw * (size_t)fh * 4u);
        if (!frame) {
            while (owned_n > 0) {
                owned_n--;
                free(owned[owned_n]);
            }
            free(sheet_px);
            set_err(err_buf, err_cap, "oom");
            return -1;
        }
        blit_rgba(frame, fw, fh, sheet_px, sheet_w, sheet_h, meta[i].x, meta[i].y);
        st->frames[i].rgba = frame;
        st->frames[i].w = fw;
        st->frames[i].h = fh;
        st->frames[i].delay = ms_to_delay(meta[i].duration_ms);
        owned[owned_n++] = frame;
    }
    free(sheet_px);
    return 0;
}

static void free_import_rgba(R01EntityImport *in) {
    int si, fi;
    if (!in) {
        return;
    }
    for (si = 0; si < in->state_count; si++) {
        for (fi = 0; fi < in->states[si].frame_count; fi++) {
            free((void *)in->states[si].frames[fi].rgba);
            in->states[si].frames[fi].rgba = NULL;
        }
    }
}

static int collect_folder_files(const R01AsepriteListing *disk, const char *folder, char names[][R01_ASEPRITE_REL_MAX],
                                int cap) {
    int i;
    int n = 0;
    size_t flen;
    if (!disk || !folder) {
        return 0;
    }
    flen = strlen(folder);
    for (i = 0; i < disk->count; i++) {
        if (strncmp(disk->files[i], folder, flen) != 0 || disk->files[i][flen] != '/') {
            continue;
        }
        if (n >= cap) {
            return -1;
        }
        snprintf(names[n], R01_ASEPRITE_REL_MAX, "%s", disk->files[i] + flen + 1);
        n++;
    }
    return n;
}

static int assign_states(char names[][R01_ASEPRITE_REL_MAX], int n, int *order) {
    int used[R01_ENTITY_STATES_MAX];
    int i;
    int filled = 0;
    memset(used, 0, sizeof(used));
    for (i = 0; i < R01_ENTITY_STATES_MAX; i++) {
        order[i] = -1;
    }
    for (i = 0; i < n; i++) {
        char stem[R01_ENTITY_NAME_MAX];
        char slug[R01_ENTITY_NAME_MAX];
        int slot;
        stem_from_filename(names[i], stem, sizeof(stem));
        r01_id_slugify(slug, sizeof(slug), stem);
        slot = state_slot_hint(slug);
        if (slot >= 0 && !used[slot]) {
            used[slot] = 1;
            order[slot] = i;
            filled++;
        }
    }
    for (i = 0; i < n; i++) {
        int slot;
        int already = 0;
        int s;
        for (s = 0; s < R01_ENTITY_STATES_MAX; s++) {
            if (order[s] == i) {
                already = 1;
                break;
            }
        }
        if (already) {
            continue;
        }
        for (slot = 0; slot < R01_ENTITY_STATES_MAX; slot++) {
            if (!used[slot]) {
                used[slot] = 1;
                order[slot] = i;
                filled++;
                break;
            }
        }
    }
    if (filled > 0 && order[0] < 0) {
        for (i = 1; i < R01_ENTITY_STATES_MAX; i++) {
            if (order[i] >= 0) {
                order[0] = order[i];
                order[i] = -1;
                break;
            }
        }
    }
    return filled;
}

static int import_one_folder(R01Project *p, R01World *w, const char *root, const char *folder,
                             const R01AsepriteListing *disk, const char *aseprite, int replace_idx,
                             char *err_buf, size_t err_cap) {
    char files[R01_ENTITY_STATES_MAX + 4][R01_ASEPRITE_REL_MAX];
    int order[R01_ENTITY_STATES_MAX];
    int n;
    int assigned;
    int i;
    char work[R01_PATH_MAX];
    R01EntityImport in;
    int rc;

    n = collect_folder_files(disk, folder, files, R01_ENTITY_STATES_MAX + 4);
    if (n < 0 || n > R01_ENTITY_STATES_MAX) {
        set_err(err_buf, err_cap, "more than 4 state files");
        return -1;
    }
    if (n < 1) {
        set_err(err_buf, err_cap, "empty entity folder");
        return -1;
    }
    assigned = assign_states(files, n, order);
    if (assigned < 1) {
        set_err(err_buf, err_cap, "no states");
        return -1;
    }
    if (snprintf(work, sizeof(work), "/tmp/r01_ase_%d_%s", (int)getpid(), folder) >= (int)sizeof(work)) {
        set_err(err_buf, err_cap, "temp path too long");
        return -1;
    }
    rm_tree(work);
    if (mkdir_p(work) != 0) {
        set_err(err_buf, err_cap, "cannot create temp dir");
        return -1;
    }
    memset(&in, 0, sizeof(in));
    snprintf(in.name, sizeof(in.name), "%s", folder);
    for (i = 0; i < R01_ENTITY_STATES_MAX; i++) {
        char ase_path[R01_PATH_MAX];
        char stem[R01_ENTITY_NAME_MAX];
        char stwork[R01_PATH_MAX];
        if (order[i] < 0) {
            continue;
        }
        stem_from_filename(files[order[i]], stem, sizeof(stem));
        snprintf(in.states[in.state_count].name, sizeof(in.states[0].name), "%s", stem);
        if (snprintf(ase_path, sizeof(ase_path), "%s/%s/%s", root, folder, files[order[i]]) >=
                (int)sizeof(ase_path) ||
            snprintf(stwork, sizeof(stwork), "%s/s%d", work, in.state_count) >= (int)sizeof(stwork)) {
            free_import_rgba(&in);
            rm_tree(work);
            set_err(err_buf, err_cap, "path too long");
            return -1;
        }
        if (mkdir_p(stwork) != 0 ||
            export_ase_state(aseprite, ase_path, stwork, &in.states[in.state_count], err_buf, err_cap) != 0) {
            free_import_rgba(&in);
            rm_tree(work);
            return -1;
        }
        in.state_count++;
    }
    if (replace_idx >= 0) {
        rc = r01_world_import_entity_frames_replace(p, w, replace_idx, &in, err_buf, err_cap);
    } else {
        rc = r01_world_import_entity_frames(p, w, &in, err_buf, err_cap);
    }
    free_import_rgba(&in);
    rm_tree(work);
    return rc < 0 ? -1 : 1;
}

static int project_dir(const char *project_path, char *out, size_t cap) {
    const char *slash;
    size_t n;
    if (!project_path || !project_path[0] || !out || cap < 2) {
        return -1;
    }
    slash = strrchr(project_path, '/');
    if (!slash) {
        snprintf(out, cap, ".");
        return 0;
    }
    n = (size_t)(slash - project_path);
    if (n == 0) {
        snprintf(out, cap, "/");
        return 0;
    }
    if (n >= cap) {
        return -1;
    }
    memcpy(out, project_path, n);
    out[n] = '\0';
    return 0;
}

int r01_project_import_aseprite_entities(R01Project *p, const char *project_path, R01AsepriteImportResult *out,
                                         char *err_buf, size_t err_cap) {
    char dir[R01_PATH_MAX];
    char root[R01_PATH_MAX];
    R01AsepriteListing disk;
    R01World *w;
    char aseprite[R01_PATH_MAX];
    int have_ase = 0;
    int i;
    int generated = 0;
    char failed[R01_MAX_ENTITY_TYPES][R01_ENTITY_NAME_MAX];
    int failed_n = 0;
    const char *skip_ptrs[R01_MAX_ENTITY_TYPES];
    char folders[R01_MAX_ENTITY_TYPES][R01_ENTITY_NAME_MAX];
    int folder_n = 0;

    if (out) {
        memset(out, 0, sizeof(*out));
    }
    if (!p || !project_path || !project_path[0]) {
        set_err(err_buf, err_cap, "save the project first");
        return -1;
    }
    w = r01_project_active_world(p);
    if (!w) {
        set_err(err_buf, err_cap, "bad world");
        return -1;
    }
    if (project_dir(project_path, dir, sizeof(dir)) != 0 ||
        snprintf(root, sizeof(root), "%s/%s", dir, R01_ASEPRITE_ENTITIES_DIR) >= (int)sizeof(root)) {
        set_err(err_buf, err_cap, "bad project path");
        return -1;
    }
    if (r01_aseprite_listing_scan(root, &disk, err_buf, err_cap) != 0) {
        return -1;
    }
    for (i = 0; i < disk.count; i++) {
        char folder[R01_ENTITY_NAME_MAX];
        const char *slash = strchr(disk.files[i], '/');
        int n;
        int k;
        int seen = 0;
        if (!slash) {
            continue;
        }
        n = (int)(slash - disk.files[i]);
        if (n >= (int)sizeof(folder)) {
            n = (int)sizeof(folder) - 1;
        }
        memcpy(folder, disk.files[i], (size_t)n);
        folder[n] = '\0';
        for (k = 0; k < folder_n; k++) {
            if (strcmp(folders[k], folder) == 0) {
                seen = 1;
                break;
            }
        }
        if (!seen && folder_n < R01_MAX_ENTITY_TYPES) {
            snprintf(folders[folder_n], sizeof(folders[0]), "%s", folder);
            folder_n++;
        }
    }
    for (i = 0; i < folder_n; i++) {
        char slug[R01_ENTITY_NAME_MAX];
        char folder_path[R01_PATH_MAX];
        char meta_path[R01_PATH_MAX];
        R01AsepriteFolderMeta disk_meta;
        R01AsepriteFolderMeta saved_meta;
        int existing;
        int have_meta;
        int match;
        int rc;
        r01_id_slugify(slug, sizeof(slug), folders[i]);
        existing = catalog_index_for_slug(p, slug);
        if (folder_dir_path(root, folders[i], folder_path, sizeof(folder_path)) != 0 ||
            folder_meta_path(root, folders[i], meta_path, sizeof(meta_path)) != 0) {
            set_err(err_buf, err_cap, "path too long");
            return -1;
        }
        if (r01_aseprite_folder_meta_scan(folder_path, &disk_meta, err_buf, err_cap) != 0) {
            return -1;
        }
        have_meta = r01_aseprite_folder_meta_load(meta_path, &saved_meta, NULL, 0) == 0;
        match = have_meta && r01_aseprite_folder_meta_equal(&disk_meta, &saved_meta);
        if (existing >= 0 && match) {
            continue;
        }
        if (existing >= 0 && access(meta_path, F_OK) != 0) {
            (void)r01_aseprite_folder_meta_save(meta_path, &disk_meta, err_buf, err_cap);
            continue;
        }
        if (!have_ase) {
            if (find_aseprite(aseprite, sizeof(aseprite)) != 0) {
                set_err(err_buf, err_cap, "aseprite not found (set ASEPRITE)");
                return -1;
            }
            have_ase = 1;
        }
        rc = import_one_folder(p, w, root, folders[i], &disk, aseprite, existing, err_buf, err_cap);
        if (rc < 0) {
            if (failed_n < R01_MAX_ENTITY_TYPES) {
                snprintf(failed[failed_n], sizeof(failed[0]), "%s", folders[i]);
                failed_n++;
            }
            /* Keep the first error; still try remaining folders. */
            continue;
        }
        if (rc > 0) {
            (void)r01_aseprite_folder_meta_save(meta_path, &disk_meta, err_buf, err_cap);
            generated++;
        }
    }
    if (generated < 1 && failed_n < 1 && r01_aseprite_listing_equal(&disk, p)) {
        if (out) {
            out->unchanged = 1;
            out->generated = 0;
        }
        return 0;
    }
    for (i = 0; i < failed_n; i++) {
        skip_ptrs[i] = failed[i];
    }
    store_listing_without_folders(p, &disk, skip_ptrs, failed_n);
    if (out) {
        out->generated = generated;
        out->unchanged = 0;
    }
    if (generated > 0) {
        return 0;
    }
    if (failed_n > 0) {
        return -1;
    }
    return 0;
}
