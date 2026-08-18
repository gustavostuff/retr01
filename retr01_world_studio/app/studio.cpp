#include "studio.h"

#include "retr01/chr_pack.h"

#include "imgui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RETR01_PALETTE_V01_PATH
#define RETR01_PALETTE_V01_PATH "retr01_world_studio/retr01_palette_v_01.txt"
#endif

static retr01_rgb_t studio_color_from_ci(const retr01_studio_app_t *app, uint8_t pal_id, uint8_t ci)
{
    const retr01_master_palette_t *pal = &app->project.palette;
    uint8_t idx;

    if (ci == 0) {
        idx = pal->backdrop_index;
    } else {
        idx = pal->bg_palettes[pal_id & 3][ci & 3];
    }
    return pal->entries[idx];
}

static retr01_rgb_t studio_sprite_color(const retr01_studio_app_t *app, uint8_t pal_id, uint8_t ci)
{
    const retr01_master_palette_t *pal = &app->project.palette;
    uint8_t idx = pal->sprite_palettes[pal_id & 3][ci & 3];
    return pal->entries[idx];
}

static void studio_set_status(retr01_studio_app_t *app, const char *msg)
{
    snprintf(app->status, sizeof(app->status), "%s", msg ? msg : "");
}

static retr01_project_screen_t *studio_active_ps(retr01_studio_app_t *app)
{
    return retr01_project_active_screen(&app->project);
}

static void studio_mark_dirty(retr01_studio_app_t *app)
{
    retr01_project_screen_t *ps = studio_active_ps(app);
    app->dirty = true;
    app->sketch_tex_dirty = true;
    if (ps) {
        ps->generate_dirty = 1;
    }
}

static void studio_paint_pixel(retr01_studio_app_t *app, int px, int py)
{
    retr01_project_screen_t *ps = studio_active_ps(app);
    if (!ps || !ps->canvas || px < 0 || py < 0 || px >= RETR01_CANVAS_W || py >= RETR01_CANVAS_H) {
        return;
    }
    ps->canvas[py * RETR01_CANVAS_W + px] = (uint8_t)(app->paint_ci & 3);
    studio_mark_dirty(app);
}

static void studio_flood_fill(retr01_studio_app_t *app, int sx, int sy)
{
    retr01_project_screen_t *ps = studio_active_ps(app);
    uint8_t target;
    uint8_t repl;
    int *stack;
    int top = 0;

    if (!ps || !ps->canvas || sx < 0 || sy < 0 || sx >= RETR01_CANVAS_W || sy >= RETR01_CANVAS_H) {
        return;
    }

    target = ps->canvas[sy * RETR01_CANVAS_W + sx];
    repl = (uint8_t)(app->paint_ci & 3);
    if (target == repl) {
        return;
    }

    stack = (int *)malloc(sizeof(int) * RETR01_CANVAS_BYTES);
    if (!stack) {
        return;
    }
    stack[top++] = sy * RETR01_CANVAS_W + sx;
    while (top > 0) {
        int i = stack[--top];
        int x = i % RETR01_CANVAS_W;
        int y = i / RETR01_CANVAS_W;
        if (ps->canvas[i] != target) {
            continue;
        }
        ps->canvas[i] = repl;
        if (x > 0) {
            stack[top++] = i - 1;
        }
        if (x + 1 < RETR01_CANVAS_W) {
            stack[top++] = i + 1;
        }
        if (y > 0) {
            stack[top++] = i - RETR01_CANVAS_W;
        }
        if (y + 1 < RETR01_CANVAS_H) {
            stack[top++] = i + RETR01_CANVAS_W;
        }
    }
    free(stack);
    studio_mark_dirty(app);
}

static void studio_update_texture(SDL_Texture *tex, const uint8_t *rgba, int w, int h)
{
    (void)h;
    if (!tex || !rgba) {
        return;
    }
    SDL_UpdateTexture(tex, NULL, rgba, w * 4);
}

static void studio_fill_sketch_rgba(retr01_studio_app_t *app, uint8_t *rgba)
{
    retr01_project_screen_t *ps = studio_active_ps(app);
    uint8_t pal = ps ? ps->canvas_palette : (uint8_t)app->project.active_bg_palette;
    int i;

    if (!ps || !ps->canvas) {
        memset(rgba, 0, RETR01_CANVAS_BYTES * 4);
        return;
    }
    for (i = 0; i < RETR01_CANVAS_BYTES; i++) {
        retr01_rgb_t c = studio_color_from_ci(app, pal, ps->canvas[i]);
        rgba[i * 4 + 0] = c.r;
        rgba[i * 4 + 1] = c.g;
        rgba[i * 4 + 2] = c.b;
        rgba[i * 4 + 3] = 255;
    }
}

