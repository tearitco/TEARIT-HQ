/* wo - world-ops: the small reusable steps of a petslike game's systems, one event-page node each (BOOK 16.game/RPG-SPACE-BLUEPRINT-SYSTEMS.md, B1; owner 2026-10-25:
 * "events based, blueprint viewable ... not shared libs, but reusable ops, events, pal").
 *
 *   wo kv    <game> <file> <key> <expr>            file in <game>/state/ ; expr  add:N  sub:N  set:VALUE  set:mhp (the actor's max hp)   (also +N -N =V)   -> prints key=value
 *   wo drop  <game> <actor> <pct|cfg> <src>        pct percent of every stack of the actor's inventory (life file inv=) falls as remnants (drops.txt) where the actor stands
 *   wo place <game> <actor> <room> <x> <y>         put the actor on a cell (its position file); room/x/y may be @respawn_room ...
 *   wo log   <game> <KIND> <f1> [f2] [f3]          append KIND|time|<actor>|f1|f2|f3 to state/events.txt (fields may use @pos @dropped @pct @mode @<LIFE key>)
 *   wo pending <game> <file>  |  wo remnant <game> <kind> <room> <x> <y>  |  wo clear_damage <game> <room> <x> <y>  |  wo node_use <game> <kind> <room> <x> <y>  |  wo event <game> <KIND> <f1>... ('/' in a field = column break)   (B2, the destroyed page)
 *   wo when  <game> <ROW.key> <value> <event>      if <game>/life.pdl-style config ROW|key == value, play <game>/events/<event> (trigger parallel)
 *
 * <game> is a game folder (<house>/@.apps/<app>); the events pass it as the grandparent of their own folder, so a page copied to another game runs on that game's files.
 * Names in arguments: :actor: = the id in state/life_actor.txt; :pos: :dropped: :pct: :mode: :drops: ("dropped N (P%)") :modeline: ("mode M") and any LIFE key (:respawn_room:). The event editor only lets
 * letters, digits and _ - . : , = | / space into a node, so no @ + ( ) % there; the @name form works too for the command line. Characters come from the game's life.pdl:  ACTOR | <id> | <position file in state/> | <max hp>.
 * Plain files only: key=value state files, `|` rows, no shared memory, no header, no link to the game. Self-contained on purpose (house rule: ops are copied, not linked). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define BIG 8192
static char GAME[PATH_MAX], ACTOR[64], LAST_ID[64];

static int read_file(const char *p, char *b, size_t cap) { FILE *f = fopen(p, "r"); if (!f) { b[0] = 0; return 0; } size_t n = fread(b, 1, cap - 1, f); b[n] = 0; fclose(f); return (int)n; }
static void write_if_changed(const char *p, const char *data) {
    char old[BIG * 4]; int n = read_file(p, old, sizeof old); if (n == (int)strlen(data) && !strcmp(old, data)) return;
    char tmp[PATH_MAX + 24]; snprintf(tmp, sizeof tmp, "%s.tmp.%d", p, (int)getpid()); FILE *f = fopen(tmp, "w"); if (!f) { fprintf(stderr, "wo: cannot write %s\n", p); exit(2); }
    fputs(data, f); fclose(f); rename(tmp, p); }
static void append_line(const char *p, const char *line) { int fd = open(p, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd < 0) return; char b[1200]; int n = snprintf(b, sizeof b, "%s\n", line); if (write(fd, b, (size_t)n) < 0) { /* best effort */ } close(fd); }
static void spath(char *out, const char *rel) { snprintf(out, PATH_MAX, "%s/state/%s", GAME, rel); }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }

/* key=value files */
static int kv_get(const char *path, const char *key, char *out, size_t cap) {
    char b[BIG * 4]; read_file(path, b, sizeof b); size_t kl = strlen(key); out[0] = 0;
    char *sv = NULL; for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) if (!strncmp(l, key, kl) && l[kl] == '=') { snprintf(out, cap, "%s", l + kl + 1); return 1; } return 0; }
