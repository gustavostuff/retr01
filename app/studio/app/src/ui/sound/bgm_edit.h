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

/* Nudge region pitch: dir +1/-1; shift=1 → half-step, else whole step.
 * Noise cycles period; DPCM cycles FD/FE. */
void ui_bgm_nudge_region(UiBgmRegion *rg, int ch, int dir, int half_step);

/* Find region covering tick on channel; -1 if none. */
int ui_bgm_find_at(const UiState *ui, int track, int ch, int tick);
/* Overwrite/split channel so [start,start+len) is clear, then insert region. */
int ui_bgm_place_region(UiState *ui, int track, int ch, const UiBgmRegion *src);
void ui_bgm_remove_region(UiState *ui, int track, int ch, int idx);
/* Resize in place; may overwrite neighbors. Returns new index. */
int ui_bgm_resize_region(UiState *ui, int track, int ch, int idx, int new_start, int new_len);

void ui_bgm_copy_sel(UiState *ui);
void ui_bgm_paste_sel(UiState *ui);

/* Flatten track into host cells; returns loop length (>= 1).
 * honor_solo: when non-zero, skip channels other than ui->sound.solo_ch. */
int ui_bgm_flatten(const UiState *ui, int track,
                   char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN], int honor_solo);

void ui_bgm_clamp_scroll(UiState *ui, int visible_ticks);

#endif
