/* tear_chain_op - the C side of the tearit WEB chain (same rules, same bytes as &.hq-apps/tearit-web/docs/chain.js; graded against it by harness case tear_offline).
 * Block line (the pal-chain format): BLOCK|index|prev_hash|nonce|hash|timestamp|miner|tx_list ; hash = SHA-256("index|prev|nonce|tx_list"), N leading hex zeros.
 * Rows in tx_list: TX|from|to|amount|ts|id (balance checked)  MSG|from|channel|text|ts  FAUCET|wallet|amount|ts (<= faucetMax, faucetGap seconds apart per wallet).
 * Reward halves; supply capped. Fork rule: both copies verified, most work wins, a tie keeps the first. No signatures yet (owner 2026-11-24).
 *
 * Usage (every command prints ONE result line on stdout, exit 0 ok / 1 refused / 2 usage; add `--zeros N` anywhere, default 2):
 *   tear_chain_op verify   <file>                      OK height=H tip=<hash> work=W   |   BAD at=I why=<text>
 *   tear_chain_op balance  <file> <wallet>             <millicones>                     (exit 1 if the chain does not verify)
 *   tear_chain_op mine     <file> <miner> <ts> [row..] mines ONE block with the rows, verifies the result, appends it   -> MINED index=I tries=N
 *   tear_chain_op apply    <file> <miner> <ts> <rowsfile>  steward rule: each row tried alone, accepted rows in one block -> APPLIED accepted=A rejected=R mined=0|1
 *   tear_chain_op merge    <a> <b> <out>               fork rule, writes the winner to <out>            -> MERGE winner=a|b reason=<text>
 *   tear_chain_op messages <file> <channel|*>          one `from<TAB>text` line per message (verified chains only)
 *   tear_chain_op tail     <file> <n>                  the last n blocks as `#index hash10 miner rows`
  * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tear_chain_op.+x tear_chain_op.c -lcrypto -lm   (a failed link DELETES the old binary: check that the file exists after building) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>
#include <openssl/sha.h>

#define MAXW 512
#define NAMEL 64
typedef struct { int zeros; long initial, halving, supply, fmax, fgap; } Cfg;
static Cfg C = { 2, 10500, 1000, 21000000, 1000, 3600 };
typedef struct { long index; char prev[72], hash[72], nonce[40], miner[NAMEL]; long ts; char *txl; } Blk;
typedef struct { char name[NAMEL]; long bal, last; int has_last; } W;
typedef struct { W w[MAXW]; int n; } Bal;
typedef struct { char name[NAMEL], owner[NAMEL], seed[48], payee[NAMEL]; int spec, sx, sy, x, y, stats[6], age, life, fat, frozen, retired, gym, barn, wins, cups, best, items[3], buffT, buffF, pvpW, pvpL, arenaW, arenaL; long price; } Pet;
typedef struct { Pet p[64]; int n; } Pets;
typedef struct { int ok; long height; double work; char tip[72]; char why[160]; long at; Bal bal; Pets pets; void *ar; char **mf, **mc, **mt; long *mts; int nm; } Ver;

static void sha_hex(const char *s, char out[65]) { unsigned char d[32]; SHA256((const unsigned char *)s, strlen(s), d); for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", d[i]); out[64] = 0; }
static const char ZERO[] = "0000000000000000000000000000000000000000000000000000000000000000";
static W *wal(Bal *b, const char *n) { for (int i = 0; i < b->n; i++) if (!strcmp(b->w[i].name, n)) return &b->w[i]; if (b->n >= MAXW) return NULL; W *w = &b->w[b->n++]; memset(w, 0, sizeof *w); snprintf(w->name, NAMEL, "%s", n); return w; }
static long reward(long i) { long e = i / C.halving; if (e > 40) return 0; return (long)floor((double)C.initial / pow(2, (double)e)); }
static int meets(const char *h, int z) { for (int i = 0; i < z; i++) if (h[i] != '0') return 0; return 1; }
/* ---- THE RANCH (port of the ranch rules in tearit-web docs/chain.js: same data, same order of checks, same reasons; parity is graded by harness case tear_offline) ---- */
static const char *SPEC[10] __attribute__((unused)) = { "Fire", "Water", "Earth", "Wind", "Light", "Dark", "Lightning", "Ice", "Cosmic", "Celestial" };
static const char *STN[6] __attribute__((unused)) = { "LIF", "POW", "INT", "SKI", "SPD", "DEF" };
typedef struct { const char *n; long cost, fat; int stat; long k; } Food;
static const Food FOODS[4] = { { "bread", 5, 5, -1, 0 }, { "meat", 20, 15, 1, 1 }, { "fruit", 12, 10, 2, 1 }, { "tonic", 50, 30, 0, 2 } };
typedef struct { const char *n; int stat; long cost, gain, fat; } Drill;
static const Drill DRILLS[6] = { { "sprint", 4, 10, 3, 12 }, { "lift", 1, 10, 3, 12 }, { "study", 2, 10, 3, 8 }, { "spar", 3, 15, 3, 15 }, { "guard", 5, 10, 3, 10 }, { "jog", 0, 10, 2, 6 } };
typedef struct { const char *n; long entry, prize, rival; } Rank;
static const Rank RANKS[6] = { { "E", 10, 60, 90 }, { "D", 20, 130, 125 }, { "C", 40, 280, 160 }, { "B", 80, 600, 200 }, { "A", 160, 1300, 245 }, { "S", 320, 3000, 295 } };
typedef struct { const char *n; long cost, n2; } Item;
static const Item ITEMS[3] = { { "dumbbell", 30, 2 }, { "charm", 40, 20 }, { "elixir", 60, 0 } };
#define RESTFAT 40
#define BARNREST 10
#define TIREDHALF 60
#define TIREDMAX 100
#define STATCAP 99
#define COMPFAT 20
#define ROLL 40
#define MAXPETS 16
#define MAXACTIVE 6
#define GCOLS 12
#define GROWS 8
static int roll_of(const char *s) { char h[65]; sha_hex(s, h); char two[3] = { h[0], h[1], 0 }; return (int)(strtol(two, NULL, 16) % ROLL); }
static int ok_pet_id(const char *s) { size_t l = strlen(s); if (!l || l > 24) return 0; for (; *s; s++) if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '_' || *s == '-')) return 0; return 1; }
static Pet *pet_of(Pets *P, const char *n) { for (int i = 0; i < P->n; i++) if (!strcmp(P->p[i].name, n)) return &P->p[i]; return NULL; }
static int power_of(const Pet *p) { return p->stats[1] + p->stats[2] + p->stats[3] + p->stats[4] + p->stats[5]; }
static int blocked_cell(int x, int y) { return (x == 1 && y == 1) || (x == 10 && y == 1); }
static int cell_free(Pets *P, int x, int y, Pet *self) { if (x < 0 || y < 0 || x >= GCOLS || y >= GROWS || blocked_cell(x, y)) return 0; for (int i = 0; i < P->n; i++) { Pet *q = &P->p[i]; if (q != self && !q->frozen && q->x == x && q->y == y) return 0; } return 1; }
static void place_pet(Pets *P, Pet *p) { for (int i = 0; i < GCOLS * GROWS; i++) { int x = (p->sx + i) % GCOLS, y = (p->sy + (p->sx + i) / GCOLS) % GROWS; if (cell_free(P, x, y, p)) { p->x = x; p->y = y; return; } } p->x = 0; p->y = 0; }
static int active_of(Pets *P, const char *o) { int n = 0; for (int i = 0; i < P->n; i++) if (!strcmp(P->p[i].owner, o) && !P->p[i].frozen) n++; return n; }
static const char *VERBS[24] = { "TX", "MSG", "FAUCET", "PET", "FEED", "TRAIN", "REST", "COMPETE", "UPGRADE", "FREEZE", "THAW", "TRANSFER", "LIST", "UNLIST", "BUY", "BUYITEM", "USEITEM", "GIVEITEM", "CHALLENGE", "ARENA", "JOIN", "LEAVE", "FIGHT", "FILL" };
static int is_verb(const char *v) { for (int i = 0; i < 24; i++) if (!strcmp(VERBS[i], v)) return 1; return !strcmp(v, "MOVE"); }
static int row_len(const char *v) { static const struct { const char *v; int n; } T[] = { { "FEED", 5 }, { "TRAIN", 5 }, { "REST", 4 }, { "COMPETE", 5 }, { "UPGRADE", 5 }, { "FREEZE", 4 }, { "THAW", 4 }, { "TRANSFER", 5 }, { "LIST", 6 }, { "UNLIST", 4 }, { "BUY", 5 }, { "BUYITEM", 5 }, { "USEITEM", 5 }, { "GIVEITEM", 6 }, { "CHALLENGE", 5 }, { "MOVE", 6 } }; for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcmp(T[i].v, v)) return T[i].n; return 0; }
/* ---- THE ARENA (port of arenaRow in chain.js: same checks in the same order, same reasons, same arithmetic) ---- */
typedef struct { char name[NAMEL]; int ai, power; } AMem;
typedef struct { char id[NAMEL], mode[8], owner[NAMEL]; AMem m[8]; int nm; long pot; int open, nw; char win[8][NAMEL]; int ns; int sc[8]; } Arena;
typedef struct { Arena a[16]; int n; } Arenas;
#define ARENA_ENTRY 20
#define ARENA_RAKE 10
#define ARENA_AIBASE 90
#define ARENA_AISPAN 40
static int arena_mode(const char *m, int *mn, int *mx) { if (!strcmp(m, "1v1")) { *mn = 2; *mx = 2; return 1; } if (!strcmp(m, "2v2")) { *mn = 4; *mx = 4; return 1; } if (!strcmp(m, "ffa")) { *mn = 3; *mx = 8; return 1; } return 0; }
static int hex2(const char *h) { char two[3] = { h[0], h[1], 0 }; return (int)strtol(two, NULL, 16); }
static int arena_has(Arena *a, const char *n) { for (int j = 0; j < a->nm; j++) if (!strcmp(a->m[j].name, n)) return 1; return 0; }
static const char *arena_row(Bal *bal, Pets *P, Arenas *AR, char **f, int nf, char *why) {
    const char *v = f[0], *id = nf > 1 ? f[1] : ""; Arena *a = NULL; int mn, mx; Pet *p, *q; char tmp[200], h[65];
    if (!strcmp(v, "ARENA")) { if (nf != 5) return "bad ARENA row"; if (!ok_pet_id(id) || id[0] == '_') return "bad arena name";
        for (int i = 0; i < AR->n; i++) if (!strcmp(AR->a[i].id, id)) return "arena name already used";
        if (!arena_mode(f[2], &mn, &mx)) return "unknown mode"; if (!ok_pet_id(f[3])) return "bad owner"; if (AR->n >= 16) return "too many arenas on this chain";
        a = &AR->a[AR->n++]; memset(a, 0, sizeof *a); snprintf(a->id, NAMEL, "%s", id); snprintf(a->mode, 8, "%s", f[2]); snprintf(a->owner, NAMEL, "%s", f[3]); a->open = 1; return NULL; }
    for (int i = 0; i < AR->n; i++) if (!strcmp(AR->a[i].id, id)) a = &AR->a[i];
    if (!a) return "no such arena"; int len = !strcmp(v, "FIGHT") || !strcmp(v, "FILL") ? 4 : 5; if (nf != len) { snprintf(why, 160, "bad %s row", v); return why; } if (!a->open) return "that arena is closed";
    arena_mode(a->mode, &mn, &mx);
    if (!strcmp(v, "JOIN") || !strcmp(v, "LEAVE")) { p = pet_of(P, f[2]); if (!p) return "no such pet"; if (strcmp(f[3], p->owner)) { snprintf(why, 160, "not the owner of %s", p->name); return why; }
        if (!strcmp(v, "LEAVE")) { if (!arena_has(a, p->name)) { snprintf(why, 160, "%s is not in that arena", p->name); return why; } for (int j = 0; j < a->nm; j++) if (!strcmp(a->m[j].name, p->name)) { memmove(&a->m[j], &a->m[j + 1], sizeof(AMem) * (size_t)(a->nm - j - 1)); a->nm--; break; } a->pot -= ARENA_ENTRY; W *w = wal(bal, p->name); if (w) w->bal += ARENA_ENTRY; return NULL; }
        if (p->frozen) { snprintf(why, 160, "%s is frozen", p->name); return why; } if (p->retired) { snprintf(why, 160, "%s has retired", p->name); return why; } if (p->fat >= TIREDMAX) { snprintf(why, 160, "%s is too tired", p->name); return why; }
        for (int k = 0; k < AR->n; k++) if (AR->a[k].open && arena_has(&AR->a[k], p->name)) { snprintf(why, 160, "%s is already in an arena", p->name); return why; }
        if (a->nm >= mx) return "the arena is full"; W *w = wal(bal, p->name); if (!w || w->bal < ARENA_ENTRY) { snprintf(why, 160, "not enough coins in %s", p->name); return why; } w->bal -= ARENA_ENTRY;
        a->pot += ARENA_ENTRY; AMem *m = &a->m[a->nm++]; memset(m, 0, sizeof *m); snprintf(m->name, NAMEL, "%s", p->name); return NULL; }
    if (!strcmp(v, "FILL")) { if (strcmp(f[2], a->owner)) return "only the arena owner can fill it"; int want = mn > 2 ? mn : 2; if (a->nm >= want) return "the arena is already full enough"; int n0 = 0; for (int j = 0; j < a->nm; j++) if (a->m[j].ai) n0++;
        while (a->nm < want) { n0++; AMem *m = &a->m[a->nm++]; memset(m, 0, sizeof *m); snprintf(m->name, NAMEL, "ai-%d", n0); m->ai = 1; snprintf(tmp, sizeof tmp, "ai|%s|%s", id, m->name); sha_hex(tmp, h); m->power = ARENA_AIBASE + hex2(h) % ARENA_AISPAN; } return NULL; }
    /* FIGHT */ { int owns = !strcmp(f[2], a->owner), reals = 0; for (int j = 0; j < a->nm; j++) if (!a->m[j].ai) { reals++; Pet *mp = pet_of(P, a->m[j].name); if (mp && !strcmp(mp->owner, f[2])) owns = 1; }
        if (!owns) return "you are not in that arena"; if (!reals) return "no pets in the arena";
        if (!strcmp(a->mode, "ffa") ? a->nm < mn : a->nm != mx) { snprintf(why, 160, "the arena needs %s%d fighters", !strcmp(a->mode, "ffa") ? "at least " : "", !strcmp(a->mode, "ffa") ? mn : mx); return why; } }
    for (int j = 0; j < a->nm; j++) { AMem *m = &a->m[j]; if (m->ai) { snprintf(tmp, sizeof tmp, "ai|%s|%s|roll", id, m->name); sha_hex(tmp, h); a->sc[j] = m->power + hex2(h) % ROLL; }
        else { q = pet_of(P, m->name); snprintf(tmp, sizeof tmp, "%s|%s|%s|arena", q->seed, id, m->name); sha_hex(tmp, h); a->sc[j] = power_of(q) - q->fat / 2 + hex2(h) % ROLL; } }
    a->ns = a->nm; long rake = a->pot * ARENA_RAKE / 100, net = a->pot - rake; W *pool = wal(bal, "_ranch"); if (pool) pool->bal += rake; int wins[3], nw = 0; long shares[3];
    if (!strcmp(a->mode, "1v1")) { int k = a->sc[1] > a->sc[0] ? 1 : 0; wins[0] = k; nw = 1; shares[0] = net; }
    else if (!strcmp(a->mode, "2v2")) { int k = (a->sc[1] + a->sc[3]) > (a->sc[0] + a->sc[2]) ? 1 : 0; wins[0] = k; wins[1] = k + 2; nw = 2; shares[0] = net / 2 + net % 2; shares[1] = net / 2; }
    else { int ord[8]; for (int j = 0; j < a->nm; j++) ord[j] = j; for (int x = 1; x < a->nm; x++) { int t = ord[x], y = x - 1; while (y >= 0 && (a->sc[t] > a->sc[ord[y]] || (a->sc[t] == a->sc[ord[y]] && t < ord[y]))) { ord[y + 1] = ord[y]; y--; } ord[y + 1] = t; }
        nw = 3; for (int j = 0; j < 3; j++) wins[j] = ord[j]; long s1 = net * 50 / 100, s2 = net * 30 / 100, s3 = net * 20 / 100; shares[0] = s1 + (net - s1 - s2 - s3); shares[1] = s2; shares[2] = s3; }
    a->nw = nw; for (int j = 0; j < nw; j++) { AMem *m = &a->m[wins[j]]; snprintf(a->win[j], NAMEL, "%s", m->name); if (m->ai) { if (pool) pool->bal += shares[j]; } else { W *w = wal(bal, m->name); if (w) w->bal += shares[j]; pet_of(P, m->name)->arenaW++; } }
    for (int j = 0; j < a->nm; j++) { AMem *m = &a->m[j]; if (m->ai) continue; q = pet_of(P, m->name); int won = 0; for (int k = 0; k < nw; k++) if (!strcmp(a->win[k], m->name)) won = 1; if (!won) q->arenaL++; q->fat += COMPFAT; if (q->fat > TIREDMAX) q->fat = TIREDMAX; q->age++; if (q->age >= q->life) q->retired = 1; }
    a->pot = 0; a->open = 0; return NULL;
}
/* returns NULL when allowed (and applied), else the reason (static text or the buffer why) */
static const char *ranch_row(Bal *bal, Pets *P, Arenas *AR, char **f, int nf, char *why) {
    const char *v = f[0]; const char *nm = nf > 1 ? f[1] : ""; Pet *p, *q; W *w, *w2;
    if (!strcmp(f[0], "ARENA") || !strcmp(f[0], "JOIN") || !strcmp(f[0], "LEAVE") || !strcmp(f[0], "FIGHT") || !strcmp(f[0], "FILL")) return arena_row(bal, P, AR, f, nf, why);
    #define SPEND(petn, amt) do { W *sw = wal(bal, (petn)); if (!sw || sw->bal < (amt)) { snprintf(why, 160, "not enough coins in %s", (petn)); return why; } sw->bal -= (amt); W *rp = wal(bal, "_ranch"); if (rp) rp->bal += (amt); } while (0)
    #define WEEK(pp) do { (pp)->age++; if ((pp)->age >= (pp)->life) (pp)->retired = 1; } while (0)
    #define ADDSTAT(pp, s, nn) do { (pp)->stats[s] += (nn); if ((pp)->stats[s] > STATCAP) (pp)->stats[s] = STATCAP; } while (0)
    if (!strcmp(v, "PET")) {
        if (nf != 5) return "bad PET row"; if (!ok_pet_id(nm) || nm[0] == '_') return "bad pet name"; if (pet_of(P, nm)) return "name already used";
        if (!f[2][0] || strlen(f[2]) > 40) return "bad seed words"; if (!ok_pet_id(f[3])) return "bad owner";
        int cnt = 0; for (int i = 0; i < P->n; i++) if (!strcmp(P->p[i].owner, f[3])) cnt++; if (cnt >= MAXPETS) { snprintf(why, 160, "too many pets for %s", f[3]); return why; }
        if (active_of(P, f[3]) >= MAXACTIVE) return "six pets are already out: freeze one first"; if (P->n >= 64) return "too many pets on this chain";
        char seedname[160], h[65]; snprintf(seedname, sizeof seedname, "%s|%s", f[2], nm); sha_hex(seedname, h); int b[32]; for (int i = 0; i < 32; i++) { char two[3] = { h[2 * i], h[2 * i + 1], 0 }; b[i] = (int)strtol(two, NULL, 16); }
        p = &P->p[P->n++]; memset(p, 0, sizeof *p); snprintf(p->name, NAMEL, "%s", nm); snprintf(p->owner, NAMEL, "%s", f[3]); snprintf(p->seed, 48, "%s", f[2]); p->spec = b[6] % 10; for (int i = 0; i < 6; i++) p->stats[i] = 10 + b[i] % 21;
        p->life = 120 + b[7] % 61; p->sx = b[12] % GCOLS; p->sy = b[13] % GROWS; p->best = -1; place_pet(P, p); return NULL; }
    int len = row_len(v); p = pet_of(P, nm); if (!p) return "no such pet"; if (nf != len) { snprintf(why, 160, "bad %s row", v); return why; }
    if (!strcmp(v, "BUY")) { q = pet_of(P, f[2]); if (!(p->price > 0)) { snprintf(why, 160, "%s is not for sale", nm); return why; } if (!q) return "no such paying pet"; if (strcmp(q->owner, f[3])) return "the paying pet is not yours";
        if (!strcmp(p->owner, f[3])) { snprintf(why, 160, "you already own %s", nm); return why; } if (!p->frozen && active_of(P, f[3]) >= MAXACTIVE) return "six pets are already out: freeze one first";
        w = wal(bal, f[2]); if (!w || w->bal < p->price) { snprintf(why, 160, "not enough coins in %s", f[2]); return why; } w->bal -= p->price; w2 = wal(bal, p->payee); if (w2) w2->bal += p->price; snprintf(p->owner, NAMEL, "%s", f[3]); p->price = 0; p->payee[0] = 0; return NULL; }
    if (strcmp(f[len - 2], p->owner)) { snprintf(why, 160, "not the owner of %s", nm); return why; }
    if (!strcmp(v, "THAW")) { if (!p->frozen) return "not frozen"; if (active_of(P, p->owner) >= MAXACTIVE) return "six pets are already out: freeze one first"; p->frozen = 0; if (!cell_free(P, p->x, p->y, p)) place_pet(P, p); return NULL; }
    if (!strcmp(v, "FREEZE")) { if (p->frozen) return "already frozen"; p->frozen = 1; return NULL; }
    if (!strcmp(v, "TRANSFER")) { if (!ok_pet_id(f[2])) return "bad new owner"; if (!p->frozen && active_of(P, f[2]) >= MAXACTIVE) { snprintf(why, 160, "%s already has six pets out", f[2]); return why; } snprintf(p->owner, NAMEL, "%s", f[2]); p->price = 0; p->payee[0] = 0; return NULL; }
    if (!strcmp(v, "LIST")) { char *e; double pr = strtod(f[2], &e); if (!(pr > 0) || floor(pr) != pr || pr > 1000000) return "bad price"; q = pet_of(P, f[3]); if (!q || strcmp(q->owner, p->owner)) return "the payee must be one of your pets"; p->price = (long)pr; snprintf(p->payee, NAMEL, "%s", f[3]); return NULL; }
    if (!strcmp(v, "UNLIST")) { if (!(p->price > 0)) return "not listed"; p->price = 0; p->payee[0] = 0; return NULL; }
    if (!strcmp(v, "GIVEITEM")) { q = pet_of(P, f[3]); if (!q) return "no such pet to give to"; int it = -1; for (int i = 0; i < 3; i++) if (!strcmp(ITEMS[i].n, f[2])) it = i; if (it < 0) return "unknown item"; if (!(p->items[it] > 0)) { snprintf(why, 160, "%s has no %s", nm, f[2]); return why; } p->items[it]--; q->items[it]++; return NULL; }
    if (p->frozen) { snprintf(why, 160, "%s is frozen", nm); return why; } if (p->retired) { snprintf(why, 160, "%s has retired", nm); return why; }
    if (!strcmp(v, "MOVE")) { char *e1, *e2; double mx = strtod(f[2], &e1), my = strtod(f[3], &e2); if (floor(mx) != mx || floor(my) != my) return "bad cell"; if (!cell_free(P, (int)mx, (int)my, p)) return "that cell is not free"; p->x = (int)mx; p->y = (int)my; return NULL; }
    if (!strcmp(v, "FEED")) { int fi = -1; for (int i = 0; i < 4; i++) if (!strcmp(FOODS[i].n, f[2])) fi = i; if (fi < 0) return "unknown food"; SPEND(nm, FOODS[fi].cost); p->fat -= (int)FOODS[fi].fat; if (p->fat < 0) p->fat = 0; if (FOODS[fi].stat >= 0) ADDSTAT(p, FOODS[fi].stat, (int)FOODS[fi].k); WEEK(p); return NULL; }
    if (!strcmp(v, "REST")) { p->fat -= RESTFAT + p->barn * BARNREST; if (p->fat < 0) p->fat = 0; WEEK(p); return NULL; }
    if (!strcmp(v, "TRAIN")) { int di = -1; for (int i = 0; i < 6; i++) if (!strcmp(DRILLS[i].n, f[2])) di = i; if (di < 0) return "unknown drill"; if (p->fat >= TIREDMAX) { snprintf(why, 160, "%s is too tired", nm); return why; } SPEND(nm, DRILLS[di].cost);
        int gain = p->fat >= TIREDHALF ? (int)(DRILLS[di].gain / 2) : (int)DRILLS[di].gain + p->gym + (p->buffT ? (int)ITEMS[0].n2 : 0); p->buffT = 0; ADDSTAT(p, DRILLS[di].stat, gain); p->fat += (int)DRILLS[di].fat; if (p->fat > TIREDMAX) p->fat = TIREDMAX; WEEK(p); return NULL; }
    if (!strcmp(v, "UPGRADE")) { int isg = !strcmp(f[2], "gym"), isb = !strcmp(f[2], "barn"); if (!isg && !isb) return "unknown facility"; int lv = isg ? p->gym : p->barn; if (lv >= 3) return "already at the top level"; long cost = (isg ? 100 : 80) * (lv + 1); SPEND(nm, cost); if (isg) p->gym++; else p->barn++; return NULL; }
    if (!strcmp(v, "BUYITEM")) { int it = -1; for (int i = 0; i < 3; i++) if (!strcmp(ITEMS[i].n, f[2])) it = i; if (it < 0) return "unknown item"; SPEND(nm, ITEMS[it].cost); p->items[it]++; return NULL; }
    if (!strcmp(v, "USEITEM")) { int it = -1; for (int i = 0; i < 3; i++) if (!strcmp(ITEMS[i].n, f[2])) it = i; if (it < 0) return "unknown item"; if (!(p->items[it] > 0)) { snprintf(why, 160, "%s has no %s", nm, f[2]); return why; } p->items[it]--; if (it == 0) p->buffT = 1; else if (it == 1) p->buffF = 1; else p->fat = 0; return NULL; }
    if (!strcmp(v, "COMPETE")) { int ri = -1; for (int i = 0; i < 6; i++) if (!strcmp(RANKS[i].n, f[2])) ri = i; if (ri < 0) return "unknown rank"; if (p->fat >= TIREDMAX) { snprintf(why, 160, "%s is too tired", nm); return why; }
        if (p->best < ri - 1) { snprintf(why, 160, "%s must beat rank %s first", nm, RANKS[ri - 1].n); return why; } SPEND(nm, RANKS[ri].entry);
        char rs[200]; snprintf(rs, sizeof rs, "%s|%s|%s|%d", p->seed, nm, f[2], p->age); int rv = roll_of(rs); int sc = power_of(p) - p->fat / 2 + rv + (p->buffF ? (int)ITEMS[1].n2 : 0); p->buffF = 0; p->cups++;
        if (sc >= RANKS[ri].rival) { W *pool = wal(bal, "_ranch"); long prize = RANKS[ri].prize < pool->bal ? RANKS[ri].prize : pool->bal; pool->bal -= prize; W *me = wal(bal, nm); me->bal += prize; p->wins++; if (ri > p->best) p->best = ri; }
        p->fat += COMPFAT; if (p->fat > TIREDMAX) p->fat = TIREDMAX; WEEK(p); return NULL; }
    if (!strcmp(v, "CHALLENGE")) { q = pet_of(P, f[2]); if (!q) return "no such opponent"; if (q == p) return "a pet cannot challenge itself"; if (q->frozen || q->retired) { snprintf(why, 160, "%s cannot fight now", f[2]); return why; } if (p->fat >= TIREDMAX) { snprintf(why, 160, "%s is too tired", nm); return why; }
        char ra[200], rb[200]; snprintf(ra, sizeof ra, "%s|%s|%s|%d", p->seed, nm, f[2], p->age); snprintf(rb, sizeof rb, "%s|%s|%s|%d", q->seed, f[2], nm, q->age);
        if (power_of(p) - p->fat / 2 + roll_of(ra) >= power_of(q) - q->fat / 2 + roll_of(rb)) { p->pvpW++; q->pvpL++; } else { p->pvpL++; q->pvpW++; } p->fat += COMPFAT; if (p->fat > TIREDMAX) p->fat = TIREDMAX; WEEK(p); return NULL; }
    return "unknown row";
}