static void studio_fill_preview_rgba(retr01_studio_app_t *app, uint8_t *rgba)
{
    retr01_project_screen_t *ps = studio_active_ps(app);
    const uint8_t *chr = app->project.chr_banks[app->project.active_bank];
    int tx, ty, y, x;

    memset(rgba, 20, RETR01_CANVAS_BYTES * 4);
    if (!ps) {
        return;
    }
    for (ty = 0; ty < RETR01_NT_H; ty++) {
        for (tx = 0; tx < RETR01_NT_W; tx++) {
            uint8_t tile_idx = ps->screen.tiles[ty * RETR01_NT_W + tx];
            uint8_t pal_id = retr01_attr_get(ps->screen.attrs, tx, ty);
            const uint8_t *tile = chr + (size_t)tile_idx * 16;
            for (y = 0; y < 8; y++) {
                uint8_t p0 = tile[y];
                uint8_t p1 = tile[8 + y];
                for (x = 0; x < 8; x++) {
                    int bit = 7 - x;
                    uint8_t ci = (uint8_t)(((p0 >> bit) & 1) | (((p1 >> bit) & 1) << 1));
                    retr01_rgb_t c = studio_color_from_ci(app, pal_id, ci);
                    int px = tx * 8 + x;
                    int py = ty * 8 + y;
                    size_t i = (size_t)(py * RETR01_CANVAS_W + px) * 4;
                    rgba[i + 0] = c.r;
                    rgba[i + 1] = c.g;
                    rgba[i + 2] = c.b;
                    rgba[i + 3] = 255;
                }
            }
        }
    }
}

static void studio_refresh_textures(retr01_studio_app_t *app)
{
    uint8_t *rgba;
    if (!app->sketch_tex || !app->preview_tex) {
        return;
    }
    rgba = (uint8_t *)malloc(RETR01_CANVAS_BYTES * 4);
    if (!rgba) {
        return;
    }
    if (app->sketch_tex_dirty) {
        studio_fill_sketch_rgba(app, rgba);
        studio_update_texture(app->sketch_tex, rgba, RETR01_CANVAS_W, RETR01_CANVAS_H);
        app->sketch_tex_dirty = false;
    }
    if (app->preview_tex_dirty) {
        studio_fill_preview_rgba(app, rgba);
        studio_update_texture(app->preview_tex, rgba, RETR01_CANVAS_W, RETR01_CANVAS_H);
        app->preview_tex_dirty = false;
    }
    free(rgba);
}

static int studio_generate(retr01_studio_app_t *app)
{
    const uint8_t *planes[RETR01_PROJECT_MAX_SCREENS];
    uint8_t pals[RETR01_PROJECT_MAX_SCREENS];
    retr01_screen_t packed[RETR01_PROJECT_MAX_SCREENS];
    int indices[RETR01_PROJECT_MAX_SCREENS];
    int n = 0;
    int i;
    int unique = 0;
    int bank = app->project.active_bank;
    int rc;
    char msg[160];

    for (i = 0; i < app->project.screen_count; i++) {
        retr01_project_screen_t *ps = &app->project.screens[i];
        if (!ps->canvas) {
            studio_set_status(app, "Generate: screen is missing a canvas");
            return -1;
        }
        planes[n] = ps->canvas;
        pals[n] = ps->canvas_palette;
        packed[n] = ps->screen;
        indices[n] = i;
        n++;
    }

    if (n <= 0) {
        studio_set_status(app, "Generate: no screens");
        return -1;
    }

    rc = retr01_pack_canvases(planes, pals, n, app->project.chr_banks[bank], RETR01_CHR_BANK_BYTES,
                              packed, &unique);
    if (rc == -2) {
        studio_set_status(app, "Generate failed: more than 256 unique BG tiles");
        return -1;
    }
    if (rc != 0) {
        studio_set_status(app, "Generate failed");
        return -1;
    }

    for (i = 0; i < n; i++) {
        retr01_project_screen_t *ps = &app->project.screens[indices[i]];
        ps->screen = packed[i];
        ps->screen.authored_bank = (uint8_t)bank;
        ps->generate_dirty = 0;
    }

    app->project.chr_used[bank] = unique;
    app->dirty = true;
    app->preview_tex_dirty = true;
    snprintf(msg, sizeof(msg), "Generated %d unique BG tiles from %d screens (unused slots hidden)",
             unique, n);
    studio_set_status(app, msg);
    return 0;
}

