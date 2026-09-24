#include "netlist_sim/pin_netlist.h"

#include <stdio.h>
#include <string.h>

void ns_pin_netlist_clear(NsPinNetlist *nl) {
    if (nl) {
        memset(nl, 0, sizeof(*nl));
    }
}

int ns_pin_netlist_find_slot(const NsPinNetlist *nl, const NsEntity *e, int pin_index) {
    int i;
    if (!nl || !e || pin_index < 0) {
        return -1;
    }
    for (i = 0; i < nl->slot_count; i++) {
        if (nl->slots[i].entity == e && nl->slots[i].pin_index == pin_index) {
            return i;
        }
    }
    return -1;
}

int ns_pin_netlist_add_slot(NsPinNetlist *nl, NsEntity *e, int pin_index) {
    int i;
    if (!nl || !e || pin_index < 0) {
        return -1;
    }
    i = ns_pin_netlist_find_slot(nl, e, pin_index);
    if (i >= 0) {
        return i;
    }
    if (nl->slot_count >= NS_PIN_NETLIST_MAX) {
        return -1;
    }
    i = nl->slot_count++;
    nl->slots[i].entity = e;
    nl->slots[i].pin_index = pin_index;
    nl->parent[i] = i;
    nl->net_name[i][0] = '\0';
    return i;
}

int ns_pin_netlist_slot_for_name(NsPinNetlist *nl, NsEntity *e, const char *name) {
    const NsPin *pin;
    int pin_index;
    if (!nl || !e || !name) {
        return -1;
    }
    pin = ns_entity_pin_named_const(e, name);
    if (!pin) {
        return -1;
    }
    pin_index = (int)(pin - e->pins);
    if (pin_index < 0 || pin_index >= e->pin_count) {
        return -1;
    }
    return ns_pin_netlist_add_slot(nl, e, pin_index);
}

int ns_pin_netlist_root(NsPinNetlist *nl, int slot) {
    int p;
    if (!nl || slot < 0 || slot >= nl->slot_count) {
        return -1;
    }
    p = nl->parent[slot];
    while (p != nl->parent[p]) {
        p = nl->parent[p];
    }
    while (nl->parent[slot] != p) {
        int next = nl->parent[slot];
        nl->parent[slot] = p;
        slot = next;
    }
    return p;
}

void ns_pin_netlist_union(NsPinNetlist *nl, int a, int b) {
    int ra;
    int rb;
    if (!nl || a < 0 || b < 0) {
        return;
    }
    ra = ns_pin_netlist_root(nl, a);
    rb = ns_pin_netlist_root(nl, b);
    if (ra < 0 || rb < 0 || ra == rb) {
        return;
    }
    nl->parent[rb] = ra;
    if (nl->net_name[ra][0] == '\0' && nl->net_name[rb][0] != '\0') {
        snprintf(nl->net_name[ra], NS_NET_NAME_LEN, "%s", nl->net_name[rb]);
    }
}

void ns_pin_netlist_link(NsPinNetlist *nl, NsEntity *ea, const char *an, NsEntity *eb, const char *bn) {
    int sa;
    int sb;
    if (!nl || !ea || !eb) {
        return;
    }
    sa = ns_pin_netlist_slot_for_name(nl, ea, an);
    sb = ns_pin_netlist_slot_for_name(nl, eb, bn);
    if (sa >= 0 && sb >= 0) {
        ns_pin_netlist_union(nl, sa, sb);
    }
}

void ns_pin_netlist_link_bus(NsPinNetlist *nl, NsEntity *ea, const char *ap, NsEntity *eb, const char *bp,
                             int width) {
    int i;
    char an[16];
    char bn[16];
    if (!nl || width < 1) {
        return;
    }
    for (i = 0; i < width; i++) {
        snprintf(an, sizeof(an), "%s%d", ap, i);
        snprintf(bn, sizeof(bn), "%s%d", bp, i);
        ns_pin_netlist_link(nl, ea, an, eb, bn);
    }
}

void ns_pin_netlist_register_entity(NsPinNetlist *nl, NsEntity *e) {
    int i;
    if (!nl || !e) {
        return;
    }
    for (i = 0; i < e->pin_count; i++) {
        (void)ns_pin_netlist_add_slot(nl, e, i);
    }
}

void ns_pin_netlist_name_net(NsPinNetlist *nl, NsEntity *e, const char *pin_name, const char *net_name) {
    int slot;
    int root;
    if (!nl || !net_name || !net_name[0]) {
        return;
    }
    slot = ns_pin_netlist_slot_for_name(nl, e, pin_name);
    if (slot < 0) {
        return;
    }
    root = ns_pin_netlist_root(nl, slot);
    if (root >= 0) {
        snprintf(nl->net_name[root], NS_NET_NAME_LEN, "%s", net_name);
    }
}

int ns_pin_netlist_same_net(NsPinNetlist *nl, NsEntity *ea, const char *an, NsEntity *eb, const char *bn) {
    int sa;
    int sb;
    if (!nl || !ea || !eb) {
        return 0;
    }
    sa = ns_pin_netlist_slot_for_name(nl, ea, an);
    sb = ns_pin_netlist_slot_for_name(nl, eb, bn);
    if (sa < 0 || sb < 0) {
        return 0;
    }
    return ns_pin_netlist_root(nl, sa) == ns_pin_netlist_root(nl, sb);
}

