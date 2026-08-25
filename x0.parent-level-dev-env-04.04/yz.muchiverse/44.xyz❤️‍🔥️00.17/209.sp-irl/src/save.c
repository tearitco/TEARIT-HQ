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

    fprintf(f, "format 2\n");
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
    fprintf(f, "bag_n %d\n", g->player.bag_n);
    for (i = 0; i < g->player.bag_n; i++) {
        fprintf(f, "item %d %d\n", g->player.bag[i].item_id, g->player.bag[i].count);
    }
    fprintf(f, "badges_n %d\n", g->player.badge_count);
    for (i = 0; i < MAX_BADGES; i++) {
        fprintf(f, "badge %d\n", g->player.badges[i]);
    }
    fprintf(f, "trainers_n %d\n", g->map.trainers_n);
    for (i = 0; i < g->map.trainers_n; i++) {
        fprintf(f, "tr %d\n", g->map.trainers[i].defeated);
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
    char path[PATH_LEN], map_path[PATH_LEN];
    FILE *f;
    char line[256];
    int pn = 0;

    snprintf(path, sizeof(path), "%s/save.txt", g->save_dir);
    f = fopen(path, "r");
    if (!f) return -1;

    g->player.party_n = 0;
    g->player.has_starter = 0;
    g->player.bag_n = 0;
    g->player.badge_count = 0;
    map_path[0] = '\0';

    { int badge_idx = 0, tr_idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "pos ", 4) == 0) {
            int x, y, fac;
            if (sscanf(line + 4, "%d %d %d", &x, &y, &fac) >= 2) {
                g->player.x = x;
                g->player.y = y;
                g->player.facing = fac;
            }
        } else if (strncmp(line, "map ", 4) == 0) {
            char *mp = line + 4;
            size_t mpl = strlen(mp);
            while (mpl > 0 && (mp[mpl-1] == '\n' || mp[mpl-1] == '\r')) mp[--mpl] = '\0';
            snprintf(map_path, sizeof(map_path), "%s", mp);
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
        } else if (strncmp(line, "bag_n ", 6) == 0) {
            sscanf(line + 6, "%d", &g->player.bag_n);
        } else if (strncmp(line, "item ", 5) == 0) {
            int id, cnt;
            if (sscanf(line + 5, "%d %d", &id, &cnt) == 2 && cnt > 0) {
                int idx = g->player.bag_n;
                if (idx < BAG_MAX) {
                    g->player.bag[idx].item_id = id;
                    g->player.bag[idx].count = cnt;
                    g->player.bag_n = idx + 1;
                }
            }
        } else if (strncmp(line, "badge ", 6) == 0) {
            int b = 0;
            if (sscanf(line + 6, "%d", &b) == 1 && badge_idx < MAX_BADGES)
                g->player.badges[badge_idx++] = b ? 1 : 0;
        } else if (strncmp(line, "badges_n ", 9) == 0) {
            /* skip */
        } else if (strncmp(line, "tr ", 3) == 0) {
            int def = 0;
            if (sscanf(line + 3, "%d", &def) == 1 && tr_idx < g->map.trainers_n)
                g->map.trainers[tr_idx++].defeated = def;
        }
    }
    fclose(f);

    }
    if (g->player.party_n < 1) return -1;
    g->player.badge_count = 0;
    { int bj; for (bj = 0; bj < MAX_BADGES; bj++) if (g->player.badges[bj]) g->player.badge_count++; }
    /* load the correct map */
    if (map_path[0] && map_load(&g->map, map_path) == 0) {
        snprintf(g->map_path, sizeof(g->map_path), "%s", map_path);
    }
    /* clamp position onto map */
    if (!map_walkable(&g->map, g->player.x, g->player.y)) {
        g->player.x = g->map.start_x;
        g->player.y = g->map.start_y;
    }
    (void)pn;
    return 0;
}
