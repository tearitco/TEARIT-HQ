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
#include <math.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
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
static char LAST[400], LAST2[400];
static int NETM, SEAT; static int read_seat(void);
static int RA = 1000, RB = 1000;      /* the two ratings at the START row (a computer match is rated; a friend match is not) */
static void elo(int *na, int *nb) { double ea = 1.0 / (1.0 + pow(10.0, (RB - RA) / 400.0)); double sa = WINNER == 0 ? 1.0 : 0.0; double da = 32.0 * (sa - ea); *na = RA + (int)(da >= 0 ? da + 0.5 : da - 0.5); *nb = RB + (int)(-da >= 0 ? -da + 0.5 : -da - 0.5); }
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
    if (!strcmp(kind, "DEAL")) { int n = atoi(a1); if (n < 2 || n > 10) return 1; deal(n, seq); if (!CH_N) return 1; PHASE = 1; snprintf(res, cap, "%d verses dealt", n); return 0; }
    if (!strcmp(kind, "ANSWER")) { char key[16]; answer_key(key); int okk = !strcmp(a1, key); char ref[80]; snprintf(ref, sizeof ref, "%.60s", V[CH_START].text); PHASE = 0;
        if (okk) { S[p].mana += CH_N; snprintf(res, cap, "right: +%d mana", CH_N); } else snprintf(res, cap, "wrong: no mana, turn ends (order %s)", key);
        end_turn(res, cap); return 0; }
    if (!strcmp(kind, "AIMANA")) { int n = atoi(a1); int pw = 62 + (RB - 1000) / 10; if (pw < 30) pw = 30; if (pw > 90) pw = 90; int win = (int)(rnd(seq, 0) % 100) < pw; if (win) { S[p].mana += n; snprintf(res, cap, "the computer sorts %d verses: +%d mana", n, n); } else snprintf(res, cap, "the computer fails %d verses: no mana", n); end_turn(res, cap); return 0; }
    if (!strcmp(kind, "MIRACLE")) { int c = cost_of(a1); if (c < 0 || S[p].mana < c) return 1; miracle(p, a1, seq, res, cap); if (!OVER) end_turn(res, cap); return 0; }
    if (!strcmp(kind, "FORFEIT")) { OVER = 1; WINNER = 1 - p; snprintf(res, cap, "player %d forfeits: player %d wins", p + 1, WINNER + 1); return 0; }
    (void)a2; return 1;
}
static void init_state(void) { memset(S, 0, sizeof S); S[0].hp = S[1].hp = 100; TURN = 0; OVER = 0; WINNER = -1; PHASE = 0; CH_N = 0; LAST[0] = 0; LAST2[0] = 0; RA = RB = 1000; }
/* replay: 0 = ok; 3 = a row is wrong */
static int replay(int quiet) {
    FILE *f = fopen(LEDGER, "r"); if (!f) { fprintf(stderr, "tsc_op: no ledger %s\n", LEDGER); return 2; }
    static char ln[2048]; SEQ = 0; init_state(); int started = 0, rc = 0;
    while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue; char *fl[6] = { 0 }; int nf = 0; char *p = ln; while (nf < 6) { fl[nf++] = p; char *bar = strchr(p, '|'); if (!bar) break; *bar = 0; p = bar + 1; }
        int seq = ++SEQ;
        if (!strcmp(fl[0], "START")) { if (started || nf < 3) { rc = 3; break; } SEED = strtoull(fl[1], NULL, 10); COMPUTER = !strcmp(fl[2], "computer"); NETM = !strcmp(fl[2], "net"); SEAT = NETM ? read_seat() : 0; if (nf >= 5) { RA = atoi(fl[3]); RB = atoi(fl[4]); } started = 1; continue; }
        if (!started || OVER || nf < 3) { rc = 3; break; }
        char res[400]; int pl = atoi(fl[1]) - 1; int kind_ok;
        if (pl < 0 || pl > 1 || (pl != TURN && strcmp(fl[0], "FORFEIT"))) { rc = 3; break; }
        if (!strcmp(fl[0], "DEAL") && nf >= 4) { kind_ok = !apply("DEAL", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "ANSWER") && nf >= 4 && PHASE == 1) { kind_ok = !apply("ANSWER", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "AIMANA") && nf >= 4) { kind_ok = !apply("AIMANA", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "MIRACLE") && nf >= 4 && PHASE == 0) { kind_ok = !apply("MIRACLE", pl, fl[2], "", seq, res, sizeof res); }
        else if (!strcmp(fl[0], "FORFEIT")) { kind_ok = !apply("FORFEIT", pl, "", "", seq, res, sizeof res); }
        else { rc = 3; break; }
        if (!kind_ok || strcmp(res, fl[nf - 1])) { rc = 3; if (!quiet) fprintf(stderr, "tsc_op: ledger row %d does not replay (stored: %s | computed: %s)\n", seq, fl[nf - 1], res); break; }
        snprintf(LAST2, sizeof LAST2, "%s", LAST); snprintf(LAST, sizeof LAST, "%s", res); }
    fclose(f); if (!started && !rc) rc = 3; return rc;
}
static int append(const char *row) { FILE *f = fopen(LEDGER, "a"); if (!f) return 0; fprintf(f, "%s\n", row); fclose(f); return 1; }
/* ---- p2p (2026-11-15): two phones, one match. Both sides hold the SAME ledger: the host writes the START row (shared seed, so every die, coin and shuffle is the same on both replays) and sends it; each side only issues moves for its own seat and sends the row
 * (`ROW|<n>|<row>`); the other side VERIFIES a received row by appending it and replaying (a row whose result differs, or that claims the wrong seat or turn, is cut off again and refused). The transport is palnet_peer (companion process: this op only writes
 * <match>/net_out.txt and reads <match>/net_in.txt, lines `<sender>|<content>`). <match>/seat.txt = 1 (host) or 2 (guest) and <match>/net_state.txt = host|join are UI scratch, not game state. v1 TRUST NOTE: both sides can compute future rolls from the seed (a commit-reveal seed is an open item). */
