/* db_battle_op — Draw Battle as one op. Event pages call it. game_core does not.
 *   catalog <units.pdl> <id>
 *   stroke  <ledger> <units.pdl> <team> <id> <x> <y>
 *   tick    <ledger> <units.pdl>
 *   project <ledger> <units.pdl> <deskdir>
 *   place   <ledger> <units.pdl> <deskdir> <pick_ui> <x> <y>     (x y, or cursor <xelector state>)
 *   listen  <ledger> <units.pdl> <deskdir> <pick_ui> <pkg> <session> <window_pid>
 *   play|stop <app_state_dir> <house>
 *   clock <app_state_dir> <house> rate <min|sec|hour|cent> | reinstall
 * Numbers come from units.pdl. Strokes are the ledger. The desk is a projection.
 * A click and Enter are the same place: the cell under the pointer, or the xelector. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <sys/stat.h>
typedef struct { char id[24], group[16], label[24], emoji[16]; int hp, dmg, range; int r, g, b; } Kind;
typedef struct { int team, x, y, hp, dmg, range; char id[24]; int r, g, b, alive; } Body;
static Kind kinds[64];
static int nk;
static int hex2(const char *s) { return (int)strtol(s, NULL, 16); }
static void load_units(const char *path) {
    FILE *f = fopen(path, "r");
    char line[320];
    nk = 0;
    if (!f) { fprintf(stderr, "no units %s\n", path); exit(2); }
    while (fgets(line, sizeof line, f) && nk < 64) {
        if (strncmp(line, "UNIT |", 6)) continue;
        char buf[320], *flds[16];
        int n = 0;
        snprintf(buf, sizeof buf, "%s", line);
        char *tok = strtok(buf, "|\n");
        while (tok && n < 16) {
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok);
            while (end > tok && (end[-1] == ' ' || end[-1] == '\r')) { end--; *end = 0; }
            flds[n++] = tok;
            tok = strtok(NULL, "|\n");
        }
        if (n < 11) continue;
        Kind *k = &kinds[nk++];
        snprintf(k->group, sizeof k->group, "%s", flds[1]);
        snprintf(k->id, sizeof k->id, "%s", flds[2]);
        snprintf(k->label, sizeof k->label, "%s", flds[3]);
        snprintf(k->emoji, sizeof k->emoji, "%s", flds[4]);
        k->hp = atoi(flds[7]);
        k->dmg = atoi(flds[8]);
        k->range = atoi(flds[9]);
        const char *c = flds[5];
        if (*c == '#') c++;
        char rr[3] = { c[0], c[1], 0 }, gg[3] = { c[2], c[3], 0 }, bb[3] = { c[4], c[5], 0 };
        k->r = hex2(rr); k->g = hex2(gg); k->b = hex2(bb);
    }
    fclose(f);
}
/* Sheets that exist under #.NNEST_ASSETS/tsots-characters. Index is the character slot on that sheet. Nine fields: a trailing letter makes the 3D loader drop the row. */
static void sheet_of(const char *id, const char **name, int *idx) {
    static const struct { const char *id, *sheet; int idx; } tab[] = {
        {"soldier", "People1", 0}, {"sniper", "People1", 1}, {"grenadier", "People1", 2}, {"flame", "People1", 3},
        {"roman", "People2", 0}, {"archer", "People2", 1}, {"cavalry", "People2", 4}, {"ninja", "People2", 5},
        {"gorilla", "Monster", 0}, {"boar", "Monster", 1}, {"cheetah", "Monster", 2}, {"piranha", "Monster", 3},
        {"rat", "Monster", 4}, {"frog", "Monster", 5},
        {"giant", "$BigMonster1", 0}, {"dragon", "$BigMonster2", 0}, {"mage", "Actor1", 0}, {"necro", "Actor1", 2},
        {"zombie", "Monster", 6}, {"slime", "Monster", 7},
        {"wall", "Nature", 0}, {"water_body", "Nature", 1}, {"ice", "Nature", 2}, {"mine", "Nature", 3},
        {"banana", "Nature", 4}, {"barrel", "!Chest", 0}, {"tree", "Nature", 5}, {"spike", "Nature", 6},
    };
    *name = "People1"; *idx = 0;
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (!strcmp(tab[i].id, id)) { *name = tab[i].sheet; *idx = tab[i].idx; return; }
}
typedef struct { int team, x, y, dx, dy, life, dmg, owner, alive; char kind[16]; } Shot;
static Shot shots[16];
static int nshots;
static int is_flame(const char *id) { return id && !strcmp(id, "flame"); }
/* A flame never damages a flame: the body and the bolt. */
static int can_harm(const char *from, const char *to) { return !(is_flame(from) && is_flame(to)); }
static void write_events(const char *path, const Body *b, int n) {
    FILE *o = fopen(path, "w");
    if (!o) return;
    for (int i = 0; i < n; i++) if (b[i].alive) {
        const char *sheet; int idx;
        sheet_of(b[i].id, &sheet, &idx);
        fprintf(o, "%d %d %d %d %d %s %d 2 1\n", b[i].x, b[i].y, b[i].r, b[i].g, b[i].b, sheet, idx);
    }
    for (int i = 0; i < nshots; i++) if (shots[i].alive)
        fprintf(o, "%d %d 239 68 68 !Flame 0 2 1\n", shots[i].x, shots[i].y);
    fclose(o);
}
static void kv_get(const char *path, const char *key, char *out, int cap) {
    out[0] = 0;
    FILE *f = fopen(path, "r");
    char line[256];
    int klen = (int)strlen(key);
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, (size_t)klen) == 0 && line[klen] == '=') {
            snprintf(out, (size_t)cap, "%s", line + klen + 1);
            char *e = out + strlen(out);
            while (e > out && (e[-1] == '\n' || e[-1] == '\r')) { e--; *e = 0; }
            break;
        }
    }
    fclose(f);
}
static Kind *findk(const char *id) {
    for (int i = 0; i < nk; i++) if (!strcmp(kinds[i].id, id)) return &kinds[i];
    return NULL;
}
static int reach(const Kind *k) { int c = k->range / 20; return c < 1 ? 1 : c; }
static int load_bodies(const char *ledger, Body *b, int cap) {
    FILE *f = fopen(ledger, "r");
    if (!f) return 0;
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "STROKE|", 7) || n >= cap) continue;
        int team, x, y;
        char id[24];
        if (sscanf(line + 7, "%d|%23[^|]|%d|%d", &team, id, &x, &y) != 4) continue;
        Kind *k = findk(id);
        if (!k) continue;
        b[n].team = team; b[n].x = x; b[n].y = y; b[n].hp = k->hp; b[n].dmg = k->dmg; b[n].range = reach(k);
        b[n].r = k->r; b[n].g = k->g; b[n].b = k->b; b[n].alive = 1;
        snprintf(b[n].id, sizeof b[n].id, "%s", id);
        n++;
    }
    fclose(f);
    return n;
}
static int cheb(const Body *a, const Body *c) {
    int dx = a->x - c->x, dy = a->y - c->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx > dy ? dx : dy;
}
static void shot_step(Body *b, int n, int *dmg) {
    for (int s = 0; s < nshots; s++) if (shots[s].alive) {
        shots[s].x += shots[s].dx;
        shots[s].y += shots[s].dy;
        shots[s].life--;
        if (shots[s].x < 0 || shots[s].y < 0 || shots[s].x > 14 || shots[s].y > 14 || shots[s].life <= 0) {
            shots[s].alive = 0;
            continue;
        }
        for (int j = 0; j < n; j++) if (b[j].alive && b[j].team != shots[s].team && b[j].x == shots[s].x && b[j].y == shots[s].y) {
            if (can_harm(shots[s].kind, b[j].id)) dmg[j] += shots[s].dmg;
            if (can_harm(shots[s].kind, b[j].id)) shots[s].alive = 0;
            break;
        }
    }
}
static void one_tick(Body *b, int n) {
    int dmg[64];
    for (int i = 0; i < n; i++) dmg[i] = 0;
    shot_step(b, n, dmg);
    for (int i = 0; i < n; i++) if (b[i].alive) {
        int best = -1, bd = 999;
        for (int j = 0; j < n; j++) if (i != j && b[j].alive && b[j].team != b[i].team) {
            int d = cheb(&b[i], &b[j]);
            if (d < bd) { bd = d; best = j; }
        }
        if (best < 0) continue;
        if (b[i].range <= 1 || bd <= 1) {
            if (bd <= b[i].range && can_harm(b[i].id, b[best].id)) dmg[best] += b[i].dmg;
            else if (bd <= 1) { /* a flame standing on a flame does not step onto it */ }
            else {
                int dx = b[best].x - b[i].x, dy = b[best].y - b[i].y;
                int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
                if (adx >= ady) b[i].x += dx > 0 ? 1 : -1;
                else b[i].y += dy > 0 ? 1 : -1;
            }
        } else if (bd <= b[i].range) {
            int owned = 0;
            for (int s = 0; s < nshots; s++) if (shots[s].alive && shots[s].owner == i) owned = 1;
            if (!owned && nshots < 16) {
                int dx = b[best].x - b[i].x, dy = b[best].y - b[i].y;
                int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
                Shot *s = &shots[nshots++];
                s->team = b[i].team; s->owner = i; s->alive = 1; s->dmg = b[i].dmg; s->life = b[i].range;
                s->dx = adx >= ady ? (dx > 0 ? 1 : -1) : 0;
                s->dy = adx >= ady ? 0 : (dy > 0 ? 1 : -1);
                s->x = b[i].x + s->dx; s->y = b[i].y + s->dy;
                snprintf(s->kind, sizeof s->kind, "%s", is_flame(b[i].id) ? "flame" : "bolt");
            }
        } else {
            int dx = b[best].x - b[i].x, dy = b[best].y - b[i].y;
            int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            if (adx >= ady) b[i].x += dx > 0 ? 1 : -1;
            else b[i].y += dy > 0 ? 1 : -1;
        }
    }
    for (int i = 0; i < n; i++) if (b[i].alive && dmg[i]) {
        b[i].hp -= dmg[i];
        if (b[i].hp <= 0) b[i].alive = 0;
    }
}
static void replay(Body *b, int n, int steps) {
    nshots = 0;
    for (int s = 0; s < steps; s++) one_tick(b, n);
}
static void show(const Body *b, int n) {
    int alive = 0;
    for (int i = 0; i < n; i++) if (b[i].alive) alive++;
    printf("alive=%d\n", alive);
    for (int i = 0; i < n; i++)
        printf("%s team=%d x=%d y=%d hp=%d %s\n", b[i].id, b[i].team, b[i].x, b[i].y, b[i].hp < 0 ? 0 : b[i].hp, b[i].alive ? "up" : "dead");
}
static int ticks_done(const char *ledger) {
    FILE *f = fopen(ledger, "r");
    if (!f) return 0;
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, f)) if (!strncmp(line, "TICK|", 5)) n++;
    fclose(f);
    return n;
}
static int cell_of(const char *line, int *x, int *y) {
    const char *p = strstr(line, "ray=");
    if (!p) p = strstr(line, "pos=");
    if (!p) return 0;
    p = strchr(p, '=');
    if (!p || !strncmp(p + 1, "miss", 4)) return 0;
    return sscanf(p + 1, "%d,%d", x, y) == 2;
}
/* Flat map (key 0) draws the page fitted to the canvas. The engine's click test still divides by that fit size but bounds it with the 80px grid, so a click on the lower board updates the orange debug square and never writes a cell. Same fit: whole 15x15 page, centred. */
static int cell_from_canvas(const char *stamp, int *x, int *y) {
    int cx, cy, cw, ch;
    if (sscanf(stamp, "%d %d %d %d", &cx, &cy, &cw, &ch) != 4 || cw < 1 || ch < 1) return 0;
    const int cols = 15, rows = 15;
    int fit = cw / cols, fh = ch / rows;
    if (fh < fit) fit = fh;
    if (fit < 4) fit = 4;
    int ox = (cw - cols * fit) / 2, oy = (ch - rows * fit) / 2;
    if (cx < ox || cy < oy) return 0;
    int sx = (cx - ox) / fit, sy = (cy - oy) / fit;
    if (sx < 0 || sy < 0 || sx >= cols || sy >= rows) return 0;
    *x = sx; *y = sy;
    return 1;
}
static void cursor_set(const char *path, int x, int y) {
    FILE *f = fopen(path, "r");
    char body[1024] = "";
    if (f) { size_t n = fread(body, 1, sizeof body - 1, f); body[n] = 0; fclose(f); }
    int z = 17;
    const char *pz = strstr(body, "pos_z=");
    if (pz) z = atoi(pz + 6);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "entity_type=xelector\npos_x=%d\npos_y=%d\npos_z=%d\n", x, y, z);
    const char *rest = strstr(body, "possessed_id=");
    if (rest) fputs(rest, f);
    else fputs("possessed_id=hero_01\n", f);
    fclose(f);
}
/* One fight step. Appends TICK and redraws the desk. Does not rewrite the game bar. */
static int battle_step(const char *ledger, const char *units, const char *desk) {
    FILE *f = fopen(ledger, "a");
    if (!f) return 2;
    fprintf(f, "TICK|%d\n", ticks_done(ledger) + 1);
    fclose(f);
    load_units(units);
    Body b[64];
    int n = load_bodies(ledger, b, 64);
    int steps = ticks_done(ledger);
    replay(b, n, steps);
    char ev[512];
    snprintf(ev, sizeof ev, "%s/events.txt", desk);
    write_events(ev, b, n);
    printf("tick %d\n", steps);
    fflush(stdout);
    return 0;
}
static int cursor_get(const char *path, int *x, int *y) {
    FILE *f = fopen(path, "r");
    char line[128];
    int gx = 0, gy = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "pos_x=", 6)) gx = atoi(line + 6);
        else if (!strncmp(line, "pos_y=", 6)) gy = atoi(line + 6);
    }
    fclose(f);
    *x = gx; *y = gy;
    return 1;
}
/* One place. Team 1 paints. The picked id is the feed. A living copy of that id already on the cell is left alone. */
static int place_at(const char *ledger, const char *units, const char *desk, const char *pick, int x, int y) {
    char id[32];
    kv_get(pick, "opt_picked", id, sizeof id);
    if (!id[0]) { fprintf(stderr, "place: nothing picked\n"); return 2; }
    load_units(units);
    if (!findk(id)) { fprintf(stderr, "place: unknown %s\n", id); return 2; }
    if (x < 0 || y < 0 || x > 14 || y > 14) { fprintf(stderr, "place: off the board %d,%d\n", x, y); return 2; }
    Body have[64];
    int n = load_bodies(ledger, have, 64);
    int steps = ticks_done(ledger);
    replay(have, n, steps);
    for (int i = 0; i < n; i++) if (have[i].alive && have[i].x == x && have[i].y == y && !strcmp(have[i].id, id)) {
        printf("place already %s %d,%d\n", id, x, y);
        return 0;
    }
    FILE *f = fopen(ledger, "a");
    if (!f) return 2;
    fprintf(f, "STROKE|1|%s|%d|%d\n", id, x, y);
    fclose(f);
    n = load_bodies(ledger, have, 64);
    steps = ticks_done(ledger);
    replay(have, n, steps);
    char ev[512];
    snprintf(ev, sizeof ev, "%s/events.txt", desk);
    write_events(ev, have, n);
    printf("placed %s %d,%d\n", id, x, y);
    fflush(stdout);
    return 0;
}
/* The board paints when this file grows. One byte per place, so the new sprite is the next dispatch frame instead of the next mouse move. */
static void bump_screen(const char *session) {
    char p[512];
    snprintf(p, sizeof p, "%s/pieces/display/bv_screen_changed.txt", session);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}