static void kv_set(const char *path, const char *key, const char *val) {
    char b[BIG * 4], out[BIG * 4] = ""; read_file(path, b, sizeof b); size_t kl = strlen(key); int found = 0;
    char *sv = NULL; for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { size_t o = strlen(out);
        if (!strncmp(l, key, kl) && l[kl] == '=') { snprintf(out + o, sizeof out - o, "%s=%s\n", key, val); found = 1; } else snprintf(out + o, sizeof out - o, "%s\n", l); }
    if (!found) { size_t o = strlen(out); snprintf(out + o, sizeof out - o, "%s=%s\n", key, val); }
    write_if_changed(path, out); }

/* life.pdl style config: ROW | key | value | ... */
static int cfg_get(const char *rowname, const char *key, char *out, size_t cap) {
    char p[PATH_MAX], b[BIG * 2]; snprintf(p, sizeof p, "%s/life.pdl", GAME); read_file(p, b, sizeof b); out[0] = 0;
    char *sv = NULL; for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { if (l[0] == '#') continue; char c[512]; snprintf(c, sizeof c, "%s", l); char *f[4] = { 0 }; int n = 0; char *sv2 = NULL; for (char *t = strtok_r(c, "|", &sv2); t && n < 4; t = strtok_r(NULL, "|", &sv2)) f[n++] = trim(t);
        if (n >= 3 && !strcmp(f[0], rowname) && !strcmp(f[1], key)) { snprintf(out, cap, "%s", f[2]); return 1; } } return 0; }
static int actor_row(const char *id, char *posfile, size_t pn, int *mhp) {
    char p[PATH_MAX], b[BIG * 2]; snprintf(p, sizeof p, "%s/life.pdl", GAME); read_file(p, b, sizeof b);
    char *sv = NULL; for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { if (l[0] == '#') continue; char c[512]; snprintf(c, sizeof c, "%s", l); char *f[5] = { 0 }; int n = 0; char *sv2 = NULL; for (char *t = strtok_r(c, "|", &sv2); t && n < 5; t = strtok_r(NULL, "|", &sv2)) f[n++] = trim(t);
        if (n >= 4 && !strcmp(f[0], "ACTOR") && !strcmp(f[1], id)) { snprintf(posfile, pn, "%s", f[2]); *mhp = atoi(f[3]); return 1; } } return 0; }

/* @actor and the other @tokens */
static void resolve_actor(void) { char p[PATH_MAX], b[64]; spath(p, "life_actor.txt"); read_file(p, b, sizeof b); b[strcspn(b, "\r\n")] = 0; snprintf(ACTOR, sizeof ACTOR, "%s", b); }
static int token_value(const char *key, char *v, size_t cap) {      /* a known name -> its value; 0 when unknown */
    char lp[PATH_MAX]; spath(lp, "wo_last.txt"); v[0] = 0;
    if (!strcmp(key, "actor")) { snprintf(v, cap, "%s", ACTOR); return 1; }
    if (!strcmp(key, "mode")) { cfg_get("LIFE", "death_mode", v, cap); return 1; }
    if (!strcmp(key, "pct")) { if (!kv_get(lp, "pct", v, cap)) cfg_get("LIFE", "drop_pct", v, cap); return 1; }
    if (!strcmp(key, "modeline")) { char m[64]; cfg_get("LIFE", "death_mode", m, sizeof m); snprintf(v, cap, "mode %s", m); return 1; }
    if (!strcmp(key, "drops")) { char d[32] = "0", pc[32]; kv_get(lp, "dropped", d, sizeof d); if (!kv_get(lp, "pct", pc, sizeof pc)) cfg_get("LIFE", "drop_pct", pc, sizeof pc); snprintf(v, cap, "dropped %s (%s%%)", d, pc); return 1; }
    if (kv_get(lp, key, v, cap)) return 1;
    return cfg_get("LIFE", key, v, cap); }
static void expand(const char *in, char *out, size_t cap) {      /* :name: and @name */
    out[0] = 0; size_t o = 0;
    for (const char *p = in; *p && o + 2 < cap; ) {
        int colon = (*p == ':' && isalpha((unsigned char)p[1])), at = (*p == '@' && isalpha((unsigned char)p[1]));
        if (colon || at) { char key[48]; size_t k = 0; const char *q = p + 1; while ((isalnum((unsigned char)*q) || *q == '_') && k < sizeof key - 1) key[k++] = *q++; key[k] = 0; char v[256];
            if ((!colon || *q == ':') && token_value(key, v, sizeof v)) { size_t vl = strlen(v); if (o + vl + 1 < cap) { memcpy(out + o, v, vl); o += vl; } p = colon ? q + 1 : q; continue; } }
        out[o++] = *p++; }
    out[o] = 0; }