/* NETM and SEAT are declared above replay() */
static int read_seat(void) { char f[P], ln[16] = ""; snprintf(f, sizeof f, "%s/seat.txt", MATCH); FILE *fp = fopen(f, "r"); if (!fp) return 0; if (fgets(ln, sizeof ln, fp)) {} fclose(fp); int s = atoi(ln); return s == 1 || s == 2 ? s : 0; }
static void put_file(const char *name, const char *text) { char f[P], t[P]; snprintf(f, sizeof f, "%s/%s", MATCH, name); snprintf(t, sizeof t, "%s.tmp", f); FILE *w = fopen(t, "w"); if (!w) return; fputs(text, w); fclose(w); rename(t, f); }
static void net_send(const char *line) { char f[P]; snprintf(f, sizeof f, "%s/net_out.txt", MATCH); FILE *fp = fopen(f, "a"); if (!fp) return; fprintf(fp, "%s\n", line); fclose(fp); }
static void net_state(char *o, size_t cap) { char f[P]; snprintf(f, sizeof f, "%s/net_state.txt", MATCH); o[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(o, (int)cap, fp)) o[strcspn(o, "\r\n")] = 0; fclose(fp); }
static int drop_last_row(void) { FILE *f = fopen(LEDGER, "r"); if (!f) return 0; static char buf[262144]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0; if (n && buf[n - 1] == '\n') buf[--n] = 0; char *nl = strrchr(buf, '\n'); if (!nl) return 0; nl[1] = 0; f = fopen(LEDGER, "w"); if (!f) return 0; fputs(buf, f); fclose(f); return 1; }
static int my_rating(void);
/* one pass over the new inbox lines; returns the number handled */
static int net_pump(void) {
    char f[P], cf[P]; snprintf(f, sizeof f, "%s/net_in.txt", MATCH); snprintf(cf, sizeof cf, "%s/net_cursor.txt", MATCH); long cur = 0; FILE *c = fopen(cf, "r"); if (c) { if (fscanf(c, "%ld", &cur) != 1) cur = 0; fclose(c); }
    FILE *fp = fopen(f, "r"); if (!fp) return 0; struct stat sb; if (!fstat(fileno(fp), &sb) && sb.st_size < cur) cur = 0; fseek(fp, cur, SEEK_SET); static char ln[2048]; int handled = 0; char st[16]; net_state(st, sizeof st);
    while (fgets(ln, sizeof ln, fp)) { size_t L = strlen(ln); if (!L || ln[L - 1] != '\n') break; long next = cur + (long)L; ln[strcspn(ln, "\r\n")] = 0; char *content = strchr(ln, '|'); if (!content) { cur = next; continue; } content++; int stop = 0;
        if (!strncmp(content, "JOIN|", 5) && !strcmp(st, "host") && access(LEDGER, F_OK)) {      /* a guest asks: the host fixes seed and ratings, writes START and answers */
            char tok[40] = "", rb[16] = ""; sscanf(content + 5, "%39[^|]|%15s", tok, rb); int ra = my_rating(), rbv = atoi(rb); if (rbv < 100 || rbv > 3000) rbv = 1000; unsigned long long seed = (unsigned long long)time(NULL) * 2654435761ULL ^ (unsigned long long)getpid() * 40503ULL;
            char row[200]; snprintf(row, sizeof row, "START|%llu|net|%d|%d", seed, ra, rbv); mkdir(MATCH, 0755); put_file("seat.txt", "1\n"); FILE *w = fopen(LEDGER, "a"); if (w) { fprintf(w, "%s\n", row); fclose(w); } char m[300]; snprintf(m, sizeof m, "START|%llu|%d|%d|%s", seed, ra, rbv, tok); net_send(m); handled++; }
        else if (!strncmp(content, "START|", 6) && !strcmp(st, "join") && access(LEDGER, F_OK)) {
            char tok[40] = ""; unsigned long long seed = 0; int ra = 1000, rb = 1000; if (sscanf(content + 6, "%llu|%d|%d|%39s", &seed, &ra, &rb, tok) == 4) { char mine[40]; char tf[P]; snprintf(tf, sizeof tf, "%s/net_token.txt", MATCH); FILE *t = fopen(tf, "r"); mine[0] = 0; if (t) { if (fgets(mine, sizeof mine, t)) mine[strcspn(mine, "\r\n")] = 0; fclose(t); }
                if (!strcmp(tok, mine)) { char row[200]; snprintf(row, sizeof row, "START|%llu|net|%d|%d", seed, ra, rb); put_file("seat.txt", "2\n"); FILE *w = fopen(LEDGER, "a"); if (w) { fprintf(w, "%s\n", row); fclose(w); } handled++; } } }
        else if (!strncmp(content, "ROW|", 4) && !access(LEDGER, F_OK)) {
            int seq = atoi(content + 4); char *row = strchr(content + 4, '|'); if (row) row++; int rc = replay(1); int seat = read_seat();
            if (!row || rc || !seat) stop = 1; else if (seq <= SEQ) { handled++; }      /* a repeat of a row we already have */
            else if (seq > SEQ + 1) stop = 1;      /* a gap: wait for the earlier row (palnet resends its backlog) */
            else { char who[8] = ""; sscanf(row, "%*[A-Z]|%7[^|]", who); int pl = atoi(who); int okseat = pl == 3 - seat && (pl - 1 == TURN || !strncmp(row, "FORFEIT|", 8));
                if (okseat && !OVER) { FILE *w = fopen(LEDGER, "a"); if (w) { fprintf(w, "%s\n", row); fclose(w); } if (replay(1)) { drop_last_row(); put_file("net_error.txt", "the opponent sent a move that does not replay: refused\n"); } else handled++; }
                else put_file("net_error.txt", "the opponent sent a move for the wrong seat or turn: refused\n"); } }
        if (stop) break; cur = next; }
    fclose(fp); { char t[40]; snprintf(t, sizeof t, "%ld\n", cur); put_file("net_cursor.txt", t); } return handled;
}
static int cmd_state(void) {
    int rc = replay(0); if (rc) return rc; char st[2][100]; side_text(&S[0], st[0], sizeof st[0]); side_text(&S[1], st[1], sizeof st[1]);
    printf("mode=%s\nturn=%d\nphase=%s\nwinner=%d\nseq=%d\n", COMPUTER ? "computer" : NETM ? "net" : "human", TURN + 1, OVER ? "over" : PHASE ? "answer" : "choose", WINNER + 1, SEQ);
    for (int i = 0; i < 2; i++) printf("p%d_hp=%d\np%d_mana=%d\np%d_status=%s\n", i + 1, S[i].hp, i + 1, S[i].mana, i + 1, st[i]);
    if (OVER && (COMPUTER || NETM)) { int na, nb; elo(&na, &nb); printf("rating_p1=%d>%d\nrating_p2=%d>%d\n", RA, na, RB, nb); }
    printf("last=%s\n", LAST); if (PHASE == 1 && !OVER) { printf("challenge_n=%d\n", CH_N); for (int d = 0; d < CH_N; d++) printf("ch_%d=%s\n", d + 1, V[CH_START + CH_PERM[d]].text); } return 0;
}
static int act(const char *kind, const char *a1) {      /* a live action by the player whose turn it is */
    int rc = replay(0); if (rc) return rc; if (OVER) { fprintf(stderr, "tsc_op: the match is over\n"); return 1; }
    int p = TURN; if (NETM) { int seat = read_seat(); if (!seat) { fprintf(stderr, "tsc_op: no seat in this online match yet\n"); return 1; } if (!strcmp(kind, "FORFEIT")) p = seat - 1; else if (p != seat - 1) { fprintf(stderr, "tsc_op: it is the opponent's turn\n"); return 1; } }
    if (COMPUTER && p == 1 && strcmp(kind, "AIMANA") && strcmp(kind, "MIRACLE_AI")) { fprintf(stderr, "tsc_op: it is the computer's turn (use ai)\n"); return 1; }
    int seq = SEQ + 1; char res[400], row[1200];
    if (!strcmp(kind, "DEAL") && PHASE) { fprintf(stderr, "tsc_op: answer the open challenge first\n"); return 1; }
    if ((!strcmp(kind, "MIRACLE") || !strcmp(kind, "MIRACLE_AI")) && PHASE) { fprintf(stderr, "tsc_op: answer the open challenge first\n"); return 1; }
    if (!strcmp(kind, "ANSWER") && !PHASE) { fprintf(stderr, "tsc_op: there is no open challenge\n"); return 1; }
    const char *k = !strcmp(kind, "MIRACLE_AI") ? "MIRACLE" : kind;
    if (apply(k, p, a1, "", seq, res, sizeof res)) { fprintf(stderr, "tsc_op: refused (%s %s: bad value or not enough mana)\n", k, a1); return 1; }
    if (!strcmp(k, "DEAL")) snprintf(row, sizeof row, "DEAL|%d|%s|%s", p + 1, a1, res); else snprintf(row, sizeof row, "%s|%d|%s|%s", k, p + 1, a1, res);
    if (!append(row)) return 2; if (NETM) { char m[1300]; snprintf(m, sizeof m, "ROW|%d|%s", seq, row); net_send(m); } printf("%s\n", res); return 0;
}
static int cmd_ai(void) {
    int rc = replay(0); if (rc) return rc; if (OVER || !COMPUTER || TURN != 1) { fprintf(stderr, "tsc_op: not the computer's turn\n"); return 1; }
    int seq = SEQ + 1, mana = S[1].mana, hp = S[1].hp; const char *pick = NULL;
    if (hp <= 40 && mana >= 4) pick = "heal"; else for (int i = 10; i >= 1; i--) { if (mana < i) continue; if (i == 4 && hp >= 80) continue; if (i == 8 && S[1].bless) continue; if (i == 6 && S[0].poison) continue; if (i == 7 && S[0].locust) continue; pick = MNAME[i]; break; }
    if (pick) return act("MIRACLE_AI", pick);
    char n[4]; snprintf(n, sizeof n, "%d", 2 + (int)(rnd(seq, 7) % 3)); return act("AIMANA", n);
}
/* ---- the phone screen (2026-11-15): this op is also the window's projector (module) and its UI verbs, the app-edit / plugz pattern.
 *   module:  tsc_op <house> <package dir>   (env KHTPM_ARG3 = the phone instance dir; republishes <inst>/.hq_manager/ui.txt when marker.txt GREW or every 5 s; ends with its parent)   [--once <inst>] = one projection, for the harness
 *   verbs:   tsc_op ui_new computer|human <inst>   ui_deal <n> <inst>   ui_digit <d> <inst>   ui_clear <inst>   ui_miracle <name> <inst>   ui_forfeit <inst>
 * The match lives at <inst>/tsc/ledger.txt (the ledger is the only state). <inst>/.hq_manager/typed.txt is the digits typed so far for the open challenge: a UI scratch value, never read by the rules.
 * Against the computer, its turn is played at once after the human's action (the result lines show both). */
