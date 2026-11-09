/* deskopoly_chat - the CHAT FRONT DOOR of Teropoly (spec DESIGN-DESKOPOLY-CLASSIC-MONOPOLY-BUILD-SPEC-2026-11-08 section 7.6).
 * A typed line goes in, the engine's own canonical verb comes out. The parser is a PURE function (text + the board's tile names -> one canonical argv, or a refusal);
 * it never writes a file and there is NO second path that appends ledger rows: mode (b) only calls the engine, which appends (or refuses) exactly as for a direct call.
 *
 *   deskopoly_chat --parse <app dir> [ruleset=<name>] <text...>
 *       prints `ok <verb and args>` and `understood: <plain English>`, or `refused: <reason>`; appends nothing. Exit 0 / 2.
 *   deskopoly_chat --hook <app dir> <game dir> <text...>      the editor chat box through the core's generic game.pdl `GAME | chat` hook (see HOOK MODE below): exit 3 = not a command; as= is the human seat
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
 *   TILE NAMES AS WRITTEN ON THE BOARD (2026-11-09): the ruleset's page.pdl `LABEL | <tile id or kind> | <text>` rows (the text the board, the trade screen and the popups show) also name tiles: `Coil Plaza`, `golden ratio row`,
 *   `Spiral Line IV`. Same rules as ids, words instead of one token: the longest exact label wins (`Golden Row` never swallows `Golden Ratio Row`), else a UNIQUE prefix (`Coil Pla`; the first words whole, the last one may be cut), a
 *   label word that is also a filler or number word still reads as part of its label when the whole label is typed. Ids still win. Deck labels (kinds chance / chest: Luck, Lost & Found) are not tiles. No page.pdl or no LABEL rows = ids only,
 *   exactly as before. The line sent to the engine is the same `prop:<n>` either way.
 *   An ambiguous or unknown name is refused WITH the candidates (id, label, number) and nothing is sent. Two money amounts, a tile twice or a side with nothing after its keyword are refused.
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

/* LABELS (2026-11-09): the on-screen tile names, read from the page.pdl that sits beside the board.pdl in use (the engine's own rule: rulesets/<rs>/page.pdl, else <app>/page.pdl).
 * `LABEL | <tile id or kind> | <text>`: a tile's own row beats its kind's row, the LAST row of a key wins, empty text = no label, a literal \n is a word break (the same reading as the engine's `page`).
 * Each label is cut into the same lowercase tokens a typed line is. Tiles of the deck kinds (chance, chest) are left out: Luck / Lost & Found are decks, not tiles. */
#define MAXLW 8
typedef struct { char w[MAXLW][TOKLEN]; int n; char text[64]; } Lab;
static Lab LAB[MAXT]; static int NLAB;
typedef struct { char key[32], text[64]; } LabRow;
static int label_tokens(const char *text, Lab *l) {
    char s[64]; size_t j = 0; l->n = 0;
    for (size_t i = 0; text[i] && j + 2 < sizeof s; i++) { if (text[i] == '\\' && text[i + 1] == 'n') { s[j++] = ' '; i++; } else s[j++] = text[i]; }
    s[j] = 0; int cl = 0; char cur[TOKLEN];
    for (size_t i = 0; i <= j; i++) {
        unsigned char c = (unsigned char)s[i]; int tokc = (i < j) && (isalnum(c) || c == ':' || c == '#' || c == '$' || c == '_' || c == '-');
        if (tokc) { if (cl < TOKLEN - 1) cur[cl++] = (char)tolower(c); }
        else if (cl) { cur[cl] = 0; cl = 0; if (l->n >= MAXLW) { l->n = 0; return 0; } strcpy(l->w[l->n++], cur); }
    }
    return l->n;
}
static void load_labels(const char *dir) {
    NLAB = 0; memset(LAB, 0, sizeof LAB);
    char p[1100], ln[512]; snprintf(p, sizeof p, "%s/page.pdl", dir); FILE *fp = fopen(p, "r"); if (!fp) return;
    static LabRow R[128]; int nr = 0;
    while (fgets(ln, sizeof ln, fp)) {
        if (ln[0] == '#') continue; char *f[8]; int n = 0; char *sv = NULL;
        for (char *t = strtok_r(ln, "|", &sv); t && n < 8; t = strtok_r(NULL, "|", &sv)) f[n++] = trim(t);
        if (n < 2 || strcmp(f[0], "LABEL") || nr >= 128) continue;
        snprintf(R[nr].key, sizeof R[nr].key, "%s", f[1]); snprintf(R[nr].text, sizeof R[nr].text, "%s", n >= 3 ? f[2] : ""); nr++;
    }
    fclose(fp);
    for (int t = 0; t < NT; t++) {
        if (!TL[t].key[0] || !strcmp(TL[t].kind, "chance") || !strcmp(TL[t].kind, "chest")) continue;
        const char *txt = NULL;
        for (int j = nr - 1; j >= 0 && !txt; j--) if (!strcmp(R[j].key, TL[t].name)) txt = R[j].text;
        for (int j = nr - 1; j >= 0 && !txt; j--) if (!strcmp(R[j].key, TL[t].kind)) txt = R[j].text;
        if (!txt || !*txt) continue;
        if (label_tokens(txt, &LAB[t])) { snprintf(LAB[t].text, sizeof LAB[t].text, "%s", txt); NLAB++; }
    }
}

