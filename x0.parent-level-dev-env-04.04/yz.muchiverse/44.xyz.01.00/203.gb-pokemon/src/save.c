/* save.c — party + position to saves/slot0/ */
#include "gb.h"

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        return -1;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

int save_exists(const Game *g) {
    char path[PATH_LEN];
    struct stat st;
    snprintf(path, sizeof(path), "%s/save.txt", g->save_dir);
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int save_write(const Game *g) {
    char path[PATH_LEN];
    FILE *f;
    int i, j;

    if (ensure_dir(g->save_dir) != 0) {
        fprintf(stderr, "save_write: mkdir %s failed\n", g->save_dir);
        return -1;
    }

    snprintf(path, sizeof(path), "%s/save.txt", g->save_dir);
    f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "format 1\n");
    fprintf(f, "map %s\n", g->map.path);
    fprintf(f, "pos %d %d %d\n", g->player.x, g->player.y, g->player.facing);
    fprintf(f, "party_n %d\n", g->player.party_n);
    for (i = 0; i < g->player.party_n; i++) {
        const PartyMon *p = &g->player.party[i];
        fprintf(f, "mon %d %d %d %d %d %d %d %d %d %d %d %d\n",
                p->species, p->level, p->hp, p->max_hp,
                p->atk, p->def, p->spd, p->exp,
                p->move_id[0], p->pp[0], p->move_id[1], p->pp[1]);
        (void)j;
    }
    fclose(f);

    /* also write party.pdl for house DNA visibility */
    snprintf(path, sizeof(path), "%s/party.pdl", g->save_dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "# party.pdl auto-written by save\n");
        for (i = 0; i < g->player.party_n; i++) {
            const PartyMon *p = &g->player.party[i];
            const Species *sp = mon_species(g, p->species);
            fprintf(f, "%d %s lv%d hp=%d/%d\n",
                    p->species, sp ? sp->name : "?", p->level, p->hp, p->max_hp);
        }
        fclose(f);
    }
    return 0;
}

int save_load(Game *g) {
    char path[PATH_LEN];
    FILE *f;
    char line[256];
    int pn = 0;

    snprintf(path, sizeof(path), "%s/save.txt", g->save_dir);
    f = fopen(path, "r");
    if (!f) return -1;

    g->player.party_n = 0;
    g->player.has_starter = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "pos ", 4) == 0) {
            int x, y, fac;
            if (sscanf(line + 4, "%d %d %d", &x, &y, &fac) >= 2) {
                g->player.x = x;
                g->player.y = y;
                g->player.facing = fac;
            }
        } else if (strncmp(line, "party_n ", 8) == 0) {
            sscanf(line + 8, "%d", &pn);
        } else if (strncmp(line, "mon ", 4) == 0) {
            PartyMon *p;
            if (g->player.party_n >= PARTY_MAX) continue;
            p = &g->player.party[g->player.party_n];
            memset(p, 0, sizeof(*p));
            if (sscanf(line + 4, "%d %d %d %d %d %d %d %d %d %d %d %d",
                       &p->species, &p->level, &p->hp, &p->max_hp,
                       &p->atk, &p->def, &p->spd, &p->exp,
                       &p->move_id[0], &p->pp[0],
                       &p->move_id[1], &p->pp[1]) >= 8) {
                if (p->max_hp < 1) p->max_hp = 1;
                if (p->hp > p->max_hp) p->hp = p->max_hp;
                g->player.party_n++;
                g->player.has_starter = 1;
            }
        }
    }
    fclose(f);

    if (g->player.party_n < 1) return -1;
    /* clamp position onto map */
    if (!map_walkable(&g->map, g->player.x, g->player.y)) {
        g->player.x = g->map.start_x;
        g->player.y = g->map.start_y;
    }
    (void)pn;
    return 0;
}