static void studio_handle_canvas_input(retr01_studio_app_t *app, ImVec2 origin, float scale)
{
    ImVec2 mp = ImGui::GetIO().MousePos;
    int px = (int)((mp.x - origin.x) / scale);
    int py = (int)((mp.y - origin.y) / scale);
    bool hover = ImGui::IsItemHovered();

    if (!hover) {
        app->last_px = -1;
        app->last_py = -1;
        return;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && app->tool == STUDIO_TOOL_FILL) {
        studio_flood_fill(app, px, py);
        return;
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && app->tool == STUDIO_TOOL_PEN) {
        if (app->last_px >= 0) {
            int x0 = app->last_px;
            int y0 = app->last_py;
            int dx = abs(px - x0);
            int dy = abs(py - y0);
            int sx = x0 < px ? 1 : -1;
            int sy = y0 < py ? 1 : -1;
            int err = dx - dy;
            while (1) {
                studio_paint_pixel(app, x0, y0);
                if (x0 == px && y0 == py) {
                    break;
                }
                int e2 = 2 * err;
                if (e2 > -dy) {
                    err -= dy;
                    x0 += sx;
                }
                if (e2 < dx) {
                    err += dx;
                    y0 += sy;
                }
            }
        } else {
            studio_paint_pixel(app, px, py);
        }
        app->last_px = px;
        app->last_py = py;
    } else {
        app->last_px = -1;
        app->last_py = -1;
    }
}

static void studio_draw_tile_preview(retr01_studio_app_t *app, const uint8_t *tile, uint8_t pal_id,
                                     bool sprite, ImVec2 p, float cell)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    int y;
    int x;
    for (y = 0; y < 8; y++) {
        uint8_t p0 = tile[y];
        uint8_t p1 = tile[8 + y];
        for (x = 0; x < 8; x++) {
            int bit = 7 - x;
            uint8_t ci = (uint8_t)(((p0 >> bit) & 1) | (((p1 >> bit) & 1) << 1));
            retr01_rgb_t c =
                sprite ? studio_sprite_color(app, pal_id, ci) : studio_color_from_ci(app, pal_id, ci);
            float px = p.x + (float)x * (cell / 8.0f);
            float py = p.y + (float)y * (cell / 8.0f);
            dl->AddRectFilled(ImVec2(px, py), ImVec2(px + cell / 8.0f, py + cell / 8.0f),
                              IM_COL32(c.r, c.g, c.b, 255));
        }
    }
}

