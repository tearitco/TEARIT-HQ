/* fridge_op - the ship's fridge as an OP (owner 2026-10-28: "lets add some food seeking, and a refrigerator on the ship, with lots of different food in its inventory"). Slow work belongs in an op, not in
 * the game's tick: the game only walks a hungry pet to the fridge and, when it stands next to it, runs `eat` (fork/exec, watchdog). Design: BOOK 16.game/AI-SIDE-ANSWERS-BLOCK-A-2026-10-28.md A2/A3 (a need must change a choice).
 *
 * Usage:  fridge_op status <state_dir> <app_dir>                  the stock, one `food=<key> n=<n> nourish=<v>` line per kind, then kinds= total=
 *         fridge_op stock  <state_dir> <app_dir> <item> <n>       put n (> 0) of a food INTO the fridge (the food must be in foods.pdl)
 *         fridge_op eat    <state_dir> <app_dir> <id>             the character <id> eats ONE food from the fridge: hunger goes down by its nourishment (not below 0)
 *         fridge_op where  <app_dir>                              the fridge's room, cell, picture and the two hunger limits from foods.pdl
 *   <app_dir>/foods.pdl  (re-read on every call, no rebuild):
 *       FOOD   | key | label | nourish | what it is           nourish = how much hunger (0..100) one portion removes (1..100)
 *       STOCK  | key | n                                      what a NEW fridge holds; <state_dir>/fridge.txt is seeded from these rows ONCE, when the file does not exist (delete it to reseed; an EMPTY file = an empty fridge)
 *       FRIDGE | room | quarters   FRIDGE | x | 1   FRIDGE | y | 5   FRIDGE | tile | SF_Inside_B 8 6 1 2     where it stands (cells local to the ship footprint) and its picture (sheet col row w h)
 *       FRIDGE | seek_hunger | 40    a character at least this hungry walks to the fridge (read by the game)
 *       FRIDGE | eat_hunger  | 25    `eat` is refused under this hunger
 *   <state_dir>/fridge.txt   key=n lines (op-owned).   <state_dir>/life_<id>.txt   the game's life file: only its `hunger=` line is changed (the game re-reads life files every 1.5 s; life_op edits the same file the same way).
 * WHICH food (a law, not a weight): the stocked kind with the LARGEST nourishment that is <= the current hunger (no waste); when every stocked kind is bigger than the hunger, the smallest one. Ties: file order.
 * Refused, never partly applied (exit 1, `refused: <reason>`): unknown character (no life file), dead character, not hungry enough, empty fridge, unknown food, bad count. Exit 2 = usage or a missing/bad foods.pdl.
 * Every meal appends one ledger row to <state_dir>/events.txt:  EAT|<unix>|<id>|<food>|<nourish>|<hunger old>|<hunger new>|left=<n>   and prints  ate=<food> nourish=<v> hunger=<old>-><new> left=<n>.
 * Race note: a life file rewritten by another process in the same instant can be overwritten (rename is atomic, the read-modify-write is not); the same limit as life_op, accepted (the ledger row still shows the meal).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/fridge_op.+x fridge_op.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#define MAXF 64
typedef struct { char key[32], label[48]; int nourish, stock; } Food;
static Food F[MAXF]; static int nF;
static char g_room[24] = "quarters", g_tile[64] = "SF_Inside_B 8 6 1 2"; static int g_x = 1, g_y = 5, g_seek = 40, g_eat = 25;
static int g_seed_n[MAXF]; static char g_seed_key[MAXF][32]; static int g_nseed;

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int key_ok(const char *s) { size_t n = strlen(s); if (!n || n > 30) return 0; for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)s[i]) && s[i] != '_') return 0; return 1; }
static int find(const char *k) { for (int i = 0; i < nF; i++) if (!strcmp(F[i].key, k)) return i; return -1; }
static int whole(const char *s, int lo, int hi, int *out) { if (!*s) return 0; for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0; if (strlen(s) > 6) return 0; int v = atoi(s); if (v < lo || v > hi) return 0; *out = v; return 1; }

static int load_foods(const char *app) {      /* 1 ok, 0 unreadable or invalid (message on stderr) */
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/foods.pdl", app); FILE *f = fopen(p, "r"); if (!f) { fprintf(stderr, "fridge_op: cannot read %s\n", p); return 0; }
    char ln[512]; nF = 0; g_nseed = 0;
    while (fgets(ln, sizeof ln, f)) {
        char *t = trim(ln); if (!*t || *t == '#') continue;
        char *fld[8]; int nf = 0; char *q = t; while (nf < 8) { fld[nf++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; } for (int i = 0; i < nf; i++) fld[i] = trim(fld[i]);
        if (!strcmp(fld[0], "FOOD") && nf >= 4) { int v; if (!key_ok(fld[1]) || !whole(fld[3], 1, 100, &v) || nF >= MAXF || find(fld[1]) >= 0) { fprintf(stderr, "fridge_op: bad or duplicate FOOD row: %s\n", fld[1]); fclose(f); return 0; }
            snprintf(F[nF].key, sizeof F[nF].key, "%s", fld[1]); snprintf(F[nF].label, sizeof F[nF].label, "%s", fld[2]); F[nF].nourish = v; F[nF].stock = 0; nF++; }
        else if (!strcmp(fld[0], "STOCK") && nf >= 3) { int v; if (key_ok(fld[1]) && whole(fld[2], 0, 100000, &v) && g_nseed < MAXF) { snprintf(g_seed_key[g_nseed], 32, "%s", fld[1]); g_seed_n[g_nseed++] = v; } }
        else if (!strcmp(fld[0], "FRIDGE") && nf >= 3) { int v;
            if (!strcmp(fld[1], "room") && key_ok(fld[2])) snprintf(g_room, sizeof g_room, "%s", fld[2]);
            else if (!strcmp(fld[1], "x") && whole(fld[2], 0, 15, &v)) g_x = v; else if (!strcmp(fld[1], "y") && whole(fld[2], 0, 15, &v)) g_y = v;
            else if (!strcmp(fld[1], "tile")) snprintf(g_tile, sizeof g_tile, "%s", fld[2]);
            else if (!strcmp(fld[1], "seek_hunger") && whole(fld[2], 0, 100, &v)) g_seek = v; else if (!strcmp(fld[1], "eat_hunger") && whole(fld[2], 0, 100, &v)) g_eat = v; }
    }
    fclose(f); if (!nF) { fprintf(stderr, "fridge_op: %s has no FOOD rows\n", p); return 0; } return 1;
}
static void stock_path(const char *st, char *p, size_t n) { snprintf(p, n, "%s/fridge.txt", st); }
static int load_stock(const char *st) {      /* seeds the file from the STOCK rows when it does not exist; fills F[].stock */
    char p[PATH_MAX]; stock_path(st, p, sizeof p); struct stat sb; int seeded = 0;
    if (stat(p, &sb) != 0) { FILE *w = fopen(p, "w"); if (!w) { fprintf(stderr, "fridge_op: cannot write %s\n", p); return -1; } for (int i = 0; i < g_nseed; i++) if (find(g_seed_key[i]) >= 0 && g_seed_n[i] > 0) fprintf(w, "%s=%d\n", g_seed_key[i], g_seed_n[i]); fclose(w); seeded = 1; }
    FILE *f = fopen(p, "r"); if (!f) return -1; char ln[128];
    while (fgets(ln, sizeof ln, f)) { char *e = strchr(ln, '='); if (!e) continue; *e = 0; int i = find(trim(ln)); int v; if (i >= 0 && whole(trim(e + 1), 0, 100000, &v)) F[i].stock = v; }
    fclose(f); return seeded;
}
static int save_stock(const char *st) {
    char p[PATH_MAX], tp[PATH_MAX + 8]; stock_path(st, p, sizeof p); snprintf(tp, sizeof tp, "%s.tmp", p); FILE *w = fopen(tp, "w"); if (!w) return 0;
    for (int i = 0; i < nF; i++) if (F[i].stock > 0) fprintf(w, "%s=%d\n", F[i].key, F[i].stock);
    if (fflush(w) || fsync(fileno(w))) { fclose(w); return 0; } fclose(w); return rename(tp, p) == 0;
}
static int refuse(const char *why) { printf("refused: %s\n", why); return 1; }

