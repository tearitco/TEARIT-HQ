/* page_probe_op - reads a board PAGE back from its bytes (cells.txt + cells.rgba) and checks it against the game's DATA, with its OWN arithmetic (it does not call the page writer's code).
 * Written 2026-11-09 for the case `chutes` (cs_page). Pure C, no Python. Build: gcc -std=gnu11 -O2 -o page_probe_op.+x page_probe_op.c   (the case compiles it from source into a scratch folder).
 *   page_probe_op all  <game_core> <app> <desk> <hex text colour> <state dir>   every square 1..N: the block at the serpentine cell of the square shows the label text the data asks for (the glyph pixels equal the label baker's mask, read through
 *                                                                      `game_core <app> <state> labelmask`): prints ALL checked=N same=S bad=B
 *   page_probe_op ends <app> <desk> <atlas> <hex text> <hex outline>   every jump end and every plain square: all pixels of the block that are not label ink equal the atlas tile the page.pdl TILEMAP row names
 *                                                                      (ladder_from/to, chute_from/to, floor_a/floor_b by the checker); prints ENDS checked=N ok=K bad=B wrong_tile_control=strong|weak (the same test against
 *                                                                      the wrong tile must fail hard) distinct=yes|no (the four jump tiles are four different pictures)
 * The board data it reads: <app>/rulesets/default/board.pdl (TRACK length, BOARD cols/rows/path, JUMP rows) and page.pdl (PAGE block/start, TILEMAP, JUMPTEXT, LABEL rows). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
static int LEN = 100, COLS = 10, ROWS = 10, SERP = 1, B = 3, PX = 24, ACOLS = 16, NJ, JF[64], JT[64]; static int TM[8] = { 1, 2, 7, 35, 38, 36, 37 }; static char JTX[4][120]; static struct { int n; char t[120]; } LBL[128]; static int NL;
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && strchr(" \t\r\n", e[-1])) *--e = 0; return s; }
static int split(char *l, char **f, int m) { int n = 0; char *p = l; while (n < m) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static void load(const char *app) {
    char p[4096], ln[1024], *f[8]; snprintf(p, sizeof p, "%s/rulesets/default/board.pdl", app); FILE *fp = fopen(p, "r"); if (!fp) { fprintf(stderr, "no board.pdl\n"); exit(2); }
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 8); if (n < 3) continue;
        if (!strcmp(f[0], "TRACK")) LEN = atoi(f[2]); else if (!strcmp(f[0], "BOARD") && !strcmp(f[1], "cols")) COLS = atoi(f[2]); else if (!strcmp(f[0], "BOARD") && !strcmp(f[1], "rows")) ROWS = atoi(f[2]); else if (!strcmp(f[0], "BOARD") && !strcmp(f[1], "path")) SERP = !strcmp(f[2], "serpentine");
        else if (!strcmp(f[0], "JUMP") && NJ < 64) { JF[NJ] = atoi(f[1]); JT[NJ] = atoi(f[2]); NJ++; } }
    fclose(fp); for (int i = 0; i < 4; i++) strcpy(JTX[i], "{n}");
    snprintf(p, sizeof p, "%s/rulesets/default/page.pdl", app); fp = fopen(p, "r"); if (!fp) { fprintf(stderr, "no page.pdl\n"); exit(2); }
    static const char *tn[] = { "floor_a", "floor_b", "start", "ladder_from", "ladder_to", "chute_from", "chute_to" }, *jn[] = { "ladder_from", "ladder_to", "chute_from", "chute_to" };
    while (fgets(ln, sizeof ln, fp)) { if (ln[0] == '#') continue; int n = split(ln, f, 8); if (n < 3) continue;
        if (!strcmp(f[0], "PAGE") && !strcmp(f[1], "block")) B = atoi(f[2]); else if (!strcmp(f[0], "HEADER") && !strcmp(f[1], "tile_px")) PX = atoi(f[2]); else if (!strcmp(f[0], "HEADER") && !strcmp(f[1], "atlas_cols")) ACOLS = atoi(f[2]);
        else if (!strcmp(f[0], "TILEMAP")) { for (int i = 0; i < 7; i++) if (!strcmp(f[1], tn[i])) TM[i] = atoi(f[2]); }
        else if (!strcmp(f[0], "JUMPTEXT")) { for (int i = 0; i < 4; i++) if (!strcmp(f[1], jn[i])) snprintf(JTX[i], sizeof JTX[i], "%s", f[2]); }
        else if (!strcmp(f[0], "LABEL") && NL < 128) { LBL[NL].n = atoi(f[1]); snprintf(LBL[NL].t, sizeof LBL[NL].t, "%s", f[2]); NL++; } }
    fclose(fp);
}
/* the square -> block rule, written again here on purpose: row from the bottom r = (n-1)/COLS; a serpentine board runs odd rows right to left */
static void where(int n, int *bx, int *by) { int i = n - 1, r = i / COLS, c = i % COLS; if (SERP && (r % 2)) c = COLS - 1 - c; *bx = c; *by = ROWS - 1 - r; }
static void expect_text(int n, char *o, size_t cap) {
    for (int j = NL - 1; j >= 0; j--) if (LBL[j].n == n) { snprintf(o, cap, "%s", LBL[j].t); return; }
    int cls = -1, from = 0, to = 0; for (int i = 0; i < NJ; i++) if (JF[i] == n) { cls = JT[i] > JF[i] ? 0 : 2; from = JF[i]; to = JT[i]; break; }
    if (cls < 0) for (int i = 0; i < NJ; i++) if (JT[i] == n) { cls = JT[i] > JF[i] ? 1 : 3; from = JF[i]; to = JT[i]; break; }
    const char *t = cls < 0 ? "{n}" : JTX[cls]; size_t w = 0; o[0] = 0;
    while (*t && w + 12 < cap) { if (!strncmp(t, "{n}", 3)) { w += (size_t)sprintf(o + w, "%d", n); t += 3; } else if (!strncmp(t, "{to}", 4)) { w += (size_t)sprintf(o + w, "%d", to); t += 4; } else if (!strncmp(t, "{from}", 6)) { w += (size_t)sprintf(o + w, "%d", from); t += 6; } else o[w++] = *t++; }
    o[w] = 0;
}
static int W, H, NTILE; static int *CT; static unsigned char *AT; static int ATW, ATH;
static void load_desk(const char *desk, const char *atlas) {
    char p[4096], ln[16384]; snprintf(p, sizeof p, "%s/cells.txt", desk); FILE *f = fopen(p, "r"); if (!f) { fprintf(stderr, "no cells.txt\n"); exit(2); } int y = -1; W = H = 0;
    while (fgets(ln, sizeof ln, f)) { if (!strncmp(ln, "width=", 6)) W = atoi(ln + 6); else if (!strncmp(ln, "height=", 7)) { H = atoi(ln + 7); CT = calloc((size_t)W * H, sizeof(int)); } else if (!strncmp(ln, "tile_px=", 8)) PX = atoi(ln + 8); else if (!strncmp(ln, "atlas_cols=", 11)) ACOLS = atoi(ln + 11); else if (!strncmp(ln, "atlas_tiles=", 12)) NTILE = atoi(ln + 12);
        else if (!strncmp(ln, "cells", 5)) y = 0; else if (y >= 0 && y < H) { int x = 0; for (char *t = strtok(ln, " \n"); t && x < W; t = strtok(NULL, " \n"), x++) CT[y * W + x] = atoi(t); y++; } }
    fclose(f); if (y != H || !CT) { fprintf(stderr, "cells.txt incomplete\n"); exit(2); }
    snprintf(p, sizeof p, "%s", atlas); f = fopen(p, "rb"); if (!f) { fprintf(stderr, "no atlas\n"); exit(2); } fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f); AT = malloc((size_t)sz); if (fread(AT, 1, (size_t)sz, f) != (size_t)sz) exit(2); fclose(f); ATW = ACOLS * PX; ATH = (int)(sz / ((long)ATW * 4));
}
static unsigned px_at(int tile, int x, int y) { const unsigned char *q = AT + (((size_t)(tile / ACOLS) * PX + y) * ATW + (size_t)(tile % ACOLS) * PX + x) * 4; return (unsigned)q[0] << 16 | (unsigned)q[1] << 8 | q[2]; }
static unsigned block_px(int bx, int by, int x, int y) { int cx = bx * B + x / PX, cy = by * B + y / PX; return px_at(CT[cy * W + cx], x % PX, y % PX); }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: page_probe_op all|ends|ink ...\n"); return 2; }
    if (!strcmp(argv[1], "all") && argc == 7) {
        const char *gc = argv[2], *app = argv[3], *desk = argv[4]; unsigned tc = (unsigned)strtoul(argv[5], NULL, 16); char atl[4096]; snprintf(atl, sizeof atl, "%s/cells.rgba", desk); (void)argv[6]; load(app); load_desk(desk, atl); int A = B * PX, same = 0, bad = 0;
        for (int n = 1; n <= LEN; n++) { int bx, by; where(n, &bx, &by); char tx[200]; expect_text(n, tx, sizeof tx);
            int pf[2]; if (pipe(pf)) return 2; pid_t pid = fork(); if (pid == 0) { dup2(pf[1], 1); close(pf[0]); close(pf[1]); char As[16]; snprintf(As, sizeof As, "%d", A); execl(gc, gc, app, argv[6], "labelmask", As, tx, "--art", (char *)NULL); _exit(127); }
            close(pf[1]); static char art[100000]; size_t on = 0; ssize_t k; while ((k = read(pf[0], art + on, sizeof art - 1 - on)) > 0) on += (size_t)k; art[on] = 0; close(pf[0]); int st; waitpid(pid, &st, 0);
            char *nl = strchr(art, '\n'); if (!nl) { bad++; continue; } char *rows = nl + 1; int diff = 0;
            for (int y = 0; y < A; y++) for (int x = 0; x < A; x++) { int want = rows[y * (A + 1) + x] == '#'; int got = block_px(bx, by, x, y) == tc && 1; if (want != got) diff++; }
            if (diff == 0) same++; else bad++; }
        printf("ALL checked=%d same=%d bad=%d\n", LEN, same, bad); return 0; }
    if (!strcmp(argv[1], "ends") && argc == 7) {
        const char *app = argv[2], *desk = argv[3], *atlas = argv[4]; unsigned tc = (unsigned)strtoul(argv[5], NULL, 16), oc = (unsigned)strtoul(argv[6], NULL, 16); load(app); char cr[4096]; snprintf(cr, sizeof cr, "%s/cells.rgba", desk); load_desk(desk, cr);
        /* the base atlas (the 43 tiles the page started from) is the file the page was made from: read it separately */
        unsigned char *page_at = AT; int page_atw = ATW; FILE *f = fopen(atlas, "rb"); if (!f) { fprintf(stderr, "no base atlas\n"); return 2; } fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f); unsigned char *base = malloc((size_t)sz); if (fread(base, 1, (size_t)sz, f) != (size_t)sz) return 2; fclose(f); (void)page_at; (void)page_atw;
        int A = B * PX, ok = 0, bad = 0, ctl = 0; int cls[1100]; memset(cls, 0, sizeof cls); /* 0 floor, 1 lf, 2 lt, 3 cf, 4 ct */
        for (int i = 0; i < NJ; i++) { if (JT[i] > JF[i]) { cls[JT[i]] = 2; } else cls[JT[i]] = 4; } for (int i = 0; i < NJ; i++) cls[JF[i]] = JT[i] > JF[i] ? 1 : 3;
        for (int n = 1; n <= LEN; n++) { int bx, by; where(n, &bx, &by); int r = (n - 1) / COLS; int tile = cls[n] == 0 ? (((bx + r) & 1) ? TM[1] : TM[0]) : cls[n] == 1 ? TM[3] : cls[n] == 2 ? TM[4] : cls[n] == 3 ? TM[5] : TM[6];
            int wrong = cls[n] == 0 ? TM[3] : cls[n] <= 2 ? TM[5] : TM[3]; int mism = 0, wm = 0;
            for (int y = 0; y < A; y++) for (int x = 0; x < A; x++) { unsigned p = block_px(bx, by, x, y); if (p == tc || p == oc) continue; const unsigned char *q = base + (((size_t)(tile / ACOLS) * PX + (size_t)(y % PX)) * ((size_t)ACOLS * PX) + (size_t)(tile % ACOLS) * PX + (size_t)(x % PX)) * 4; unsigned e = (unsigned)q[0] << 16 | (unsigned)q[1] << 8 | q[2]; if (p != e) mism++;
                const unsigned char *q2 = base + (((size_t)(wrong / ACOLS) * PX + (size_t)(y % PX)) * ((size_t)ACOLS * PX) + (size_t)(wrong % ACOLS) * PX + (size_t)(x % PX)) * 4; unsigned e2 = (unsigned)q2[0] << 16 | (unsigned)q2[1] << 8 | q2[2]; if (p != e2) wm++; }
            if (mism == 0) ok++; else bad++; if (n == 1 || n == 16) ctl += wm; }
        int dist = 1; for (int a = 3; a < 7; a++) for (int b = a + 1; b < 7; b++) { int same = 1; for (int y = 0; y < PX && same; y++) for (int x = 0; x < PX; x++) { const unsigned char *qa = base + (((size_t)(TM[a] / ACOLS) * PX + y) * ((size_t)ACOLS * PX) + (size_t)(TM[a] % ACOLS) * PX + x) * 4, *qb = base + (((size_t)(TM[b] / ACOLS) * PX + y) * ((size_t)ACOLS * PX) + (size_t)(TM[b] % ACOLS) * PX + x) * 4; if (memcmp(qa, qb, 3)) { same = 0; break; } } if (same) dist = 0; }
        printf("ENDS checked=%d ok=%d bad=%d wrong_tile_control=%s distinct=%s\n", LEN, ok, bad, ctl > 1000 ? "strong" : "weak", dist ? "yes" : "no"); return 0; }
    fprintf(stderr, "usage: page_probe_op all|ends ...\n"); return 2;
}
