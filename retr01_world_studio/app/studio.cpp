#include "studio.h"

#include "retr01/chr_pack.h"

#include "imgui.h"

#include <stdio.h>
#include <string.h>

#ifndef RETR01_PALETTE_V01_PATH
#define RETR01_PALETTE_V01_PATH "retr01_world_studio/retr01_palette_v_01.txt"
#endif

static retr01_rgb_t studio_color_from_ci(const retr01_studio_app_t *app, uint8_t pal_id,
                                         uint8_t ci)
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

static void studio_set_status(retr01_studio_app_t *app, const char *msg)
{
    snprintf(app->status, sizeof(app->status), "%s", msg ? msg : "");
}

static retr01_screen_t *studio_active_screen(retr01_studio_app_t *app)
{
    retr01_project_screen_t *ps = retr01_project_active_screen(&app->project);
    return ps ? &ps->screen : NULL;
}

static void studio_new_project(retr01_studio_app_t *app)
{
    retr01_studio_init(app, RETR01_PALETTE_V01_PATH);
    studio_set_status(app, "New project");
}

static void studio_mark_dirty(retr01_studio_app_t *app)
{
    app->dirty = true;
}

static void studio_sync_ci_from_screen(retr01_studio_app_t *app)
{
    retr01_screen_t *sc = studio_active_screen(app);
    int tx;
    int ty;
    int y;
    int x;

    if (!sc) {
        return;
    }

    memset(app->ci_canvas, 0, sizeof(app->ci_canvas));
    for (ty = 0; ty < RETR01_NT_H; ty++) {
        for (tx = 0; tx < RETR01_NT_W; tx++) {
            uint8_t tile_idx = sc->tiles[ty * RETR01_NT_W + tx];
            uint8_t pal_id = retr01_attr_get(sc->attrs, tx, ty);
            const uint8_t *tile =
                app->project.chr_banks[app->project.active_bank] + (size_t)tile_idx * 16;

            for (y = 0; y < 8; y++) {
                uint8_t p0 = tile[y];
                uint8_t p1 = tile[8 + y];
                for (x = 0; x < 8; x++) {
                    int bit = 7 - x;
                    uint8_t ci =
                        (uint8_t)(((p0 >> bit) & 1) | (((p1 >> bit) & 1) << 1));
                    int px = tx * 8 + x;
                    int py = ty * 8 + y;
                    app->ci_canvas[py * 256 + px] = ci;
                    (void)pal_id;
                }
            }
        }
    }
    app->ci_valid = true;
}

static int studio_generate_from_canvas(retr01_studio_app_t *app)
{
    retr01_screen_t *sc = studio_active_screen(app);
    retr01_screen_t packed;
    int unique = 0;
    int bank = app->project.active_bank;
    int rc;

    if (!sc) {
        return -1;
    }

    if (!app->ci_valid) {
        studio_sync_ci_from_screen(app);
    }

    rc = retr01_pack_canvas(app->ci_canvas, 256, 240,
                            (uint8_t)app->project.active_bg_palette,
                            app->project.chr_banks[bank], RETR01_CHR_BANK_BYTES, &packed,
                            &unique);
    if (rc == -2) {
        studio_set_status(app, "Generate failed: more than 256 unique tiles");
        return -1;
    }
    if (rc != 0) {
        studio_set_status(app, "Generate failed");
        return -1;
    }

    *sc = packed;
    sc->authored_bank = (uint8_t)bank;
    if (unique > app->project.chr_used[bank]) {
        app->project.chr_used[bank] = unique;
    }
    studio_mark_dirty(app);
    studio_set_status(app, "Generated screen from canvas");
    return 0;
}