/* board.pdl: `TILE | idx | id | kind | group | price | ...` (the classic format). Names come from here, never from the code. */
static int load_board(const char *app, const char *rs) {
    char p[1100], ln[512], dir[1100]; snprintf(dir, sizeof dir, "%s/rulesets/%s", app, rs); snprintf(p, sizeof p, "%s/board.pdl", dir);
    FILE *fp = fopen(p, "r"); if (!fp) { snprintf(dir, sizeof dir, "%s", app); snprintf(p, sizeof p, "%s/board.pdl", app); fp = fopen(p, "r"); }
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
    load_labels(dir);
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

/* ---- name resolution: exact id, else the exact on-screen label (longest first), else a UNIQUE prefix among the tiles the verb may touch (kind 0 = any ownable, 1 = street) ---- */
static int eligible(int t, int kind) { return t < NT && TL[t].key[0] && (kind == 1 ? is_street(t) : ownable(t)); }
static void cand_str(const int *c, int n, char *b, size_t cap) {   /* `Orange3 (Coil Plaza) (19), ...`: the id, the label when the tile has one, the tile number */
    b[0] = 0;
    for (int i = 0; i < n && i < 12; i++) { size_t u = strlen(b);
        if (LAB[c[i]].n) snprintf(b + u, cap - u, "%s%s (%s) (%d)", i ? ", " : "", TL[c[i]].name, LAB[c[i]].text, c[i]);
        else snprintf(b + u, cap - u, "%s%s (%d)", i ? ", " : "", TL[c[i]].name, c[i]); }
    if (n > 12) { size_t u = strlen(b); snprintf(b + u, cap - u, ", ..."); }
}
static int wrong_kind(int t, int kind) { return refuse("%s (tile %d) is a %s tile, not a %s", TL[t].name, t, TL[t].kind, kind == 1 ? "street (only streets take houses)" : "property"); }
/* the candidates c[0..n) for what was typed (`typed`): one eligible = the answer; several = ambiguous, listed; none eligible = say what the nearest is. exact = the typed text was a whole name, not a prefix */
static int pick(const int *c, int n, int kind, const char *typed, int exact, int *out) {
    int el[MAXT], ne = 0, other = -1;
    for (int i = 0; i < n; i++) { if (eligible(c[i], kind)) el[ne++] = c[i]; else if (other < 0) other = c[i]; }
    if (ne == 1) { *out = el[0]; return 0; }
    if (ne > 1) { char b[900]; cand_str(el, ne, b, sizeof b); return refuse("ambiguous name \"%s\": could be %s", typed, b); }
    if (n == 1 && exact) return wrong_kind(other, kind);
    return refuse("\"%s\" is %s, which is a %s tile, not a %s", typed, TL[other].name, TL[other].kind, kind == 1 ? "street" : "property");
}
static int unknown_name(const char *typed) {
    if (NLAB) return refuse("unknown name \"%s\" (tile ids like Navy2, Line1, Water; a tile name as written on the board; or a tile number)", typed);
    return refuse("unknown name \"%s\" (tile ids like Navy2, Line1, Water; or a tile number)", typed);
}
static int resolve(const char *w, int kind, int *out) {   /* one token, ids only (prop:<id>, #<id>) */
    size_t wl = strlen(w);
    for (int t = 0; t < NT; t++) if (TL[t].key[0] && !strcmp(TL[t].key, w)) { if (!eligible(t, kind)) return wrong_kind(t, kind); *out = t; return 0; }
    int c[MAXT], n = 0;
    for (int t = 0; t < NT; t++) if (TL[t].key[0] && !strncmp(TL[t].key, w, wl)) c[n++] = t;
    if (!n) return unknown_name(w);
    return pick(c, n, kind, w, 0, out);
}

/* words typed at TK[i..i+k) against tile t's label: exact = the whole label; else every word but the last whole and the last a prefix of the label's word */
static int lab_hit(int t, int i, int k, int exact) {
    const Lab *l = &LAB[t]; if (!l->n || k > l->n || (exact && k != l->n)) return 0;
    for (int j = 0; j < k - 1; j++) if (strcmp(TK[i + j], l->w[j])) return 0;
    return exact ? !strcmp(TK[i + k - 1], l->w[k - 1]) : !strncmp(l->w[k - 1], TK[i + k - 1], strlen(TK[i + k - 1]));
}
static void typed_str(int i, int k, char *o, size_t cap) { o[0] = 0; for (int j = 0; j < k; j++) { size_t u = strlen(o); snprintf(o + u, cap - u, "%s%s", j ? " " : "", TK[i + j]); } }
static int reserved_word(const char *w) { return noise(w) || !strcmp(w, "nothing") || !strcmp(w, "none") || !strcmp(w, "tile") || !strcmp(w, "luck") || !strcmp(w, "fund") || !strcmp(w, "give") || !strcmp(w, "get"); }
/* the longest whole label typed at TK[i..b), at least minw words (a one-word label that is also a filler/number word stays a plain word). 1 = found (*out tile, *used words), 0 = none, -1 = refused */
static int label_exact(int i, int b, int kind, int minw, int *out, int *used) {
    int kmax = b - i < MAXLW ? b - i : MAXLW;
    for (int k = kmax; k >= minw; k--) {
        if (k == 1 && (reserved_word(TK[i]) || is_int(TK[i]) || TK[i][0] == '$' || TK[i][0] == '#')) break;
        int c[MAXT], n = 0; for (int t = 0; t < NT; t++) if (lab_hit(t, i, k, 1)) c[n++] = t;
        if (!n) continue;
        char ty[200]; typed_str(i, k, ty, sizeof ty); *used = k; return pick(c, n, kind, ty, 1, out) ? -1 : 1;
    }
    return 0;
}
/* a tile name starting at TK[i] (a word, limit b): `Navy 2` = navy2 when that id exists; an exact id; an exact label; else a unique prefix of an id (one word) or of a label (words, the last one may be cut). *used = words consumed */
static int name_at(int i, int b, int kind, int *out, int *used) {
    const char *w = TK[i]; size_t wl = strlen(w); int t;
    if (i + 1 < b && is_int(TK[i + 1])) {
        char j[80]; snprintf(j, sizeof j, "%s%s", w, TK[i + 1]);
        for (t = 0; t < NT; t++) if (TL[t].key[0] && !strcmp(TL[t].key, j)) break;
        if (t < NT) { if (!eligible(t, kind)) return wrong_kind(t, kind); *out = t; *used = 2; return 0; }
    }
    for (t = 0; t < NT; t++) if (TL[t].key[0] && !strcmp(TL[t].key, w)) { if (!eligible(t, kind)) return wrong_kind(t, kind); *out = t; *used = 1; return 0; }
    int r = label_exact(i, b, kind, 1, out, used); if (r) return r < 0 ? -1 : 0;
    int kmax = b - i < MAXLW ? b - i : MAXLW;
    for (int k = kmax; k >= 1; k--) {
        int c[MAXT], n = 0;
        for (t = 0; t < NT; t++) {
            int hit = lab_hit(t, i, k, 0);
            if (!hit && k == 1 && TL[t].key[0] && !strncmp(TL[t].key, w, wl)) hit = 1;
            if (hit) c[n++] = t;
        }
        if (!n) continue;
        char ty[200]; typed_str(i, k, ty, sizeof ty); *used = k; return pick(c, n, kind, ty, 0, out);
    }
    return unknown_name(w);
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
        if (reserved_word(w)) { int t, u, r = label_exact(i, b, 0, 2, &t, &u); if (r < 0) return -1; if (r) { if (add_tile(s, t)) return -1; i += u - 1; continue; } }   /* a whole label that starts with a filler word (`The Hub`) is a name */
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
        if (isalpha((unsigned char)w[0])) {   /* `Navy 2` = navy2 when that id exists (otherwise the number after a name is cash); an id; a label as written on the board; a unique prefix */
            int t, u; if (name_at(i, b, 0, &t, &u)) return -1; if (add_tile(s, t)) return -1; i += u - 1; continue;
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
            if (reserved_word(w)) { int u, r = label_exact(i, NTK, kind, 2, &one, &u); if (r < 0) return -1; if (r) { i += u - 1; goto have_one; } }
            if (noise(w)) continue;
            if (w[0] == '#' || !strncmp(w, "prop:", 5) || is_int(w)) {
                const char *d = w[0] == '#' ? w + 1 : !strncmp(w, "prop:", 5) ? w + 5 : w;
                if (is_int(d)) { int n = atoi(d); if (n >= NT || !eligible(n, kind)) return refuse("tile %d is not %s", n, kind ? "a street" : "a property"); one = n; }
                else if (resolve(d, kind, &one)) return -1;
            } else if (isalpha((unsigned char)w[0])) {
                if (!strcmp(w, "tile")) continue;
                int u; if (name_at(i, NTK, kind, &one, &u)) return -1; i += u - 1;   /* id (`Red 2`, `red2`), label as written on the board, or a unique prefix */
            } else return refuse("cannot read \"%s\"", w);
        have_one:
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

/* ===== HOOK MODE (2026-11-08): the editor chat box, through the shared core's generic hook (game.pdl `GAME | chat | {book}/ops/+x/deskopoly_chat.+x --hook {book} {editor}/state/games/deskopoly/game`; the core appends the typed text as the last argument).
 *   deskopoly_chat --hook <app dir> <game dir> <text...>
 * EXIT 3 = "not a command" (the core hands the line to the pets as it always did): no human seat in this game (ui.txt has no p<i>_driver=human: nobody to command for), or the line is not a command (`hello`).
 * Otherwise this is mode (b) with as=<the human seat>: the same parser and the same engine verbs, so a typed `offer p2 give Orange3 get 20` writes the very ledger row a popup button or a pet would. Exit 0 done, 2 refused (the engine's own answer is printed, never 3).
 * A line that does not parse is a COMMAND ATTEMPT (shown as `refused: <why>`) only when it starts with `/` or with offer counter amend accept reject cancel bid mortgage unmortgage (so `pass the salt` or `end of the story` still reach the pets);
 * a line that parses runs (a bare `end` or `roll` is the same verb as the popup button).
 * After EVERY handled line (done or refused) the driver is told to republish: the line `refresh` is appended to <game dir>/../action.txt (the file deskopoly_ctl appends to) and the answer line is left in <game dir>/../chat_reply.txt (one line, overwritten; the driver
 * shows it in the popup once and deletes the file); after a verb that was done the engine's `ui` is refreshed first. Nothing here writes the ledger. ===== */
static int human_seat(const char *game, char *as, size_t cap) {
    char p[1100], ln[512]; snprintf(p, sizeof p, "%s/ui.txt", game); FILE *f = fopen(p, "r"); if (!f) return 0; int found = 0;
    while (!found && fgets(ln, sizeof ln, f)) { int n = 0; char drv[16] = ""; if (sscanf(ln, "p%d_driver=%15s", &n, drv) == 2 && n >= 1 && n <= 8 && !strcmp(drv, "human")) { snprintf(as, cap, "p%d", n); found = 1; } }
    fclose(f); return found;
}
static int run_engine(const char *app, const char *game, const char *as, int ui_only, char *out, size_t cap) {   /* fork+execv, no shell; stdout+stderr collected; the engine's exit code (or 2) */
    char eng[1200]; snprintf(eng, sizeof eng, "%s/ops/+x/deskopoly.+x", app); out[0] = 0; if (access(eng, X_OK)) { snprintf(out, cap, "the engine is not built (%s)", eng); return 2; }
    char *av[16]; int n = 0; av[n++] = eng; av[n++] = (char *)app; av[n++] = (char *)game; char asarg[24];
    if (ui_only) av[n++] = (char *)"ui"; else { av[n++] = CV; for (int i = 0; i < NCA; i++) av[n++] = CA[i]; if (as[0]) { snprintf(asarg, sizeof asarg, "as=%s", as); av[n++] = asarg; } } av[n] = NULL;
    int pf[2]; if (pipe(pf)) return 2; pid_t pid = fork(); if (pid < 0) return 2;
    if (pid == 0) { close(pf[0]); dup2(pf[1], 1); dup2(pf[1], 2); close(pf[1]); int nul = open("/dev/null", 0); if (nul >= 0) { dup2(nul, 0); close(nul); } alarm(8); execv(eng, av); _exit(127); }
    close(pf[1]); size_t got = 0; ssize_t r; while (got + 1 < cap && (r = read(pf[0], out + got, cap - 1 - got)) > 0) got += (size_t)r; out[got] = 0; close(pf[0]);
    int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 2;
}
static int zero_arg_word(const char *w) { static const char *Z[] = { "pass", "end", "end_turn", "endturn", "roll", "buy", "skip", NULL }; for (int i = 0; Z[i]; i++) if (!strcasecmp(w, Z[i])) return 1; return 0; }
static int trade_word(const char *w) { static const char *T[] = { "offer", "counter", "amend", "accept", "reject", "cancel", "bid", "mortgage", "unmortgage", NULL }; for (int i = 0; T[i]; i++) if (!strcasecmp(w, T[i])) return 1; return 0; }
/* tell the driver: the one answer line for the popup (<game dir>/../chat_reply.txt, overwritten) and a `refresh` line in <game dir>/../action.txt (the file deskopoly_ctl appends to) so it republishes */
static void notify_driver(const char *game, const char *reply) {
    char par[1100]; snprintf(par, sizeof par, "%s", game); size_t gl = strlen(par); while (gl > 1 && par[gl - 1] == '/') par[--gl] = 0; char *sl = strrchr(par, '/'); if (!sl) return;
    *sl = 0; char rp[1200], ap[1200]; snprintf(rp, sizeof rp, "%s/chat_reply.txt", par); snprintf(ap, sizeof ap, "%s/action.txt", par);
    FILE *rf = fopen(rp, "w"); if (rf) { fprintf(rf, "%.300s\n", reply); fclose(rf); }
    int fd = open(ap, O_WRONLY | O_CREAT | O_APPEND, 0644); if (fd >= 0) { if (write(fd, "refresh\n", 8) != 8) { /* the driver picks the change up on its next event */ } close(fd); }
}
static int hook_mode(int argc, char **argv) {
    if (argc < 5) return 3;
    const char *app = argv[2], *game = argv[3]; char text[1200]; join_args(argc - 4, argv + 4, text, sizeof text);
    if (!is_dir(app) || !is_dir(game)) return 3;
    char as[16] = ""; if (!human_seat(game, as, sizeof as)) return 3;
    const char *w0 = text; while (*w0 == ' ' || *w0 == '/') w0++; char first[24]; size_t fl = strspn(w0, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"); if (fl >= sizeof first) fl = sizeof first - 1; memcpy(first, w0, fl); first[fl] = 0;
    int slash = text[0] == '/', attempt = slash || trade_word(first);
    if (!slash && zero_arg_word(first)) return 3;   /* `pass the salt`, `end of the story`, `roll with it`: the parser would take the first word and drop the rest, so a bare verb that takes no arguments is a command only with a leading `/` (the popup buttons do the same) */
    char rs[40]; game_ruleset(game, rs, sizeof rs); char reply[400] = "", out[1500];
    if (load_board(app, rs) || parse_text(text)) { if (!attempt) return 3; snprintf(reply, sizeof reply, "refused: %s", ERR); notify_driver(game, reply); printf("%s\n", reply); return 2; }
    int rc = run_engine(app, game, as, 0, out, sizeof out); char und[700]; snprintf(und, sizeof und, "%s", UND);
    char *last = out; for (char *l = strtok(out, "\n"); l; l = strtok(NULL, "\n")) if (*l) last = l;
    const char *ans = last; if (!strncmp(ans, "deskopoly: ", 11)) ans += 11; if (rc && !strncmp(ans, "refused: ", 9)) ans += 9;   /* the engine prefixes its own name and `refused:` */
    snprintf(reply, sizeof reply, "%s%s", rc ? "refused: " : "", ans[0] ? ans : (rc ? "the engine said no" : und));
    if (!rc) { char sink[400]; run_engine(app, game, "", 1, sink, sizeof sink); }   /* ui.txt is the projection the driver reads: refreshed after a verb that was done */
    notify_driver(game, reply); printf("understood: %s\n%s\n", und, reply); return rc ? 2 : 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "--hook")) return hook_mode(argc, argv);
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
