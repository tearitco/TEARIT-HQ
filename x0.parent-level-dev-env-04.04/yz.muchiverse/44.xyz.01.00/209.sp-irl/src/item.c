#include "gb.h"

const ItemDef *item_def(const Game *g, int id) {
    int i;
    if (!g) return NULL;
    for (i = 0; i < g->item_defs_n; i++)
        if (g->item_defs[i].id == id) return &g->item_defs[i];
    return NULL;
}

int data_load_items(Game *g, const char *path) {
    FILE *f;
    char line[128];
    int n = 0;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "data_load_items: %s\n", path);
        return -1;
    }
    g->item_defs_n = 0;
    while (fgets(line, sizeof(line), f) && n < ITEM_DEF_MAX) {
        ItemDef *d;
        int id, type, heal, price;
        char name[NAME_LEN], tbuf[16];
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%d %15s %15s %d %d", &id, name, tbuf, &heal, &price) < 4)
            continue;
        if (strcmp(tbuf, "HEAL") == 0) type = ITEM_HEAL;
        else if (strcmp(tbuf, "CATCH") == 0) type = ITEM_CATCH;
        else type = ITEM_OTHER;
        d = &g->item_defs[n++];
        d->id = id;
        snprintf(d->name, sizeof(d->name), "%s", name);
        d->type = type;
        d->heal_amt = heal;
        d->price = price;
    }
    fclose(f);
    g->item_defs_n = n;
    return n > 0 ? 0 : -1;
}

int item_add(Player *p, int item_id, int count) {
    int i;
    if (!p || count <= 0) return -1;
    for (i = 0; i < p->bag_n; i++) {
        if (p->bag[i].item_id == item_id) {
            p->bag[i].count += count;
            return 0;
        }
    }
    if (p->bag_n >= BAG_MAX) return -1;
    p->bag[p->bag_n].item_id = item_id;
    p->bag[p->bag_n].count = count;
    p->bag_n++;
    return 0;
}

int item_remove(Player *p, int item_id, int count) {
    int i;
    if (!p || count <= 0) return -1;
    for (i = 0; i < p->bag_n; i++) {
        if (p->bag[i].item_id == item_id) {
            if (p->bag[i].count < count) return -1;
            p->bag[i].count -= count;
            if (p->bag[i].count <= 0) {
                int j;
                for (j = i; j < p->bag_n - 1; j++)
                    p->bag[j] = p->bag[j + 1];
                p->bag_n--;
            }
            return 0;
        }
    }
    return -1;
}

int item_count(const Player *p, int item_id) {
    int i;
    if (!p) return 0;
    for (i = 0; i < p->bag_n; i++)
        if (p->bag[i].item_id == item_id) return p->bag[i].count;
    return 0;
}

int item_use_heal(Game *g, int item_id, int party_idx) {
    const ItemDef *def = item_def(g, item_id);
    PartyMon *pm;
    if (!def || def->type != ITEM_HEAL) return -1;
    if (party_idx < 0 || party_idx >= g->player.party_n) return -1;
    pm = &g->player.party[party_idx];
    if (pm->hp <= 0) return -2; /* fainted */
    if (pm->hp >= pm->max_hp) return -3; /* already full */
    pm->hp += def->heal_amt;
    if (pm->hp > pm->max_hp) pm->hp = pm->max_hp;
    item_remove(&g->player, item_id, 1);
    return 0;
}