static void studio_draw_canvas(retr01_studio_app_t *app)
{
    retr01_screen_t *sc = studio_active_screen(app);
    ImDrawList *dl;
    ImVec2 origin;
    float scale;
    int ty;
    int tx;

    if (!sc) {
        return;
    }

    scale = (float)app->zoom;
    origin = ImGui::GetCursorScreenPos();
    dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("canvas", ImVec2(256.0f * scale, 240.0f * scale));
    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        int px = (int)((mp.x - origin.x) / scale);
        int py = (int)((mp.y - origin.y) / scale);
        int tx_hit = px / 8;
        int ty_hit = py / 8;

        if (px >= 0 && py >= 0 && px < 256 && py < 240) {
            if (app->paint_mode == STUDIO_PAINT_TILE) {
                sc->tiles[ty_hit * RETR01_NT_W + tx_hit] =
                    (uint8_t)app->project.active_chr_tile;
                sc->authored_bank = (uint8_t)app->project.active_bank;
                studio_mark_dirty(app);
            } else if (app->paint_mode == STUDIO_PAINT_PIXEL) {
                app->ci_canvas[py * 256 + px] = (uint8_t)(app->paint_ci & 3);
                app->ci_valid = true;
                studio_mark_dirty(app);
            } else if (app->paint_mode == STUDIO_PAINT_ATTR) {
                retr01_attr_set(sc->attrs, tx_hit, ty_hit, (uint8_t)app->project.active_bg_palette);
                studio_mark_dirty(app);
            }
        }
    }

    for (ty = 0; ty < RETR01_NT_H; ty++) {
        for (tx = 0; tx < RETR01_NT_W; tx++) {
            uint8_t tile_idx = sc->tiles[ty * RETR01_NT_W + tx];
            uint8_t pal_id = retr01_attr_get(sc->attrs, tx, ty);
            const uint8_t *tile =
                app->project.chr_banks[app->project.active_bank] + (size_t)tile_idx * 16;
            int y;
            int x;

            for (y = 0; y < 8; y++) {
                uint8_t p0 = tile[y];
                uint8_t p1 = tile[8 + y];
                for (x = 0; x < 8; x++) {
                    int bit = 7 - x;
                    uint8_t ci =
                        (uint8_t)(((p0 >> bit) & 1) | (((p1 >> bit) & 1) << 1));
                    retr01_rgb_t c = studio_color_from_ci(app, pal_id, ci);
                    ImVec2 p0s = ImVec2(origin.x + (float)(tx * 8 + x) * scale,
                                        origin.y + (float)(ty * 8 + y) * scale);
                    ImVec2 p1s = ImVec2(p0s.x + scale, p0s.y + scale);
                    dl->AddRectFilled(p0s, p1s,
                                      IM_COL32(c.r, c.g, c.b, 255));
                }
            }

            if (app->show_attr_overlay) {
                ImVec2 a0 = ImVec2(origin.x + (float)(tx * 8) * scale,
                                   origin.y + (float)(ty * 8) * scale);
                ImVec2 a1 = ImVec2(origin.x + (float)(tx * 8 + 8) * scale,
                                   origin.y + (float)(ty * 8 + 8) * scale);
                dl->AddRect(a0, a1, IM_COL32(255, 255, 0, 180));
            }
        }
    }
}

