#ifndef R01_STUDIO_BGM_EDIT_H
#define R01_STUDIO_BGM_EDIT_H

#include "ui/ui.h"

/* Content end tick for a track (max region end); 0 if empty. */
int ui_bgm_content_end(const UiState *ui, int track);
/* Max scroll_x: content_end + pad - visible ticks (clamped >= 0). */
int ui_bgm_scroll_max(const UiState *ui, int visible_ticks);

void ui_bgm_midi_to_tok(int midi, char tok[5]);
int ui_bgm_tok_to_midi(const char *tok);
void ui_bgm_default_tok(int ch, char tok[5], int *out_midi);

/* Nudge region pitch: dir +1/-1. chromatic 0 = naturals (accidental stays),
 * 1 = one semitone (black keys # up, b down). Noise cycles period; DPCM cycles FD/FE. */
void ui_bgm_nudge_region(UiBgmRegion *rg, int ch, int dir, int chromatic);
void ui_bgm_key_name(int pc, int solfa, char buf[8]);
void ui_bgm_note_label(const UiBgmRegion *rg, int ch, int solfa, char buf[32]);
void ui_bgm_toggle_sharp(UiBgmRegion *rg, int ch);
void ui_bgm_toggle_flat(UiBgmRegion *rg, int ch);

/* Find region covering tick on channel; -1 if none. */
int ui_bgm_find_at(const UiState *ui, int track, int ch, int tick);
/* Overwrite/split channel so [start,start+len) is clear, then insert region. */
int ui_bgm_place_region(UiState *ui, int track, int ch, const UiBgmRegion *src);
void ui_bgm_remove_region(UiState *ui, int track, int ch, int idx);
/* Resize in place; may overwrite neighbors. Returns new index. */
int ui_bgm_resize_region(UiState *ui, int track, int ch, int idx, int new_start, int new_len);

void ui_bgm_copy_sel(UiState *ui);
void ui_bgm_paste_sel(UiState *ui);
void ui_bgm_sel_clear(UiState *ui);
void ui_bgm_sel_only(UiState *ui, int ch, int idx);
void ui_bgm_sel_toggle(UiState *ui, int ch, int idx);
void ui_bgm_sel_all(UiState *ui);
void ui_bgm_sel_rect(UiState *ui, int ch0, int t0, int ch1, int t1, int add);
void ui_bgm_sel_sync(UiState *ui);
int ui_bgm_sel_count(const UiState *ui);
int ui_bgm_is_sel(const UiState *ui, int ch, int idx);
void ui_bgm_remove_sel(UiState *ui);
void ui_bgm_nudge_sel(UiState *ui, int dir, int chromatic);
void ui_bgm_toggle_sel_sharp(UiState *ui);
void ui_bgm_toggle_sel_flat(UiState *ui);
void ui_bgm_move_sel_grab(UiState *ui);
void ui_bgm_move_sel_apply(UiState *ui, int dt);

/* Sync Studio Audio editor <-> project.bgm for save/load. */
void ui_bgm_sync_to_project(UiState *ui);
void ui_bgm_apply_from_project(UiState *ui);

/* Flatten track into host cells; returns loop length (>= 1).
 * honor_solo: when non-zero, skip channels whose ch_mask bit is clear. */
int ui_bgm_flatten(const UiState *ui, int track,
                   char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN], int honor_solo);
/* Flatten the selected note strip onto tick 0 of its channel. Returns steps, or 0. */
int ui_bgm_flatten_sel(const UiState *ui, char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN],
                       int *out_origin);

/* Write output/data/bgm_trackN.bin for each Studio track (debug dump). */
void ui_bgm_write_export_bins(const UiState *ui);

void ui_bgm_clamp_scroll(UiState *ui, int visible_ticks);
void ui_bgm_zoom(UiState *ui, int dir);

#endif
