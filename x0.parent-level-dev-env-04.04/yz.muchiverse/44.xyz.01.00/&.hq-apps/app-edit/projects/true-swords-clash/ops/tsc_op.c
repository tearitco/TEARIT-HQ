/* tsc_op.c - True Swords Clash rules, ONE op (2026-11-15). Rules: +.TSOTS-ALPHA-OMEGA/#.RULES/True_Swords_Clash_Rules.txt (HP 100, Mana 0; a turn is ONE action: a Mana Challenge or a Miracle;
 * statuses apply at the start of the owner's turn; win at HP 0 or by forfeit). Verses: ../verses.txt of the package (the Chinese-to-English Bible), or $TSC_VERSES.
 * STATE IS THE LEDGER ONLY (no ghost data): <match>/ledger.txt, one row per action:  START|seed|mode   DEAL|p|n|line   ANSWER|p|digits|ok|fail|text   AIMANA|p|n|ok|fail   MIRACLE|p|name|text   FORFEIT|p
 * Every row ends with the RESULT text; `state` REPLAYS the rows, recomputes every result and REFUSES the match (exit 3) when one differs (a hand-edited ledger). All chance (dice, coins, shuffles, the AI) is a hash of seed + row number, so a replay is byte-identical.
 *   tsc_op new <match> [--vs computer|human] [--seed N]    tsc_op state <match>    tsc_op deal <match> <n 2-10>    tsc_op answer <match> <digits>    tsc_op miracle <match> <name>    tsc_op forfeit <match>    tsc_op ai <match>
 * A challenge shows n consecutive verses of one chapter, shuffled, WITHOUT references; the answer is, for each verse in Bible order, the screen position holding it (0 = position 10).
 * Build: gcc -std=gnu11 -Wall -O2 -o +x/tsc_op.+x tsc_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#define P 4096
static char MATCH[P], LEDGER[P];
/* ---- verses ---- */
typedef struct { char *text; int chap; int ok; } Verse;
static Verse *V; static int NV;
static int load_verses(void) {
    char path[P]; const char *e = getenv("TSC_VERSES");
    if (e && *e) snprintf(path, sizeof path, "%s", e); else { char self[P]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return 0; self[n] = 0; char *s = strrchr(self, '/'); if (s) *s = 0; s = strrchr(self, '/'); if (s) *s = 0; s = strrchr(self, '/'); if (s) *s = 0; snprintf(path, sizeof path, "%s/verses.txt", self); }
    FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "tsc_op: no verses file %s\n", path); return 0; }
    static char ln[8192]; int cap = 0; int prev_key = -1, chap = 0; char prev_book[8] = ""; int prev_chap = -1;
    while (fgets(ln, sizeof ln, f)) {
        ln[strcspn(ln, "\r\n")] = 0; char *sp = strchr(ln, ' '); if (!sp) continue; char *bk = sp + 1; char *b2 = strstr(bk, "||"); if (!b2) continue; char book[8] = ""; snprintf(book, sizeof book, "%.*s", (int)(b2 - bk) > 6 ? 6 : (int)(b2 - bk), bk);
        char *c = b2 + 2; int ch = atoi(c); char *t3 = strstr(c, "|||"); if (!t3) continue; char *t2 = strstr(t3 + 3, "||"); if (!t2) continue; char *tx = t2 + 2; while (*tx == ' ') tx++;
        if (strcmp(book, prev_book) || ch != prev_chap) { chap++; snprintf(prev_book, sizeof prev_book, "%s", book); prev_chap = ch; }
        size_t L = strlen(tx); int ok = L >= 20 && L <= 130 && isalpha((unsigned char)tx[0]) && strchr(".?!;:", tx[L - 1]) != NULL;
        if (NV == cap) { cap = cap ? cap * 2 : 40000; V = realloc(V, (size_t)cap * sizeof *V); if (!V) return 0; }
        V[NV].text = strdup(tx); V[NV].chap = chap; V[NV].ok = ok; NV++; (void)prev_key;
    }
    fclose(f); return NV > 100;
}
/* ---- chance: a hash of seed, row number and a counter ---- */
static uint64_t SEED;
static uint32_t rnd(int seq, int k) { uint64_t z = SEED * 0x9E3779B97F4A7C15ULL + (uint64_t)seq * 0xBF58476D1CE4E5B9ULL + (uint64_t)(k + 1) * 0x94D049BB133111EBULL; z ^= z >> 30; z *= 0xBF58476D1CE4E5B9ULL; z ^= z >> 27; z *= 0x94D049BB133111EBULL; z ^= z >> 31; return (uint32_t)(z >> 16); }
/* ---- the match state, rebuilt from the rows ---- */
typedef struct { int hp, mana, bleed, poison, locust, bless; } Side;
static Side S[2]; static int TURN, OVER, WINNER = -1, SEQ, COMPUTER, PHASE;   /* PHASE 0 = choose, 1 = a deal waits for its answer */
static int CH_N, CH_START, CH_PERM[10];                                         /* CH_PERM[display position] = rank in Bible order */
static char LAST[400];
static const char *MNAME[11] = { "hailmary", "slash", "fire", "berserk", "heal", "judgement", "poison", "locust", "blessing", "angel", "holysword" };
static void side_text(const Side *s, char *o, size_t cap) { o[0] = 0; if (s->bleed) strncat(o, "bleeding ", cap - strlen(o) - 1); if (s->poison) strncat(o, "poison ", cap - strlen(o) - 1); if (s->locust) strncat(o, "locust ", cap - strlen(o) - 1); if (s->bless) strncat(o, "blessing ", cap - strlen(o) - 1); if (!o[0]) snprintf(o, cap, "none"); else o[strlen(o) - 1] = 0; }
static void end_turn(char *res, size_t cap) {      /* hand the turn over; the new current player takes the status effects at the START of their turn (rules) */
    TURN = 1 - TURN; Side *s = &S[TURN]; char add[160] = "";
    if (s->bleed) { s->hp -= 1; strcat(add, " bleeding-1hp"); } if (s->poison) { s->hp -= 2; strcat(add, " poison-2hp"); } if (s->locust) { s->mana = s->mana > 0 ? s->mana - 1 : 0; strcat(add, " locust-1mana"); } if (s->bless) { s->mana += 1; strcat(add, " blessing+1mana"); }
    if (add[0]) { size_t l = strlen(res); snprintf(res + l, cap - l, " | turn %d start:%s", TURN + 1, add); }
    if (s->hp <= 0) { OVER = 1; WINNER = 1 - TURN; size_t l = strlen(res); snprintf(res + l, cap - l, " | player %d falls: player %d wins", TURN + 1, WINNER + 1); }
}
static void clamp(void) { for (int i = 0; i < 2; i++) { if (S[i].hp > 100) S[i].hp = 100; if (S[i].hp < 0) S[i].hp = 0; } }
static void clear_neg(Side *s) { s->bleed = s->poison = s->locust = 0; }
static int cost_of(const char *name) { for (int i = 0; i < 11; i++) if (!strcmp(name, MNAME[i])) return i == 0 ? 0 : i; return -1; }
static void miracle(int p, const char *name, int seq, char *res, size_t cap) {      /* applies the effect; the caller checked the cost */
    Side *me = &S[p], *en = &S[1 - p]; int c = cost_of(name), d6 = (int)(rnd(seq, 0) % 6) + 1, coin = (int)(rnd(seq, 1) & 1); me->mana -= c;
    if (!strcmp(name, "hailmary")) { if (coin) { int m = me->mana; me->mana *= 2; snprintf(res, cap, "hail mary: heads, mana %d -> %d", m, me->mana); } else { snprintf(res, cap, "hail mary: tails, mana %d -> 0", me->mana); me->mana = 0; } }
    else if (!strcmp(name, "slash")) { en->hp -= 2; snprintf(res, cap, "slash: 2 damage, die %d%s", d6, d6 == 6 ? ", the enemy bleeds" : ""); if (d6 == 6) en->bleed = 1; }
    else if (!strcmp(name, "fire")) { en->hp -= d6; snprintf(res, cap, "fire: die %d, %d damage", d6, d6); }
    else if (!strcmp(name, "berserk")) { if (coin) { en->hp -= 13; snprintf(res, cap, "berserk: heads, 13 damage"); } else snprintf(res, cap, "berserk: tails, a miss"); }
    else if (!strcmp(name, "heal")) { me->hp += 10; clear_neg(me); snprintf(res, cap, "heal: +10 hp, bad statuses cleared"); }
    else if (!strcmp(name, "judgement")) { if (coin) { snprintf(res, cap, "judgement: heads, steals %d mana", en->mana); me->mana += en->mana; en->mana = 0; } else snprintf(res, cap, "judgement: tails, nothing"); }
    else if (!strcmp(name, "poison")) { en->poison = 1; snprintf(res, cap, "poison: the enemy is poisoned (-2 hp a turn)"); }
    else if (!strcmp(name, "locust")) { en->locust = 1; snprintf(res, cap, "locust: the enemy is swarmed (-1 mana a turn)"); }
    else if (!strcmp(name, "blessing")) { me->bless = 1; snprintf(res, cap, "blessing: +1 mana a turn"); }
    else if (!strcmp(name, "angel")) { en->hp -= 10; me->hp += 10; clear_neg(me); snprintf(res, cap, "angel: steals 10 hp, own bad statuses cleared"); }
    else { en->hp -= 25; snprintf(res, cap, "holy sword: 25 damage, die %d%s", d6, d6 == 6 ? ", the enemy bleeds" : ""); if (d6 == 6) en->bleed = 1; }
    clamp(); if (en->hp <= 0) { OVER = 1; WINNER = p; size_t l = strlen(res); snprintf(res + l, cap - l, " | player %d wins", p + 1); }
}
static void deal(int n, int seq) {      /* n consecutive eligible verses of one chapter, start picked by the hash; shuffled by the hash */
    for (int tries = 0; tries < 4000; tries++) { int st = (int)(rnd(seq, 10 + tries) % (uint32_t)(NV - n)); int good = 1; for (int i = 0; i < n && good; i++) if (!V[st + i].ok || V[st + i].chap != V[st].chap) good = 0; if (!good) continue;
        CH_N = n; CH_START = st; for (int i = 0; i < n; i++) CH_PERM[i] = i; for (int i = n - 1; i > 0; i--) { int j = (int)(rnd(seq, 5000 + i) % (uint32_t)(i + 1)); int t = CH_PERM[i]; CH_PERM[i] = CH_PERM[j]; CH_PERM[j] = t; }
        int same = 1; for (int i = 0; i < n; i++) if (CH_PERM[i] != i) same = 0; if (same) { int t = CH_PERM[0]; CH_PERM[0] = CH_PERM[1]; CH_PERM[1] = t; } return; }
    CH_N = 0;
}
static void answer_key(char *o) { for (int r = 0; r < CH_N; r++) for (int d = 0; d < CH_N; d++) if (CH_PERM[d] == r) { o[r] = (char)('0' + (d + 1) % 10); } o[CH_N] = 0; }
/* ONE row interpreter for replay AND live: returns 0 ok; res = the result text for the row (compared on replay) */
static int apply(const char *kind, int p, const char *a1, const char *a2, int seq, char *res, size_t cap) {
    res[0] = 0;
    if (!strcmp(kind, "DEAL")) { int n = atoi(a1); if (n < 2 || n > 10) return 1; deal(n, seq); if (!CH_N) return 1; PHASE = 1; snprintf(res, cap, "%d verses dealt at line %d", n, CH_START); return 0; }
    if (!strcmp(kind, "ANSWER")) { char key[16]; answer_key(key); int okk = !strcmp(a1, key); char ref[80]; snprintf(ref, sizeof ref, "%.60s", V[CH_START].text); PHASE = 0;
        if (okk) { S[p].mana += CH_N; snprintf(res, cap, "right: +%d mana (from: %s...)", CH_N, ref); } else snprintf(res, cap, "wrong: no mana, the turn ends (the order was %s)", key);
        end_turn(res, cap); return 0; }
    if (!strcmp(kind, "AIMANA")) { int n = atoi(a1); int win = (int)(rnd(seq, 0) % 100) < 62; if (win) { S[p].mana += n; snprintf(res, cap, "the computer sorts %d verses: +%d mana", n, n); } else snprintf(res, cap, "the computer fails %d verses: no mana", n); end_turn(res, cap); return 0; }
    if (!strcmp(kind, "MIRACLE")) { int c = cost_of(a1); if (c < 0 || S[p].mana < c) return 1; miracle(p, a1, seq, res, cap); if (!OVER) end_turn(res, cap); return 0; }
    if (!strcmp(kind, "FORFEIT")) { OVER = 1; WINNER = 1 - p; snprintf(res, cap, "player %d forfeits: player %d wins", p + 1, WINNER + 1); return 0; }
    (void)a2; return 1;
}
static void init_state(void) { memset(S, 0, sizeof S); S[0].hp = S[1].hp = 100; TURN = 0; OVER = 0; WINNER = -1; PHASE = 0; CH_N = 0; LAST[0] = 0; }
/* replay: 0 = ok; 3 = a row is wrong */
static int replay(int quiet) {
    FILE *f = fopen(LEDGER, "r"); if (!f) { fprintf(stderr, "tsc_op: no ledger %s\n", LEDGER); return 2; }
    static char ln[2048]; SEQ = 0; init_state(); int started = 0, rc = 0;
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue; char *fl[6] = { 0 }; int nf = 0; char *p = ln; while (nf < 6) { fl[nf++] = p; char *bar = strchr(p, '|'); if (!bar) break; *bar = 0; p = bar + 1; }
        int seq = ++SEQ;
        if (!strcmp(fl[0], "START")) { if (started || nf < 3) { rc = 3; break; } SEED = strtoull(fl[1], NULL, 10); COMPUTER = !strcmp(fl[2], "computer"); started = 1; continue; }
        if (!started || OVER || nf < 3) { rc = 3; break; }
        char res[400]; int pl = atoi(fl[1]) - 1; int kind_ok;
        if (pl != TURN) { rc = 3; break; }
        if (!strcmp(fl[0], "DEAL") && nf >= 4) { kind_ok = !apply("DEAL", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "ANSWER") && nf >= 4 && PHASE == 1) { kind_ok = !apply("ANSWER", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "AIMANA") && nf >= 4) { kind_ok = !apply("AIMANA", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "MIRACLE") && nf >= 4 && PHASE == 0) { kind_ok = !apply("MIRACLE", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "FORFEIT")) { kind_ok = !apply("FORFEIT", pl, "", "", seq, res, sizeof res); }
        else { rc = 3; break; }
        if (!kind_ok || strcmp(res, fl[nf - 1])) { rc = 3; if (!quiet) fprintf(stderr, "tsc_op: ledger row %d does not replay (stored: %s | computed: %s)\n", seq, fl[nf - 1], res); break; }
        snprintf(LAST, sizeof LAST, "%s", res); }
    fclose(f); if (!started && !rc) rc = 3; return rc;
}
static int append(const char *row) { FILE *f = fopen(LEDGER, "a"); if (!f) return 0; fprintf(f, "%s\n", row); fclose(f); return 1; }
static int cmd_state(void) {
    int rc = replay(0); if (rc) return rc; char st[2][100]; side_text(&S[0], st[0], sizeof st[0]); side_text(&S[1], st[1], sizeof st[1]);
    printf("mode=%s\nturn=%d\nphase=%s\nwinner=%d\nseq=%d\n", COMPUTER ? "computer" : "human", TURN + 1, OVER ? "over" : PHASE ? "answer" : "choose", WINNER + 1, SEQ);
    for (int i = 0; i < 2; i++) printf("p%d_hp=%d\np%d_mana=%d\np%d_status=%s\n", i + 1, S[i].hp, i + 1, S[i].mana, i + 1, st[i]);
    printf("last=%s\n", LAST); if (PHASE == 1 && !OVER) { printf("challenge_n=%d\n", CH_N); for (int d = 0; d < CH_N; d++) printf("ch_%d=%s\n", d + 1, V[CH_START + CH_PERM[d]].text); } return 0;
}
static int act(const char *kind, const char *a1) {      /* a live action by the player whose turn it is */
    int rc = replay(0); if (rc) return rc; if (OVER) { fprintf(stderr, "tsc_op: the match is over\n"); return 1; }
    int p = TURN; if (COMPUTER && p == 1 && strcmp(kind, "AIMANA") && strcmp(kind, "MIRACLE_AI")) { fprintf(stderr, "tsc_op: it is the computer's turn (use ai)\n"); return 1; }
    int seq = SEQ + 1; char res[400], row[1200];
    if (!strcmp(kind, "DEAL") && PHASE) { fprintf(stderr, "tsc_op: answer the open challenge first\n"); return 1; }
    if ((!strcmp(kind, "MIRACLE") || !strcmp(kind, "MIRACLE_AI")) && PHASE) { fprintf(stderr, "tsc_op: answer the open challenge first\n"); return 1; }
    if (!strcmp(kind, "ANSWER") && !PHASE) { fprintf(stderr, "tsc_op: there is no open challenge\n"); return 1; }
    const char *k = !strcmp(kind, "MIRACLE_AI") ? "MIRACLE" : kind;
    if (apply(k, p, a1, "", seq, res, sizeof res)) { fprintf(stderr, "tsc_op: refused (%s %s: bad value or not enough mana)\n", k, a1); return 1; }
    if (!strcmp(k, "DEAL")) snprintf(row, sizeof row, "DEAL|%d|%s|%s", p + 1, a1, res); else snprintf(row, sizeof row, "%s|%d|%s|%s", k, p + 1, a1, res);
    if (!append(row)) return 2; printf("%s\n", res); return 0;
}
static int cmd_ai(void) {
    int rc = replay(0); if (rc) return rc; if (OVER || !COMPUTER || TURN != 1) { fprintf(stderr, "tsc_op: not the computer's turn\n"); return 1; }
    int seq = SEQ + 1, mana = S[1].mana, hp = S[1].hp; const char *pick = NULL;
    if (hp <= 40 && mana >= 4) pick = "heal"; else for (int i = 10; i >= 1; i--) { if (mana < i) continue; if (i == 4 && hp >= 80) continue; if (i == 8 && S[1].bless) continue; if (i == 6 && S[0].poison) continue; if (i == 7 && S[0].locust) continue; pick = MNAME[i]; break; }
    if (pick) return act("MIRACLE_AI", pick);
    char n[4]; snprintf(n, sizeof n, "%d", 2 + (int)(rnd(seq, 7) % 3)); return act("AIMANA", n);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: tsc_op new|state|deal|answer|miracle|forfeit|ai <match dir> [args]\n"); return 2; }
    const char *verb = argv[1]; snprintf(MATCH, sizeof MATCH, "%s", argv[2]); snprintf(LEDGER, sizeof LEDGER, "%s/ledger.txt", MATCH);
    if (!load_verses()) return 2;
    if (!strcmp(verb, "new")) { const char *vs = "computer"; uint64_t seed = (uint64_t)time(NULL) * 2654435761u; for (int i = 3; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "--vs")) vs = argv[i + 1]; else if (!strcmp(argv[i], "--seed")) seed = strtoull(argv[i + 1], NULL, 10); }
        if (strcmp(vs, "computer") && strcmp(vs, "human")) { fprintf(stderr, "tsc_op: --vs computer|human\n"); return 2; } mkdir(MATCH, 0755); if (!access(LEDGER, F_OK)) { fprintf(stderr, "tsc_op: a match already exists here\n"); return 1; }
        char row[200]; snprintf(row, sizeof row, "START|%llu|%s", (unsigned long long)seed, vs); if (!append(row)) return 2; printf("NEW %s seed %llu vs %s\n", MATCH, (unsigned long long)seed, vs); return 0; }
    if (!strcmp(verb, "state")) return cmd_state();
    if (!strcmp(verb, "ai")) return cmd_ai();
    if (!strcmp(verb, "forfeit")) return act("FORFEIT", "");
    if (argc < 4) { fprintf(stderr, "tsc_op: %s needs a value\n", verb); return 2; }
    if (!strcmp(verb, "deal")) return act("DEAL", argv[3]);
    if (!strcmp(verb, "answer")) return act("ANSWER", argv[3]);
    if (!strcmp(verb, "miracle")) return act("MIRACLE", argv[3]);
    fprintf(stderr, "tsc_op: unknown verb %s\n", verb); return 2;
}