static void need_actor(void) { if (!ACTOR[0]) { fprintf(stderr, "wo: no actor (state/life_actor.txt is empty)\n"); exit(3); } }

static int v_kv(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: wo kv <game> <file> <key> <expr>\n"); return 1; } char file[256], key[64], expr[128], p[PATH_MAX], cur[128], nb[64];
    expand(argv[1], file, sizeof file); expand(argv[2], key, sizeof key); expand(argv[3], expr, sizeof expr); spath(p, file);
    if (!strncmp(expr, "add:", 4)) memmove(expr + 1, expr + 4, strlen(expr + 4) + 1), expr[0] = '+';
    else if (!strncmp(expr, "sub:", 4)) memmove(expr + 1, expr + 4, strlen(expr + 4) + 1), expr[0] = '-';
    else if (!strncmp(expr, "set:", 4)) memmove(expr + 1, expr + 4, strlen(expr + 4) + 1), expr[0] = '=';
    if (expr[0] == '+' || expr[0] == '-') { kv_get(p, key, cur, sizeof cur); snprintf(nb, sizeof nb, "%d", atoi(cur) + atoi(expr)); kv_set(p, key, nb); printf("%s=%s\n", key, nb); return 0; }
    if (expr[0] == '=') { const char *v = expr + 1; if (!strcmp(v, "mhp") || !strcmp(v, "@mhp")) { char pf[128]; int mh = 0; if (!actor_row(ACTOR, pf, sizeof pf, &mh)) { fprintf(stderr, "wo: %s has no ACTOR row in life.pdl\n", ACTOR); return 3; } snprintf(nb, sizeof nb, "%d", mh); v = nb; }
        kv_set(p, key, v); printf("%s=%s\n", key, v); return 0; }
    fprintf(stderr, "wo kv: expr must be add:N sub:N set:V (or +N -N =V)\n"); return 1; }

static int v_drop(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: wo drop <game> <actor> <pct|cfg> <src>\n"); return 1; } need_actor();
    char pf[128]; int mh; if (!actor_row(ACTOR, pf, sizeof pf, &mh)) { fprintf(stderr, "wo: %s has no ACTOR row in life.pdl\n", ACTOR); return 3; }
    int pct; { char t[32]; if (!strcmp(argv[2], "cfg")) { cfg_get("LIFE", "drop_pct", t, sizeof t); pct = atoi(t); } else pct = atoi(argv[2]); }
    char lp[PATH_MAX], pp[PATH_MAX], dp[PATH_MAX], inv[400], room[48], xs[16], ys[16]; char nm[96]; snprintf(nm, sizeof nm, "life_%s.txt", ACTOR); spath(lp, nm); spath(pp, pf); spath(dp, "drops.txt");
    kv_get(lp, "inv", inv, sizeof inv); kv_get(pp, "room", room, sizeof room); kv_get(pp, "x", xs, sizeof xs); kv_get(pp, "y", ys, sizeof ys);
    char rows[64 * 100 + 64]; read_file(dp, rows, sizeof rows); int nrows = 0, maxid = 1; for (char *c = rows; *c; c++) if (*c == '\n') nrows++;
    { char cp[sizeof rows]; snprintf(cp, sizeof cp, "%s", rows); char *svd = NULL; for (char *l = strtok_r(cp, "\n", &svd); l; l = strtok_r(NULL, "\n", &svd)) { int id = atoi(l); if (id >= maxid) maxid = id + 1; } }
    char newinv[400] = "", add[64 * 100] = ""; int dropped = 0; char cp[400]; snprintf(cp, sizeof cp, "%s", inv);
    char *svi = NULL; for (char *t = strtok_r(cp, ",", &svi); t; t = strtok_r(NULL, ",", &svi)) { char *c = strchr(t, ':'); if (!c) continue; char k[24]; snprintf(k, sizeof k, "%.*s", (int)(c - t), t); int cnt = atoi(c + 1), d = cnt * pct / 100;
        if (d > 0 && nrows < 64) { char r[160]; snprintf(r, sizeof r, "%d|%s|%s|%s|%d|%s|0|0|%s\n", maxid++, room, xs, ys, d, k, argv[3]); strncat(add, r, sizeof add - strlen(add) - 1); nrows++; dropped += d; } else d = 0;
        if (cnt - d > 0) { size_t o = strlen(newinv); snprintf(newinv + o, sizeof newinv - o, "%s%s:%d", o ? "," : "", k, cnt - d); } }
    strncat(rows, add, sizeof rows - strlen(rows) - 1); write_if_changed(dp, rows); kv_set(lp, "inv", newinv);
    char wl[PATH_MAX], wb[200]; spath(wl, "wo_last.txt"); snprintf(wb, sizeof wb, "dropped=%d\npct=%d\npos=%s,%s,%s\n", dropped, pct, room, xs, ys); write_if_changed(wl, wb);
    printf("dropped %d (%d%%) at %s,%s,%s\n", dropped, pct, room, xs, ys); return 0; }

