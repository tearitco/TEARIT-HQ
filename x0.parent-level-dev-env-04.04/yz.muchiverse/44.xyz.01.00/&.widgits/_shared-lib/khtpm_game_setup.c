/* khtpm_game_setup.c - reader for a game's setup file, sessions/<id>/game.pdl (one per db session = one per game).
 * Design: #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/GAME-SETUP-PDL-DESIGN.md   Text-included (pure logic, no I/O beyond reading the one file), never linked.
 *
 * Rows (pipe-delimited, '#' comments, unknown rows ignored so the format can grow):
 *   GAME | title | <text>      GAME | start | <entity id>      GAME | body | <entity id>
 *   MAP  | <desk id> | available | 1
 *   CELL | <cell id> | <label> | modes=build,play,playtest | where=livedesk,pchq | order=<n> | cmd=<action>
 *   ROW  | <cell id> | <n> | <label> | cmd=<action>
 *   EDIT | <entity id or path> | playtest | 1
 *
 * Rules (chosen so an existing house with NO game.pdl behaves exactly as before):
 *   - mode "build" (and anything unknown): everything is available, visible and editable.
 *   - gs_map_available: restricts only if the file lists at least one MAP row; then only listed desks are allowed in play/playtest.
 *   - gs_cell_visible: a cell that has no CELL row is visible (built-in headers are unchanged); a listed cell is visible only when its modes= (default all three)
 *     contains the mode AND its where= (default all) contains the place. A game can therefore HIDE a built-in header, not rename it.
 *   - gs_edit_allowed: build yes, play no, playtest only for ids/paths with an EDIT row. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* every function is static and may be unused by a given includer: no -Wunused-function noise */
#define GS_FN static __attribute__((unused))
#define GS_MAX_MAPS 64
#define GS_MAX_CELLS 64
#define GS_MAX_ROWS 256
#define GS_MAX_EDITS 256
#define GS_STR 256

typedef struct { char id[64], label[GS_STR], modes[64], where[64], cmd[GS_STR]; int order; } GsCell;
typedef struct { char cell[64], label[GS_STR], cmd[GS_STR]; int n; } GsRow;
typedef struct {
    int loaded;                                   /* 1 once a file was read */
    char title[GS_STR], start[64], body[64];
    char maps[GS_MAX_MAPS][64]; int n_maps;
    GsCell cells[GS_MAX_CELLS]; int n_cells;
    GsRow rows[GS_MAX_ROWS]; int n_rows;
    char edits[GS_MAX_EDITS][GS_STR]; int n_edits;
} GameSetup;

GS_FN char *gs_trim(char *s) {
    char *e; while (*s == ' ' || *s == '\t') s++;
    e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}