/* Draw Battle's own clock. Id "draw", root <app>/state/clock_root. No AT reminders: a reminder forks a page, and the fight step is the listener. */
static int lc_call(const char *house, const char *root, const char *a, const char *b, const char *c, const char *d, const char *e) {
    char bin[512];
    snprintf(bin, sizeof bin, "%s/&.widgits/livedesk-clock/ops/+x/lc_clock.+x", house);
    pid_t pid = fork();
    if (pid < 0) return 2;
    if (pid == 0) {
        const char *av[10];
        int n = 0;
        av[n++] = bin; av[n++] = root; av[n++] = a;
        if (b) av[n++] = b;
        if (c) av[n++] = c;
        if (d) av[n++] = d;
        if (e) av[n++] = e;
        av[n] = NULL;
        execv(bin, (char *const *)av);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return st == 0 ? 0 : 2;
}
static int rate_ok(const char *s) {
    return s && (!strcmp(s, "min") || !strcmp(s, "sec") || !strcmp(s, "hour") || !strcmp(s, "cent"));
}
static void clock_paths(const char *state, char *root, size_t rcap, char *file, size_t fcap) {
    snprintf(root, rcap, "%s/clock_root", state);
    if (strlen(root) + 28 < fcap) snprintf(file, fcap, "%s/#.desktop/clocks/draw.pdl", root);
    else file[0] = 0;
}
static void publish_labels(const char *state, int playing, const char *rate) {
    char p[512], body[160];
    snprintf(p, sizeof p, "%s/run_ui.txt", state);
    snprintf(body, sizeof body, "running=%d\nrun_on=%d\nrun_label=%s\nrun_cls=%s\n", playing, playing, playing ? "Player: playing" : "Player: stopped", playing ? "ph-green" : "ph-red");
    FILE *f = fopen(p, "w");
    if (f) { fputs(body, f); fclose(f); }
    char root[640], clk[700], epoch[32] = "0", clkrate[16] = "min";
    clock_paths(state, root, sizeof root, clk, sizeof clk);
    kv_get(clk, "game_time_epoch_ms", epoch, sizeof epoch);
    kv_get(clk, "rate", clkrate, sizeof clkrate);
    if (!clkrate[0]) snprintf(clkrate, sizeof clkrate, "%s", rate && rate[0] ? rate : "min");
    long long ms = atoll(epoch);
    if (ms < 0) ms = 0;
    long long day = ms / 86400000LL;
    int sec = (int)((ms / 1000) % 86400);
    snprintf(p, sizeof p, "%s/clock_ui.txt", state);
    snprintf(body, sizeof body, "time_label=D%lld %02d:%02d\ntime_rate=%s\ntime_running=%d\n", day + 1, sec / 3600, (sec % 3600) / 60, clkrate, playing);
    f = fopen(p, "w");
    if (f) { fputs(body, f); fclose(f); }
}
static int clock_install(const char *state, const char *house) {
    char root[640], file[700], dir[700];
    clock_paths(state, root, sizeof root, file, sizeof file);
    snprintf(dir, sizeof dir, "%s/#.desktop/clocks", root);
    mkdir(root, 0755);
    snprintf(dir, sizeof dir, "%s/#.desktop", root);
    mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%s/#.desktop/clocks", root);
    mkdir(dir, 0755);
    if (access(file, R_OK) == 0) return 0;
    if (lc_call(house, root, "new", "draw", "user", "draw battle", NULL)) return 2;
    if (lc_call(house, root, "rate", "draw", "min", NULL, NULL)) return 2;
    if (lc_call(house, root, "cmd", "draw", "settime", "21600000", NULL)) return 2;
    lc_call(house, root, "cmd", "draw", "pause", "--source", "db-install");
    return 0;
}
/* lc_call cannot pass --source as a 6th arg through the fixed slots. settime uses 5 args and drops --source. Pause uses the slots it has. */
static int clock_verb(const char *state, const char *house, const char *sub, const char *arg) {
    if (clock_install(state, house)) return 2;
    char root[640], file[700];
    clock_paths(state, root, sizeof root, file, sizeof file);
    if (!strcmp(sub, "play")) {
        lc_call(house, root, "cmd", "draw", "resume", "--source", "db-play");
        lc_call(house, root, "daemon-start", NULL, NULL, NULL, NULL);
        publish_labels(state, 1, "min");
        printf("playing\n");
        return 0;
    }
    if (!strcmp(sub, "stop")) {
        lc_call(house, root, "cmd", "draw", "pause", "--source", "db-stop");
        lc_call(house, root, "daemon-stop", NULL, NULL, NULL, NULL);
        publish_labels(state, 0, "min");
        printf("stopped\n");
        return 0;
    }
    if (!strcmp(sub, "rate")) {
        if (!rate_ok(arg)) return 2;
        if (lc_call(house, root, "rate", "draw", arg, NULL, NULL)) return 2;
        char run[8] = "0";
        char rp[700];
        snprintf(rp, sizeof rp, "%s/run_ui.txt", state);
        kv_get(rp, "running", run, sizeof run);
        publish_labels(state, atoi(run), arg);
        printf("rate %s\n", arg);
        return 0;
    }
    if (!strcmp(sub, "reinstall")) {
        lc_call(house, root, "daemon-stop", NULL, NULL, NULL, NULL);
        char run[8] = "0";
        char rp[700];
        snprintf(rp, sizeof rp, "%s/run_ui.txt", state);
        kv_get(rp, "running", run, sizeof run);
        if (atoi(run)) lc_call(house, root, "daemon-start", NULL, NULL, NULL, NULL);
        publish_labels(state, atoi(run), "");
        printf("reinstalled\n");
        return 0;
    }
    return 2;
}
static int listen_main(const char *ledger, const char *units, const char *desk, const char *pick, const char *pkg, const char *session, int wpid) {
    char click[512], keys[512], cur[512], relay[512];
    snprintf(click, sizeof click, "%s/pieces/display/click_hud.txt", pkg);
    snprintf(keys, sizeof keys, "%s/pieces/keyboard/history.txt", session);
    snprintf(relay, sizeof relay, "%s/pieces/apps/player_app/interact_relay.txt", session);
    snprintf(cur, sizeof cur, "%s/pieces/xelector_01/state.txt", pkg);
    char seen[256] = "", seen_cv[64] = "";
    long koff = 0, roff = 0, hoff = 0;
    struct stat st;
    if (stat(keys, &st) == 0) koff = st.st_size;
    if (stat(relay, &st) == 0) roff = st.st_size;
    char cvpath[512], modest[512];
    snprintf(cvpath, sizeof cvpath, "%s/pieces/display/canvas_click.txt", session);
    snprintf(modest, sizeof modest, "%s/pieces/system/bv_state.txt", session);
    /* HOSTKEY Enter is written beside the app, one directory above the pchq package. */
    char hostrel[512];
    snprintf(hostrel, sizeof hostrel, "%s/../state/interact_relay.txt", pkg);
    if (stat(hostrel, &st) == 0) hoff = st.st_size;
    { FILE *cf = fopen(click, "r"); if (cf) { size_t n = fread(seen, 1, sizeof seen - 1, cf); seen[n] = 0; fclose(cf); } }
    { FILE *cf = fopen(cvpath, "r"); if (cf) { if (fgets(seen_cv, sizeof seen_cv, cf)) seen_cv[strcspn(seen_cv, "\r\n")] = 0; fclose(cf); } }
    int iw = inotify_init1(IN_NONBLOCK);
    if (iw >= 0) {
        char dir[512];
        snprintf(dir, sizeof dir, "%s/pieces/display", session);
        inotify_add_watch(iw, dir, IN_CLOSE_WRITE | IN_MOVED_TO);
        snprintf(dir, sizeof dir, "%s/pieces/display", pkg);
        inotify_add_watch(iw, dir, IN_CLOSE_WRITE | IN_MOVED_TO);
        snprintf(dir, sizeof dir, "%s/../state", pkg);
        inotify_add_watch(iw, dir, IN_MODIFY | IN_CLOSE_WRITE);
    }
    while (kill(wpid, 0) == 0) {
        char modeb[16] = "0";
        kv_get(modest, "render_mode", modeb, sizeof modeb);
        int flat = atoi(modeb) == 0;
        if (flat) {
            FILE *cf = fopen(cvpath, "r");
            char stamp[64] = "";
            if (cf) { if (fgets(stamp, sizeof stamp, cf)) stamp[strcspn(stamp, "\r\n")] = 0; fclose(cf); }
            if (stamp[0] && strcmp(stamp, seen_cv)) {
                int x, y;
                snprintf(seen_cv, sizeof seen_cv, "%s", stamp);
                if (cell_from_canvas(stamp, &x, &y)) {
                    cursor_set(cur, x, y);
                    place_at(ledger, units, desk, pick, x, y);
                    bump_screen(session);
                }
            }
        } else {
            FILE *cf = fopen(click, "r");
            char chunk[256] = "";
            if (cf) { size_t n = fread(chunk, 1, sizeof chunk - 1, cf); chunk[n] = 0; fclose(cf); }
            if (chunk[0] && strcmp(chunk, seen)) {
                int x, y;
                snprintf(seen, sizeof seen, "%s", chunk);
                if (cell_of(chunk, &x, &y)) {
                    cursor_set(cur, x, y);
                    place_at(ledger, units, desk, pick, x, y);
                    bump_screen(session);
                }
            }
        }
        /* Enter arrives twice if we also read the session relay: the engine copies HOSTKEY 13 into the app relay. Watch only that copy. */
        const char *paths[1] = { hostrel };
        long *offs[1] = { &hoff };
        (void)relay; (void)keys; (void)roff; (void)koff;
        for (int pass = 0; pass < 1; pass++) {
            FILE *kf = fopen(paths[pass], "r");
            if (!kf) continue;
            if (fseek(kf, 0, SEEK_END) == 0 && ftell(kf) < offs[pass][0]) offs[pass][0] = 0;
            if (fseek(kf, offs[pass][0], SEEK_SET) != 0) { fclose(kf); continue; }
            char line[64];
            while (fgets(line, sizeof line, kf)) {
                int code = atoi(line);
                if (code == 13) {
                    int x, y;
                    if (cursor_get(cur, &x, &y)) place_at(ledger, units, desk, pick, x, y);
                }
            }
            offs[pass][0] = ftell(kf);
            fclose(kf);
        }
        {
            char statedir[700], stpath[720], run[8] = "0";
            snprintf(statedir, sizeof statedir, "%s/../state", pkg);
            snprintf(stpath, sizeof stpath, "%s/run_ui.txt", statedir);
            kv_get(stpath, "running", run, sizeof run);
            static long long last_step, last_label;
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            long long now = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
            if (atoi(run)) {
                if (now - last_step >= 500) {
                    last_step = now;
                    /* No living other side: the steps still run and every body stands still. One team-2 roman gives the pile someone to walk to. */
                    Body chk[64];
                    load_units(units);
                    int cn = load_bodies(ledger, chk, 64);
                    int steps_now = ticks_done(ledger);
                    replay(chk, cn, steps_now);
                    int foe = 0;
                    for (int i = 0; i < cn; i++) if (chk[i].alive && chk[i].team != 1) foe = 1;
                    if (!foe) {
                        FILE *sf = fopen(ledger, "a");
                        if (sf) { fprintf(sf, "STROKE|2|roman|13|2\n"); fclose(sf); }
                    }
                    if (battle_step(ledger, units, desk) == 0) {
                        bump_screen(session);
                        /* The proven wake (game_core engine_nudge): 126 is bound to nothing, and a changed events.txt is drawn on the next frame only. */
                        char rp[720];
                        snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", session);
                        FILE *rf = fopen(rp, "a");
                        if (rf) { fputs("126\n", rf); fclose(rf); }
                        char feed[720];
                        snprintf(feed, sizeof feed, "%s/game_ui.txt", statedir);
                        char opath[512];
                        snprintf(opath, sizeof opath, "%s", "/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/_shared-lib/ops/+x/db_battle_op.+x");
                        Body b[64];
                        int n = load_bodies(ledger, b, 64);
                        int steps = ticks_done(ledger);
                        replay(b, n, steps);
                        FILE *g = fopen(feed, "w");
                        if (g) {
                            fprintf(g, "gm_on=1\ngm_title=DRAW BATTLE\ngm_nt=%d\n", n > 0 ? n : 1);
                            for (int i = 0; i < n && i < 12; i++)
                                fprintf(g, "gmt_%d_text=%s team %d  hp %d  %s  cell %d,%d\n", i, b[i].id, b[i].team, b[i].hp < 0 ? 0 : b[i].hp, b[i].alive ? "up" : "dead", b[i].x, b[i].y);
                            if (n > 12) fprintf(g, "gmt_12_text=and %d more\n", n - 12);
                            fprintf(g, "gm_nt=%d\n", n > 12 ? 13 : (n > 0 ? n : 1));
                            fprintf(g, "gm_nb=2\ngmb_0_label=Tick\ngmb_0_act='%s' step '%s' '%s' '%s' '%s'\n", opath, ledger, units, desk, feed);
                            fprintf(g, "gmb_1_label=Reset\ngmb_1_act='%s' reset '%s' '%s' '%s' '%s'\n", opath, ledger, units, desk, feed);
                            fclose(g);
                        }
                    }
                }
                if (now - last_label >= 5000) {
                    last_label = now;
                    publish_labels(statedir, 1, "");
                }
            }
        }
        if (iw >= 0) {
            struct pollfd pfd = { iw, POLLIN, 0 };
            if (poll(&pfd, 1, 16) > 0) {
                char dump[1024];
                while (read(iw, dump, sizeof dump) > 0) {}
            }
        } else usleep(16000);
    }
    if (iw >= 0) close(iw);
    return 0;
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: catalog|stroke|tick|project|place|listen\n"); return 2; }
    if (!strcmp(argv[1], "place")) {
        if (argc != 8) return 2;
        int x, y;
        if (!strcmp(argv[6], "cursor")) {
            if (!cursor_get(argv[7], &x, &y)) return 2;
            return place_at(argv[2], argv[3], argv[4], argv[5], x, y);
        }
        x = atoi(argv[6]); y = atoi(argv[7]);
        return place_at(argv[2], argv[3], argv[4], argv[5], x, y);
    }
    if (!strcmp(argv[1], "listen")) {
        if (argc != 9) return 2;
        return listen_main(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], atoi(argv[8]));
    }
    if (!strcmp(argv[1], "play") || !strcmp(argv[1], "stop")) {
        if (argc < 4) return 2;
        return clock_verb(argv[2], argv[3], argv[1], NULL);
    }
    if (!strcmp(argv[1], "clock")) {
        if (argc < 5) return 2;
        return clock_verb(argv[2], argv[3], argv[4], argc > 5 ? argv[5] : NULL);
    }
    if (!strcmp(argv[1], "catalog")) {
        if (argc != 4) return 2;
        load_units(argv[2]);
        Kind *k = findk(argv[3]);
        if (!k) { fprintf(stderr, "unknown %s\n", argv[3]); return 2; }
        printf("id=%s hp=%d dmg=%d range_cells=%d\n", k->id, k->hp, k->dmg, reach(k));
        return 0;
    }
    if (!strcmp(argv[1], "stroke")) {
        if (argc != 8) return 2;
        load_units(argv[3]);
        if (!findk(argv[5])) { fprintf(stderr, "unknown %s\n", argv[5]); return 2; }
        FILE *f = fopen(argv[2], "a");
        if (!f) return 2;
        fprintf(f, "STROKE|%s|%s|%s|%s\n", argv[4], argv[5], argv[6], argv[7]);
        fclose(f);
        printf("stroked\n");
        return 0;
    }
    if (!strcmp(argv[1], "tick") || !strcmp(argv[1], "project")) {
        if (argc != 5 && !(!strcmp(argv[1], "tick") && argc == 4)) return 2;
        const char *ledger = argv[2], *units = argv[3];
        load_units(units);
        Body b[64];
        int n = load_bodies(ledger, b, 64);
        int steps = ticks_done(ledger);
        if (!strcmp(argv[1], "tick")) steps += 1;
        replay(b, n, steps);
        show(b, n);
        if (!strcmp(argv[1], "tick")) {
            FILE *f = fopen(ledger, "a");
            if (!f) return 2;
            fprintf(f, "TICK|%d\n", steps);
            fclose(f);
            return 0;
        }
        char path[512];
        snprintf(path, sizeof path, "%s/events.txt", argv[4]);
        write_events(path, b, n);
        printf("projected %s\n", path);
        return 0;
    }
    if (!strcmp(argv[1], "show") || !strcmp(argv[1], "step") || !strcmp(argv[1], "reset")) {
        if (argc != 6) return 2;
        const char *ledger = argv[2], *units = argv[3], *desk = argv[4], *feed = argv[5];
        if (!strcmp(argv[1], "reset")) { FILE *f = fopen(ledger, "w"); if (f) fclose(f); }
        load_units(units);
        if (!strcmp(argv[1], "reset") || ticks_done(ledger) + 1) {
            Body have[64];
            if (load_bodies(ledger, have, 64) == 0) {
                FILE *f = fopen(ledger, "a");
                if (!f) return 2;
                fprintf(f, "STROKE|1|soldier|2|7\nSTROKE|2|roman|10|7\n");
                fclose(f);
            }
        }
        if (!strcmp(argv[1], "step")) {
            FILE *f = fopen(ledger, "a");
            if (!f) return 2;
            fprintf(f, "TICK|%d\n", ticks_done(ledger) + 1);
            fclose(f);
        }
        Body b[64];
        int n = load_bodies(ledger, b, 64);
        int steps = ticks_done(ledger);
        replay(b, n, steps);
        char ev[512];
        snprintf(ev, sizeof ev, "%s/events.txt", desk);
        write_events(ev, b, n);
        char opath[512];
        snprintf(opath, sizeof opath, "%s", argv[0]);
        FILE *g = fopen(feed, "w");
        if (!g) return 2;
        fprintf(g, "gm_on=1\ngm_title=DRAW BATTLE\ngm_nt=%d\n", n > 0 ? n : 1);
        if (n == 0) fprintf(g, "gmt_0_text=no bodies\n");
        for (int i = 0; i < n; i++)
            fprintf(g, "gmt_%d_text=%s team %d  hp %d  %s  cell %d,%d\n", i, b[i].id, b[i].team, b[i].hp < 0 ? 0 : b[i].hp, b[i].alive ? "up" : "dead", b[i].x, b[i].y);
        fprintf(g, "gm_nb=2\n");
        fprintf(g, "gmb_0_label=Tick\ngmb_0_act='%s' step '%s' '%s' '%s' '%s'\n", opath, ledger, units, desk, feed);
        fprintf(g, "gmb_1_label=Reset\ngmb_1_act='%s' reset '%s' '%s' '%s' '%s'\n", opath, ledger, units, desk, feed);
        fclose(g);
        printf("bar ticks=%d alive_rows=%d\n", steps, n);
        return 0;
    }
    if (!strcmp(argv[1], "pick")) {
        if (argc < 5) return 2;
        const char *units = argv[2], *feed = argv[3], *which = argv[4];
        load_units(units);
        char cat[16], picked[24];
        kv_get(feed, "cat", cat, sizeof cat);
        kv_get(feed, "opt_picked", picked, sizeof picked);
        int show_opt = 0;
        if (!strcmp(which, "back") || !strcmp(which, "list")) show_opt = 0;
        else {
            int is_group = 0;
            for (int i = 0; i < nk; i++) if (!strcmp(kinds[i].group, which)) is_group = 1;
            if (is_group) { snprintf(cat, sizeof cat, "%s", which); show_opt = 1; }
            else if (findk(which)) { snprintf(picked, sizeof picked, "%s", which); show_opt = 1; }
            else return 2;
        }
        FILE *g = fopen(feed, "w");
        if (!g) return 2;
        fprintf(g, "side_list=%s\nside_opt=%s\ncat=%s\nopt_picked=%s\npick_title=%s\n",
            show_opt ? "" : "1", show_opt ? "1" : "", cat, picked, show_opt ? cat : "types");
        char groups[8][16];
        int ng = 0;
        for (int i = 0; i < nk && ng < 8; i++) {
            int seen = 0;
            for (int j = 0; j < ng; j++) if (!strcmp(groups[j], kinds[i].group)) seen = 1;
            if (!seen) snprintf(groups[ng++], 16, "%s", kinds[i].group);
        }
        fprintf(g, "n_dcat=%d\n", ng);
        for (int i = 0; i < ng; i++)
            fprintf(g, "dcat_%d_label=%s\ndcat_%d_act='%s' pick '%s' '%s' %s\n", i, groups[i], i, argv[0], units, feed, groups[i]);
        int no = 0;
        for (int i = 0; i < nk; i++) if (!strcmp(kinds[i].group, cat)) no++;
        fprintf(g, "n_opt=%d\ncan_back=%s\n", show_opt ? no : 0, show_opt ? "1" : "");
        int oi = 0;
        for (int i = 0; i < nk; i++) if (!strcmp(kinds[i].group, cat)) {
            int on = !strcmp(kinds[i].id, picked);
            fprintf(g, "opt_%d_label=%s%s %s\nopt_%d_cls=%s\nopt_%d_act='%s' pick '%s' '%s' %s\n",
                oi, on ? "x " : "", kinds[i].emoji, kinds[i].label,
                oi, on ? "opt-picked" : "tile",
                oi, argv[0], units, feed, kinds[i].id);
            oi++;
        }
        fclose(g);
        printf("pick view=%s cat=%s picked=%s\n", show_opt ? "opt" : "list", cat, picked);
        return 0;
    }
    fprintf(stderr, "unknown verb %s\n", argv[1]);
    return 2;
}