static int v_place(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: wo place <game> <actor> <room> <x> <y>\n"); return 1; } need_actor();
    char pf[128]; int mh; if (!actor_row(ACTOR, pf, sizeof pf, &mh)) { fprintf(stderr, "wo: %s has no ACTOR row in life.pdl\n", ACTOR); return 3; }
    char room[64], x[32], y[32], pp[PATH_MAX]; expand(argv[2], room, sizeof room); expand(argv[3], x, sizeof x); expand(argv[4], y, sizeof y); spath(pp, pf);
    kv_set(pp, "room", room); kv_set(pp, "x", x); kv_set(pp, "y", y); kv_set(pp, "tx", "-1"); kv_set(pp, "ty", "-1"); printf("%s on %s,%s,%s\n", ACTOR, room, x, y); return 0; }

static int v_log(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: wo log <game> <KIND> <f1> [f2] [f3]\n"); return 1; } need_actor();
    char row[1000]; snprintf(row, sizeof row, "%s|%ld|%s", argv[1], (long)time(NULL), ACTOR);
    for (int i = 2; i < argc && i < 5; i++) { char f[300]; expand(argv[i], f, sizeof f); if (!f[0]) continue; size_t o = strlen(row); snprintf(row + o, sizeof row - o, "|%s", f); }
    char ep[PATH_MAX]; spath(ep, "events.txt"); append_line(ep, row); printf("%s\n", row); return 0; }

static int v_when(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: wo when <game> <ROW.key> <value> <event>\n"); return 1; } char row[48], key[48]; const char *dot = strchr(argv[1], '.');
    if (!dot) { fprintf(stderr, "wo when: first field is ROW.key\n"); return 1; } snprintf(row, sizeof row, "%.*s", (int)(dot - argv[1]), argv[1]); snprintf(key, sizeof key, "%s", dot + 1);
    char v[128]; if (!cfg_get(row, key, v, sizeof v) || strcmp(v, argv[2])) { printf("%s.%s is %s, not %s: %s does not play\n", row, key, v[0] ? v : "unset", argv[2], argv[3]); return 0; }
    char house[PATH_MAX], pl[PATH_MAX], pd[PATH_MAX], cmd[4 * PATH_MAX], wd[PATH_MAX]; snprintf(house, sizeof house, "%s", GAME); char *s = strrchr(house, '/'); if (s) *s = 0; s = strrchr(house, '/'); if (s) *s = 0;
    snprintf(pl, sizeof pl, "%s/&.widgits/events-hq/ops/play_event.sh", house); snprintf(pd, sizeof pd, "%s/events/%s", GAME, argv[3]); snprintf(wd, sizeof wd, "%s/state/world", GAME); mkdir(wd, 0755);
    if (access(pl, F_OK) || access(pd, F_OK)) { fprintf(stderr, "wo when: no player or no page %s\n", pd); return 3; }
    snprintf(cmd, sizeof cmd, "MUCHI_TARGET_ENT='%s' timeout 20 bash '%s' '%s' '%s' parallel >/dev/null 2>&1", wd, pl, pd, house); int rc = system(cmd);
    printf("%s.%s = %s: played %s (exit %d)\n", row, key, v, argv[3], WIFEXITED(rc) ? WEXITSTATUS(rc) : -1); return 0; }