GS_FN void gs_copy(char *d, size_t n, const char *s) { snprintf(d, n, "%s", s); }
/* "key=value" option lookup among the trailing fields */
GS_FN const char *gs_opt(char **f, int nf, int from, const char *key) {
    size_t kl = strlen(key);
    for (int i = from; i < nf; i++) if (!strncmp(f[i], key, kl) && f[i][kl] == '=') return f[i] + kl + 1;
    return NULL;
}
/* is `word` one of the comma-separated items of `list` */
GS_FN int gs_list_has(const char *list, const char *word) {
    size_t wl = strlen(word); const char *p = list;
    while (*p) {
        const char *e = strchr(p, ','); size_t l = e ? (size_t)(e - p) : strlen(p);
        while (l && (*p == ' ')) { p++; l--; }
        while (l && p[l - 1] == ' ') l--;
        if (l == wl && !strncmp(p, word, l)) return 1;
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

/* Returns 0 when the file was read, 1 when it is missing/unreadable (g stays zeroed with loaded=0 = build defaults). */
GS_FN int gs_load(const char *path, GameSetup *g) {
    char line[1024]; FILE *f;
    memset(g, 0, sizeof(*g));
    if (!path || !(f = fopen(path, "r"))) return 1;
    g->loaded = 1;
    while (fgets(line, sizeof(line), f)) {
        char *f_[12]; int nf = 0; char *p = gs_trim(line), *q;
        if (!*p || *p == '#') continue;
        for (q = p; nf < 12; ) { char *bar = strchr(q, '|'); f_[nf++] = q; if (!bar) break; *bar = 0; q = bar + 1; }
        for (int i = 0; i < nf; i++) f_[i] = gs_trim(f_[i]);
        if (!strcmp(f_[0], "GAME") && nf >= 3) {
            if (!strcmp(f_[1], "title")) gs_copy(g->title, sizeof g->title, f_[2]);
            else if (!strcmp(f_[1], "start")) gs_copy(g->start, sizeof g->start, f_[2]);
            else if (!strcmp(f_[1], "body")) gs_copy(g->body, sizeof g->body, f_[2]);
        } else if (!strcmp(f_[0], "MAP") && nf >= 4 && !strcmp(f_[2], "available")) {
            if (atoi(f_[3]) && f_[1][0] && g->n_maps < GS_MAX_MAPS) gs_copy(g->maps[g->n_maps++], 64, f_[1]);
        } else if (!strcmp(f_[0], "CELL") && nf >= 3 && f_[1][0] && g->n_cells < GS_MAX_CELLS) {
            GsCell *c = &g->cells[g->n_cells++]; const char *v;
            memset(c, 0, sizeof *c);
            gs_copy(c->id, sizeof c->id, f_[1]); gs_copy(c->label, sizeof c->label, f_[2]);
            gs_copy(c->modes, sizeof c->modes, (v = gs_opt(f_, nf, 3, "modes")) ? v : "build,play,playtest");
            gs_copy(c->where, sizeof c->where, (v = gs_opt(f_, nf, 3, "where")) ? v : "livedesk,pchq");
            gs_copy(c->cmd, sizeof c->cmd, (v = gs_opt(f_, nf, 3, "cmd")) ? v : "");
            c->order = (v = gs_opt(f_, nf, 3, "order")) ? atoi(v) : 0;
        } else if (!strcmp(f_[0], "ROW") && nf >= 4 && f_[1][0] && g->n_rows < GS_MAX_ROWS) {
            GsRow *r = &g->rows[g->n_rows++]; const char *v;
            memset(r, 0, sizeof *r);
            gs_copy(r->cell, sizeof r->cell, f_[1]); r->n = atoi(f_[2]); gs_copy(r->label, sizeof r->label, f_[3]);
            gs_copy(r->cmd, sizeof r->cmd, (v = gs_opt(f_, nf, 4, "cmd")) ? v : "");
        } else if (!strcmp(f_[0], "EDIT") && nf >= 4 && !strcmp(f_[2], "playtest")) {
            if (atoi(f_[3]) && f_[1][0] && g->n_edits < GS_MAX_EDITS) gs_copy(g->edits[g->n_edits++], GS_STR, f_[1]);
        }
    }
    fclose(f);
    return 0;
}

GS_FN int gs_is_play(const char *mode) { return mode && (!strcmp(mode, "play") || !strcmp(mode, "playtest")); }

GS_FN int gs_map_available(const GameSetup *g, const char *mode, const char *desk) {
    if (!gs_is_play(mode) || !g->loaded || g->n_maps == 0) return 1;
    for (int i = 0; i < g->n_maps; i++) if (!strcmp(g->maps[i], desk)) return 1;
    return 0;
}
GS_FN int gs_cell_visible(const GameSetup *g, const char *mode, const char *place, const char *cell_id) {
    if (!gs_is_play(mode) || !g->loaded) return 1;
    for (int i = 0; i < g->n_cells; i++) {
        if (strcmp(g->cells[i].id, cell_id)) continue;
        return gs_list_has(g->cells[i].modes, mode) && (!place || gs_list_has(g->cells[i].where, place));
    }
    return 1;
}
GS_FN int gs_edit_allowed(const GameSetup *g, const char *mode, const char *id_or_path) {
    if (!mode || !strcmp(mode, "play")) return 0;
    if (strcmp(mode, "playtest")) return 1;           /* build / unknown */
    for (int i = 0; i < g->n_edits; i++) if (!strcmp(g->edits[i], id_or_path)) return 1;
    return 0;
}

/* ---- one-call helpers for the real switch points (mr_transfer_desk, the taskbar menu) ---- */

/* The house-wide play flag: #.desktop/khtpm_play_mode.state.txt. Format (every older reader keeps working, because they only test the FIRST line for "mode=on"):
 *     mode=on|off          on = the game runs (events fire); off = build
 *     playtest=1           optional second line, only meaningful with mode=on: editing is also allowed (play-test)
 * Returns "playtest" for mode=on + playtest=1, "play" for mode=on, else "build". Missing file = build. Every writer rewrites the whole file, so toggling or stopping play
 * (which writes only `mode=...`) clears playtest by itself. */
GS_FN const char *gs_current_mode(const char *house_root) {
    char p[4400], l[128]; FILE *f; int on = 0, pt = 0;
    snprintf(p, sizeof p, "%s/#.desktop/khtpm_play_mode.state.txt", house_root);
    if ((f = fopen(p, "r"))) {
        while (fgets(l, sizeof l, f)) {
            char *v = gs_trim(l);
            if (!strcmp(v, "mode=on")) on = 1;
            else if (!strcmp(v, "playtest=1")) pt = 1;
        }
        fclose(f);
    }
    return on ? (pt ? "playtest" : "play") : "build";
}

/* May the player switch to desk `target` right now? 1 = yes, 0 = refused (reason filled). Refuses ONLY when the house is in a play mode AND sess_dir/game.pdl lists at least
 * one available MAP and `target` is not among them; every other case (build mode, no game.pdl, no MAP rows) allows, so existing houses behave exactly as before.
 * A refusal appends one line to #.desktop/game_access_ledger.txt: <epoch_ms>|refused-map|<mode>|<target>|<session dir>. */
GS_FN int gs_check_map_switch(const char *house_root, const char *sess_dir, const char *target, char *reason, size_t rn) {
    const char *mode = gs_current_mode(house_root); char gp[4400]; GameSetup *g; int ok;
    if (reason && rn) reason[0] = 0;
    if (!gs_is_play(mode)) return 1;
    snprintf(gp, sizeof gp, "%s/game.pdl", sess_dir);
    g = (GameSetup *)malloc(sizeof *g);
    if (!g) return 1;                                      /* cannot check: never lock the player out */
    if (gs_load(gp, g) != 0) { free(g); return 1; }
    ok = gs_map_available(g, mode, target);
    free(g);
    if (!ok) {
        char lp[4400]; FILE *f; struct timespec ts;
        if (reason && rn) snprintf(reason, rn, "map '%s' is not available in %s", target, mode);
        snprintf(lp, sizeof lp, "%s/#.desktop/game_access_ledger.txt", house_root);
        clock_gettime(CLOCK_REALTIME, &ts);
        if ((f = fopen(lp, "a"))) { fprintf(f, "%lld|refused-map|%s|%s|%s\n", (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000, mode, target, sess_dir); fclose(f); }
    }
    return ok;
}
