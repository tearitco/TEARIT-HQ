/* rmmc_beat_op - the HARNESS / AI controller of Asa: plays rows of beats.pdl through the verb PAGES (rmmc_play.inc), refusing while the SEAT is human (plan 9, gate G5, D137).
 *   rmmc_beat_op <house> <beats.pdl> check          resolve every beat against the seed desk WITHOUT writing: simulates spawn/walk on the cells of <seed>/{cells.txt,map.txt,legend.txt}; exit 4 when a beat ends off the map,
 *                                                   over sky (glyph .) or on a tile other than its `expect` column; prints  beats=<n> ok
 *   rmmc_beat_op <house> <beats.pdl> next           play the beat after the one in play/beat.txt (a PREFERENCE: a bookmark that decides which row is next, never the game state), then bump it
 *   rmmc_beat_op <house> <beats.pdl> all            play every beat in order; stops at the first refusal and exits with its code
 *   rmmc_beat_op <house> <beats.pdl> <n>            play beat n only
 * Environment as rmmc_verb: RMMC_LEDGER, RMMC_DESK, RMMC_SEED, RMMC_ACTOR (default asa). Before it writes anything it asks vox_op for the seat: seat human -> exit 24, NOTHING written.
 * Output per beat:  beat <n>: <verb> <arg> x<count> ok wait=<seconds>   or   beat <n>: refused at step <k>: <reason>
 * Exit: 0 ok, 2 usage or bad row, 4 check failed, 24 seat is human, otherwise the refusing verb's code (11, 18, 14, ...).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_beat_op.+x rmmc_beat_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "rmmc_play.inc"
#define MAXBEAT 256
typedef struct { int n, count; char sec[16], verb[24], arg[48], expect[32]; } Beat;
static Beat B[MAXBEAT]; static int nbeat;
static int split(char *line, char **f, int max) {
    int n = 0; char *s = line;
    while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
    for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; }
    return n;
}
static int load_beats(const char *p) { FILE *f = fopen(p, "r"); if (!f) return 2; char l[512], *x[10];
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 10); if (n >= 7 && !strcmp(x[0], "BEAT")) { if (nbeat >= MAXBEAT) { fclose(f); return 2; } Beat *b = &B[nbeat++];
        b->n = atoi(x[1]); snprintf(b->sec, sizeof b->sec, "%s", x[2]); snprintf(b->verb, sizeof b->verb, "%s", x[3]); snprintf(b->arg, sizeof b->arg, "%s", x[4]); b->count = atoi(x[5]); snprintf(b->expect, sizeof b->expect, "%s", x[6]);
        if (b->count < 1 || b->count > 500 || !b->verb[0]) { fclose(f); return 2; } } }
    fclose(f); return nbeat ? 0 : 2; }
static char *slurp(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); char *b = malloc((size_t)n + 1); if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; } b[n] = 0; fclose(f); return b; }
/* check: the seed desk read independently of vox */
static int check(const char *seed) {
    char p[4400], l[1024], *x[8]; static char slot[64][32]; static int W, H; FILE *f; snprintf(p, sizeof p, "%s/legend.txt", seed); if (!(f = fopen(p, "r"))) { fprintf(stderr, "no legend in %s\n", seed); return 2; }
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 8); if (n >= 3 && !strcmp(x[0], "SLOT") && isdigit((unsigned char)x[1][0])) { int s = atoi(x[1]); if (s > 0 && s < 64) snprintf(slot[s], 32, "%s", x[2]); } } fclose(f);
    snprintf(p, sizeof p, "%s/cells.txt", seed); char *ct = slurp(p); snprintf(p, sizeof p, "%s/map.txt", seed); char *mp = slurp(p); if (!ct || !mp) { fprintf(stderr, "no cells or map in %s\n", seed); return 2; }
    char *q = strstr(ct, "width="); W = q ? atoi(q + 6) : 0; q = strstr(ct, "height="); H = q ? atoi(q + 7) : 0; char *cs = strstr(ct, "\ncells\n"); if (!cs || W < 1 || H < 1) return 2; cs += 7;
    static int floor_[256][256]; static char glyph[256][256]; if (W > 256 || H > 256) return 2; char *s = cs, *m = mp;
    for (int z = 0; z < H; z++) { char *me = strchr(m, '\n'); if (!me || (int)(me - m) < W) return 2; memcpy(glyph[z], m, (size_t)W); m = me + 1; for (int xx = 0; xx < W; xx++) { while (*s == ' ' || *s == '\n') s++; floor_[z][xx] = atoi(s); while (*s && *s != ' ' && *s != '\n') s++; } }
    int cx = -1, cz = -1, bad = 0;
    for (int i = 0; i < nbeat && !bad; i++) { Beat *b = &B[i];
        if (!strcmp(b->verb, "spawn")) { if (sscanf(b->arg, "%d %d", &cx, &cz) != 2) { fprintf(stderr, "beat %d: spawn needs x z\n", b->n); return 4; } }
        else if (!strcmp(b->verb, "walk")) { if (cx < 0) { fprintf(stderr, "beat %d: walk before spawn\n", b->n); return 4; } for (int k = 0; k < b->count; k++) { switch (b->arg[0]) { case 'n': cz--; break; case 's': cz++; break; case 'e': cx++; break; case 'w': cx--; break; default: fprintf(stderr, "beat %d: bad direction\n", b->n); return 4; }
            if (cx < 0 || cz < 0 || cx >= W || cz >= H || glyph[cz][cx] == '.') { fprintf(stderr, "beat %d: step %d is air or off the map at %d,%d\n", b->n, k + 1, cx, cz); bad = 1; break; } } }
        if (!bad) { if (cx < 0 || cz < 0 || cx >= W || cz >= H || glyph[cz][cx] == '.') { fprintf(stderr, "beat %d: ends over air at %d,%d\n", b->n, cx, cz); bad = 1; }
            else if (strcmp(b->expect, "-") && strcmp(slot[floor_[cz][cx]], b->expect)) { fprintf(stderr, "beat %d: cell %d,%d is %s, expected %s\n", b->n, cx, cz, slot[floor_[cz][cx]], b->expect); bad = 1; } } }
    free(ct); free(mp); if (bad) return 4; printf("beats=%d ok\n", nbeat); return 0;
}
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: rmmc_beat_op <house> <beats.pdl> check|next|all|<n>\n"); return 2; }
    const char *house = argv[1], *mode = argv[3]; if (load_beats(argv[2])) { fprintf(stderr, "rmmc_beat_op: bad beats file\n"); return 2; }
    static char book[PATH_MAX + 32], desk[PATH_MAX + 160], seed[PATH_MAX + 200], led[PATH_MAX + 128], vop[PATH_MAX + 64], data[PATH_MAX + 64], bk[PATH_MAX + 96];
    snprintf(book, sizeof book, "%s/@.apps/rmmc", house); snprintf(vop, sizeof vop, "%s/@.apps/vox/ops/+x/vox_op.+x", house); snprintf(data, sizeof data, "%s/vox", book);
    const char *e = getenv("RMMC_DESK"); if (e && *e) snprintf(desk, sizeof desk, "%s", e); else snprintf(desk, sizeof desk, "%s/rmmc-pchq/pieces/system/maps/rmmc/rmmc-0001", book);
    e = getenv("RMMC_SEED"); if (e && *e) snprintf(seed, sizeof seed, "%s", e); else snprintf(seed, sizeof seed, "%s/seed", desk);
    e = getenv("RMMC_LEDGER"); if (e && *e) snprintf(led, sizeof led, "%s", e); else snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/rmmc/game/ledger.txt", house);
    const char *actor = getenv("RMMC_ACTOR"); if (!actor || !*actor) actor = "asa";
    if (!strcmp(mode, "check")) return check(seed);
    int from, to; snprintf(bk, sizeof bk, "%s/play/beat.txt", book);
    if (!strcmp(mode, "all")) { from = 0; to = nbeat - 1; }
    else if (!strcmp(mode, "next")) { int last = 0; FILE *f = fopen(bk, "r"); if (f) { if (fscanf(f, "%d", &last) != 1) last = 0; fclose(f); } from = -1; for (int i = 0; i < nbeat; i++) if (B[i].n > last) { from = i; break; } if (from < 0) { printf("no beat after %d\n", last); return 0; } to = from; }
    else if (isdigit((unsigned char)mode[0])) { int n = atoi(mode); from = -1; for (int i = 0; i < nbeat; i++) if (B[i].n == n) from = i; if (from < 0) { fprintf(stderr, "no beat %d\n", n); return 2; } to = from; }
    else { fprintf(stderr, "usage: ... check|next|all|<n>\n"); return 2; }
    { char out[200] = ""; char *av[] = { vop, "seat", data, led, (char *)actor, NULL }; int p[2]; if (pipe(p)) return 2; pid_t c = fork(); if (c == 0) { dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(vop, av); _exit(127); }
      close(p[1]); ssize_t k = read(p[0], out, sizeof out - 1); if (k < 0) k = 0; out[k] = 0; close(p[0]); int st; waitpid(c, &st, 0); out[strcspn(out, "\r\n")] = 0;
      if (!strcmp(out, "seat=human")) { fprintf(stderr, "refused: the seat of %s is human (exit 24, nothing written)\n", actor); return 24; } }
    for (int i = from; i <= to; i++) { Beat *b = &B[i]; char arg[80]; snprintf(arg, sizeof arg, "%s", !strcmp(b->arg, "-") ? "" : b->arg); char msg[500];
        for (int k = 0; k < b->count; k++) { int code = rmmc_play_verb(house, book, b->verb, arg, "harness", msg, sizeof msg);
            if (code) { printf("beat %d: refused at step %d: %s\n", b->n, k + 1, msg); return code; } }
        printf("beat %d: %s %s x%d ok wait=%s\n", b->n, b->verb, b->arg, b->count, b->sec);
        { char dd[PATH_MAX + 96]; snprintf(dd, sizeof dd, "%s/play", book); mkdir(dd, 0755); FILE *f = fopen(bk, "w"); if (f) { fprintf(f, "%d\n", b->n); fclose(f); } } }
    return 0;
}