/* ---- world verbs (B2: the destroyed page) ---- */
static void last_set(const char *k, const char *v) { char wl[PATH_MAX]; spath(wl, "wo_last.txt"); kv_set(wl, k, v); }
static int split_row(char *l, char **f, int max) { int n = 0; char *sv = NULL; for (char *t = strtok_r(l, "|", &sv); t && n < max; t = strtok_r(NULL, "|", &sv)) f[n++] = trim(t); return n; }

/* wo pending <game> <file>   state/<file> first row  kind|room|x|y|src  ->  :kind: :room: :x: :y: :src: */
static int v_pending(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: wo pending <game> <file>\n"); return 1; } char p[PATH_MAX], b[256], *f[6]; spath(p, argv[1]); read_file(p, b, sizeof b); b[strcspn(b, "\n")] = 0;
    int n = split_row(b, f, 6); if (n < 4) { printf("nothing pending in %s\n", argv[1]); return 4; }
    last_set("kind", f[0]); last_set("room", f[1]); last_set("x", f[2]); last_set("y", f[3]); last_set("src", n > 4 ? f[4] : "?"); printf("pending %s at %s,%s,%s\n", f[0], f[1], f[2], f[3]); return 0; }

/* wo remnant <game> <kind> <room> <x> <y>   the BLOCK row (blocks.pdl: BLOCK|kind|hardness|drop|count|damaged_pct|...) becomes a remnant row in drops.txt */
static int v_remnant(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: wo remnant <game> <kind> <room> <x> <y>\n"); return 1; }
    char kind[48], room[48], x[16], y[16]; expand(argv[1], kind, sizeof kind); expand(argv[2], room, sizeof room); expand(argv[3], x, sizeof x); expand(argv[4], y, sizeof y);
    char bp[PATH_MAX], bb[BIG * 2]; snprintf(bp, sizeof bp, "%s/blocks.pdl", GAME); read_file(bp, bb, sizeof bb); char drop[32] = ""; int cnt = 0, dpct = 0; char *sv = NULL;
    for (char *l = strtok_r(bb, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { if (l[0] == '#') continue; char c[300], *f[8]; snprintf(c, sizeof c, "%s", l); int n = split_row(c, f, 8); if (n >= 6 && !strcmp(f[0], "BLOCK") && !strcmp(f[1], kind)) { snprintf(drop, sizeof drop, "%s", f[3]); cnt = atoi(f[4]); dpct = atoi(f[5]); break; } }
    int made = 0, dm = 0; if (cnt > 0) { char dp[PATH_MAX], rows[64 * 100 + 64]; spath(dp, "drops.txt"); read_file(dp, rows, sizeof rows); int nrows = 0, id = 1; char cp[sizeof rows]; snprintf(cp, sizeof cp, "%s", rows); char *sv2 = NULL;
        for (char *l = strtok_r(cp, "\n", &sv2); l; l = strtok_r(NULL, "\n", &sv2)) { nrows++; if (atoi(l) >= id) id = atoi(l) + 1; }
        if (nrows < 64) { srand((unsigned)(time(NULL) ^ ((long)getpid() << 7))); dm = (rand() % 100) < dpct; char r[160]; snprintf(r, sizeof r, "%d|%s|%s|%s|%d|%s|0|%d|%s\n", id, room, x, y, cnt, drop, dm, kind); strncat(rows, r, sizeof rows - strlen(rows) - 1); write_if_changed(dp, rows); made = cnt; } }
    last_set("made", made ? "1" : "0"); char t[96]; snprintf(t, sizeof t, "%s x%d%s", drop[0] ? drop : "-", made, dm ? " (damaged)" : ""); last_set("what", t); printf("%s\n", t); return 0; }

/* wo clear_damage <game> <room> <x> <y>   drop that cell's row from damage.txt (room|x|y|...) */
static int v_clear_damage(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: wo clear_damage <game> <room> <x> <y>\n"); return 1; } char room[48], x[16], y[16], p[PATH_MAX], b[1200], out[1200] = ""; expand(argv[1], room, sizeof room); expand(argv[2], x, sizeof x); expand(argv[3], y, sizeof y);
    spath(p, "damage.txt"); read_file(p, b, sizeof b); char *sv = NULL; for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char c[100], *f[5]; snprintf(c, sizeof c, "%s", l); if (split_row(c, f, 5) >= 4 && !strcmp(f[0], room) && atoi(f[1]) == atoi(x) && atoi(f[2]) == atoi(y)) continue; strncat(out, l, sizeof out - strlen(out) - 2); strcat(out, "\n"); }
    write_if_changed(p, out); printf("damage cleared at %s,%s,%s\n", room, x, y); return 0; }

/* wo node_use <game> <kind> <room> <x> <y>   a resource node (nodes.txt room|x|y|kind|amount) gives one load per break; endless (-1) never runs out; 0 removes it */
static int v_node_use(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: wo node_use <game> <kind> <room> <x> <y>\n"); return 1; } char kind[48], room[48], x[16], y[16], p[PATH_MAX], b[BIG], out[BIG] = ""; expand(argv[1], kind, sizeof kind); expand(argv[2], room, sizeof room); expand(argv[3], x, sizeof x); expand(argv[4], y, sizeof y);
    spath(p, "nodes.txt"); read_file(p, b, sizeof b); int hit = 0; char *sv = NULL;
    for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) { char c[160], *f[6]; snprintf(c, sizeof c, "%s", l); int n = split_row(c, f, 6); size_t o = strlen(out);
        if (n >= 5 && !strcmp(f[0], room) && atoi(f[1]) == atoi(x) && atoi(f[2]) == atoi(y) && !strcmp(f[3], kind) && atoi(f[4]) > 0) { int a = atoi(f[4]) - 1; hit = 1; if (a > 0) snprintf(out + o, sizeof out - o, "%s|%s|%s|%s|%d\n", f[0], f[1], f[2], f[3], a); }
        else snprintf(out + o, sizeof out - o, "%s\n", l); }
    if (hit) write_if_changed(p, out); printf(hit ? "node used\n" : "no node here\n"); return 0; }