static char *slurp(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) { *n = 0; return NULL; } fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET); char *b = malloc((size_t)l + 1); size_t r = fread(b, 1, (size_t)l, f); b[r] = 0; fclose(f); *n = r; return b; }

/* parse text into blocks; returns count, or -1 with err set */
static long parse(char *text, Blk **out, char *err, size_t es) {
    long cap = 64, n = 0; Blk *bl = malloc(sizeof(Blk) * (size_t)cap); long ln = 0;
    for (char *p = text; *p;) { char *e = strchr(p, '\n'); if (e) *e = 0; ln++;
        if (*p) { char *f[8]; int k = 0; char *q = p; while (k < 7) { f[k++] = q; char *bar = strchr(q, '|'); if (!bar) break; *bar = 0; q = bar + 1; }
            if (k < 7 || strcmp(f[0], "BLOCK")) { snprintf(err, es, "line %ld is not a BLOCK row", ln); free(bl); return -1; }
            if (n == cap) { cap *= 2; bl = realloc(bl, sizeof(Blk) * (size_t)cap); }
            Blk *b = &bl[n++]; b->index = atol(f[1]); snprintf(b->prev, 72, "%s", f[2]); snprintf(b->nonce, 40, "%s", f[3]); snprintf(b->hash, 72, "%s", f[4]); b->ts = atol(f[5]); snprintf(b->miner, NAMEL, "%s", f[6]); b->txl = q; }
        if (!e) { break; }
        p = e + 1; }
    *out = bl; return n; }