static void studio_draw_world_grid(retr01_studio_app_t *app)
{
    int gw = app->project.grid_w;
    int gh = app->project.grid_h;
    int gx;
    int gy;
    retr01_project_screen_t *active = studio_active_ps(app);
    float cell = 28.0f;

    int world = app->project.active_world;
    int n_in_world = 0;
    int i;

    ImGui::SliderInt("World", &world, 0, RETR01_MAX_WORLDS - 1);
    if (world != app->project.active_world) {
        app->project.active_world = world;
        retr01_project_screen_t *found = NULL;
        for (i = 0; i < app->project.screen_count; i++) {
            if (app->project.screens[i].world == world) {
                found = &app->project.screens[i];
                app->project.active_screen = i;
                break;
            }
        }
        if (!found) {
            retr01_project_ensure_screen(&app->project, world, 0, 0);
        }
        app->sketch_tex_dirty = true;
        app->preview_tex_dirty = true;
        studio_set_status(app, "Switched world");
    }

    for (i = 0; i < app->project.screen_count; i++) {
        if (app->project.screens[i].world == app->project.active_world) {
            n_in_world++;
        }
    }
    ImGui::Text("This world has %d screen(s). Export writes every world.", n_in_world);
    ImGui::TextUnformatted("Empty cells are holes — they are not rooms in the emulator.");
    if (n_in_world > 0) {
        ImGui::TextUnformatted("Rooms:");
        ImGui::SameLine();
        for (i = 0; i < app->project.screen_count; i++) {
            if (app->project.screens[i].world == app->project.active_world) {
                ImGui::Text("(%u,%u)", app->project.screens[i].screen.col,
                            app->project.screens[i].screen.row);
                ImGui::SameLine();
            }
        }
        ImGui::NewLine();
    }
    ImGui::SliderInt("Grid W", &gw, 1, 16);
    ImGui::SliderInt("Grid H", &gh, 1, 16);
    app->project.grid_w = (uint8_t)gw;
    app->project.grid_h = (uint8_t)gh;

    for (gy = 0; gy < gh; gy++) {
        for (gx = 0; gx < gw; gx++) {
            retr01_project_screen_t *ps =
                retr01_project_find_screen(&app->project, app->project.active_world, (uint8_t)gx,
                                           (uint8_t)gy);
            char label[16];
            ImVec4 col;
            bool selected = active && ps == active;
            snprintf(label, sizeof(label), "%d,%d", gx, gy);
            if (!ps) {
                col = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
            } else if (ps->screen.flags == 1) {
                col = ImVec4(0.45f, 0.25f, 0.65f, 1.0f);
            } else {
                col = ImVec4(0.20f, 0.40f, 0.75f, 1.0f);
            }
            if (selected) {
                col.x += 0.15f;
                col.y += 0.15f;
                col.z += 0.10f;
            }
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            ImGui::PushID(gy * 64 + gx);
            if (ImGui::Button(label, ImVec2(cell + 18.0f, cell))) {
                bool shift = ImGui::GetIO().KeyShift;
                if (!ps) {
                    ps = retr01_project_ensure_screen(&app->project, app->project.active_world,
                                                      (uint8_t)gx, (uint8_t)gy);
                    if (ps && shift) {
                        ps->screen.flags = 1;
                    }
                    app->dirty = true;
                    app->sketch_tex_dirty = true;
                    app->preview_tex_dirty = true;
                    studio_set_status(app, shift ? "Created parallax screen" : "Created playfield screen");
                } else {
                    app->project.active_screen = (int)(ps - app->project.screens);
                    if (shift) {
                        ps->screen.flags = (uint8_t)(ps->screen.flags ? 0 : 1);
                        app->dirty = true;
                    }
                    app->sketch_tex_dirty = true;
                    app->preview_tex_dirty = true;
                    studio_set_status(app, "Selected screen");
                }
            }
            ImGui::PopID();
            ImGui::PopStyleColor();
            if (gx + 1 < gw) {
                ImGui::SameLine();
            }
        }
    }

    if (active) {
        ImGui::Separator();
        ImGui::Text("Selected: col %u  row %u  %s", active->screen.col, active->screen.row,
                    active->screen.flags ? "parallax" : "playfield");
        if (ImGui::Button("Set as start cell")) {
            app->project.build_start_col = active->screen.col;
            app->project.build_start_row = active->screen.row;
            app->dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete screen") && app->project.screen_count > 1) {
            retr01_project_delete_screen(&app->project, app->project.active_screen);
            app->dirty = true;
            app->sketch_tex_dirty = true;
            app->preview_tex_dirty = true;
        }
    }
    ImGui::Text("Start: (%d, %d)", app->project.build_start_col, app->project.build_start_row);
}