int ns_pin_netlist_net_count(NsPinNetlist *nl) {
    int roots[NS_PIN_NETLIST_MAX];
    int nroots = 0;
    int i;
    if (!nl) {
        return 0;
    }
    for (i = 0; i < nl->slot_count; i++) {
        int r = ns_pin_netlist_root(nl, i);
        int k;
        int seen = 0;
        if (r < 0) {
            continue;
        }
        for (k = 0; k < nroots; k++) {
            if (roots[k] == r) {
                seen = 1;
                break;
            }
        }
        if (!seen && nroots < NS_PIN_NETLIST_MAX) {
            roots[nroots++] = r;
        }
    }
    return nroots;
}

static void json_escape_str(FILE *out, const char *s) {
    if (!out) {
        return;
    }
    fputc('"', out);
    if (s) {
        while (*s) {
            unsigned char c = (unsigned char)*s++;
            if (c == '"' || c == '\\') {
                fputc('\\', out);
                fputc((int)c, out);
            } else if (c < 32) {
                fprintf(out, "\\u%04x", c);
            } else {
                fputc((int)c, out);
            }
        }
    }
    fputc('"', out);
}

static int collect_roots(const NsPinNetlist *nl, int *roots, int max_roots) {
    int nroots = 0;
    int i;
    if (!nl || !roots || max_roots <= 0) {
        return 0;
    }
    for (i = 0; i < nl->slot_count; i++) {
        int r = ns_pin_netlist_root(nl, i);
        int k;
        int seen = 0;
        if (r < 0) {
            continue;
        }
        for (k = 0; k < nroots; k++) {
            if (roots[k] == r) {
                seen = 1;
                break;
            }
        }
        if (!seen && nroots < max_roots) {
            roots[nroots++] = r;
        }
    }
    return nroots;
}

static void sort_roots(int *roots, int n) {
    int i;
    int j;
    for (i = 1; i < n; i++) {
        int key = roots[i];
        for (j = i; j > 0 && roots[j - 1] > key; j--) {
            roots[j] = roots[j - 1];
        }
        roots[j] = key;
    }
}

int ns_pin_netlist_write_json(const NsPinNetlist *nl, FILE *out) {
    int roots[NS_PIN_NETLIST_MAX];
    int nroots;
    int ni;
    int first_net = 1;

    if (!out) {
        return -1;
    }
    if (!nl) {
        fprintf(out, "{\"error\":\"null netlist\"}\n");
        return -1;
    }

    nroots = collect_roots(nl, roots, NS_PIN_NETLIST_MAX);
    sort_roots(roots, nroots);

    fputs("{\n", out);
    fputs("  \"meta\": {\n", out);
    fputs("    \"source\": \"retr01-tier-h-pin-netlist\",\n", out);
    fputs("    \"purpose\": \"preliminary_pcb_illustrative_only\",\n", out);
    fputs("    \"fabrication_ready\": false,\n", out);
    fputs("    \"note\": \"Not for production PCB. Sim connectivity + BOM passives; AD724/HC14/cart edge gaps remain.\"\n", out);
    fputs("  },\n", out);
    fprintf(out, "  \"net_count\": %d,\n", nroots);
    fputs("  \"nets\": [\n", out);

    for (ni = 0; ni < nroots; ni++) {
        int root = roots[ni];
        const char *nname = nl->net_name[root];
        char auto_name[32];
        int i;
        int first_node = 1;

        if (!first_net) {
            fputs(",\n", out);
        }
        first_net = 0;

        fputs("    {\n", out);
        fputs("      \"name\": ", out);
        if (nname && nname[0]) {
            json_escape_str(out, nname);
        } else {
            snprintf(auto_name, sizeof(auto_name), "NET_%d", root);
            json_escape_str(out, auto_name);
        }
        fputs(",\n", out);
        fputs("      \"nodes\": [\n", out);

        for (i = 0; i < nl->slot_count; i++) {
            NsEntity *e;
            const NsPin *pin;
            if (ns_pin_netlist_root(nl, i) != root) {
                continue;
            }
            e = nl->slots[i].entity;
            if (!e || nl->slots[i].pin_index < 0 || nl->slots[i].pin_index >= e->pin_count) {
                continue;
            }
            pin = &e->pins[nl->slots[i].pin_index];
            if (!first_node) {
                fputs(",\n", out);
            }
            first_node = 0;
            fputs("        {\"ref\": ", out);
            json_escape_str(out, e->refdes ? e->refdes : "?");
            fputs(", \"pin\": ", out);
            json_escape_str(out, pin->name ? pin->name : "?");
            fprintf(out, ", \"num\": %d", pin->number);
            if (e->part && e->part[0]) {
                fputs(", \"part\": ", out);
                json_escape_str(out, e->part);
            }
            fputs("}", out);
        }

        fputs("\n      ]\n    }", out);
    }

    fputs("\n  ]\n}\n", out);
    return 0;
}