/* split tx_list into rows at ';' (copy into rows[], each malloc'd, trimmed) like the JS splitRows */
static int split_rows(const char *t, char ***rows) {
    int cap = 8, n = 0; char **r = malloc(sizeof(char *) * (size_t)cap); const char *p = t;
    while (*p) { const char *e = strchr(p, ';'); size_t l = e ? (size_t)(e - p) : strlen(p); const char *s0 = p; while (l && isspace((unsigned char)*s0)) { s0++; l--; } while (l && isspace((unsigned char)s0[l - 1])) l--;
        if (l) { if (n == cap) { cap *= 2; r = realloc(r, sizeof(char *) * (size_t)cap); } r[n++] = strndup(s0, l); } if (!e) { break; } p = e + 1; }
    *rows = r; return n; }

static int fields(char *s, char **f, int max) { int k = 0; f[k++] = s; for (char *p = s; *p && k < max; p++) if (*p == '|') { *p = 0; f[k++] = p + 1; } return k; }
static void fail(Ver *v, long at, const char *why) { v->ok = 0; v->at = at; snprintf(v->why, sizeof v->why, "%s", why); }

static void verify(char *text, Ver *v) {
    memset(v, 0, sizeof *v); v->ok = 1; snprintf(v->tip, 72, "%s", ZERO); v->ar = calloc(1, sizeof(Arenas));
    char err[160]; Blk *bl; long n = parse(text, &bl, err, sizeof err); if (n < 0) { fail(v, 0, err); return; }
    long minted = 0; char pre[8192]; v->mf = calloc(256, sizeof(char *)); v->mc = calloc(256, sizeof(char *)); v->mt = calloc(256, sizeof(char *)); v->mts = calloc(256, sizeof(long));
    for (long i = 0; i < n && v->ok; i++) { Blk *b = &bl[i]; v->height = i;
        if (b->index != i) { snprintf(err, sizeof err, "index %ld expected %ld", b->index, i); fail(v, i, err); break; }
        if (strcmp(b->prev, i ? bl[i - 1].hash : ZERO)) { fail(v, i, "prev_hash does not match the tip"); break; }
        snprintf(pre, sizeof pre, "%ld|%s|%s|%s", i, b->prev, b->nonce, b->txl); char h[65]; sha_hex(pre, h);
        if (strcmp(h, b->hash)) { fail(v, i, "hash does not recompute"); break; }
        if (!meets(b->hash, C.zeros)) { fail(v, i, "difficulty not met"); break; }
        char **rows; int nr = split_rows(b->txl, &rows);
        for (int j = 0; j < nr && v->ok; j++) { char tmp[4096]; snprintf(tmp, sizeof tmp, "%s", rows[j]); char *f[8]; int k = fields(tmp, f, 8);
            if (!strcmp(f[0], "TX")) { char *e; double amt = k > 3 ? strtod(f[3], &e) : 0; if (k < 4 || !(amt > 0) || floor(amt) != amt) { fail(v, i, "bad amount"); break; }
                W *a = wal(&v->bal, f[1]); if (!a || a->bal < (long)amt) { snprintf(err, sizeof err, "insufficient balance for %s", f[1]); fail(v, i, err); break; } a->bal -= (long)amt; W *c = wal(&v->bal, f[2]); if (c) c->bal += (long)amt; }
            else if (!strcmp(f[0], "FAUCET")) { double fa = k > 2 ? atof(f[2]) : 0; long ft = k > 3 ? atol(f[3]) : 0; if (!(fa > 0) || fa > (double)C.fmax) { fail(v, i, "faucet amount over the cap"); break; }
                W *w = wal(&v->bal, f[1]); if (!w) break; if (w->has_last && ft - w->last < C.fgap) { fail(v, i, "faucet cooldown"); break; } w->last = ft; w->has_last = 1; w->bal += (long)fa; }
            else if (!strcmp(f[0], "MSG")) { if (k != 5) { fail(v, i, "bad MSG row"); break; } if (v->nm < 256) { v->mf[v->nm] = strdup(f[1]); v->mc[v->nm] = strdup(f[2]); v->mt[v->nm] = strdup(f[3]); v->mts[v->nm] = atol(f[4]); v->nm++; } }
            else if (!is_verb(f[0])) { snprintf(err, sizeof err, "unknown row %s", f[0]); fail(v, i, err); break; }
            else { char wb[160]; const char *rs = ranch_row(&v->bal, &v->pets, (Arenas *)v->ar, f, k, wb); if (rs) { fail(v, i, rs); break; } } }
        for (int j = 0; j < nr; j++) { free(rows[j]); }
        free(rows); if (!v->ok) break;
        long r = reward(i); if (minted + r > C.supply) r = C.supply - minted; minted += r; W *m = wal(&v->bal, b->miner); if (m) m->bal += r; v->work += pow(16, C.zeros); }
    if (v->ok) { v->height = n; if (n) snprintf(v->tip, 72, "%s", bl[n - 1].hash); }
}