static void studio_draw_palette(retr01_studio_app_t *app)
{
    retr01_project_screen_t *ps = studio_active_ps(app);
    int p;
    int c;
    const char *labels[4] = {"0 backdrop", "1", "2", "3"};

    ImGui::TextUnformatted("Click a color, then paint on Sketch Canvas.");
    ImGui::TextUnformatted("BG palettes (slot 0 is shared backdrop)");

    for (p = 0; p < 4; p++) {
        ImGui::PushID(p);
        if (ImGui::RadioButton("##pal", app->project.active_bg_palette == p)) {
            app->project.active_bg_palette = p;
            if (ps) {
                ps->canvas_palette = (uint8_t)p;
                app->sketch_tex_dirty = true;
                app->preview_tex_dirty = true;
            }
        }
        ImGui::SameLine();
        ImGui::Text("BG %d", p);
        ImGui::SameLine();
        for (c = 0; c < 4; c++) {
            retr01_rgb_t col = studio_color_from_ci(app, (uint8_t)p, (uint8_t)c);
            ImVec4 v = ImVec4(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, 1.0f);
            char id[16];
            snprintf(id, sizeof(id), "##b%d%d", p, c);
            if (ImGui::ColorButton(id, v, ImGuiColorEditFlags_NoTooltip, ImVec2(26, 26))) {
                app->project.active_bg_palette = p;
                app->paint_ci = c;
                if (ps) {
                    ps->canvas_palette = (uint8_t)p;
                    app->sketch_tex_dirty = true;
                }
            }
            if (app->project.active_bg_palette == p && app->paint_ci == c) {
                ImVec2 a = ImGui::GetItemRectMin();
                ImVec2 b = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 255, 255, 255), 0, 0, 2.0f);
            }
            ImGui::SameLine();
        }
        ImGui::Dummy(ImVec2(1, 1));
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text("Paint color: %s   (keys 1–4)", labels[app->paint_ci & 3]);

    ImGui::Separator();
    ImGui::TextUnformatted("Sprite palettes (pattern 0 = transparent)");
    for (p = 0; p < 4; p++) {
        ImGui::Text("SP %d", p);
        ImGui::SameLine();
        for (c = 0; c < 4; c++) {
            retr01_rgb_t col = studio_sprite_color(app, (uint8_t)p, (uint8_t)c);
            ImVec4 v = ImVec4(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, 1.0f);
            char id[16];
            snprintf(id, sizeof(id), "##s%d%d", p, c);
            ImGui::ColorButton(id, v, 0, ImVec2(22, 22));
            ImGui::SameLine();
        }
        ImGui::Dummy(ImVec2(1, 1));
    }
}

