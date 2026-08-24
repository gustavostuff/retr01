#ifndef RETR01_SIM_BOARD_FAST_H
#define RETR01_SIM_BOARD_FAST_H

#include <stdint.h>

/*
 * Optimization Playbook "Super Component / glue inlining" — optional fast routes
 * that bypass per-pin entity eval where the slow netlist is already proven.
 * Default: all off (full pin-level settle + entity video path).
 *
 * Enable: R01S_FAST=1 | R01S_FAST=settle,video | ./sim run -- --fast
 * Toggle at runtime: F key in the sim UI.
 */
typedef enum R01sFastGlue {
    R01S_FAST_GLUE_NONE    = 0,
    R01S_FAST_GLUE_SETTLE  = 1u << 0, /* 1-pass combinatorial settle vs 4 */
    R01S_FAST_GLUE_VIDEO   = 1u << 1, /* direct compositor+PROM peek per dot */
    /* Reserved for future playbook passes: */
    R01S_FAST_GLUE_BUS     = 1u << 2, /* bitmask buses (Pass 2) */
    R01S_FAST_GLUE_MEMORY  = 1u << 3, /* inline wire_memory decode (Target 3) */
    R01S_FAST_GLUE_PINS    = 1u << 4, /* cached pin indices (Target 1) */
    R01S_FAST_GLUE_ALL     = 0xFFFFu,
} R01sFastGlue;

#define R01S_FAST_GLUE_BOOT_DEFAULT (R01S_FAST_GLUE_SETTLE | R01S_FAST_GLUE_VIDEO)

uint32_t r01s_fast_glue_mask(void);
void r01s_fast_glue_set(uint32_t mask);
int r01s_fast_glue_enabled(R01sFastGlue bit);
void r01s_fast_glue_toggle(R01sFastGlue bit);

/* Parse "1", "all", "settle,video", "none" — returns 0 on empty/bad token. */
uint32_t r01s_fast_glue_parse(const char *spec);

/* Apply R01S_FAST env if set; returns mask applied (0 if unset). */
uint32_t r01s_fast_glue_from_env(void);

const char *r01s_fast_glue_label(uint32_t mask);

#endif