static int do_mine(const char *file, const char *miner, long ts, char **rows, int nr, long *idx_out, long *tries_out) {
    size_t n; char *t = slurp(file, &n); if (!t) { t = strdup(""); } char *copy = strdup(t); Blk *bl; char err[160]; long nb = parse(copy, &bl, err, sizeof err); if (nb < 0) return 1;
    char prev[72]; snprintf(prev, 72, "%s", nb ? bl[nb - 1].hash : ZERO); char txl[8192] = ""; for (int i = 0; i < nr; i++) { if (i) strcat(txl, ";"); strncat(txl, rows[i], sizeof txl - strlen(txl) - 1); }
    char pre[9000], h[65]; long nonce; for (nonce = 0; nonce < 4000000; nonce++) { snprintf(pre, sizeof pre, "%ld|%s|%ld|%s", nb, prev, nonce, txl); sha_hex(pre, h); if (meets(h, C.zeros)) break; }
    if (nonce >= 4000000) return 1;
    char line[10000]; snprintf(line, sizeof line, "BLOCK|%ld|%s|%ld|%s|%ld|%s|%s\n", nb, prev, nonce, h, ts, miner, txl);
    char *nt = malloc(strlen(t) + strlen(line) + 1); strcpy(nt, t); strcat(nt, line); Ver v; verify(nt, &v); if (!v.ok) { snprintf(err, sizeof err, "%s", v.why); fprintf(stderr, "refused: %s\n", err); return 1; }
    FILE *f = fopen(file, "a"); if (!f) return 1; fputs(line, f); fclose(f); *idx_out = nb; *tries_out = nonce + 1; return 0; }