static char HQD[P];
static void rdq(const char *name, char *o, size_t cap) { char f[P]; snprintf(f, sizeof f, "%s/%s", HQD, name); o[0] = 0; FILE *fp = fopen(f, "r"); if (!fp) return; if (fgets(o, (int)cap, fp)) o[strcspn(o, "\r\n")] = 0; fclose(fp); }
static void wrq(const char *name, const char *text) { char f[P], t[P]; snprintf(f, sizeof f, "%s/%s", HQD, name); snprintf(t, sizeof t, "%s.tmp", f); FILE *w = fopen(t, "w"); if (!w) return; fputs(text, w); fputc('\n', w); fclose(w); rename(t, f); }
static void bumpq(void) { char f[P]; snprintf(f, sizeof f, "%s/marker.txt", HQD); struct stat sb; if (!stat(f, &sb) && sb.st_size > 4096) unlink(f); FILE *m = fopen(f, "a"); if (m) { fputc('x', m); fclose(m); } }
static long msizeq(void) { char f[P]; struct stat sb; snprintf(f, sizeof f, "%s/marker.txt", HQD); return stat(f, &sb) ? 0 : (long)sb.st_size; }
static int set_inst(const char *inst) { snprintf(HQD, sizeof HQD, "%s/.hq_manager", inst); mkdir(HQD, 0755); snprintf(MATCH, sizeof MATCH, "%s/tsc", inst); snprintf(LEDGER, sizeof LEDGER, "%s/ledger.txt", MATCH); return 1; }
static char UI[16384], LAST_UI[16384];
static void u(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void u(const char *fmt, ...) { size_t l = strlen(UI); va_list ap; va_start(ap, fmt); vsnprintf(UI + l, sizeof UI - l, fmt, ap); va_end(ap); }
static void clean(char *s) { for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' '; }
static void digs(int n) { for (int dg = 0; dg < 10; dg++) { int pos = dg == 0 ? 10 : dg; u("dig_%d=%d\n", dg, pos <= n); } }
static int pref_row(const char *house, const char *key, char *out, size_t cap); static int house_of(const char *inst, char *house, size_t cap);
static void book_vars(const char *page) {      /* the book:page breadcrumb of the tab bar is DATA (owner 2026-11-15: names are flexible): the book name is the BOOK row of phone_prefs.pdl (default phone), the page is this app's own name */
    char house[P], b[80] = ""; if (house_of(HQD, house, sizeof house)) pref_row(house, "BOOK", b, sizeof b); if (!b[0]) snprintf(b, sizeof b, "phone"); u("book_label=%s\npage_label=%s\n", b, page); }
static void rate_if_over(void);
static void publish(void) {
    rate_if_over();      /* a finished match is counted once, on whichever path it ended (a received row, a verb): the projection is where every path passes */
    UI[0] = 0; char typed[16]; rdq("typed.txt", typed, sizeof typed); int have = !access(LEDGER, F_OK); int rc = have ? replay(1) : 0; const char *n1 = "Player 1", *n2 = "Player 2";
    u("window_title=True Swords Clash\n"); book_vars("tsc"); digs(0);
    char netst[16]; net_state(netst, sizeof netst); const char *setup_msg = !strcmp(netst, "host") ? "Hosting: waiting for an opponent..." : !strcmp(netst, "join") ? "Joining: looking for a host..." : "Sort verses for mana, then cast Miracles. HP 100."; char nerr[200] = ""; { char ef[P]; snprintf(ef, sizeof ef, "%s/net_error.txt", MATCH); FILE *e = fopen(ef, "r"); if (e) { if (fgets(nerr, sizeof nerr, e)) nerr[strcspn(nerr, "\r\n")] = 0; fclose(e); } }
    if (!have) { u("show_setup=1\nshow_play=0\nshow_choose=0\nshow_answer=0\nshow_over=0\nline_p1=\nline_p2=\nline_turn=%s\nline_last=%s\nline_last2=\nn_mir=0\nn_ch=0\ntyped_line=\n", setup_msg, nerr); }
    else if (rc) { u("show_setup=1\nshow_play=0\nshow_choose=0\nshow_answer=0\nshow_over=0\nline_p1=\nline_p2=\nline_turn=This match's ledger does not replay (it was edited). Start a new match.\nline_last=\nline_last2=\nn_mir=0\nn_ch=0\ntyped_line=\n"); }
    else { if (COMPUTER) { n1 = "You"; n2 = "Computer"; } if (NETM) { n1 = SEAT == 1 ? "You" : "Opponent"; n2 = SEAT == 2 ? "You" : "Opponent"; } int mine = !NETM || TURN + 1 == SEAT; UI[0] = 0; u("window_title=True Swords Clash\n"); digs(PHASE && !OVER ? CH_N : 0); char s1[100], s2[100]; side_text(&S[0], s1, sizeof s1); side_text(&S[1], s2, sizeof s2);
        int choose = !OVER && !PHASE && mine, answer = !OVER && PHASE && mine; u("show_setup=0\nshow_play=%d\nshow_choose=%d\nshow_answer=%d\nshow_over=%d\n", OVER ? 0 : 1, choose, answer, OVER);
        u("line_p1=%s%s   HP %d   Mana %d   [%s]\nline_p2=%s%s   HP %d   Mana %d   [%s]\n", TURN == 0 && !OVER ? "> " : "  ", n1, S[0].hp, S[0].mana, s1, TURN == 1 && !OVER ? "> " : "  ", n2, S[1].hp, S[1].mana, s2);
        if (OVER) { if (COMPUTER || NETM) { int na, nb; elo(&na, &nb); int s2 = NETM && SEAT == 2; u("line_turn=%s %s.  Rating %d -> %d\n", WINNER == 0 ? n1 : n2, !strcmp(WINNER == 0 ? n1 : n2, "You") ? "win" : "wins", s2 ? RB : RA, s2 ? nb : na); } else u("line_turn=%s wins.  (friend matches are not rated)\n", WINNER == 0 ? n1 : n2); } else if (!mine) u("line_turn=Opponent's turn: wait for their move.\n"); else if (answer) u("line_turn=%s: order the %d verses (Bible order).\n", TURN == 0 ? n1 : n2, CH_N); else u("line_turn=%s: sort verses for mana, or cast a Miracle.\n", TURN == 0 ? n1 : n2);
        char la[400], lb[400]; snprintf(la, sizeof la, "%s", answer ? "" : LAST); snprintf(lb, sizeof lb, "%s", answer ? "" : LAST2); clean(la); clean(lb); u("line_last=%s\nline_last2=%s\n", la, lb);
        int nm = 0; if (choose) { for (int i = 0; i < 11; i++) { int cost = i == 0 ? 0 : i; int ok = S[TURN].mana >= cost; static const char *nice[11] = { "Hail Mary", "Slash", "Fire", "Berserk", "Heal", "Judgement", "Poison", "Locust", "Blessing", "Angel", "Holy Sword" };
                u("mir_%d_label=  %s   (%d)%s  \nmir_%d_name=%s\nmir_%d_cls=%s\n", nm, nice[i], cost, ok ? "" : "   low", nm, MNAME[i], nm, ok ? "btn" : "dimbtn"); nm++; } }
        u("n_mir=%d\n", nm); if (answer) { u("n_ch=%d\n", CH_N); for (int d = 0; d < CH_N; d++) { char t[300]; snprintf(t, sizeof t, "%d)  %s", (d + 1) % 10 == 0 && d + 1 == 10 ? 10 : d + 1, V[CH_START + CH_PERM[d]].text); clean(t); u("ch_%d_text=%s\n", d, t); } } else u("n_ch=0\n");
        
        char tl[80] = "your order: "; for (int i = 0; i < CH_N && answer; i++) { char c[8]; snprintf(c, sizeof c, "%c ", i < (int)strlen(typed) ? typed[i] : '_'); strcat(tl, c); } u("typed_line=%s\n", answer ? tl : ""); }
    if (strcmp(UI, LAST_UI)) { char f[P], t[P]; snprintf(f, sizeof f, "%s/ui.txt", HQD); snprintf(t, sizeof t, "%s/ui.txt.tmp", HQD); FILE *w = fopen(t, "w"); if (w) { fputs(UI, w); fclose(w); rename(t, f); } snprintf(LAST_UI, sizeof LAST_UI, "%s", UI); }
}
static void computer_plays(void) { for (int i = 0; i < 4; i++) { if (replay(1) || OVER || !COMPUTER || TURN != 1) return; cmd_ai(); } }
/* RATING (a PROJECTION of finished computer matches, derived from the ledger by this op; the rules never read it): <inst>/tsc/rating.txt = rating|wins|losses|last rated seed. The START row of a match copies the rating, so the replay stays pure. */
static int my_rating(void) { char f[P]; snprintf(f, sizeof f, "%s/rating.txt", MATCH); FILE *fp = fopen(f, "r"); if (!fp) return 1000; int r = 1000; if (fscanf(fp, "%d", &r) != 1) r = 1000; fclose(fp); return r < 100 ? 100 : r > 3000 ? 3000 : r; }
static void rate_if_over(void) {
    if (replay(1) || !OVER || (!COMPUTER && !NETM)) return; char f[P], ln[200]; snprintf(f, sizeof f, "%s/rating.txt", MATCH); int r = 1000, w = 0, l = 0; unsigned long long last = 0; FILE *fp = fopen(f, "r"); if (fp) { if (fgets(ln, sizeof ln, fp)) sscanf(ln, "%d|%d|%d|%llu", &r, &w, &l, &last); fclose(fp); }
    if (last == (unsigned long long)SEED) return;      /* this match was already counted */
    int na, nb; elo(&na, &nb); int won = NETM ? WINNER == SEAT - 1 : WINNER == 0; int mine = (NETM && SEAT == 2) ? nb : na; if (won) w++; else l++; snprintf(ln, sizeof ln, "%d|%d|%d|%llu\n", mine, w, l, (unsigned long long)SEED); char t[P]; snprintf(t, sizeof t, "%s.tmp", f); fp = fopen(t, "w"); if (fp) { fputs(ln, fp); fclose(fp); rename(t, f); } (void)r;
}
/* online verbs. ui_host / ui_join start a fresh session: the old match is kept as ledger.prev.txt, the net files are emptied and net_epoch.txt gets a new value, which makes the screen's module restart the peer (a new peer reads the outbox from byte 0). */
static void net_reset(const char *state, const char *epoch) {
    const char *z[] = { "net_in.txt", "net_out.txt" }; for (int i = 0; i < 2; i++) { char f[P]; snprintf(f, sizeof f, "%s/%s", MATCH, z[i]); FILE *fp = fopen(f, "w"); if (fp) fclose(fp); }
    const char *u[] = { "net_error.txt", "seat.txt", "net_token.txt" }; for (int i = 0; i < 3; i++) { char f[P]; snprintf(f, sizeof f, "%s/%s", MATCH, u[i]); unlink(f); }
    put_file("net_cursor.txt", "0\n"); if (!access(LEDGER, F_OK)) { char prev[P]; snprintf(prev, sizeof prev, "%s/ledger.prev.txt", MATCH); rename(LEDGER, prev); }
    put_file("net_epoch.txt", epoch); put_file("net_state.txt", state);
}
static int ui_online(const char *v) {
    mkdir(MATCH, 0755); char ep[40]; snprintf(ep, sizeof ep, "%lx%x\n", (long)time(NULL), (unsigned)getpid());
    if (!strcmp(v, "ui_host")) { net_reset("host\n", ep); return 0; }
    net_reset("join\n", ep); char tok[40]; snprintf(tok, sizeof tok, "%lx%x\n", (long)time(NULL) ^ 0x5bd1, (unsigned)getpid() * 7u); put_file("net_token.txt", tok); tok[strcspn(tok, "\n")] = 0;
    char m[120]; snprintf(m, sizeof m, "JOIN|%s|%d", tok, my_rating()); net_send(m); return 0;
}
/* the peer process of this window (palnet_peer, the house p2p companion), started and owned by the screen's module: the spawn funnel gets the module pid as owner, so it ends with the window */
/* MASTER HOST LIST wiring (2026-11-15). phone_prefs.pdl rows (read here, written by `phone_op prefs`): HOSTLIST_URL | https://... (a master list to fetch before joining), NET_ADVERTISE | <address peers should dial> (also makes the peer listen on every interface).
 * Hosting: the module publishes this phone's address:port (from palnet's own presence file) to <house>/&.hq-apps/plus-phone/net/hosts.pdl and again every 5 minutes until the match starts. Joining: the peer is started with PALNET_SEEDS = the live hosts of kind tsc. */
static int pref_row(const char *house, const char *key, char *out, size_t cap) { char f[P], ln[600]; out[0] = 0; snprintf(f, sizeof f, "%s/&.hq-apps/plus-phone/phone_prefs.pdl", house); FILE *fp = fopen(f, "r"); if (!fp) return 0; size_t kl = strlen(key); while (fgets(ln, sizeof ln, fp)) { if (strncmp(ln, key, kl) || strncmp(ln + kl, " | ", 3)) continue; ln[strcspn(ln, "\r\n")] = 0; snprintf(out, cap, "%s", ln + kl + 3); fclose(fp); return 1; } fclose(fp); return 0; }
static int run_capture(char *const av[], char *out, size_t cap) { int pf[2]; if (pipe(pf)) return -1; pid_t c = fork(); if (c < 0) return -1; if (c == 0) { close(pf[0]); dup2(pf[1], 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); } execv(av[0], av); _exit(127); } close(pf[1]); size_t n = 0; ssize_t r; while (n + 1 < cap && (r = read(pf[0], out + n, cap - 1 - n)) > 0) n += (size_t)r; out[n] = 0; close(pf[0]); int st; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1; }
static int house_of(const char *inst, char *house, size_t cap) { const char *h = strstr(inst, "/&.hq-apps/plus-phone/instances/"); if (!h) return 0; snprintf(house, cap, "%.*s", (int)(h - inst), inst); return 1; }
static void host_list_path(const char *house, char *o, size_t cap) { snprintf(o, cap, "%s/&.hq-apps/plus-phone/net/hosts.pdl", house); }
static char HL_BIN[P]; static int PUBLISHED; static time_t LAST_PUB;
static void host_publish(const char *inst) {      /* find our own peer in the presence directory (project_id tsc-<n>) and publish its address and port */
    char house[P]; if (!house_of(inst, house, sizeof house)) return; char dir[P]; snprintf(dir, sizeof dir, "%s/&.hq-apps/plus-phone/net/presence", house); const char *b = strrchr(inst, '/'); char proj[96]; snprintf(proj, sizeof proj, "tsc-%s", b ? b + 1 : "0");
    DIR *d = opendir(dir); if (!d) return; struct dirent *e; char host[84] = "", port[16] = "";
    while ((e = readdir(d))) { char want[110]; snprintf(want, sizeof want, "%s-", proj); if (strncmp(e->d_name, want, strlen(want))) continue; char f[P], ln[200]; snprintf(f, sizeof f, "%s/%s", dir, e->d_name); FILE *fp = fopen(f, "r"); if (!fp) continue; while (fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; if (!strncmp(ln, "host=", 5)) snprintf(host, sizeof host, "%s", ln + 5); else if (!strncmp(ln, "port=", 5)) snprintf(port, sizeof port, "%s", ln + 5); } fclose(fp); if (host[0] && port[0]) break; }
    closedir(d); if (!host[0] || !port[0]) return; char list[P], bin[P]; host_list_path(house, list, sizeof list); snprintf(bin, sizeof bin, "%s/&.hq-apps/plus-phone/ops/+x/hostlist_op.+x", house); if (access(bin, X_OK)) return; char out[200]; char id[96]; snprintf(id, sizeof id, "%s", proj);
    char *av[] = { bin, "publish", list, "tsc", id, host, port, "--ttl", "600", "--note", "True Swords Clash", NULL }; run_capture(av, out, sizeof out); PUBLISHED = strstr(out, "HOST-PUBLISHED") != NULL;
}
static void host_unpublish(const char *inst) { char house[P]; if (!house_of(inst, house, sizeof house)) return; const char *b = strrchr(inst, '/'); char proj[96], list[P], bin[P], out[100]; snprintf(proj, sizeof proj, "tsc-%s", b ? b + 1 : "0"); host_list_path(house, list, sizeof list); snprintf(bin, sizeof bin, "%s/&.hq-apps/plus-phone/ops/+x/hostlist_op.+x", house); if (access(bin, X_OK)) return; char *av[] = { bin, "remove", list, proj, NULL }; run_capture(av, out, sizeof out); }
static void seeds_for_join(const char *inst, char *seeds, size_t cap) {      /* fetch the master list first when a URL is set, then the live hosts of kind tsc as addr:port,addr:port */
    seeds[0] = 0; char house[P]; if (!house_of(inst, house, sizeof house)) return; char list[P], bin[P], url[400], out[600]; host_list_path(house, list, sizeof list); snprintf(bin, sizeof bin, "%s/&.hq-apps/plus-phone/ops/+x/hostlist_op.+x", house); if (access(bin, X_OK)) return;
    if (pref_row(house, "HOSTLIST_URL", url, sizeof url) && !strncmp(url, "https://", 8)) { char *av[] = { bin, "fetch", url, list, NULL }; run_capture(av, out, sizeof out); }
    char *av2[] = { bin, "seeds", list, "tsc", NULL }; if (run_capture(av2, out, sizeof out) == 0) { out[strcspn(out, "\r\n")] = 0; snprintf(seeds, cap, "%s", out); }
}
static long PEER_PID; static char PEER_EPOCH[40];
static void peer_stop(void) { if (PEER_PID > 1) { kill((pid_t)PEER_PID, SIGTERM); PEER_PID = 0; } }
static void peer_start(const char *inst) {
    const char *h = strstr(inst, "/&.hq-apps/plus-phone/instances/"); if (!h) return; char house[P]; snprintf(house, sizeof house, "%.*s", (int)(h - inst), inst);
    char self[P]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return; self[n] = 0; char *s = strrchr(self, '/'); if (s) *s = 0; char peer[P], funnel[P], root[P], outf[P], inf[P], sp[P], proj[96], owner[24];
    snprintf(peer, sizeof peer, "%s/palnet_peer.+x", self); snprintf(funnel, sizeof funnel, "%s/&.widgits/_shared-lib/kh_spawn.sh", house); snprintf(root, sizeof root, "%s/&.hq-apps/plus-phone/net/presence", house); snprintf(outf, sizeof outf, "%s/net_out.txt", MATCH); snprintf(inf, sizeof inf, "%s/net_in.txt", MATCH); snprintf(sp, sizeof sp, "%s/net_spawn.out", MATCH);
    const char *b = strrchr(inst, '/'); snprintf(proj, sizeof proj, "tsc-%s", b ? b + 1 : "0"); snprintf(owner, sizeof owner, "%d", (int)getpid()); if (access(peer, X_OK) || access(funnel, R_OK)) return;
    { char cmd[P + 20]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", root); (void)cmd; char d[P]; snprintf(d, sizeof d, "%s/&.hq-apps/plus-phone/net", house); mkdir(d, 0755); mkdir(root, 0755); }
    char seeds[512] = "", adv[84] = "", stn[16]; net_state(stn, sizeof stn); if (!strcmp(stn, "join")) seeds_for_join(inst, seeds, sizeof seeds); pref_row(house, "NET_ADVERTISE", adv, sizeof adv);
    pid_t c = fork(); if (c < 0) return;
    if (c == 0) { int fd = open(sp, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1); int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); } setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "tsc", 1); setenv("PRISC_NET_ROOT", root, 1); setenv("PRISC_PROJECT_ROOT", MATCH, 1); { char ow[24]; snprintf(ow, sizeof ow, "%d", (int)getppid()); setenv("PALNET_OWNER_PID", ow, 1); } if (seeds[0]) setenv("PALNET_SEEDS", seeds, 1); if (adv[0]) { setenv("PALNET_BIND", "0.0.0.0", 1); setenv("PALNET_ADVERTISE", adv, 1); }
        char *av[] = { "sh", funnel, house, owner, "tsc-net", "--", peer, "tsc", proj, "-", outf, inf, "tsc", NULL }; execv("/bin/sh", av); _exit(127); }
    waitpid(c, NULL, 0); FILE *fp = fopen(sp, "r"); char out[64] = ""; if (fp) { if (fgets(out, sizeof out, fp)) {} fclose(fp); } unlink(sp); PEER_PID = atol(out);
}
static int ui_verb0(const char *v, const char *arg, const char *inst);
static int ui_verb(const char *v, const char *arg, const char *inst) { int rc = ui_verb0(v, arg, inst); set_inst(inst); rate_if_over(); bumpq(); return rc; }
static int ui_verb0(const char *v, const char *arg, const char *inst) {
    set_inst(inst); if (!load_verses()) return 2;
    if (!strcmp(v, "ui_new")) { if (strcmp(arg, "computer") && strcmp(arg, "human")) return 2; mkdir(MATCH, 0755); if (!access(LEDGER, F_OK)) { char prev[P]; snprintf(prev, sizeof prev, "%s/ledger.prev.txt", MATCH); rename(LEDGER, prev); } put_file("net_state.txt", ""); int ra = my_rating(); char row[200]; snprintf(row, sizeof row, "START|%llu|%s|%d|%d", (unsigned long long)((uint64_t)time(NULL) * 2654435761u), arg, ra, ra); append(row); wrq("typed.txt", ""); bumpq(); return 0; }
    if (!strcmp(v, "ui_host") || !strcmp(v, "ui_join")) return ui_online(v);
    if (!strcmp(v, "ui_pump")) { net_pump(); return 0; }
    if (replay(1)) { bumpq(); return 3; }
    if (!strcmp(v, "ui_clear")) { wrq("typed.txt", ""); bumpq(); return 0; }
    if (!strcmp(v, "ui_deal")) { int rc = act("DEAL", arg); if (!rc) wrq("typed.txt", ""); bumpq(); return rc; }
    if (!strcmp(v, "ui_digit")) { char typed[16]; rdq("typed.txt", typed, sizeof typed); if (!PHASE || OVER || !isdigit((unsigned char)arg[0])) return 1; int pos = arg[0] == '0' ? 10 : arg[0] - '0'; if (pos > CH_N || (int)strlen(typed) >= CH_N) return 1; size_t l = strlen(typed); typed[l] = arg[0]; typed[l + 1] = 0; wrq("typed.txt", typed);
        if ((int)strlen(typed) == CH_N) { act("ANSWER", typed); wrq("typed.txt", ""); computer_plays(); } bumpq(); return 0; }
    if (!strcmp(v, "ui_miracle")) { int rc = act("MIRACLE", arg); if (!rc) computer_plays(); bumpq(); return rc; }
    if (!strcmp(v, "ui_forfeit")) { int rc = act("FORFEIT", ""); bumpq(); return rc; }
    return 2;
}
static int module_main(const char *inst, int once) {
    set_inst(inst); if (!load_verses()) return 2; if (once) { char st0[16]; net_state(st0, sizeof st0); if (st0[0]) net_pump(); publish(); return 0; }
    long seen = -1; time_t last = 0; for (;;) {
        char st[16]; net_state(st, sizeof st); int force = 0;
        if (st[0]) { char ep[40] = "", ef[P]; snprintf(ef, sizeof ef, "%s/net_epoch.txt", MATCH); FILE *e = fopen(ef, "r"); if (e) { if (fgets(ep, sizeof ep, e)) ep[strcspn(ep, "\r\n")] = 0; fclose(e); }
            if (!PEER_PID || strcmp(ep, PEER_EPOCH)) { peer_stop(); peer_start(inst); snprintf(PEER_EPOCH, sizeof PEER_EPOCH, "%s", ep); force = 1; }
            if (net_pump()) force = 1;
            if (!strcmp(st, "host")) { time_t tn = time(NULL); if (!access(LEDGER, F_OK)) { if (PUBLISHED) { host_unpublish(inst); PUBLISHED = 0; } } else if (!PUBLISHED || tn - LAST_PUB >= 300) { host_publish(inst); if (PUBLISHED) LAST_PUB = tn; } } }
        else { if (PUBLISHED) { host_unpublish(inst); PUBLISHED = 0; } if (PEER_PID) { peer_stop(); PEER_EPOCH[0] = 0; } }
        long m = msizeq(); time_t now = time(NULL); if (force || m != seen || now - last >= 5) { publish(); seen = m; last = now; } if (getppid() == 1) { peer_stop(); return 0; } usleep(400000); }
}
int main(int argc, char **argv) {
    { const char *ki = getenv("KHTPM_ARG3"); if (argc >= 3 && argv[1][0] == '/') { if (argc >= 5 && !strcmp(argv[3], "--once")) return module_main(argv[4], 1); if (!ki || !ki[0]) { fprintf(stderr, "tsc_op: no KHTPM_ARG3 (open it through the phone)\n"); return 2; } return module_main(ki, 0); } }
    if (argc >= 4 && !strncmp(argv[1], "ui_", 3)) return ui_verb(argv[1], argv[2], argv[3]);
    if (argc < 3) { fprintf(stderr, "usage: tsc_op new|state|deal|answer|miracle|forfeit|ai <match dir> [args]\n"); return 2; }
    const char *verb = argv[1]; snprintf(MATCH, sizeof MATCH, "%s", argv[2]); snprintf(LEDGER, sizeof LEDGER, "%s/ledger.txt", MATCH);
    if (!load_verses()) return 2;
    if (!strcmp(verb, "new")) { const char *vs = "computer"; uint64_t seed = (uint64_t)time(NULL) * 2654435761u; int ra = 1000, rb = -1; for (int i = 3; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "--vs")) vs = argv[i + 1]; else if (!strcmp(argv[i], "--seed")) seed = strtoull(argv[i + 1], NULL, 10); else if (!strcmp(argv[i], "--rating")) ra = atoi(argv[i + 1]); else if (!strcmp(argv[i], "--opp")) rb = atoi(argv[i + 1]); } if (rb < 0) rb = ra;
        if (strcmp(vs, "computer") && strcmp(vs, "human")) { fprintf(stderr, "tsc_op: --vs computer|human\n"); return 2; } mkdir(MATCH, 0755); if (!access(LEDGER, F_OK)) { fprintf(stderr, "tsc_op: a match already exists here\n"); return 1; }
        char row[200]; snprintf(row, sizeof row, "START|%llu|%s|%d|%d", (unsigned long long)seed, vs, ra, rb); if (!append(row)) return 2; printf("NEW %s seed %llu vs %s\n", MATCH, (unsigned long long)seed, vs); return 0; }
    if (!strcmp(verb, "state")) return cmd_state();
    if (!strcmp(verb, "ai")) return cmd_ai();
    if (!strcmp(verb, "forfeit")) return act("FORFEIT", "");
    if (argc < 4) { fprintf(stderr, "tsc_op: %s needs a value\n", verb); return 2; }
    if (!strcmp(verb, "deal")) return act("DEAL", argv[3]);
    if (!strcmp(verb, "answer")) return act("ANSWER", argv[3]);
    if (!strcmp(verb, "miracle")) return act("MIRACLE", argv[3]);
    fprintf(stderr, "tsc_op: unknown verb %s\n", verb); return 2;
}
