/* deskopoly_chat - the CHAT FRONT DOOR of Teropoly (spec DESIGN-DESKOPOLY-CLASSIC-MONOPOLY-BUILD-SPEC-2026-11-08 section 7.6).
 * A typed line goes in, the engine's own canonical verb comes out. The parser is a PURE function (text + the board's tile names -> one canonical argv, or a refusal);
 * it never writes a file and there is NO second path that appends ledger rows: mode (b) only calls the engine, which appends (or refuses) exactly as for a direct call.
 *
 *   deskopoly_chat --parse <app dir> [ruleset=<name>] <text...>
 *       prints `ok <verb and args>` and `understood: <plain English>`, or `refused: <reason>`; appends nothing. Exit 0 / 2.
 *   deskopoly_chat <app dir> <game dir> [as=<pN>] <text...>
 *       parses (the ruleset is read from the game's lobby row); on ok prints the two lines above, then RUNS `<app>/ops/+x/deskopoly.+x <app> <game> <verb> args.. [as=pN]`
 *       with fork+execv (never a shell: a hostile line cannot reach one) and prints the engine's answer; the exit code is the engine's (0 done, 2 refused).
 *
 * Grammar (case-insensitive; the table of 7.6):
 *   offer pN [give <assets>] [get <assets>]      counter <id> give .. get ..      amend <id> give .. get ..
 *   accept [id]   reject [id]   cancel [id]      bid <n>   pass      build|sell|mortgage|unmortgage <tile>      roll buy skip end
 *   <assets>: tiles by id (`Navy 2`, `navy2`, `Line1`; a unique PREFIX also works, `wat` = Water), by number (`#39`, `tile 39`, `prop:39`), a plain integer = cash
 *             (`100`, `$100`, `cash:100`), a card (`luck`, `fund`, `card:luck`), `nothing`. Filler words are ignored (and plus with the a an my your me you to of please cash
 *             coins money dollars card cards tile tiles property prop then also i); ANY OTHER word is refused as unknown (so `for` can never silently change the meaning).
 *   An ambiguous or unknown name is refused WITH the candidates and nothing is sent. Two money amounts, a tile twice or a side with nothing after its keyword are refused.
 *   Only [a-z0-9 , + & . : # $ _ - ' ! ?] and one leading `/` are accepted; any other byte is refused by code, not echoed.
 * Exit: 0 ok | 2 refused or usage (engine's own code in mode b). No file is written, no runtime file exists. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAXT 64
#define MAXTOK 64
#define TOKLEN 41
#define MAXTEXT 300
#define MAXOFF 8

typedef struct { char key[32], name[32], kind[16]; } Tile;
static Tile TL[MAXT]; static int NT;
static char ERR[600];

static int refuse(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int refuse(const char *fmt, ...) { __builtin_va_list ap; __builtin_va_start(ap, fmt); vsnprintf(ERR, sizeof ERR, fmt, ap); __builtin_va_end(ap); return -1; }

static int ownable(int t) { return !strcmp(TL[t].kind, "street") || !strcmp(TL[t].kind, "rail") || !strcmp(TL[t].kind, "util"); }
static int is_street(int t) { return !strcmp(TL[t].kind, "street"); }

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }

/* board.pdl: `TILE | idx | id | kind | group | price | ...` (the classic format). Names come from here, never from the code. */
static int load_board(const char *app, const char *rs) {
    char p[1100], ln[512]; snprintf(p, sizeof p, "%s/rulesets/%s/board.pdl", app, rs);
    FILE *fp = fopen(p, "r"); if (!fp) { snprintf(p, sizeof p, "%s/board.pdl", app); fp = fopen(p, "r"); }
    if (!fp) return refuse("cannot read the board of ruleset %s", rs);
    NT = 0; memset(TL, 0, sizeof TL);
    while (fgets(ln, sizeof ln, fp)) {
        if (ln[0] == '#') continue; char *f[16]; int n = 0; char *sv = NULL;
        for (char *t = strtok_r(ln, "|", &sv); t && n < 16; t = strtok_r(NULL, "|", &sv)) f[n++] = trim(t);
        if (n < 5 || strcmp(f[0], "TILE")) continue;
        int idx = atoi(f[1]); if (idx < 0 || idx >= MAXT) { fclose(fp); return refuse("board.pdl: tile index out of range"); }
        Tile *t = &TL[idx]; snprintf(t->name, sizeof t->name, "%s", f[2]); snprintf(t->kind, sizeof t->kind, "%s", f[3]);
        int j = 0; for (const char *c = f[2]; *c && j < 30; c++) if (*c != ' ') t->key[j++] = (char)tolower((unsigned char)*c);
        t->key[j] = 0; if (idx + 1 > NT) NT = idx + 1;
    }
    fclose(fp);
    if (NT < 2) return refuse("board.pdl holds no tiles (a classic board is needed for chat)");
    return 0;
}

