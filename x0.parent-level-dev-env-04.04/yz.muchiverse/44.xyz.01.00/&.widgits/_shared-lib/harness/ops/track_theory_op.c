/* track_theory_op - the EXACT theory of a race game on a track (the ledger kind of game_core: TRACK / JUMP rows in board.pdl, RULE finish in rules.pdl, the house die 1..6), for the Chutes and Ladders cases (2026-11-09).
 * A harness op, read-only, no files written, no process started. It reads the SAME data files the engine reads and computes by linear algebra and dynamic programming what the engine can only sample.
 *
 *   track_theory_op sanity <board.pdl>                       data checks of the jump table (every jump starts on 1..L-1 and lands on 1..L, no jump to itself, one jump per square, a ladder goes up and a chute goes down,
 *                                                            no jump starts where another ends, no loop); prints `SANITY ok jumps=N ladders=A chutes=B` or `SANITY FAIL <why>` (exit 1)
 *   track_theory_op metric <board.pdl> <rules.pdl> <name> [--players N] [--seat K] [--faces F] [--finish reach|exact_stay|bounce]
 *                                                            ONE number (6 decimals) per call. name: mean_turns var_turns median_turns p90_turns mean_rounds mean_rolls p_win_seat total_win_prob p_turns_over (--over T)
 *   track_theory_op table <board.pdl> <rules.pdl> [--faces F] [--finish X]     CSV `square,expected_turns` for every resting square (the shape of data-hq fixtures/chutes_markov.csv, solved by Gaussian elimination)
 *   track_theory_op sim <board.pdl> <rules.pdl> <seed0> <count> <players> <name> [--finish X]   plays <count> games with the ENGINE'S dice (splitmix64 die(seed, roll_index)) for seeds seed0.. and prints ONE number:
 *                                                            name: mean_rounds mean_rolls sd_rounds share_seat1.. (e.g. share_seat2) shortest_rounds longest_rounds
 *   track_theory_op game <board.pdl> <rules.pdl> <seed> <players>   one game, printed exactly as game_core prints `autoplay`: `AUTOPLAY events=E rounds=R over=1 winner=A` (names A, B, C, D joined in order)
 *
 * MODEL (the engine's roll(), game_core.c): from square s a roll d (uniform 1..F, F = 6 in the engine) goes to s+d; past the end: reach = win, exact_stay = stay on s, bounce = walk the surplus back from the end;
 * then, if the landing square is below the end and is the foot of a jump, move to its top; reaching the end wins. n players take turns in join order; seat k wins at its t-th turn exactly when every earlier seat has not finished
 * in t turns and every later seat has not finished in t-1: P(seat k wins at t) = f(t) S(t)^(k-1) S(t-1)^(n-k), f(t) = P(T = t), S(t) = P(T > t) for ONE player. Mass beyond 1e-13 is cut (reported by `metric tail`).
 * Exit: 0 ok, 1 a sanity failure, 2 usage / bad data. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/track_theory_op.+x track_theory_op.c -lm */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define MAXL 1000