/* wo event <game> <KIND> <f1> [f2] [f3] [f4]   like log but with no actor column: KIND|time|f1|f2... */
static int v_event(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: wo event <game> <KIND> <f1> ...\n"); return 1; } char row[1000]; snprintf(row, sizeof row, "%s|%ld", argv[1], (long)time(NULL));
    for (int i = 2; i < argc && i < 7; i++) { char f[300]; expand(argv[i], f, sizeof f); if (!f[0]) continue; for (char *c = f; *c; c++) if (*c == '/') *c = '|';      /* a node holds at most 4 values: "/" inside one makes several columns */
        size_t o = strlen(row); snprintf(row + o, sizeof row - o, "|%s", f); }
    char ep[PATH_MAX]; spath(ep, "events.txt"); append_line(ep, row); printf("%s\n", row); return 0; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: wo <verb> <game> ...  (kv drop place log when pending remnant clear_damage node_use event)\n"); return 1; }
    const char *verb = argv[1]; if (!realpath(argv[2], GAME)) { fprintf(stderr, "wo: no game folder %s\n", argv[2]); return 1; } resolve_actor();
    /* each verb sees argv[0] = the game folder, then its own arguments */
    if (!strcmp(verb, "kv")) return v_kv(argc - 2, argv + 2);
    if (!strcmp(verb, "drop")) { /* actor argument: @actor or an id */ char a[64]; expand(argv[3], a, sizeof a); if (a[0]) snprintf(ACTOR, sizeof ACTOR, "%s", a); return v_drop(argc - 2, argv + 2); }
    if (!strcmp(verb, "place")) { char a[64]; expand(argv[3], a, sizeof a); if (a[0]) snprintf(ACTOR, sizeof ACTOR, "%s", a); return v_place(argc - 2, argv + 2); }
    if (!strcmp(verb, "log")) return v_log(argc - 2, argv + 2);
    if (!strcmp(verb, "pending")) return v_pending(argc - 2, argv + 2);
    if (!strcmp(verb, "remnant")) return v_remnant(argc - 2, argv + 2);
    if (!strcmp(verb, "clear_damage")) return v_clear_damage(argc - 2, argv + 2);
    if (!strcmp(verb, "node_use")) return v_node_use(argc - 2, argv + 2);
    if (!strcmp(verb, "event")) return v_event(argc - 2, argv + 2);
    if (!strcmp(verb, "when")) return v_when(argc - 2, argv + 2);
    fprintf(stderr, "wo: unknown verb %s\n", verb); return 1; }