/* ---- tokens ---- */
static char TK[MAXTOK][TOKLEN]; static int NTK;
static int tokenize(const char *text) {
    size_t L = strlen(text); if (L == 0) return refuse("empty: type a command (offer, accept, bid 120, build Red 2, roll ...)");
    if (L > MAXTEXT) return refuse("too long: at most %d characters", MAXTEXT);
    NTK = 0; char cur[TOKLEN]; int cl = 0;
    for (size_t i = 0; i <= L; i++) {
        unsigned char c = (unsigned char)text[i]; int sep = 0, tokc = 0;
        if (i == L) sep = 1;
        else if (c == ' ' || c == '\t' || c == ',' || c == '+' || c == '&' || c == '.' || c == '!' || c == '?' || c == '\'') sep = 1;
        else if (isalnum(c) || c == ':' || c == '#' || c == '$' || c == '_' || c == '-') tokc = 1;
        else if (c == '/' && i == 0) continue;
        else return refuse("unsupported character (code %d) at position %zu", c, i + 1);
        if (tokc) { if (cl >= TOKLEN - 1) return refuse("a word is too long (at most %d characters)", TOKLEN - 2); cur[cl++] = (char)tolower(c); }
        else if (sep && cl) { cur[cl] = 0; if (NTK >= MAXTOK) return refuse("too many words (at most %d)", MAXTOK); strcpy(TK[NTK++], cur); cl = 0; }
    }
    if (NTK == 0) return refuse("empty: type a command (offer, accept, bid 120, build Red 2, roll ...)");
    return 0;
}
static int all_digits(const char *s) { if (!*s) return 0; for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0; return 1; }
static int is_int(const char *s) { return all_digits(s) && strlen(s) <= 6; }
static int is_player(const char *s) { return s[0] == 'p' && all_digits(s + 1) && strlen(s) <= 3; }
static int noise(const char *w) {
    static const char *N[] = { "and", "plus", "with", "the", "a", "an", "my", "your", "me", "you", "i", "to", "of", "please", "pls", "cash", "coins", "coin", "money", "dollars", "dollar", "bucks", "card", "cards", "tiles", "property", "properties", "prop", "then", "also", NULL };
    for (int i = 0; N[i]; i++) if (!strcmp(w, N[i])) return 1; return 0;
}

/* ---- name resolution: exact id, else a UNIQUE prefix among the tiles the verb may touch (kind 0 = any ownable, 1 = street) ---- */
static int eligible(int t, int kind) { return t < NT && TL[t].key[0] && (kind == 1 ? is_street(t) : ownable(t)); }
static int resolve(const char *w, int kind, int *out) {
    size_t wl = strlen(w);
    for (int t = 0; t < NT; t++) if (TL[t].key[0] && !strcmp(TL[t].key, w)) {
        if (!eligible(t, kind)) return refuse("%s (tile %d) is a %s tile, not a %s", TL[t].name, t, TL[t].kind, kind == 1 ? "street (only streets take houses)" : "property");
        *out = t; return 0; }
    int c[MAXT], n = 0, other = -1;
    for (int t = 0; t < NT; t++) if (TL[t].key[0] && !strncmp(TL[t].key, w, wl)) { if (eligible(t, kind)) c[n++] = t; else if (other < 0) other = t; }
    if (n == 1) { *out = c[0]; return 0; }
    if (n > 1) { char b[400] = ""; for (int i = 0; i < n && i < 12; i++) { size_t u = strlen(b); snprintf(b + u, sizeof b - u, "%s%s (%d)", i ? ", " : "", TL[c[i]].name, c[i]); }
        return refuse("ambiguous name \"%s\": could be %s%s", w, b, n > 12 ? ", ..." : ""); }
    if (other >= 0) return refuse("\"%s\" is %s, which is a %s tile, not a %s", w, TL[other].name, TL[other].kind, kind == 1 ? "street" : "property");
    return refuse("unknown name \"%s\" (tile ids like Navy2, Line1, Water; or a tile number)", w);
}