#define MAXJ 64
#define TMAX 200000
static int L = 30, NJ, JF[MAXJ], JT[MAXJ]; static char JK[MAXJ][16]; static int FINISH = 0, FACES = 6;   /* 0 reach 1 exact_stay 2 bounce */
static double f_[TMAX + 2], S_[TMAX + 2]; static int TN;   /* f_[t] = P(T = t), S_[t] = P(T > t) for one player, t = 0..TN */

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int read_board(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "cannot read %s\n", path); return 2; } char ln[1024], *fl[10]; NJ = 0;
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; int n = split(ln, fl, 10); if (n < 3) continue;
        if (!strcmp(fl[0], "TRACK") && !strcmp(fl[1], "length")) L = atoi(fl[2]);
        else if (!strcmp(fl[0], "JUMP") && NJ < MAXJ) { JF[NJ] = atoi(fl[1]); JT[NJ] = atoi(fl[2]); snprintf(JK[NJ], sizeof JK[NJ], "%s", n >= 4 ? fl[3] : ""); NJ++; } }
    fclose(f); if (L < 3 || L > MAXL) { fprintf(stderr, "TRACK length %d is outside 3..%d\n", L, MAXL); return 2; } return 0;
}
static void read_rules(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return; char ln[1024], *fl[10];
    while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; int n = split(ln, fl, 10); if (n >= 3 && !strcmp(fl[0], "RULE") && !strcmp(fl[1], "finish")) FINISH = !strcmp(fl[2], "exact_stay") ? 1 : !strcmp(fl[2], "bounce") ? 2 : 0; }
    fclose(f);
}
static int jump_at(int sq) { for (int i = 0; i < NJ; i++) if (JF[i] == sq) return JT[i]; return -1; }
static int sanity(void) {
    int la = 0, ch = 0;
    for (int i = 0; i < NJ; i++) {
        if (JF[i] < 1 || JF[i] > L - 1) { printf("SANITY FAIL jump %d>%d starts outside 1..%d\n", JF[i], JT[i], L - 1); return 1; }
        if (JT[i] < 1 || JT[i] > L) { printf("SANITY FAIL jump %d>%d lands outside 1..%d\n", JF[i], JT[i], L); return 1; }
        if (JF[i] == JT[i]) { printf("SANITY FAIL jump %d>%d is a jump to itself\n", JF[i], JT[i]); return 1; }
        for (int j = i + 1; j < NJ; j++) if (JF[i] == JF[j]) { printf("SANITY FAIL square %d is the foot of two jumps\n", JF[i]); return 1; }
        if (!strcmp(JK[i], "ladder") && JT[i] < JF[i]) { printf("SANITY FAIL ladder %d>%d goes down\n", JF[i], JT[i]); return 1; }
        if (!strcmp(JK[i], "chute") && JT[i] > JF[i]) { printf("SANITY FAIL chute %d>%d goes up\n", JF[i], JT[i]); return 1; }
        if (JT[i] > JF[i]) la++; else ch++;
        for (int j = 0; j < NJ; j++) if (JT[i] == JF[j]) { printf("SANITY FAIL jump %d>%d lands on the foot of jump %d>%d (a chain)\n", JF[i], JT[i], JF[j], JT[j]); return 1; }
    }
    for (int i = 0; i < NJ; i++) { int s = JF[i], steps = 0; while (jump_at(s) >= 0 && steps <= NJ + 1) { s = jump_at(s); steps++; } if (steps > NJ) { printf("SANITY FAIL a loop of jumps starts at %d\n", JF[i]); return 1; } }
    printf("SANITY ok jumps=%d ladders=%d chutes=%d\n", NJ, la, ch); return 0;
}
/* where a roll d from square s ends: -1 = the end (win), else the resting square */
static int next_sq(int s, int d) {
    int np = s + d, bounced = 0;
    if (np > L) { if (FINISH == 1) return s; if (FINISH == 2) { np = L - (np - L); bounced = 1; } }
    (void)bounced; if (np < L) { int j = jump_at(np); if (j >= 0) np = j; }
    return np >= L ? -1 : np;
}
static double TAIL;
static void one_player(void) {   /* f_, S_ by iterating the position distribution turn by turn */
    static double cur[MAXL + 1], nxt[MAXL + 1]; for (int i = 0; i <= L; i++) cur[i] = 0; cur[0] = 1; double rem = 1; S_[0] = 1; f_[0] = 0; TN = 0;
    for (int t = 1; t <= TMAX && rem > 1e-13; t++) { for (int i = 0; i <= L; i++) nxt[i] = 0; double fin = 0;
        for (int s = 0; s < L; s++) { if (cur[s] == 0) continue; for (int d = 1; d <= FACES; d++) { int ns = next_sq(s, d); double p = cur[s] / FACES; if (ns < 0) fin += p; else nxt[ns] += p; } }
        memcpy(cur, nxt, sizeof cur); rem -= fin; if (rem < 0) rem = 0; f_[t] = fin; S_[t] = rem; TN = t; }
    TAIL = rem;
}
/* n players: P(seat k wins at round t) */
static double pw(int n, int k, int t) { return f_[t] * pow(S_[t], k - 1) * pow(S_[t - 1], n - k); }
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static int metric(const char *name, int n, int seat, int over) {
    one_player();
    if (!strcmp(name, "mean_turns")) { double m = 0; for (int t = 1; t <= TN; t++) m += t * f_[t]; printf("%.6f\n", m); return 0; }
    if (!strcmp(name, "var_turns")) { double m = 0, m2 = 0; for (int t = 1; t <= TN; t++) { m += t * f_[t]; m2 += (double)t * t * f_[t]; } printf("%.6f\n", m2 - m * m); return 0; }
    if (!strcmp(name, "median_turns") || !strcmp(name, "p90_turns")) { double q = name[0] == 'm' ? 0.5 : 0.9, c = 0; for (int t = 1; t <= TN; t++) { c += f_[t]; if (c >= q) { printf("%d.000000\n", t); return 0; } } printf("%d.000000\n", TN); return 0; }
    if (!strcmp(name, "tail")) { printf("%.6e\n", TAIL); return 0; }
    if (!strcmp(name, "p_turns_over")) { printf("%.6f\n", over >= 0 && over <= TN ? S_[over] : 0.0); return 0; }
    if (!strcmp(name, "mean_rounds")) { double m = 0; for (int t = 1; t <= TN; t++) for (int k = 1; k <= n; k++) m += t * pw(n, k, t); printf("%.6f\n", m); return 0; }
    if (!strcmp(name, "mean_rolls")) { double m = 0; for (int t = 1; t <= TN; t++) for (int k = 1; k <= n; k++) m += pw(n, k, t) * ((double)(t - 1) * n + k); printf("%.6f\n", m); return 0; }
    if (!strcmp(name, "p_win_seat")) { if (seat < 1 || seat > n) { fprintf(stderr, "p_win_seat needs --seat 1..players\n"); return 2; } double m = 0; for (int t = 1; t <= TN; t++) m += pw(n, seat, t); printf("%.6f\n", m); return 0; }
    if (!strcmp(name, "total_win_prob")) { double m = 0; for (int k = 1; k <= n; k++) for (int t = 1; t <= TN; t++) m += pw(n, k, t); printf("%.6f\n", m); return 0; }
    (void)cmp_d; fprintf(stderr, "unknown metric %s\n", name); return 2;
}
static int table(void) {   /* expected turns to finish from every resting square: (I - Q) E = 1, Gaussian elimination with partial pivoting */
    int N = L; static double A[MAXL][MAXL + 1]; for (int i = 0; i < N; i++) for (int j = 0; j <= N; j++) A[i][j] = 0;
    for (int s = 0; s < N; s++) { A[s][s] += 1; A[s][N] = 1; for (int d = 1; d <= FACES; d++) { int ns = next_sq(s, d); if (ns >= 0) A[s][ns] -= 1.0 / FACES; } }
    for (int c = 0; c < N; c++) { int p = c; for (int r = c + 1; r < N; r++) if (fabs(A[r][c]) > fabs(A[p][c])) p = r; if (fabs(A[p][c]) < 1e-14) { fprintf(stderr, "singular: the finish is unreachable from square %d\n", c); return 2; }
        if (p != c) for (int j = 0; j <= N; j++) { double t = A[c][j]; A[c][j] = A[p][j]; A[p][j] = t; }
        for (int r = 0; r < N; r++) if (r != c && A[r][c] != 0) { double m = A[r][c] / A[c][c]; for (int j = c; j <= N; j++) A[r][j] -= m * A[c][j]; } }
    printf("square,expected_turns\n"); for (int s = 0; s < N; s++) { if (s > 0 && s < L) { int foot = 0; for (int i = 0; i < NJ; i++) if (JF[i] == s) foot = 1; if (foot) continue; } printf("%d,%.6f\n", s, A[s][N] / A[s][s]); }
    return 0;
}
/* the engine's dice (game_core.c): splitmix64, die(seed, idx, k) = mix(seed*1000003 + idx*16 + k) % 6 + 1 */
static uint64_t mix(uint64_t x) { x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31); }
static int die6(long sd, long idx) { return (int)(mix((uint64_t)sd * 1000003ULL + (uint64_t)idx * 16) % 6) + 1; }
static void play(long seed, int n, long *rolls, long *rounds, int *winner) {
    int pos[8] = { 0 }; long idx = 0; int cur = 0;
    for (;;) { int d = die6(seed, idx++), from = pos[cur], np = from + d;
        if (np > L && FINISH == 1) np = from; else { if (np > L && FINISH == 2) np = L - (np - L); if (np < L) { int j = jump_at(np); if (j >= 0) np = j; } }
        if (np >= L) { *rolls = idx; *rounds = (idx - 1) / n + 1; *winner = cur; return; }
        pos[cur] = np; cur = (cur + 1) % n; if (idx > 5000000) { *rolls = idx; *rounds = -1; *winner = -1; return; } }
}
static int simulate(long seed0, long count, int n, const char *name) {
    double sr = 0, sr2 = 0, sl = 0; long wins[8] = { 0 }, lo = 1L << 40, hi = 0;
    for (long i = 0; i < count; i++) { long rolls, rounds; int w; play(seed0 + i, n, &rolls, &rounds, &w); if (w < 0) { fprintf(stderr, "a game did not end\n"); return 2; } sr += (double)rounds; sr2 += (double)rounds * rounds; sl += (double)rolls; wins[w]++; if (rounds < lo) lo = rounds; if (rounds > hi) hi = rounds; }
    double m = sr / count; if (!strcmp(name, "mean_rounds")) printf("%.6f\n", m); else if (!strcmp(name, "mean_rolls")) printf("%.6f\n", sl / count);
    else if (!strcmp(name, "sd_rounds")) printf("%.6f\n", sqrt(sr2 / count - m * m)); else if (!strncmp(name, "share_seat", 10)) { int k = atoi(name + 10); if (k < 1 || k > n) { fprintf(stderr, "share_seatK: K is 1..players\n"); return 2; } printf("%.6f\n", (double)wins[k - 1] / count); }
    else if (!strcmp(name, "shortest_rounds")) printf("%ld\n", lo); else if (!strcmp(name, "longest_rounds")) printf("%ld\n", hi); else { fprintf(stderr, "unknown sim metric %s\n", name); return 2; }
    return 0;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: track_theory_op sanity|metric|table|sim|game ...   (see the header of ops/track_theory_op.c)\n"); return 2; }
    const char *cmd = argv[1]; if (read_board(argv[2])) return 2;
    if (!strcmp(cmd, "sanity")) return sanity();
    if (argc < 4) { fprintf(stderr, "need <rules.pdl>\n"); return 2; } read_rules(argv[3]);
    int n = 4, seat = 0, over = -1; const char *pos[8]; int np = 0;
    for (int i = 4; i < argc; i++) { if (!strcmp(argv[i], "--players") && i + 1 < argc) n = atoi(argv[++i]); else if (!strcmp(argv[i], "--seat") && i + 1 < argc) seat = atoi(argv[++i]); else if (!strcmp(argv[i], "--faces") && i + 1 < argc) FACES = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--over") && i + 1 < argc) over = atoi(argv[++i]); else if (!strcmp(argv[i], "--finish") && i + 1 < argc) { i++; FINISH = !strcmp(argv[i], "exact_stay") ? 1 : !strcmp(argv[i], "bounce") ? 2 : 0; } else if (np < 8) pos[np++] = argv[i]; }
    if (FACES < 1 || FACES > 12 || n < 1 || n > 8) { fprintf(stderr, "bad --faces or --players\n"); return 2; }
    if (!strcmp(cmd, "metric")) { if (np < 1) { fprintf(stderr, "metric needs a name\n"); return 2; } return metric(pos[0], n, seat, over); }
    if (!strcmp(cmd, "table")) return table();
    if (!strcmp(cmd, "sim")) { if (np < 4) { fprintf(stderr, "sim <seed0> <count> <players> <name>\n"); return 2; } FACES = 6; return simulate(atol(pos[0]), atol(pos[1]), atoi(pos[2]), pos[3]); }
    if (!strcmp(cmd, "game")) { if (np < 2) { fprintf(stderr, "game <seed> <players>\n"); return 2; } int pl = atoi(pos[1]); long rolls, rounds; int w; play(atol(pos[0]), pl, &rolls, &rounds, &w); if (w < 0) return 2; printf("AUTOPLAY events=%ld rounds=%ld over=1 winner=%c\n", rolls + 1 + pl + 1, rounds, 'A' + w); return 0; }
    fprintf(stderr, "unknown command %s\n", cmd); return 2;
}
