/* piece_mint - mint ONE piece into the user's Master Pal List (BOOK 16.game/PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md sections 3-5; owner 2026-10-28: "minting is an op / event").
 * v1 = PAPER: no chain, price and fee are 0. Sources built: grant | copy | fork. mine buy auction npc market are REFUSED ("source not built") until the chain work exists.
 * The wallet id comes from the sibling op hash_op.+x (`hash_op wallet <uid>`), run by fork/exec: no shared header, no copy of hash code.
 *
 * Usage: piece_mint <house> --user <uuid> --template <actor name> --source grant|copy|fork [--slug <s>] [--kind <k>] [--book <id>] [--parent <uid>]
 *                          [--key <idempotency key>] [--len 8..64] [--adopt-uid <hex>] [--dry]
 *   --template  a name in the trunk db actors.pdl (or, with --book, in that book's db layer)      --slug  default = the template in lowercase; a pal folder of that name must not exist
 *   --book      also place the piece in <house>/@.apps/<book> (cast row, home_book = active_book = book); the book folder must exist and its cast must not already use the slug
 *   --parent    required for copy and fork (a uid in master_pals.pdl), forbidden for grant      --key   a second call with the same key returns the SAME piece and writes nothing
 *   --len       uid length in hex characters, default 32                                       --adopt-uid  grant only: use this existing uid instead of a random one (migration of game-local ids)
 *   --dry       run every check, print `dry: would mint ...`, write nothing
 * Writes (only after every check passes; all or nothing, a failed step is rolled back):
 *   <house>/xyzfs/users/<user>/home/livedesk/pals/<slug>/entity_uid.txt
 *   <pals>/master_pals.pdl   PIECE | uid | slug | kind | template | owner | origin | parent | home_book | active_book | minted_at | wallet
 *   <house>/@.apps/<book>/db/cast.pdl   CAST | uid | slug | role | shared | home          (only with --book)
 *   <pals>/piece_ledger.txt   MINT | ts | uid | source | price | fee | key
 * Output (stdout): `minted uid=<hex> wallet=<e..> slug=<s> source=<..> price=0 fee=0` | `existing uid=<hex> ...` (same key) | `dry: would mint ...` | `refused: <reason>`.
 * Exit: 0 ok | 1 refused or failed (nothing left behind) | 2 usage.   Build: sh build_piece_mint.sh  (needs hash_op.+x built next to it) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXF 16
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static int is_reg(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISREG(s.st_mode); }
static int exists(const char *p) { struct stat s; return stat(p, &s) == 0; }
static int name_ok(const char *s, int lower) { size_t n = strlen(s); if (!n || n > 64) return 0; for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (!lower && c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0; } return 1; }
static int hex_ok(const char *s) { size_t n = strlen(s); if (n < 8 || n > 64) return 0; for (size_t i = 0; i < n; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0; return 1; }
static int refuse(const char *why) { printf("refused: %s\n", why); return 1; }
/* whole file as a NUL-terminated string; *ok = 0 when the path exists but cannot be read as a regular file */
static char *slurp(const char *p, int *ok) {
    *ok = 1; if (!exists(p)) return NULL; if (!is_reg(p)) { *ok = 0; return NULL; }
    FILE *f = fopen(p, "rb"); if (!f) { *ok = 0; return NULL; } size_t cap = 4096, len = 0; char *b = malloc(cap); size_t r;
    while (b && (r = fread(b + len, 1, cap - len - 1, f)) > 0) { len += r; if (len + 1 >= cap) { cap *= 2; char *nb = realloc(b, cap); if (!nb) { free(b); b = NULL; break; } b = nb; } }
    fclose(f); if (!b) { *ok = 0; return NULL; } b[len] = 0; return b;
}
static int write_atomic(const char *p, const char *data) {
    char t[PATH_MAX + 8]; snprintf(t, sizeof t, "%s.tmp", p); FILE *f = fopen(t, "wb"); if (!f) return 0;
    size_t n = strlen(data); int ok = fwrite(data, 1, n, f) == n; if (fclose(f)) ok = 0; if (ok && rename(t, p) != 0) ok = 0; if (!ok) remove(t); return ok;
}
/* split one line on '|' into trimmed fields (in place); returns the field count */
static int split(char *l, char **f) {
    int n = 0; char *p = l; while (n < MAXF) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; }
    for (int i = 0; i < n; i++) { while (*f[i] == ' ' || *f[i] == '\t') f[i]++; size_t k = strlen(f[i]); while (k && (f[i][k - 1] == ' ' || f[i][k - 1] == '\t' || f[i][k - 1] == '\r')) f[i][--k] = 0; }
    return n;
}
/* first row of `text` whose line starts with `tag` and whose field[col] == val; copies field[ret] to out */
static int find_row(const char *text, const char *tag, int col, const char *val, int ret, char *out, size_t cap) {
    if (!text) { return 0; }
    const char *p = text;
    while (*p) { const char *e = strchr(p, '\n'); size_t n = e ? (size_t)(e - p) : strlen(p); char line[1024]; if (n >= sizeof line) n = sizeof line - 1; memcpy(line, p, n); line[n] = 0;
        if (line[0] != '#') { char *f[MAXF]; int c = split(line, f); if (c > col && c > ret && !strcmp(f[0], tag) && !strcmp(f[col], val)) { snprintf(out, cap, "%s", f[ret]); return 1; } }
        if (!e) { break; }
        p = e + 1;
    }
    return 0;
}
static int actor_named(const char *path, const char *name) {      /* an `ACTOR | name | <name>` row */
    int ok; char *t = slurp(path, &ok); if (!t) return 0; char tmp[256]; int r = 0; const char *p = t;
    while (*p) { const char *e = strchr(p, '\n'); size_t n = e ? (size_t)(e - p) : strlen(p); char line[1024]; if (n >= sizeof line) n = sizeof line - 1; memcpy(line, p, n); line[n] = 0; char *f[MAXF]; int c = split(line, f);
        if (c >= 3 && !strcmp(f[0], "ACTOR") && !strcmp(f[1], "name") && !strcmp(f[2], name)) { r = 1; break; }
        if (!e) { break; }
        p = e + 1;
    }
    (void)tmp; free(t); return r;
}
static int run_hash_wallet(const char *uid, char *out, size_t cap) {      /* fork/exec the sibling op hash_op.+x */
    char self[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return 0; self[n] = 0; char *sl = strrchr(self, '/'); if (!sl) return 0; strcpy(sl + 1, "hash_op.+x");
    int pp[2]; if (pipe(pp) != 0) return 0; pid_t pid = fork(); if (pid < 0) return 0;
    if (pid == 0) { dup2(pp[1], 1); close(pp[0]); close(pp[1]); execl(self, self, "wallet", uid, (char *)NULL); _exit(127); }
    close(pp[1]); char b[128]; ssize_t r = read(pp[0], b, sizeof b - 1); close(pp[0]); int st = 0; waitpid(pid, &st, 0);
    if (r <= 0 || !WIFEXITED(st) || WEXITSTATUS(st) != 0) { return 0; }
    b[r] = 0;
    b[strcspn(b, "\r\n")] = 0;
    if (strlen(b) != 25 || b[0] != 'e') { return 0; }
    snprintf(out, cap, "%s", b);
    return 1;
}
static int random_hex(int len, char *out) {
    unsigned char b[40]; FILE *f = fopen("/dev/urandom", "rb"); if (!f) return 0; int need = (len + 1) / 2; int ok = fread(b, 1, (size_t)need, f) == (size_t)need; fclose(f); if (!ok) return 0;
    for (int i = 0; i < need; i++) { sprintf(out + 2 * i, "%02x", b[i]); }
    out[len] = 0;
    return 1;
}
int main(int argc, char **argv) {
    if (argc < 2) goto usage;
    const char *house = argv[1], *user = NULL, *tmpl = NULL, *source = NULL, *slug = NULL, *kind = "pet", *book = NULL, *parent = NULL, *key = NULL, *adopt = NULL; int len = 32, dry = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--dry")) dry = 1;
        else if (i + 1 >= argc) goto usage;
        else if (!strcmp(argv[i], "--user")) user = argv[++i]; else if (!strcmp(argv[i], "--template")) tmpl = argv[++i]; else if (!strcmp(argv[i], "--source")) source = argv[++i];
        else if (!strcmp(argv[i], "--slug")) slug = argv[++i]; else if (!strcmp(argv[i], "--kind")) kind = argv[++i]; else if (!strcmp(argv[i], "--book")) book = argv[++i];
        else if (!strcmp(argv[i], "--parent")) parent = argv[++i]; else if (!strcmp(argv[i], "--key")) key = argv[++i]; else if (!strcmp(argv[i], "--adopt-uid")) adopt = argv[++i];
        else if (!strcmp(argv[i], "--len")) len = atoi(argv[++i]); else goto usage;
    }
    if (!is_dir(house) || !user || !tmpl || !source) goto usage;
    if (!name_ok(user, 0)) return refuse("bad user id");
    char pals[PATH_MAX], idxp[PATH_MAX + 32], ledp[PATH_MAX + 32]; snprintf(pals, sizeof pals, "%.2000s/xyzfs/users/%.64s/home/livedesk/pals", house, user);
    if (!is_dir(pals)) return refuse("owner unknown (no pals folder for that user)");
    snprintf(idxp, sizeof idxp, "%s/master_pals.pdl", pals); snprintf(ledp, sizeof ledp, "%s/piece_ledger.txt", pals);
    if (strcmp(source, "grant") && strcmp(source, "copy") && strcmp(source, "fork")) {
        if (!strcmp(source, "mine") || !strcmp(source, "buy") || !strcmp(source, "auction") || !strcmp(source, "npc") || !strcmp(source, "market")) return refuse("source not built (needs the chain work)");
        return refuse("unknown source");
    }
    if (len < 8 || len > 64) return refuse("--len must be 8 to 64");
    if (!slug) { static char sl[80]; snprintf(sl, sizeof sl, "%s", tmpl); for (char *c = sl; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32); slug = sl; }
    if (!name_ok(slug, 1) || strlen(slug) > 48) return refuse("bad slug (lowercase letters, digits, _ -)");
    if (key && !name_ok(key, 0)) return refuse("bad key");
    if (book && !name_ok(book, 0)) return refuse("bad book id");
    if (adopt && strcmp(source, "grant")) return refuse("--adopt-uid is for source grant only");
    if (adopt && !hex_ok(adopt)) return refuse("--adopt-uid must be 8-64 lowercase hex");
    if (!strcmp(source, "grant") && parent) return refuse("grant takes no --parent");
    if (strcmp(source, "grant") && !parent) return refuse("copy and fork need --parent");
    int ok1, ok2, ok3; char *idx = slurp(idxp, &ok1), *led = slurp(ledp, &ok2); if (!ok1 || !ok2) return refuse("cannot read the index or the ledger");
    char found[128];
    if (key && led && find_row(led, "MINT", 6, key, 2, found, sizeof found)) { char w[40] = ""; run_hash_wallet(found, w, sizeof w); printf("existing uid=%s wallet=%s slug=- source=%s price=0 fee=0\n", found, w, source); return 0; }
    if (parent && !find_row(idx, "PIECE", 1, parent, 1, found, sizeof found)) return refuse("parent not in the Master Pal List");
    char trunk[PATH_MAX + 64], lay[PATH_MAX + 96]; snprintf(trunk, sizeof trunk, "%.2000s/&.widgits/db-hq/data/actors.pdl", house); lay[0] = 0;
    char castp[PATH_MAX + 96] = "", bookdb[PATH_MAX + 96] = "", *cast = NULL;
    if (book) { char bd[PATH_MAX + 64]; snprintf(bd, sizeof bd, "%.2000s/@.apps/%.64s", house, book); if (!is_dir(bd)) return refuse("book folder not found");
        snprintf(bookdb, sizeof bookdb, "%s/db", bd); snprintf(castp, sizeof castp, "%s/cast.pdl", bookdb); snprintf(lay, sizeof lay, "%s/actors.pdl", bookdb);
        cast = slurp(castp, &ok3); if (!ok3) return refuse("cannot read the book's cast");
        if (cast && find_row(cast, "CAST", 2, slug, 2, found, sizeof found)) return refuse("slug taken in the book"); }
    if (!actor_named(trunk, tmpl) && !(lay[0] && actor_named(lay, tmpl))) return refuse("unknown template");
    char folder[PATH_MAX + 96], uidp[PATH_MAX + 160]; snprintf(folder, sizeof folder, "%s/%s", pals, slug); snprintf(uidp, sizeof uidp, "%s/entity_uid.txt", folder);
    if (exists(folder) || (idx && find_row(idx, "PIECE", 2, slug, 2, found, sizeof found))) return refuse("slug taken (a pal of that name exists)");
    char uid[80];
    if (adopt) { if (idx && find_row(idx, "PIECE", 1, adopt, 1, found, sizeof found)) return refuse("uid already in the Master Pal List"); snprintf(uid, sizeof uid, "%s", adopt); }
    else { int tries = 0; do { if (!random_hex(len, uid)) return refuse("no random source"); } while (idx && find_row(idx, "PIECE", 1, uid, 1, found, sizeof found) && ++tries < 16); if (tries >= 16) return refuse("could not find a free uid"); }
    char wallet[40]; if (!run_hash_wallet(uid, wallet, sizeof wallet)) return refuse("hash_op unavailable");
    if (dry) { printf("dry: would mint uid=%s wallet=%s slug=%s source=%s price=0 fee=0\n", uid, wallet, slug, source); return 0; }
    long ts = (long)time(NULL); char row[1024], crow[512], lrow[512];
    snprintf(row, sizeof row, "PIECE | %s | %s | %s | %s | %s | %s | %s | %s | %s | %ld | %s\n", uid, slug, kind, tmpl, user, source, parent ? parent : "-", book ? book : "-", book ? book : "-", ts, wallet);
    snprintf(crow, sizeof crow, "CAST | %s | %s | %s | 0 | %s\n", uid, slug, kind, book ? book : "-");
    snprintf(lrow, sizeof lrow, "MINT | %ld | %s | %s | 0 | 0 | %s\n", ts, uid, source, key ? key : "-");
    int made_dir = 0, made_uid = 0, w_idx = 0, w_cast = 0, w_led = 0, good = 0; int ok = 1;
    do {
        if (mkdir(folder, 0755) != 0) { ok = 0; break; } made_dir = 1;
        { char t[100]; snprintf(t, sizeof t, "%s\n", uid); if (!write_atomic(uidp, t)) { ok = 0; break; } made_uid = 1; }
        { const char *hd = "# PIECE | uid | slug | kind | template | owner | origin | parent | home_book | active_book | minted_at | wallet\n"; size_t n = (idx ? strlen(idx) : strlen(hd)) + strlen(row) + 2; char *nw = malloc(n); if (!nw) { ok = 0; break; }
          snprintf(nw, n, "%s%s", idx ? idx : hd, row); int r = write_atomic(idxp, nw); free(nw); if (!r) { ok = 0; break; } w_idx = 1; }
        if (book) { const char *hd = "# CAST | uid | slug | role | shared | home\n"; if (!is_dir(bookdb) && mkdir(bookdb, 0755) != 0) { ok = 0; break; }
          size_t n = (cast ? strlen(cast) : strlen(hd)) + strlen(crow) + 2; char *nw = malloc(n); if (!nw) { ok = 0; break; } snprintf(nw, n, "%s%s", cast ? cast : hd, crow); int r = write_atomic(castp, nw); free(nw); if (!r) { ok = 0; break; } w_cast = 1; }
        { size_t n = (led ? strlen(led) : 0) + strlen(lrow) + 2; char *nw = malloc(n); if (!nw) { ok = 0; break; } snprintf(nw, n, "%s%s", led ? led : "", lrow); int r = write_atomic(ledp, nw); free(nw); if (!r) { ok = 0; break; } w_led = 1; }
        good = 1;
    } while (0);
    if (!good) {      /* roll back in reverse so nothing is left behind */
        (void)ok; if (w_led) { if (led) write_atomic(ledp, led); else remove(ledp); }
        if (w_cast) { if (cast) write_atomic(castp, cast); else remove(castp); }
        if (w_idx) { if (idx) write_atomic(idxp, idx); else remove(idxp); }
        if (made_uid) { remove(uidp); }
        if (made_dir) { rmdir(folder); }
        return refuse("write failed (rolled back)");
    }
    printf("minted uid=%s wallet=%s slug=%s source=%s price=0 fee=0\n", uid, wallet, slug, source);
    return 0;
usage:
    fprintf(stderr, "Usage: piece_mint <house> --user <uuid> --template <actor> --source grant|copy|fork [--slug s] [--kind k] [--book id] [--parent uid] [--key k] [--len 8..64] [--adopt-uid hex] [--dry]\n"); return 2;
}