static int ok_id(const char *s) { size_t l = strlen(s); if (!l || l >= NAMEL) return 0; for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '_' || *s == '-')) return 0; return 1; }

int main(int argc, char **argv) {
    char *av[64]; int ac = 0; for (int i = 1; i < argc && ac < 64; i++) { if (!strcmp(argv[i], "--zeros") && i + 1 < argc) { C.zeros = atoi(argv[++i]); continue; } av[ac++] = argv[i]; }
    if (ac < 2) { fprintf(stderr, "usage: tear_chain_op verify|balance|mine|apply|merge|messages|tail <file> ...\n"); return 2; }
    const char *cmd = av[0], *file = av[1]; size_t n; Ver v;
    if (!strcmp(cmd, "verify") || !strcmp(cmd, "balance") || !strcmp(cmd, "messages") || !strcmp(cmd, "tail") || !strcmp(cmd, "pets") || !strcmp(cmd, "arenas")) {
        char *t = slurp(file, &n); if (!t) t = strdup(""); char *keep = strdup(t); verify(t, &v);
        if (!strcmp(cmd, "verify")) { if (v.ok) { printf("OK height=%ld tip=%s work=%.0f\n", v.height, v.tip, v.work); return 0; } printf("BAD at=%ld why=%s\n", v.at, v.why); return 1; }
        if (!v.ok) { printf("BAD at=%ld why=%s\n", v.at, v.why); return 1; }
        if (!strcmp(cmd, "pets")) { for (int i = 0; i < v.pets.n; i++) { Pet *q = &v.pets.p[i]; W *pw = wal(&v.bal, q->name); printf("%s|%s|%s|%d/%d|%d|%d,%d|%d %d %d %d %d %d|%d/%d|%d|%ld|%s|%d|%d|%d-%d|%d/%d|%ld|%d %d %d\n", q->name, q->owner, SPEC[q->spec], q->age, q->life, q->fat, q->x, q->y, q->stats[0], q->stats[1], q->stats[2], q->stats[3], q->stats[4], q->stats[5], q->wins, q->cups, q->best, q->price, q->payee, q->frozen, q->retired, q->pvpW, q->pvpL, q->gym, q->barn, pw ? pw->bal : 0, q->items[0], q->items[1], q->items[2]); } return 0; }
        if (!strcmp(cmd, "arenas")) { Arenas *AR = (Arenas *)v.ar; for (int i = 0; i < AR->n; i++) { Arena *a = &AR->a[i]; printf("%s|%s|%s|%d|%ld|", a->id, a->mode, a->owner, a->open, a->pot); for (int j = 0; j < a->nm; j++) printf("%s%s", j ? "," : "", a->m[j].name); printf("|"); for (int j = 0; j < a->nw; j++) printf("%s%s", j ? "," : "", a->win[j]); printf("|"); for (int j = 0; j < a->ns; j++) printf("%s%d", j ? "," : "", a->sc[j]); printf("|"); int fw = 0; for (int j = 0; j < a->nm; j++) if (!a->m[j].ai) { Pet *pp = pet_of(&v.pets, a->m[j].name); if (pp) { printf("%s%s:%d-%d", fw++ ? "," : "", pp->name, pp->arenaW, pp->arenaL); } } printf("\n"); } return 0; }
        if (!strcmp(cmd, "balance")) { if (ac < 3) return 2; W *w = wal(&v.bal, av[2]); printf("%ld\n", w ? w->bal : 0); return 0; }
        if (!strcmp(cmd, "messages")) { if (ac < 3) return 2; for (int i = 0; i < v.nm; i++) if (!strcmp(av[2], "*") || !strcmp(av[2], v.mc[i])) printf("%s\t%s\n", v.mf[i], v.mt[i]); return 0; }
        long want = ac > 2 ? atol(av[2]) : 10; Blk *bl; char err[160]; long nb = parse(keep, &bl, err, sizeof err); for (long i = nb > want ? nb - want : 0; i < nb; i++) printf("#%ld %.10s %s %.80s\n", bl[i].index, bl[i].hash, bl[i].miner, bl[i].txl); return 0; }
    if (!strcmp(cmd, "mine")) { if (ac < 4 || !ok_id(av[2])) return 2; long idx, tries; if (do_mine(file, av[2], atol(av[3]), av + 4, ac - 4, &idx, &tries)) { printf("REFUSED\n"); return 1; } printf("MINED index=%ld tries=%ld\n", idx, tries); return 0; }
    if (!strcmp(cmd, "apply")) { if (ac < 5 || !ok_id(av[2])) return 2; size_t rn; char *rt = slurp(av[4], &rn); if (!rt) return 2; char *good[20]; int ng = 0, nbad = 0; long ts = atol(av[3]), idx, tries;
        char *tmpf = strdup("/tmp/tear_chain_apply.XXXXXX"); int fd = mkstemp(tmpf); if (fd < 0) return 2; close(fd);
        for (char *p = rt; *p;) { char *e = strchr(p, '\n'); if (e) *e = 0; while (isspace((unsigned char)*p)) p++; size_t l = strlen(p); while (l && isspace((unsigned char)p[l - 1])) p[--l] = 0;
            if (ng + nbad < 20 && l) { int okrow = (!strncmp(p, "TX|", 3) || !strncmp(p, "MSG|", 4) || !strncmp(p, "FAUCET|", 7)) && l <= 400 && !strchr(p, ';');
                if (!okrow) nbad++; else { size_t cn; char *cur = slurp(file, &cn); FILE *f = fopen(tmpf, "w"); if (cur) fputs(cur, f); fclose(f); char *rows[21]; for (int i = 0; i < ng; i++) rows[i] = good[i]; rows[ng] = p;
                    if (do_mine(tmpf, av[2], ts, rows, ng + 1, &idx, &tries)) nbad++; else good[ng++] = p; } }
            if (!e) { break; }
        p = e + 1; }
        remove(tmpf); int mined = 0; if (ng) { if (!do_mine(file, av[2], ts, good, ng, &idx, &tries)) mined = 1; }
        printf("APPLIED accepted=%d rejected=%d mined=%d\n", ng, nbad, mined); return 0; }
    if (!strcmp(cmd, "merge")) { if (ac < 4) return 2; size_t na, nb2; char *a = slurp(file, &na); if (!a) a = strdup(""); char *b = slurp(av[2], &nb2); if (!b) b = strdup(""); char *ak = strdup(a), *bk = strdup(b); Ver va, vb; verify(a, &va); verify(b, &vb);
        const char *win, *why; if (!va.ok && !vb.ok) { printf("REFUSED neither copy verifies\n"); return 1; } if (!vb.ok) { win = "a"; why = "b rejected"; } else if (!va.ok) { win = "b"; why = "a rejected"; } else if (vb.work > va.work) { win = "b"; why = "more work"; } else { win = "a"; why = va.work == vb.work ? "tie keeps a" : "more work"; }
        FILE *f = fopen(av[3], "w"); if (!f) return 1; fputs(*win == 'a' ? ak : bk, f); fclose(f); printf("MERGE winner=%s reason=%s\n", win, why); return 0; }
    fprintf(stderr, "unknown command %s\n", cmd); return 2; }