/* ---- one side of an offer ---- */
typedef struct { int tiles[MAXOFF], nt, cash, card[2], none, seen; } Side;
static int add_tile(Side *s, int t) {
    for (int i = 0; i < s->nt; i++) if (s->tiles[i] == t) return refuse("%s is named twice", TL[t].name);
    if (s->nt >= MAXOFF) return refuse("at most %d tiles on one side of an offer", MAXOFF);
    s->tiles[s->nt++] = t; return 0;
}
static int add_cash(Side *s, const char *digits) {
    if (!is_int(digits) || atoi(digits) < 1) return refuse("cash must be a whole number from 1 up (got \"%s\")", digits);
    if (s->cash) return refuse("two money amounts on one side (%d and %s): give one number", s->cash, digits);
    s->cash = atoi(digits); return 0;
}
static int parse_side(int a, int b, Side *s, const char *which) {
    for (int i = a; i < b; i++) {
        const char *w = TK[i];
        if (noise(w)) continue;
        if (!strcmp(w, "nothing") || !strcmp(w, "none") || !strcmp(w, "-")) { s->none = 1; continue; }
        if (!strncmp(w, "prop:", 5)) { int t; if (is_int(w + 5)) { t = atoi(w + 5); if (t >= NT || !eligible(t, 0)) return refuse("tile %s cannot be traded (not a property)", w + 5); }
            else if (resolve(w + 5, 0, &t)) return -1; if (add_tile(s, t)) return -1; continue; }
        if (!strncmp(w, "cash:", 5)) { if (add_cash(s, w + 5)) return -1; continue; }
        if (!strncmp(w, "card:", 5)) { int d = !strcmp(w + 5, "luck") ? 0 : !strcmp(w + 5, "fund") ? 1 : -1; if (d < 0) return refuse("a card is luck or fund (card:luck, card:fund)");
            if (s->card[d]) return refuse("the %s card is named twice", w + 5); s->card[d] = 1; continue; }
        if (!strcmp(w, "luck") || !strcmp(w, "fund")) { int d = w[0] == 'f'; if (s->card[d]) return refuse("the %s card is named twice", w); s->card[d] = 1; continue; }
        if (w[0] == '$') { if (add_cash(s, w + 1)) return -1; continue; }
        if (w[0] == '#') { if (!is_int(w + 1)) return refuse("\"%s\": # must be followed by a tile number", w);
            int t = atoi(w + 1); if (t >= NT || !eligible(t, 0)) return refuse("tile %d cannot be traded (not a property)", t); if (add_tile(s, t)) return -1; continue; }
        if (!strcmp(w, "tile")) { if (i + 1 < b && is_int(TK[i + 1])) { int t = atoi(TK[i + 1]); if (t >= NT || !eligible(t, 0)) return refuse("tile %d cannot be traded (not a property)", t); if (add_tile(s, t)) return -1; i++; } continue; }
        if (is_int(w)) { if (add_cash(s, w)) return -1; continue; }
        if (isalpha((unsigned char)w[0])) {
            int t;
            if (i + 1 < b && is_int(TK[i + 1])) {   /* `Navy 2` = navy2 when that id exists; otherwise the number after a name is cash */
                char j[80]; snprintf(j, sizeof j, "%s%s", w, TK[i + 1]);
                for (t = 0; t < NT; t++) if (TL[t].key[0] && !strcmp(TL[t].key, j)) break;
                if (t < NT) { if (!ownable(t)) return refuse("%s (tile %d) is a %s tile, not a property", TL[t].name, t, TL[t].kind); if (add_tile(s, t)) return -1; i++; continue; }
            }
            if (resolve(w, 0, &t)) return -1; if (add_tile(s, t)) return -1; continue;
        }
        return refuse("cannot read \"%s\" in what you %s", w, which);
    }
    s->seen = 1;
    int empty = !s->nt && !s->cash && !s->card[0] && !s->card[1];
    if (s->none && !empty) return refuse("\"nothing\" together with other things in what you %s", which);
    if (empty && !s->none) return refuse("\"%s\" needs something after it: tiles, a number for cash, a luck/fund card, or \"nothing\"", which);
    return 0;
}
static int side_empty(const Side *s) { return !s->nt && !s->cash && !s->card[0] && !s->card[1]; }
static void side_str(const Side *s, char *o, size_t cap, int canon) {   /* canonical order: tiles ascending, cash, cards (luck, fund) = the engine's own as_str */
    int v[MAXOFF]; memcpy(v, s->tiles, sizeof(int) * (size_t)s->nt);
    for (int i = 1; i < s->nt; i++) { int x = v[i], j = i - 1; while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; } v[j + 1] = x; }
    o[0] = 0; size_t u;
