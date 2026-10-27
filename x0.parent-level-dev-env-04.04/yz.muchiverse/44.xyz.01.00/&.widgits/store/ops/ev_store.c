/* ev_store - ONE event-command op for the Store: lets an event page (any app or game) ask the catalog and the player's ownership, and take a FREE item.
 * House op convention (ring-board-rmmv/ops/var_math.c): <state_dir> first, results as `key=int` lines in <state_dir>/variables.txt, ERR rows in op_errors.txt,
 * self-contained. Design: #.#.calendar-dox/!.HQ-IQ-BOOK/16.game/GAMES-AS-PACKAGES-AND-STORE-DESIGN.md section 17. Catalog: &.widgits/store/catalog.pdl (ITEM rows);
 * ownership: <house>/xyzfs/users/<uuid>/owned.pdl, append-only `OWN | item | version | qty | price | method | ts | tx`.
 *
 * Usage:  ev_store <state_dir> <house_root> <verb> <item>
 *   price  <item>  -> store_known=1|0, store_price=<coins> (free = 0, unpriced/unparseable = -1)
 *   owned  <item>  -> store_owned=<qty>
 *   get    <item>  -> FREE items with requires=none only: appends one OWN row (idempotent: owned already = ok, no second row) -> store_ok=1|0
 * The player is <state_dir>/user_uuid.txt (a uuid); it must name an existing <house>/xyzfs/users/<uuid>/ folder. An event cannot name another player.
 * NOT BUILT (design, section 17): buy (escrow on a chain), install (quarantine + screening), review. `get` records ownership only; it does not install anything.
 * Exit: 0 ok | 1 refused/failed | 2 usage. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/ev_store.+x ev_store.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#define PL 4096
static char STATE[PL], HOUSE[PL], UUID[80];
static void errrow(const char *m, const char *a) { char p[PL + 32]; snprintf(p, sizeof p, "%s/op_errors.txt", STATE); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "ERR|ev_store|%s|%s\n", m, a ? a : ""); fclose(f); } fprintf(stderr, "ev_store: %s %s\n", m, a ? a : ""); }
static int ident_ok(const char *s, size_t max) { size_t n = strlen(s); if (!n || n > max) return 0; for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-' && s[i] != '.') return 0; return strstr(s, "..") == NULL; }
static void setvar(const char *name, long long v) { char p[PL + 32], t[PL + 64], line[512]; snprintf(p, sizeof p, "%s/variables.txt", STATE); snprintf(t, sizeof t, "%s.tmp.%d", p, (int)getpid());
    FILE *in = fopen(p, "r"), *out = fopen(t, "w"); if (!out) { if (in) fclose(in); return; } size_t nl = strlen(name); int done = 0;
    if (in) { while (fgets(line, sizeof line, in)) { if (!strncmp(line, name, nl) && line[nl] == '=') { fprintf(out, "%s=%lld\n", name, v); done = 1; } else fputs(line, out); } fclose(in); }
    if (!done) fprintf(out, "%s=%lld\n", name, v);
    fclose(out); rename(t, p); }
static void ledger(const char *verb, const char *item, int rc, const char *note) { char p[PL + 32]; snprintf(p, sizeof p, "%s/store_events.txt", STATE); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "STOREEV|%ld|%s|%s|%s|rc=%d|%s\n", (long)time(NULL), UUID, verb, item, rc, note); fclose(f); } }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
/* find ITEM row: fills version, price text, requires. Returns 1 if found. */
static int find_item(const char *id, char *ver, char *price, char *req, size_t cap) {
    char p[PL]; snprintf(p, sizeof p, "%s/&.widgits/store/catalog.pdl", HOUSE); FILE *f = fopen(p, "r"); if (!f) return 0; char line[2048]; int found = 0;
    while (fgets(line, sizeof line, f)) { if (line[0] == '#' || strncmp(line, "ITEM", 4)) continue; char *fld[10]; int n = 0; char *s = line, *tok;
        while (n < 10 && (tok = strsep(&s, "|"))) fld[n++] = trim(tok);
        if (n >= 7 && !strcmp(fld[1], id)) { snprintf(ver, cap, "%s", fld[4]); snprintf(price, cap, "%s", fld[5]); snprintf(req, cap, "%s", fld[6]); found = 1; break; } }
    fclose(f); return found;
}
static long long price_coins(const char *t) { if (!strcmp(t, "free")) return 0; char *e; long long v = strtoll(t, &e, 10); if (e != t && v >= 0 && (!*e || !strncmp(trim(e), "coin", 4))) return v; return -1; }
static int owned_qty(const char *id, char *opath, size_t cap) {
    snprintf(opath, cap, "%s/xyzfs/users/%s/owned.pdl", HOUSE, UUID); FILE *f = fopen(opath, "r"); if (!f) return 0; char line[1024]; int q = 0;
    while (fgets(line, sizeof line, f)) { if (strncmp(line, "OWN", 3)) continue; char *s = line, *tok, *fld[8]; int n = 0; while (n < 8 && (tok = strsep(&s, "|"))) fld[n++] = trim(tok); if (n >= 4 && !strcmp(fld[1], id)) q += atoi(fld[3]); }
    fclose(f); return q;
}
int main(int argc, char **argv) {
    if (argc != 5) { fprintf(stderr, "usage: ev_store <state_dir> <house_root> <verb> <item>\n"); return 2; }
    snprintf(STATE, sizeof STATE, "%s", argv[1]); snprintf(HOUSE, sizeof HOUSE, "%s", argv[2]); const char *verb = argv[3], *item = argv[4];
    if (!ident_ok(item, 64)) { errrow("bad item id", item); setvar("store_ok", 0); return 2; }
    { char p[PL + 32]; snprintf(p, sizeof p, "%s/user_uuid.txt", STATE); FILE *f = fopen(p, "r"); if (!f || !fgets(UUID, sizeof UUID, f)) { if (f) fclose(f); errrow("no player bound (user_uuid.txt)", ""); setvar("store_ok", 0); return 1; } fclose(f);
      UUID[strcspn(UUID, "\r\n")] = 0; for (char *c = UUID; *c; c++) if (!isxdigit((unsigned char)*c) && *c != '-') { errrow("bad user uuid", UUID); setvar("store_ok", 0); return 1; }
      char d[PL]; struct stat st; snprintf(d, sizeof d, "%s/xyzfs/users/%s", HOUSE, UUID); if (stat(d, &st) || !S_ISDIR(st.st_mode)) { errrow("unknown user folder", UUID); setvar("store_ok", 0); return 1; } }
    char ver[256] = "", price[256] = "", req[256] = "", opath[PL]; int known = find_item(item, ver, price, req, sizeof ver);
    if (!strcmp(verb, "price")) { setvar("store_known", known); setvar("store_price", known ? price_coins(price) : -1); ledger(verb, item, 0, known ? price : "unknown"); setvar("store_ok", known); return known ? 0 : 1; }
    if (!strcmp(verb, "owned")) { int q = owned_qty(item, opath, sizeof opath); setvar("store_owned", q); setvar("store_ok", 1); ledger(verb, item, 0, "ok"); return 0; }
    if (!strcmp(verb, "get")) {
        if (!known) { errrow("unknown item", item); setvar("store_ok", 0); ledger(verb, item, 1, "unknown"); return 1; }
        if (price_coins(price) != 0) { errrow("not a free item (buy is not built)", item); setvar("store_ok", 0); ledger(verb, item, 1, "not free"); return 1; }
        if (strcmp(req, "none")) { errrow("item has requirements; role checks are not built", item); setvar("store_ok", 0); ledger(verb, item, 1, "requires"); return 1; }
        if (owned_qty(item, opath, sizeof opath) > 0) { setvar("store_owned", owned_qty(item, opath, sizeof opath)); setvar("store_ok", 1); ledger(verb, item, 0, "already owned"); return 0; }
        FILE *f = fopen(opath, "a"); if (!f) { errrow("cannot write owned.pdl", opath); setvar("store_ok", 0); return 1; }
        fprintf(f, "OWN | %s | %s | 1 | free | event | %ld | -\n", item, ver, (long)time(NULL)); fclose(f);
        setvar("store_owned", 1); setvar("store_ok", 1); ledger(verb, item, 0, "recorded (nothing installed)"); return 0; }
    errrow("bad verb", verb); setvar("store_ok", 0); return 2;
}