static void studio_draw_pattern_table(retr01_studio_app_t *app)
{
    int bank = app->project.active_bank;
    const uint8_t *chr = app->project.chr_banks[bank];
    int tile;
    float cell = 16.0f;
    bool sprite = app->pattern_page != 0;
    int base = sprite ? RETR01_CHR_BG_TILES : 0;

    ImGui::SliderInt("Bank", &app->project.active_bank, 0, 3);
    bank = app->project.active_bank;
    chr = app->project.chr_banks[bank];

    ImGui::Text("BG page: %d unique / 256  (unused slots are empty, not copies)",
                app->project.chr_used[bank]);
    ImGui::TextUnformatted("Sprite page stays empty until you author sprites.");

    if (ImGui::BeginTabBar("chr_pages")) {
        if (ImGui::BeginTabItem("Background (0–255)")) {
            app->pattern_page = 0;
            sprite = false;
            base = 0;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sprites (256–511)")) {
            app->pattern_page = 1;
            sprite = true;
            base = RETR01_CHR_BG_TILES;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    int used = sprite ? 256 : app->project.chr_used[bank];
    if (used < 0) {
        used = 0;
    }
    if (used > 256) {
        used = 256;
    }

    ImGui::BeginChild("chr_grid", ImVec2(16.0f * (cell + 2.0f) + 12.0f, 16.0f * (cell + 2.0f) + 12.0f),
                      true);
    for (tile = 0; tile < 256; tile++) {
        const uint8_t *t = chr + (size_t)(base + tile) * 16;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(base + tile);
        if (ImGui::InvisibleButton("t", ImVec2(cell, cell))) {
            app->project.active_chr_tile = tile;
        }
        if (!sprite && tile >= used) {
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + cell, p.y + cell),
                                                      IM_COL32(18, 18, 22, 255));
        } else {
            studio_draw_tile_preview(app, t, (uint8_t)app->project.active_bg_palette, sprite, p,
                                     cell);
        }
        if (!sprite && app->project.active_chr_tile == tile && tile < used) {
            ImGui::GetWindowDrawList()->AddRect(p, ImVec2(p.x + cell, p.y + cell),
                                                IM_COL32(255, 255, 255, 255));
        }
        if ((tile + 1) % 16 != 0) {
            ImGui::SameLine();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void studio_handle_file_modals(retr01_studio_app_t *app)
{
    if (app->modal_open_proj) {
        ImGui::OpenPopup("Open Project");
        app->modal_open_proj = false;
    }
    if (app->modal_save_as) {
        ImGui::OpenPopup("Save Project As");
        app->modal_save_as = false;
    }
    if (app->modal_export) {
        ImGui::OpenPopup("Export Cart");
        app->modal_export = false;
    }

    if (ImGui::BeginPopupModal("Open Project", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", app->path_input, sizeof(app->path_input));
        if (ImGui::Button("Open")) {
            retr01_project_t loaded;
            memset(&loaded, 0, sizeof(loaded));
            if (retr01_project_load(&loaded, app->path_input) == 0) {
                retr01_project_free(&app->project);
                app->project = loaded;
                retr01_palette_load_v01(RETR01_PALETTE_V01_PATH, &app->project.palette);
                app->dirty = false;
                app->sketch_tex_dirty = true;
                app->preview_tex_dirty = true;
                studio_set_status(app, "Opened project");
            } else {
                retr01_project_free(&loaded);
                studio_set_status(app, "Open failed");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Save Project As", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", app->path_input, sizeof(app->path_input));
        if (ImGui::Button("Save")) {
            if (retr01_project_save(&app->project, app->path_input) == 0) {
                snprintf(app->project.path, sizeof(app->project.path), "%s", app->path_input);
                app->dirty = false;
                studio_set_status(app, "Saved project");
            } else {
                studio_set_status(app, "Save failed");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##save")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Export Cart", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Output .retr01", app->export_input, sizeof(app->export_input));
        if (ImGui::Button("Export")) {
            int g;
            for (g = 0; g < app->project.screen_count; g++) {
                if (app->project.screens[g].generate_dirty) {
                    studio_generate(app);
                    break;
                }
            }
            if (retr01_project_export_retr01(&app->project, app->export_input) == 0) {
                studio_set_status(app, "Exported cart — N/P cycles rooms in retr01_emu");
            } else {
                studio_set_status(app, "Export failed");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##export")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void studio_try_save(retr01_studio_app_t *app)
{
    if (app->project.path[0]) {
        if (retr01_project_save(&app->project, app->project.path) == 0) {
            app->dirty = false;
            studio_set_status(app, "Saved project");
        }
    } else {
        app->modal_save_as = true;
    }
}

void retr01_studio_init(retr01_studio_app_t *app, SDL_Renderer *renderer, const char *palette_v01_path)
{
    memset(app, 0, sizeof(*app));
    retr01_project_init_default(&app->project, palette_v01_path);
    app->tool = STUDIO_TOOL_PEN;
    app->paint_ci = 2;
    app->zoom = 2;
    app->show_grid = true;
    app->renderer = renderer;
    app->last_px = -1;
    app->last_py = -1;
    app->sketch_tex_dirty = true;
    app->preview_tex_dirty = true;
    snprintf(app->export_input, sizeof(app->export_input), "build/untitled.retr01");
    studio_set_status(app, "Paint on Sketch Canvas, then Generate.");

    if (renderer) {
        app->sketch_tex =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                              RETR01_CANVAS_W, RETR01_CANVAS_H);
        app->preview_tex =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                              RETR01_CANVAS_W, RETR01_CANVAS_H);
        if (app->sketch_tex) {
            SDL_SetTextureScaleMode(app->sketch_tex, SDL_ScaleModeNearest);
        }
        if (app->preview_tex) {
            SDL_SetTextureScaleMode(app->preview_tex, SDL_ScaleModeNearest);
        }
    }
}

void retr01_studio_shutdown(retr01_studio_app_t *app)
{
    if (app->sketch_tex) {
        SDL_DestroyTexture(app->sketch_tex);
    }
    if (app->preview_tex) {
        SDL_DestroyTexture(app->preview_tex);
    }
    retr01_project_free(&app->project);
}

void retr01_studio_frame(retr01_studio_app_t *app)
{
    retr01_project_screen_t *ps = studio_active_ps(app);
    ImGuiIO &io = ImGui::GetIO();
    float scale;
    ImVec2 origin;
    bool generate_dirty = ps && ps->generate_dirty;

    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            studio_try_save(app);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_G)) {
            studio_generate(app);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_1)) {
            app->paint_ci = 0;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_2)) {
            app->paint_ci = 1;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_3)) {
            app->paint_ci = 2;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_4)) {
            app->paint_ci = 3;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_B)) {
            app->tool = STUDIO_TOOL_PEN;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F)) {
            app->tool = STUDIO_TOOL_FILL;
        }
    }

    studio_refresh_textures(app);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                retr01_project_free(&app->project);
                retr01_project_init_default(&app->project, RETR01_PALETTE_V01_PATH);
                app->dirty = false;
                app->paint_ci = 2;
                app->sketch_tex_dirty = true;
                app->preview_tex_dirty = true;
                studio_set_status(app, "New project — paint on Sketch Canvas, then Generate");
            }
            if (ImGui::MenuItem("Open...")) {
                app->modal_open_proj = true;
                snprintf(app->path_input, sizeof(app->path_input), "%s", app->project.path);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                studio_try_save(app);
            }
            if (ImGui::MenuItem("Save As...")) {
                app->modal_save_as = true;
                snprintf(app->path_input, sizeof(app->path_input), "%s",
                         app->project.path[0] ? app->project.path : "untitled.r01proj");
            }
            if (ImGui::MenuItem("Export .retr01...")) {
                app->modal_export = true;
            }
            ImGui::EndMenu();
        }
        ImGui::TextDisabled("  |  Paint sketch → Generate → pattern table  |  %s", app->status);
        ImGui::EndMainMenuBar();
    }

    studio_handle_file_modals(app);

    ImGui::SetNextWindowPos(ImVec2(8, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("World Grid");
    studio_draw_world_grid(app);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(356, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 620), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sketch Canvas");
    ImGui::TextUnformatted("This is the paint buffer (256×240 pixels). You draw here.");
    ImGui::RadioButton("Pen", (int *)&app->tool, STUDIO_TOOL_PEN);
    ImGui::SameLine();
    ImGui::RadioButton("Fill", (int *)&app->tool, STUDIO_TOOL_FILL);
    ImGui::SameLine();
    ImGui::Checkbox("8px grid", &app->show_grid);
    ImGui::SliderInt("Zoom", &app->zoom, 1, 4);
    if (generate_dirty) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.83f, 0.55f, 0.16f, 1.0f));
    }
    if (ImGui::Button("Generate pattern table")) {
        studio_generate(app);
    }
    if (generate_dirty) {
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextUnformatted("pixels changed — Generate to refresh tiles");
    }
    ImGui::Text("Screen (%u, %u)  palette BG %u  color %d", ps ? ps->screen.col : 0,
                ps ? ps->screen.row : 0, ps ? ps->canvas_palette : 0, app->paint_ci);

    scale = (float)app->zoom;
    origin = ImGui::GetCursorScreenPos();
    {
        ImVec2 size = ImVec2(256.0f * scale, 240.0f * scale);
        ImGui::InvisibleButton("sketch_hit", size);
        if (app->sketch_tex) {
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)app->sketch_tex, origin,
                ImVec2(origin.x + size.x, origin.y + size.y));
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            studio_handle_canvas_input(app, origin, scale);
        }
        if (app->show_grid) {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            int t;
            ImU32 gcol = IM_COL32(255, 255, 255, 28);
            for (t = 0; t <= RETR01_NT_W; t++) {
                float x = origin.x + (float)(t * 8) * scale;
                dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + 240.0f * scale), gcol);
            }
            for (t = 0; t <= RETR01_NT_H; t++) {
                float y = origin.y + (float)(t * 8) * scale;
                dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + 256.0f * scale, y), gcol);
            }
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(356, 656), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 220), ImGuiCond_FirstUseEver);
    ImGui::Begin("Screen Preview");
    ImGui::TextUnformatted("Nametable after Generate — what the emulator will show.");
    ImGui::Checkbox("Attr overlay", &app->show_attr_overlay);
    if (app->preview_tex) {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)app->preview_tex, ImVec2(256.0f, 240.0f));
        if (app->show_attr_overlay) {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            int tx, ty;
            for (ty = 0; ty < RETR01_NT_H; ty += 2) {
                for (tx = 0; tx < RETR01_NT_W; tx += 2) {
                    dl->AddRect(ImVec2(p0.x + (float)(tx * 8), p0.y + (float)(ty * 8)),
                                ImVec2(p0.x + (float)(tx * 8 + 16), p0.y + (float)(ty * 8 + 16)),
                                IM_COL32(255, 220, 0, 90));
                }
            }
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(924, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 430), ImGuiCond_FirstUseEver);
    ImGui::Begin("Pattern Table");
    ImGui::TextUnformatted("Generate fills the BG page from the sketch.");
    studio_draw_pattern_table(app);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(924, 466), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 310), ImGuiCond_FirstUseEver);
    ImGui::Begin("Palette");
    studio_draw_palette(app);
    ImGui::End();
}
