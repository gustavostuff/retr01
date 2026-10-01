#ifndef R01A_NETLIST_H
#define R01A_NETLIST_H

#include "r01a_board.h"

enum {
    R01A_NET_OK = 0,
    R01A_NET_UNSEATED,
    R01A_NET_MISSING,
    R01A_NET_SHORT_PWR_GND,
    R01A_NET_SHORT_DATA_GND,
    R01A_NET_SHORT_DATA_PWR,
    R01A_NET_SHORT_CLK_GND,
    R01A_NET_SHORT_CLK_PWR,
    R01A_NET_SHORT_CLK_DATA,
    R01A_NET_SHORT_DATA,
    R01A_NET_SHORT_CHIP
};

enum {
    R01A_NET_KIND_PWR = 0,
    R01A_NET_KIND_GND,
    R01A_NET_KIND_CLK,
    R01A_NET_KIND_DATA
};

#define R01A_NET_ISSUE_MAX 48
#define R01A_NET_TEXT_LEN 96

typedef struct R01aNetIssue {
    int kind;
    char text[R01A_NET_TEXT_LEN];
} R01aNetIssue;

/* Physical vs Auto netlist. Jumpers and strips are hard ties. Resistors are not. */
int r01a_netlist_check(R01aBoard *board, R01aNetIssue *out, int max_out);
const char *r01a_netlist_kind_label(int kind);

#define R01A_BYPASS_N 5

typedef struct R01aBypassPair {
    const char *cap;
    const char *ic;
} R01aBypassPair;

/* 100 nF bypass: C1 U04, C2 U74, C3 UPLDX, C4 UPLDY, C5 U24.
 * Air overlay is pin 1 → that IC VCC only. Pin 2 is any GND. */
extern const R01aBypassPair r01a_bypass_pairs[R01A_BYPASS_N];
const char *r01a_netlist_bypass_ic(const char *cap_ref);
const char *r01a_netlist_bypass_cap(const char *ic_ref);

#endif
