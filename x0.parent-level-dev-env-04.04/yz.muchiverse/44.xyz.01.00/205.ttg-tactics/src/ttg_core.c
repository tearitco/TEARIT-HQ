/* ttg_core.c — state, rules, ledger, load/save */
#include "ttg.h"

void ttg_set_root(Game *g, const char *root) {
    snprintf(g->root, sizeof(g->root), "%s", root ? root : ".");
}

void ttg_path(const Game *g, char *out, size_t n, const char *rel) {
    snprintf(out, n, "%s/%s", g->root, rel);
}

int ttg_mkdir_p(const char *path) {
    char tmp[MAX_PATH];
    char *p;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

int ttg_write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(content ? content : "", f);
    fclose(f);
    return 0;
}

int ttg_append_file(const char *path, const char *line) {
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fputs(line, f);
    if (line[0] && line[strlen(line) - 1] != '\n') fputc('\n', f);
    fclose(f);
    return 0;
}

int ttg_read_kv(const char *path, const char *key, char *val, size_t n) {
    char line[MAX_LINE], k[64];
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    val[0] = 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        snprintf(k, sizeof(k), "%s", line);
        /* trim */
        {
            char *e = eq + 1;
            char *t = e + strlen(e);
            while (t > e && (t[-1] == '\n' || t[-1] == '\r' || t[-1] == ' ')) *--t = 0;
            if (strcmp(k, key) == 0) {
                snprintf(val, n, "%s", e);
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
}

const char *ttg_role_name(enum Role r) {
    switch (r) {
    case ROLE_KING: return "King";
    case ROLE_SOLDIER: return "Soldier";
    case ROLE_WIZARD: return "Wizard";
    case ROLE_FARMER: return "Farmer";
    default: return "?";
    }
}

char ttg_role_glyph(enum Role r, int seat) {
    char c = '?';
    switch (r) {
    case ROLE_KING: c = 'K'; break;
    case ROLE_SOLDIER: c = 'S'; break;
    case ROLE_WIZARD: c = 'W'; break;
    case ROLE_FARMER: c = 'F'; break;
    default: break;
    }
    if (seat == 1) c = (char)tolower((unsigned char)c);
    return c;
}

static void stats_for_role(enum Role r, int *hp, int *atk, int *def) {
    switch (r) {
    case ROLE_KING: *hp = 40; *atk = 8; *def = 4; break;
    case ROLE_SOLDIER: *hp = 22; *atk = 10; *def = 3; break;
    case ROLE_WIZARD: *hp = 16; *atk = 12; *def = 1; break;
    case ROLE_FARMER: *hp = 18; *atk = 6; *def = 2; break;
    default: *hp = 10; *atk = 5; *def = 1; break;
    }
}

void ttg_init_empty(Game *g) {
    memset(g, 0, sizeof(*g));
    g->phase = PH_TITLE;
    g->board_w = BOARD_W;
    g->board_h = BOARD_H;
    g->seat_count = 2;
    g->match_clock_ms = 300000;
    g->ante = 50;
    g->ui_mode = 2;
    g->menu_sel = 0;
    snprintf(g->msg, sizeof(g->msg), "TTG — Enter play  arrows select  q quit");
}

void ttg_ledger(Game *g, const char *actor, const char *atype, const char *adata) {
    char path[MAX_PATH], line[512], ts[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);
    ttg_path(g, path, sizeof(path), "data/master_ledger.txt");
    snprintf(line, sizeof(line), "%s|%u|%s|%d|%s|%s\n",
             ts, g->epoch, actor ? actor : "-", g->turn_index,
             adata ? adata : "", atype ? atype : "note");
    ttg_append_file(path, line);
}

Unit *ttg_unit_at(Game *g, int x, int y) {
    int i;
    for (i = 0; i < g->n_units; i++)
        if (g->units[i].used && g->units[i].alive &&
            g->units[i].x == x && g->units[i].y == y)
            return &g->units[i];
    return NULL;
}

Unit *ttg_unit_by_id(Game *g, const char *id) {
    int i;
    if (!id || !id[0]) return NULL;
    for (i = 0; i < g->n_units; i++)
        if (g->units[i].used && strcmp(g->units[i].id, id) == 0)
            return &g->units[i];
    return NULL;
}

Unit *ttg_selected(Game *g) {
    return ttg_unit_by_id(g, g->selected);
}

static int bfs_reachable(const Game *g, const Unit *u, int tx, int ty) {
    /* 4-dir BFS, block occupied tiles (except target empty), range = 1 step only for MVP soldier/king? */
    /* DESIGN: BFS for path — Phase1 move range = 1 for all (chess king-like) except Wizard range attack.
       Actually design said BFS for path legality with empty tiles — move distance?
       For chess-like tactics: Soldier move 1, King 1, Wizard 1, Farmer 1 MVP simple. */
    int dist[BOARD_H][BOARD_W];
    int qx[BOARD_W * BOARD_H], qy[BOARD_W * BOARD_H], qh = 0, qt = 0;
    int x, y, d;
    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    if (tx < 0 || ty < 0 || tx >= g->board_w || ty >= g->board_h) return 0;
    if (u->x == tx && u->y == ty) return 0;
    if (ttg_unit_at((Game *)g, tx, ty)) return 0;
    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            dist[y][x] = -1;
    dist[u->y][u->x] = 0;
    qx[qt] = u->x; qy[qt] = u->y; qt++;
    while (qh < qt) {
        int cx = qx[qh], cy = qy[qh];
        qh++;
        if (dist[cy][cx] >= 1) continue; /* max move 1 for MVP */
        for (d = 0; d < 4; d++) {
            int nx = cx + dirs[d][0], ny = cy + dirs[d][1];
            Unit *block;
            if (nx < 0 || ny < 0 || nx >= g->board_w || ny >= g->board_h) continue;
            if (dist[ny][nx] >= 0) continue;
            block = ttg_unit_at((Game *)g, nx, ny);
            if (block && !(nx == tx && ny == ty)) continue;
            dist[ny][nx] = dist[cy][cx] + 1;
            qx[qt] = nx; qy[qt] = ny; qt++;
        }
    }
    return dist[ty][tx] == 1;
}

int ttg_can_move(const Game *g, const Unit *u, int x, int y) {
    if (!u || !u->alive || u->moved) return 0;
    if (u->seat != g->active_seat) return 0;
    if (g->phase != PH_MATCH) return 0;
    return bfs_reachable(g, u, x, y);
}

int ttg_move(Game *g, Unit *u, int x, int y) {
    char data[128];
    if (!ttg_can_move(g, u, x, y)) {
        snprintf(g->msg, sizeof(g->msg), "Illegal move");
        return -1;
    }
    snprintf(data, sizeof(data), "unit:%s,from:%d,%d,to:%d,%d", u->id, u->x, u->y, x, y);
    u->x = x; u->y = y;
    u->moved = 1;
    ttg_ledger(g, u->id, "move", data);
    snprintf(g->msg, sizeof(g->msg), "%s moved to %d,%d", ttg_role_name(u->role), x, y);
    return 0;
}

int ttg_can_attack(const Game *g, const Unit *a, const Unit *d) {
    int dx, dy, cheb, range;
    if (!a || !d || !a->alive || !d->alive) return 0;
    if (a->acted) return 0;
    if (a->seat != g->active_seat) return 0;
    if (a->seat == d->seat) return 0;
    dx = abs(a->x - d->x);
    dy = abs(a->y - d->y);
    cheb = dx > dy ? dx : dy;
    range = (a->role == ROLE_WIZARD) ? 2 : 1;
    return cheb >= 1 && cheb <= range;
}

int ttg_attack(Game *g, Unit *a, Unit *d) {
    int dmg;
    char data[160];
    if (!ttg_can_attack(g, a, d)) {
        snprintf(g->msg, sizeof(g->msg), "Illegal attack");
        return -1;
    }
    dmg = a->atk - d->def;
    if (dmg < 1) dmg = 1;
    d->hp -= dmg;
    a->acted = 1;
    snprintf(data, sizeof(data), "atk:%s,def:%s,dmg:%d,kill:%d",
             a->id, d->id, dmg, d->hp <= 0 ? 1 : 0);
    ttg_ledger(g, a->id, "attack", data);
    if (d->hp <= 0) {
        d->alive = 0;
        d->hp = 0;
        snprintf(g->msg, sizeof(g->msg), "%s defeated %s!", ttg_role_name(a->role), ttg_role_name(d->role));
        if (d->role == ROLE_KING) {
            snprintf(g->winner, sizeof(g->winner), "%d", a->seat);
            snprintf(g->end_reason, sizeof(g->end_reason), "regicide");
            g->phase = PH_END;
            ttg_ledger(g, "system", "match_end",
                       a->seat == 0 ? "winner:0,reason:regicide" : "winner:1,reason:regicide");
            g->pot_settled = 1; /* settle stub: pot to winner recorded */
            {
                char p[MAX_PATH], line[128];
                ttg_path(g, p, sizeof(p), "data/pot_ledger.txt");
                snprintf(line, sizeof(line), "settle|winner:%d|amount:%d\n", a->seat, g->pot_balance);
                ttg_append_file(p, line);
            }
        }
    } else {
        snprintf(g->msg, sizeof(g->msg), "Hit for %d (%s HP %d)", dmg, ttg_role_name(d->role), d->hp);
    }
    return 0;
}

void ttg_end_turn(Game *g) {
    int i;
    char data[32];
    if (g->phase != PH_MATCH) return;
    snprintf(data, sizeof(data), "seat:%d", g->active_seat);
    ttg_ledger(g, "system", "end_turn", data);
    /* clear flags for next seat's units */
    g->active_seat = (g->active_seat + 1) % g->seat_count;
    if (g->active_seat == 0) g->turn_index++;
    for (i = 0; i < g->n_units; i++) {
        if (g->units[i].seat == g->active_seat) {
            g->units[i].moved = 0;
            g->units[i].acted = 0;
        }
    }
    g->selected[0] = 0;
    snprintf(g->msg, sizeof(g->msg), "Turn seat %d", g->active_seat);
}

void ttg_check_end(Game *g) {
    int k0 = 0, k1 = 0, i;
    if (g->phase != PH_MATCH) return;
    for (i = 0; i < g->n_units; i++) {
        if (!g->units[i].alive || g->units[i].role != ROLE_KING) continue;
        if (g->units[i].seat == 0) k0 = 1;
        if (g->units[i].seat == 1) k1 = 1;
    }
    if (!k0) {
        snprintf(g->winner, sizeof(g->winner), "1");
        snprintf(g->end_reason, sizeof(g->end_reason), "regicide");
        g->phase = PH_END;
    } else if (!k1) {
        snprintf(g->winner, sizeof(g->winner), "0");
        snprintf(g->end_reason, sizeof(g->end_reason), "regicide");
        g->phase = PH_END;
    }
}

static void add_unit(Game *g, int seat, enum Role role, int x, int y, int nn) {
    Unit *u;
    int hp, atk, def;
    if (g->n_units >= MAX_UNITS) return;
    u = &g->units[g->n_units++];
    memset(u, 0, sizeof(*u));
    u->used = 1;
    u->alive = 1;
    u->seat = seat;
    u->role = role;
    u->x = x; u->y = y;
    stats_for_role(role, &hp, &atk, &def);
    u->hp = u->max_hp = hp;
    u->atk = atk; u->def = def;
    snprintf(u->id, sizeof(u->id), "u_s%d_%s_%02d", seat,
             role == ROLE_KING ? "king" :
             role == ROLE_SOLDIER ? "soldier" :
             role == ROLE_WIZARD ? "wizard" : "farmer", nn);
}

int ttg_init_match(Game *g, int clock_ms, int ante) {
    char path[MAX_PATH], data[160];
    int i;
    g->phase = PH_MATCH;
    g->ui_mode = 0;
    g->board_w = BOARD_W;
    g->board_h = BOARD_H;
    g->seat_count = 2;
    g->active_seat = 0;
    g->turn_index = 0;
    g->seat_type[0] = 0;
    g->seat_type[1] = 1; /* AI */
    g->match_clock_ms = clock_ms > 0 ? clock_ms : 300000;
    g->clock_ms[0] = g->match_clock_ms;
    g->clock_ms[1] = g->match_clock_ms;
    g->clock_frozen = 0;
    g->ante = ante > 0 ? ante : 50;
    g->pot_balance = g->ante * 2;
    g->pot_settled = 0;
    g->winner[0] = 0;
    g->end_reason[0] = 0;
    g->n_units = 0;
    g->selected[0] = 0;
    g->cursor_x = 6; g->cursor_y = 2;
    g->epoch++;
    g->history_off = 0;

    /* seat0 north */
    add_unit(g, 0, ROLE_KING, 6, 2, 1);
    add_unit(g, 0, ROLE_SOLDIER, 2, 0, 1);
    add_unit(g, 0, ROLE_SOLDIER, 7, 0, 2);
    add_unit(g, 0, ROLE_SOLDIER, 3, 2, 3);
    add_unit(g, 0, ROLE_WIZARD, 4, 0, 1);
    add_unit(g, 0, ROLE_FARMER, 9, 0, 1);
    /* seat1 south mirror */
    add_unit(g, 1, ROLE_KING, 6, 9, 1);
    add_unit(g, 1, ROLE_SOLDIER, 2, 11, 1);
    add_unit(g, 1, ROLE_SOLDIER, 7, 11, 2);
    add_unit(g, 1, ROLE_SOLDIER, 3, 9, 3);
    add_unit(g, 1, ROLE_WIZARD, 4, 11, 1);
    add_unit(g, 1, ROLE_FARMER, 9, 11, 1);

    ttg_path(g, path, sizeof(path), "data");
    ttg_mkdir_p(path);
    ttg_path(g, path, sizeof(path), "pieces/units");
    ttg_mkdir_p(path);
    ttg_path(g, path, sizeof(path), "data/master_ledger.txt");
    ttg_write_file(path, "timestamp|epoch|actor|turn|action_data|action_type\n");
    ttg_path(g, path, sizeof(path), "data/pot_ledger.txt");
    ttg_write_file(path, "");
    snprintf(data, sizeof(data), "ante|seat:0|%d\nante|seat:1|%d\n", g->ante, g->ante);
    ttg_append_file(path, data);

    snprintf(data, sizeof(data), "mode:1v1,clock:%d,ante:%d,board:12x12", g->match_clock_ms, g->ante);
    ttg_ledger(g, "system", "match_start", data);
    for (i = 0; i < g->n_units; i++) {
        Unit *u = &g->units[i];
        char udir[MAX_PATH], st[MAX_PATH], body[256];
        snprintf(data, sizeof(data), "unit:%s,role:%s,x:%d,y:%d", u->id, ttg_role_name(u->role), u->x, u->y);
        ttg_ledger(g, "system", "spawn", data);
        ttg_path(g, udir, sizeof(udir), "pieces/units");
        snprintf(st, sizeof(st), "%s/%s", udir, u->id);
        ttg_mkdir_p(st);
        snprintf(st, sizeof(st), "%s/%s/state.txt", udir, u->id);
        snprintf(body, sizeof(body),
                 "id=%s\nseat=%d\nrole=%s\nx=%d\ny=%d\nhp=%d\nmax_hp=%d\natk=%d\ndef=%d\nalive=1\n",
                 u->id, u->seat, ttg_role_name(u->role), u->x, u->y, u->hp, u->max_hp, u->atk, u->def);
        ttg_write_file(st, body);
    }
    snprintf(g->msg, sizeof(g->msg), "Match start — seat0 human vs AI. Arrows move cursor, Enter select/move, A attack, E end");
    return ttg_save_all(g);
}

int ttg_save_all(const Game *g) {
    char path[MAX_PATH], body[2048];
    int i;
    ttg_path(g, path, sizeof(path), "data/match_state.txt");
    snprintf(body, sizeof(body),
             "phase=%s\nmode=1v1\nactive_seat=%d\nseat_count=%d\nturn_index=%d\n"
             "pot_balance=%d\npot_settled=%d\nante=%d\n"
             "clock_ms_seat_0=%d\nclock_ms_seat_1=%d\nclock_frozen=%d\n"
             "cursor_x=%d\ncursor_y=%d\nselected=%s\n"
             "winner=%s\nend_reason=%s\nmsg=%s\n"
             "board_w=%d\nboard_h=%d\nframe_w=%d\nframe_h=%d\n"
             "seat_0_type=%s\nseat_1_type=%s\nui_mode=%d\nmenu_sel=%d\n",
             g->phase == PH_TITLE ? "title" : g->phase == PH_MATCH ? "in_match" : "end",
             g->active_seat, g->seat_count, g->turn_index,
             g->pot_balance, g->pot_settled, g->ante,
             g->clock_ms[0], g->clock_ms[1], g->clock_frozen,
             g->cursor_x, g->cursor_y, g->selected,
             g->winner, g->end_reason, g->msg,
             g->board_w, g->board_h, FRAME_W, FRAME_H,
             g->seat_type[0] ? "ai" : "human",
             g->seat_type[1] ? "ai" : "human",
             g->ui_mode, g->menu_sel);
    ttg_write_file(path, body);

    for (i = 0; i < g->n_units; i++) {
        const Unit *u = &g->units[i];
        char st[MAX_PATH], ub[256], udir[MAX_PATH];
        if (!u->used) continue;
        ttg_path(g, udir, sizeof(udir), "pieces/units");
        snprintf(st, sizeof(st), "%s/%s", udir, u->id);
        ttg_mkdir_p(st);
        snprintf(st, sizeof(st), "%s/%s/state.txt", udir, u->id);
        snprintf(ub, sizeof(ub),
                 "id=%s\nseat=%d\nrole=%s\nx=%d\ny=%d\nhp=%d\nmax_hp=%d\natk=%d\ndef=%d\n"
                 "alive=%d\nmoved=%d\nacted=%d\n",
                 u->id, u->seat, ttg_role_name(u->role), u->x, u->y, u->hp, u->max_hp,
                 u->atk, u->def, u->alive, u->moved, u->acted);
        ttg_write_file(st, ub);
    }
    return 0;
}

int ttg_load_all(Game *g) {
    char path[MAX_PATH], v[128];
    /* minimal: re-init if no match; otherwise parse match_state + units dirs */
    ttg_path(g, path, sizeof(path), "data/match_state.txt");
    if (ttg_read_kv(path, "phase", v, sizeof(v)) != 0) {
        ttg_init_empty(g);
        return 0;
    }
    if (strcmp(v, "title") == 0) {
        ttg_init_empty(g);
        return 0;
    }
    /* For robust resume, re-scan unit dirs — simplified: if in_match missing units, re-init */
    if (g->n_units < 1) {
        /* keep clocks from file if present */
        int clock = 300000, ante = 50;
        if (ttg_read_kv(path, "clock_ms_seat_0", v, sizeof(v)) == 0) clock = atoi(v);
        if (ttg_read_kv(path, "ante", v, sizeof(v)) == 0) ante = atoi(v);
        return ttg_init_match(g, clock, ante);
    }
    return 0;
}
