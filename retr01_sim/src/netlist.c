#include "retr01_sim/netlist.h"

#include <stdio.h>
#include <string.h>

static R01sNetEdge g_edges[R01S_NET_MAX_EDGES];
static int g_edge_count;

void r01s_net_clear(void) {
    g_edge_count = 0;
}

static int pin_name_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static int edge_eq(const R01sNetEdge *e, R01sEntity *a, const char *pa, R01sEntity *b, const char *pb) {
    if (e->a == a && e->b == b && pin_name_eq(e->pin_a, pa) && pin_name_eq(e->pin_b, pb)) {
        return 1;
    }
    if (e->a == b && e->b == a && pin_name_eq(e->pin_a, pb) && pin_name_eq(e->pin_b, pa)) {
        return 1;
    }
    return 0;
}

void r01s_net_link(R01sEntity *a, const char *pin_a, R01sEntity *b, const char *pin_b) {
    int i;
    if (!a || !b || !pin_a || !pin_b || !pin_a[0] || !pin_b[0]) {
        return;
    }
    if (a == b && pin_name_eq(pin_a, pin_b)) {
        return;
    }
    for (i = 0; i < g_edge_count; i++) {
        if (edge_eq(&g_edges[i], a, pin_a, b, pin_b)) {
            return;
        }
    }
    if (g_edge_count >= R01S_NET_MAX_EDGES) {
        return;
    }
    g_edges[g_edge_count].a = a;
    g_edges[g_edge_count].b = b;
    snprintf(g_edges[g_edge_count].pin_a, sizeof(g_edges[g_edge_count].pin_a), "%s", pin_a);
    snprintf(g_edges[g_edge_count].pin_b, sizeof(g_edges[g_edge_count].pin_b), "%s", pin_b);
    g_edge_count++;
}

int r01s_net_edge_count(void) {
    return g_edge_count;
}

const R01sNetEdge *r01s_net_edge_at(int index) {
    if (index < 0 || index >= g_edge_count) {
        return NULL;
    }
    return &g_edges[index];
}