#define ADD(...) do { u = strlen(o); snprintf(o + u, cap - u, __VA_ARGS__); } while (0)
    for (int i = 0; i < s->nt; i++) { if (canon) ADD("%sprop:%d", o[0] ? "+" : "", v[i]); else ADD("%s%s", o[0] ? " + " : "", TL[v[i]].name); }
    if (s->cash) { if (canon) ADD("%scash:%d", o[0] ? "+" : "", s->cash); else ADD("%s%d cash", o[0] ? " + " : "", s->cash); }
    static const char *D[2] = { "luck", "fund" };
    for (int d = 0; d < 2; d++) if (s->card[d]) { if (canon) ADD("%scard:%s", o[0] ? "+" : "", D[d]); else ADD("%s%s card", o[0] ? " + " : "", D[d]); }
    if (!o[0]) snprintf(o, cap, canon ? "-" : "nothing");
#undef ADD
}

/* ---- the parse: result is the engine argv (verb + args, canonical) and the plain English ---- */
static char CV[24], CA[8][200]; static int NCA; static char UND[600];
static void argv_put(const char *a) { snprintf(CA[NCA++], sizeof CA[0], "%s", a); }

static int first_int(int from, int to) { for (int i = from; i < to; i++) { const char *w = TK[i]; if (w[0] == '#') w++; if (is_int(w) && atoi(w) >= 1) return atoi(w); } return 0; }

static int parse_trade(const char *verb) {
    int m = NTK; for (int i = 1; i < NTK; i++) if (!strcmp(TK[i], "give") || !strcmp(TK[i], "get")) { m = i; break; }
    char who[8] = ""; int id = 0;
    if (!strcmp(verb, "offer")) { for (int i = 1; i < m; i++) if (is_player(TK[i])) { snprintf(who, sizeof who, "%s", TK[i]); break; }
        if (!who[0]) return refuse("offer needs a player: offer p3 give ... get ..."); }
    else { id = first_int(1, m); if (!id) return refuse("%s needs the number of the offer: %s 4 give ... get ...", verb, verb); }
    Side sd[2] = { {{0}, 0, 0, {0, 0}, 0, 0}, {{0}, 0, 0, {0, 0}, 0, 0} };
    int i = m;
    while (i < NTK) {
        int which = !strcmp(TK[i], "give") ? 0 : 1; if (sd[which].seen || sd[which].none) return refuse("\"%s\" appears twice", which ? "get" : "give");
        int j = i + 1; while (j < NTK && strcmp(TK[j], "give") && strcmp(TK[j], "get")) j++;
        if (parse_side(i + 1, j, &sd[which], which ? "get" : "give")) return -1;
        i = j;
    }
    if (side_empty(&sd[0]) && side_empty(&sd[1])) return refuse("an offer needs something on at least one side");
    snprintf(CV, sizeof CV, "%s", verb); NCA = 0;
    if (!strcmp(verb, "offer")) argv_put(who); else { char b[16]; snprintf(b, sizeof b, "%d", id); argv_put(b); }
    char g[200], t[200], cg[200], ct[200]; side_str(&sd[0], g, sizeof g, 0); side_str(&sd[1], t, sizeof t, 0); side_str(&sd[0], cg, sizeof cg, 1); side_str(&sd[1], ct, sizeof ct, 1);
    if (!side_empty(&sd[0])) { argv_put("give"); argv_put(cg); }
    if (!side_empty(&sd[1])) { argv_put("get"); argv_put(ct); }
    if (!strcmp(verb, "offer")) snprintf(UND, sizeof UND, "offer %s: you give %s, you get %s", who, g, t);
    else snprintf(UND, sizeof UND, "%s offer %d: you give %s, you get %s", !strcmp(verb, "counter") ? "counter" : "amend", id, g, t);
    return 0;
}