int main(int argc, char **argv) {
    if (argc < 3) goto usage;
    const char *verb = argv[1];
    if (!strcmp(verb, "where")) { if (argc != 3) goto usage; if (!load_foods(argv[2])) return 2; printf("room=%s x=%d y=%d tile=%s seek_hunger=%d eat_hunger=%d foods=%d\n", g_room, g_x, g_y, g_tile, g_seek, g_eat, nF); return 0; }
    if (argc < 4) goto usage;
    const char *st = argv[2], *app = argv[3];
    if (!load_foods(app)) return 2;
    int seeded = load_stock(st); if (seeded < 0) return 2;
    if (!strcmp(verb, "status")) {
        int kinds = 0, total = 0;
        for (int i = 0; i < nF; i++) if (F[i].stock > 0) { kinds++; total += F[i].stock; }
        printf("kinds=%d total=%d seeded=%d\n", kinds, total, seeded);      /* the summary FIRST: a game verb shows only the start of a long reply */
        for (int i = 0; i < nF; i++) if (F[i].stock > 0) printf("food=%s n=%d nourish=%d\n", F[i].key, F[i].stock, F[i].nourish);
        return 0; }
    if (!strcmp(verb, "stock")) { if (argc != 6) goto usage; int i = find(argv[4]), n; if (i < 0) return refuse("unknown food");
        if (!whole(argv[5], 1, 100000, &n)) return refuse("count must be a whole number of at least 1");
        F[i].stock += n; if (!save_stock(st)) { fprintf(stderr, "fridge_op: cannot write the stock\n"); return 2; } printf("stocked=%s n=%d now=%d\n", F[i].key, n, F[i].stock); return 0; }
    if (!strcmp(verb, "eat")) {
        if (argc != 5) goto usage;
        const char *id = argv[4];
        if (!key_ok(id)) return refuse("unknown character");
        char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/life_%s.txt", st, id); FILE *lf = fopen(lp, "r"); if (!lf) return refuse("unknown character (no life file)");
        char body[1200]; size_t bn = fread(body, 1, sizeof body - 1, lf); body[bn] = 0; fclose(lf);
        int hunger = -1, dead = 0; { char cp[1200]; snprintf(cp, sizeof cp, "%s", body); for (char *l = strtok(cp, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "hunger=", 7)) hunger = atoi(l + 7); else if (!strncmp(l, "dead=", 5)) dead = atoi(l + 5); } }
        if (hunger < 0) return refuse("the life file has no hunger line");
        if (dead) return refuse("dead characters do not eat");
        if (hunger < g_eat) { char w[96]; snprintf(w, sizeof w, "not hungry (hunger %d < %d)", hunger, g_eat); return refuse(w); }
        int pick = -1, small = -1; for (int i = 0; i < nF; i++) { if (F[i].stock <= 0) continue; if (small < 0 || F[i].nourish < F[small].nourish) small = i; if (F[i].nourish <= hunger && (pick < 0 || F[i].nourish > F[pick].nourish)) pick = i; }
        if (small < 0) return refuse("the fridge is empty");
        if (pick < 0) pick = small;
        int nh = hunger - F[pick].nourish; if (nh < 0) nh = 0;
        char out[1400] = ""; size_t o = 0; int replaced = 0; { char cp[1200]; snprintf(cp, sizeof cp, "%s", body); for (char *l = strtok(cp, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "hunger=", 7)) { o += (size_t)snprintf(out + o, sizeof out - o, "hunger=%d\n", nh); replaced = 1; } else o += (size_t)snprintf(out + o, sizeof out - o, "%s\n", l); } }
        if (!replaced) return refuse("the life file has no hunger line");
        F[pick].stock--; if (!save_stock(st)) { fprintf(stderr, "fridge_op: cannot write the stock\n"); return 2; }      /* stock first: a crash between the two steps loses a food, never a meal's worth of hunger for free */
        char tp[PATH_MAX + 8]; snprintf(tp, sizeof tp, "%s.tmp", lp); FILE *w = fopen(tp, "w"); if (!w || fputs(out, w) == EOF || fflush(w) || fsync(fileno(w))) { if (w) fclose(w); fprintf(stderr, "fridge_op: cannot write the life file\n"); F[pick].stock++; save_stock(st); return 2; } fclose(w);
        if (rename(tp, lp) != 0) { fprintf(stderr, "fridge_op: cannot replace the life file\n"); F[pick].stock++; save_stock(st); return 2; }
        char ep[PATH_MAX], row[256]; snprintf(ep, sizeof ep, "%s/events.txt", st); snprintf(row, sizeof row, "EAT|%ld|%s|%s|%d|%d|%d|left=%d\n", (long)time(NULL), id, F[pick].key, F[pick].nourish, hunger, nh, F[pick].stock);
        int fd = open(ep, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd >= 0) { if (write(fd, row, strlen(row)) < 0) fprintf(stderr, "fridge_op: ledger write failed\n"); close(fd); }
        printf("ate=%s nourish=%d hunger=%d->%d left=%d\n", F[pick].key, F[pick].nourish, hunger, nh, F[pick].stock); return 0; }
usage:
    fprintf(stderr, "Usage: fridge_op status|stock|eat <state_dir> <app_dir> [item n | id]   |   fridge_op where <app_dir>\n"); return 2;
}