static void studio_draw_chr_panel(retr01_studio_app_t *app)
{
    int bank = app->project.active_bank;
    const uint8_t *chr = app->project.chr_banks[bank];
    int tile;
    float cell = 20.0f;

    ImGui::Text("CHR bank %d (%d used)", bank, app->project.chr_used[bank]);
    ImGui::SliderInt("Active tile", &app->project.active_chr_tile, 0, 255);

    ImGui::BeginChild("chr_grid", ImVec2(340, 340), true);
    for (tile = 0; tile < 256; tile++) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const uint8_t *t = chr + (size_t)tile * 16;
        int y;
        int x;
        ImGui::PushID(tile);
        if (ImGui::InvisibleButton("t", ImVec2(cell, cell))) {
            app->project.active_chr_tile = tile;
        }
        if (app->project.active_chr_tile == tile) {
            dl->AddRect(p, ImVec2(p.x + cell, p.y + cell), IM_COL32(255, 255, 255, 255));
        }
        for (y = 0; y < 8; y++) {
            uint8_t p0 = t[y];
            uint8_t p1 = t[8 + y];
            for (x = 0; x < 8; x++) {
                int bit = 7 - x;
                uint8_t ci = (uint8_t)(((p0 >> bit) & 1) | (((p1 >> bit) & 1) << 1));
                retr01_rgb_t c =
                    studio_color_from_ci(app, (uint8_t)app->project.active_bg_palette, ci);
                float px = p.x + (float)x * (cell / 8.0f);
                float py = p.y + (float)y * (cell / 8.0f);
                dl->AddRectFilled(ImVec2(px, py),
                                  ImVec2(px + cell / 8.0f, py + cell / 8.0f),
                                  IM_COL32(c.r, c.g, c.b, 255));
            }
        }
        if ((tile + 1) % 8 != 0) {
            ImGui::SameLine();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void studio_draw_palette_panel(retr01_studio_app_t *app)
{
    int p;
    ImGui::Text("BG palettes");
    for (p = 0; p < 4; p++) {
        int c;
        ImGui::PushID(p);
        if (ImGui::RadioButton("pal", app->project.active_bg_palette == p)) {
            app->project.active_bg_palette = p;
        }
        ImGui::SameLine();
        for (c = 0; c < 4; c++) {
            retr01_rgb_t col = studio_color_from_ci(app, (uint8_t)p, (uint8_t)c);
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pos, ImVec2(pos.x + 18, pos.y + 18),
                              IM_COL32(col.r, col.g, col.b, 255));
            ImGui::Dummy(ImVec2(22, 22));
            ImGui::SameLine();
        }
        ImGui::PopID();
    }

    if (app->paint_mode == STUDIO_PAINT_PIXEL) {
        ImGui::Separator();
        ImGui::Text("Paint color index");
        ImGui::SliderInt("CI", &app->paint_ci, 0, 3);
    }
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
            if (retr01_project_load(&app->project, app->path_input) == 0) {
                snprintf(app->project.path, sizeof(app->project.path), "%s", app->path_input);
                app->dirty = false;
                app->ci_valid = false;
                studio_set_status(app, "Opened project");
            } else {
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
            if (retr01_project_export_retr01(&app->project, app->export_input) == 0) {
                studio_set_status(app, "Exported cart");
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

void retr01_studio_init(retr01_studio_app_t *app, const char *palette_v01_path)
{
    memset(app, 0, sizeof(*app));
    retr01_project_init_default(&app->project, palette_v01_path);
    app->paint_mode = STUDIO_PAINT_TILE;
    app->paint_ci = 1;
    app->zoom = 2;
    app->show_attr_overlay = false;
    snprintf(app->export_input, sizeof(app->export_input), "build/untitled.retr01");
    studio_set_status(app, "Ready");
}

void retr01_studio_shutdown(retr01_studio_app_t *app)
{
    (void)app;
}

void retr01_studio_frame(retr01_studio_app_t *app)
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                studio_new_project(app);
            }
            if (ImGui::MenuItem("Open...")) {
                app->modal_open_proj = true;
                snprintf(app->path_input, sizeof(app->path_input), "%s", app->project.path);
            }
            if (ImGui::MenuItem("Save")) {
                if (app->project.path[0]) {
                    if (retr01_project_save(&app->project, app->project.path) == 0) {
                        app->dirty = false;
                        studio_set_status(app, "Saved project");
                    }
                } else {
                    app->modal_save_as = true;
                }
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
        ImGui::EndMainMenuBar();
    }

    studio_handle_file_modals(app);

    ImGui::Begin("Screen Painter");
    ImGui::Text("Mode");
    ImGui::RadioButton("Tile", (int *)&app->paint_mode, STUDIO_PAINT_TILE);
    ImGui::SameLine();
    ImGui::RadioButton("Pixel", (int *)&app->paint_mode, STUDIO_PAINT_PIXEL);
    ImGui::SameLine();
    ImGui::RadioButton("Attr", (int *)&app->paint_mode, STUDIO_PAINT_ATTR);
    ImGui::Checkbox("Attr overlay", &app->show_attr_overlay);
    ImGui::SliderInt("Zoom", &app->zoom, 1, 4);
    if (ImGui::Button("Generate from pixel canvas")) {
        studio_generate_from_canvas(app);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sync pixel canvas from screen")) {
        studio_sync_ci_from_screen(app);
        studio_set_status(app, "Synced pixel canvas");
    }
    studio_draw_canvas(app);
    ImGui::End();

    ImGui::Begin("CHR Bank");
    ImGui::SliderInt("Active bank", &app->project.active_bank, 0, 3);
    studio_draw_chr_panel(app);
    ImGui::End();

    ImGui::Begin("Palette");
    studio_draw_palette_panel(app);
    ImGui::End();

    ImGui::Begin("Status");
    ImGui::Text("%s", app->status);
    ImGui::Text("Project: %s%s", app->project.title, app->dirty ? " *" : "");
    if (app->project.path[0]) {
        ImGui::Text("Path: %s", app->project.path);
    }
    ImGui::End();
}