static int parse_text(const char *text) {
    CV[0] = 0; NCA = 0; UND[0] = 0; ERR[0] = 0;
    if (tokenize(text)) return -1;
    const char *v = TK[0];
    if (!strcmp(v, "offer") || !strcmp(v, "counter") || !strcmp(v, "amend")) return parse_trade(v);
    if (!strcmp(v, "accept") || !strcmp(v, "reject") || !strcmp(v, "cancel")) {
        int id = first_int(1, NTK); snprintf(CV, sizeof CV, "%s", v);
        if (id) { char b[16]; snprintf(b, sizeof b, "%d", id); argv_put(b); snprintf(UND, sizeof UND, "%s offer %d", v, id); }
        else snprintf(UND, sizeof UND, "%s the open offer %s", v, !strcmp(v, "cancel") ? "you made" : "addressed to you");
        return 0;
    }
    if (!strcmp(v, "bid")) {
        int n = first_int(1, NTK); if (!n) return refuse("bid needs an amount: bid 120");
        char b[16]; snprintf(b, sizeof b, "%d", n); snprintf(CV, sizeof CV, "bid"); argv_put(b); snprintf(UND, sizeof UND, "bid %d in the auction", n); return 0;
    }
    if (!strcmp(v, "pass")) { snprintf(CV, sizeof CV, "pass"); snprintf(UND, sizeof UND, "pass in the auction"); return 0; }
    if (!strcmp(v, "build") || !strcmp(v, "sell") || !strcmp(v, "mortgage") || !strcmp(v, "unmortgage")) {
        int kind = (!strcmp(v, "build") || !strcmp(v, "sell")) ? 1 : 0, t = -1, got = 0;
        for (int i = 1; i < NTK; i++) {
            const char *w = TK[i]; int one = -1;
            if (noise(w)) continue;
            if (w[0] == '#' || !strncmp(w, "prop:", 5) || is_int(w)) {
                const char *d = w[0] == '#' ? w + 1 : !strncmp(w, "prop:", 5) ? w + 5 : w;
                if (is_int(d)) { int n = atoi(d); if (n >= NT || !eligible(n, kind)) return refuse("tile %d is not %s", n, kind ? "a street" : "a property"); one = n; }
                else if (resolve(d, kind, &one)) return -1;
            } else if (isalpha((unsigned char)w[0])) {
                if (!strcmp(w, "tile")) continue;
                int hit = -1;
                if (i + 1 < NTK && is_int(TK[i + 1])) { char j[80]; snprintf(j, sizeof j, "%s%s", w, TK[i + 1]); for (int q = 0; q < NT; q++) if (TL[q].key[0] && !strcmp(TL[q].key, j)) hit = q; }
                if (hit >= 0) { if (!eligible(hit, kind)) return refuse("%s (tile %d) is a %s tile, not a %s", TL[hit].name, hit, TL[hit].kind, kind ? "street (only streets take houses)" : "property"); one = hit; i++; }
                else if (resolve(w, kind, &one)) return -1;
            } else return refuse("cannot read \"%s\"", w);
            if (got && one != t) return refuse("%s takes ONE tile (got %s and %s)", v, TL[t].name, TL[one].name);
            t = one; got = 1;
        }
        if (!got) return refuse("%s needs a tile: %s Red 2", v, v);
        char b[16]; snprintf(b, sizeof b, "%d", t); snprintf(CV, sizeof CV, "%s", v); argv_put(b); snprintf(UND, sizeof UND, "%s %s (tile %d)", v, TL[t].name, t); return 0;
    }
    if (!strcmp(v, "roll")) { snprintf(CV, sizeof CV, "roll"); snprintf(UND, sizeof UND, "roll the dice"); return 0; }
    if (!strcmp(v, "buy")) { snprintf(CV, sizeof CV, "buy"); snprintf(UND, sizeof UND, "buy the tile you landed on"); return 0; }
    if (!strcmp(v, "skip")) { snprintf(CV, sizeof CV, "skip"); snprintf(UND, sizeof UND, "decline the tile (it goes to auction)"); return 0; }
    if (!strcmp(v, "end") || !strcmp(v, "end_turn") || !strcmp(v, "endturn")) { snprintf(CV, sizeof CV, "end_turn"); snprintf(UND, sizeof UND, "end your turn"); return 0; }
    return refuse("unknown command \"%s\" (offer, counter, amend, accept, reject, cancel, bid, pass, build, sell, mortgage, unmortgage, roll, buy, skip, end)", v);
}

static void canon_line(char *o, size_t cap) { snprintf(o, cap, "%s", CV); for (int i = 0; i < NCA; i++) { size_t u = strlen(o); snprintf(o + u, cap - u, " %s", CA[i]); } }

