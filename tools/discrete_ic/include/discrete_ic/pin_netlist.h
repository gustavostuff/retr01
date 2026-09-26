#ifndef DISCRETE_IC_PIN_NETLIST_H
#define DISCRETE_IC_PIN_NETLIST_H

#include "discrete_ic/entity.h"

#include <stdio.h>

/*
 * Pin-level connectivity (union-find). Used for schematic overlay, breadboard
 * routing, and future KiCad/Skidl export. Does not drive ns_bus settle by itself.
 */

#define NS_PIN_NETLIST_MAX 2048
#define NS_NET_NAME_LEN 32

typedef struct NsPinNetSlot {
    NsEntity *entity;
    int pin_index;
} NsPinNetSlot;

typedef struct NsPinNetlist {
    NsPinNetSlot slots[NS_PIN_NETLIST_MAX];
    int parent[NS_PIN_NETLIST_MAX];
    char net_name[NS_PIN_NETLIST_MAX][NS_NET_NAME_LEN];
    int slot_count;
} NsPinNetlist;

void ns_pin_netlist_clear(NsPinNetlist *nl);

int ns_pin_netlist_find_slot(const NsPinNetlist *nl, const NsEntity *e, int pin_index);
int ns_pin_netlist_add_slot(NsPinNetlist *nl, NsEntity *e, int pin_index);
int ns_pin_netlist_slot_for_name(NsPinNetlist *nl, NsEntity *e, const char *name);

int ns_pin_netlist_root(const NsPinNetlist *nl, int slot);
void ns_pin_netlist_union(NsPinNetlist *nl, int a, int b);

void ns_pin_netlist_link(NsPinNetlist *nl, NsEntity *ea, const char *an, NsEntity *eb, const char *bn);
void ns_pin_netlist_link_bus(NsPinNetlist *nl, NsEntity *ea, const char *ap, NsEntity *eb, const char *bp,
                             int width);

/* One slot per pin (singleton nets until linked). */
void ns_pin_netlist_register_entity(NsPinNetlist *nl, NsEntity *e);

/* Attach a human/netlist label to the net containing (entity, pin_name). */
void ns_pin_netlist_name_net(NsPinNetlist *nl, NsEntity *e, const char *pin_name, const char *net_name);

int ns_pin_netlist_net_count(NsPinNetlist *nl);

/* True when both named pins share the same union-find net (after linking). */
int ns_pin_netlist_same_net(NsPinNetlist *nl, NsEntity *ea, const char *an, NsEntity *eb, const char *bn);

/*
 * Write Tier-H / sim pin graph as JSON for Skidl or other ECAD tools.
 * Output is illustrative only (see docs/bringup/tier-h-skidl-export.md).
 */
int ns_pin_netlist_write_json(const NsPinNetlist *nl, FILE *out);

#endif
