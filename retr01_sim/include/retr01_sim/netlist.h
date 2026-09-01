#ifndef retr01_SIM_NETLIST_H
#define retr01_SIM_NETLIST_H

#include "retr01_sim/entity.h"

/* Undirected pin-to-pin edges collected from board wire helpers (Connections UI). */

#define R01S_NET_MAX_EDGES 1024
#define R01S_NET_PIN_NAME 16

typedef struct R01sNetEdge {
    R01sEntity *a;
    R01sEntity *b;
    char pin_a[R01S_NET_PIN_NAME];
    char pin_b[R01S_NET_PIN_NAME];
} R01sNetEdge;

void r01s_net_clear(void);
/* Undirected link; duplicates ignored. NULL / empty names ignored. */
void r01s_net_link(R01sEntity *a, const char *pin_a, R01sEntity *b, const char *pin_b);

int r01s_net_edge_count(void);
const R01sNetEdge *r01s_net_edge_at(int index);

#endif