/* the ruleset a game was started under: its first ledger row is the lobby row (`ruleset=<name>,...`); default classic */
static void game_ruleset(const char *game, char *rs, size_t cap) {
    snprintf(rs, cap, "classic"); char p[1100], ln[2048]; snprintf(p, sizeof p, "%s/ledger.txt", game); FILE *fp = fopen(p, "r"); if (!fp) return;
    if (fgets(ln, sizeof ln, fp)) { char *q = strstr(ln, "ruleset="); if (q) { q += 8; size_t n = strspn(q, "abcdefghijklmnopqrstuvwxyz0123456789_-"); if (n > 0 && n < cap && n < 25) { memcpy(rs, q, n); rs[n] = 0; } } }
    fclose(fp);
}

static int is_dir(const char *p) { struct stat sb; return !stat(p, &sb) && S_ISDIR(sb.st_mode); }

static void join_args(int argc, char **argv, char *o, size_t cap) { o[0] = 0; for (int i = 0; i < argc; i++) { size_t u = strlen(o); snprintf(o + u, cap - u, "%s%s", i ? " " : "", argv[i]); } }

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "--parse")) {
        if (argc < 3) { fprintf(stderr, "usage: deskopoly_chat --parse <app dir> [ruleset=<name>] <text...>\n"); return 2; }
        const char *app = argv[2]; int a = 3; char rs[40] = "classic";
        if (a < argc && !strncmp(argv[a], "ruleset=", 8)) { size_t n = strspn(argv[a] + 8, "abcdefghijklmnopqrstuvwxyz0123456789_-"); if (!n || n > 24 || argv[a][8 + n]) { puts("refused: bad ruleset name"); return 2; } snprintf(rs, sizeof rs, "%s", argv[a] + 8); a++; }
        char text[1200]; join_args(argc - a, argv + a, text, sizeof text);
        if (load_board(app, rs) || parse_text(text)) { printf("refused: %s\n", ERR); return 2; }
        char c[1000]; canon_line(c, sizeof c); printf("ok %s\nunderstood: %s\n", c, UND); return 0;
    }
    if (argc < 3) { fprintf(stderr, "usage: deskopoly_chat <app dir> <game dir> [as=<pN>] <text...>\n       deskopoly_chat --parse <app dir> [ruleset=<name>] <text...>\n"); return 2; }
    const char *app = argv[1], *game = argv[2]; int a = 3; char as[16] = "";
    if (!is_dir(app) || !is_dir(game)) { puts("refused: the app folder and the game folder must both exist"); return 2; }
    if (a < argc && !strncmp(argv[a], "as=", 3)) { const char *p = argv[a] + 3; if (!is_player(p)) { puts("refused: as= must name a player like p1"); return 2; } snprintf(as, sizeof as, "%s", p); a++; }
    char text[1200]; join_args(argc - a, argv + a, text, sizeof text);
    char rs[40]; game_ruleset(game, rs, sizeof rs);
    if (load_board(app, rs) || parse_text(text)) { printf("refused: %s\n", ERR); return 2; }
    char c[1000]; canon_line(c, sizeof c); printf("ok %s\nunderstood: %s\n", c, UND); fflush(stdout);
    char eng[1200]; snprintf(eng, sizeof eng, "%s/ops/+x/deskopoly.+x", app);
    if (access(eng, X_OK)) { printf("refused: the engine is not built (%s)\n", eng); return 2; }
    char *av[16]; int n = 0; av[n++] = eng; av[n++] = (char *)app; av[n++] = (char *)game; av[n++] = CV; for (int i = 0; i < NCA; i++) av[n++] = CA[i];
    char asarg[24]; if (as[0]) { snprintf(asarg, sizeof asarg, "as=%s", as); av[n++] = asarg; } av[n] = NULL;
    int pf[2]; if (pipe(pf)) { perror("pipe"); return 2; }
    pid_t pid = fork(); if (pid < 0) { perror("fork"); return 2; }
    if (pid == 0) { close(pf[0]); dup2(pf[1], 1); dup2(pf[1], 2); close(pf[1]); int nul = open("/dev/null", 0); if (nul >= 0) { dup2(nul, 0); close(nul); } execv(eng, av); _exit(127); }
    close(pf[1]); char buf[2048]; ssize_t r; while ((r = read(pf[0], buf, sizeof buf)) > 0) { if (fwrite(buf, 1, (size_t)r, stdout) != (size_t)r) break; }
    close(pf[0]); fflush(stdout); int st = 0; waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}
