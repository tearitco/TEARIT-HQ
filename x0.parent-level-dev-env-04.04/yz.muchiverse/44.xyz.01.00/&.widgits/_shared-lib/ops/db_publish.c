/* db_publish - publish the OPEN BOOK's db to the db window's mirror files (owner 2026-10-28: "it should just auto load depending on which book is open";
 * BOOK 16.game/PER-BOOK-DB-2026-10-28.md sections 6-7). The db-hq window shows <house>/#.desktop/db_hq_<tab>.state.txt; for the 13 tabs that are plain copies of
 * <tab>.pdl this op writes that mirror from the book's LAYER (<house>/@.apps/<book>/db/<tab>.pdl) when the book has the file, else from the shared TRUNK
 * (<house>/&.widgits/db-hq/data/<tab>.pdl), and rewrites a mirror ONLY when its bytes differ (content, not mtime). Not touched: terms (System.json) and common_events (generated lists).
 * The open book = the LyfeDesk active session: <house>/0.user-pal*\/00.login-signup/current_login.txt (current_user_uuid=) -> <house>/xyzfs/users/<uuid>/home/livedesk/sessions/session.pdl (STATE | active_session | <book>).
 * Usage:  db_publish <house> [--book <id>] [--dry]
 *   --book  publish that book instead of the active session (a plain id: letters, digits, _ -)   --dry  report only, write nothing
 * Output: `book=<id|-> layer=<yes|no> updated=<n> same=<m> skipped=<k>` after one line per tab (`<tab>: updated|same|skipped (layer|trunk)`).  Exit 0, 2 usage / bad book id / no house.
 * Build: sh build_db_publish.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *TABS[] = {"actors", "animations", "armors", "classes", "enemies", "items", "skills", "states", "system", "tilesets", "troops", "types", "weapons"};
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static int exists(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISREG(s.st_mode); }
static int id_ok(const char *s) { size_t n = strlen(s); if (!n || n > 48) return 0; for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0; } return 1; }
static int kv_bar(const char *file, const char *key, char *out, size_t cap) {      /* `STATE | key | value` or `key=value` */
    FILE *f = fopen(file, "r"); char l[512]; out[0] = 0; if (!f) return 0;
    while (fgets(l, sizeof l, f)) { l[strcspn(l, "\r\n")] = 0; char *p = l;
        if (!strncmp(p, "STATE", 5)) { char *a = strchr(p, '|'); if (!a) continue; a++; while (*a == ' ') a++; char *b = strchr(a, '|'); if (!b) continue; char *e = b - 1; while (e > a && *e == ' ') *e-- = 0; *b++ = 0; while (*b == ' ') b++; if (!strcmp(a, key)) { snprintf(out, cap, "%s", b); fclose(f); return 1; } }
        else { size_t kl = strlen(key); if (!strncmp(p, key, kl) && p[kl] == '=') { snprintf(out, cap, "%s", p + kl + 1); fclose(f); return 1; } } }
    fclose(f); return 0;
}
static char *slurp(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) return NULL; size_t cap = 1 << 16, len = 0; char *b = malloc(cap); if (!b) { fclose(f); return NULL; } size_t r;
    while ((r = fread(b + len, 1, cap - len, f)) > 0) { len += r; if (len == cap) { cap *= 2; char *nb = realloc(b, cap); if (!nb) { free(b); fclose(f); return NULL; } b = nb; } } fclose(f); *n = len; return b; }
int main(int argc, char **argv) {
    if (argc < 2) goto usage;
    char house[PATH_MAX], book[64] = "", sess_root[PATH_MAX + 200]; int dry = 0, have_book = 0; snprintf(house, sizeof house, "%s", argv[1]);
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--dry")) dry = 1; else if (!strcmp(argv[i], "--book") && i + 1 < argc) { snprintf(book, sizeof book, "%s", argv[++i]); have_book = 1; } else goto usage; }
    if (!is_dir(house)) { fprintf(stderr, "db_publish: no house at %s\n", house); return 2; }
    if (have_book && !id_ok(book)) { fprintf(stderr, "db_publish: bad book id\n"); return 2; }
    if (!have_book) {      /* the open book = the active LyfeDesk session */
        char uuid[128] = ""; DIR *d = opendir(house); struct dirent *e;
        while (d && (e = readdir(d))) if (!strncmp(e->d_name, "0.user-pal", 10)) { char p[PATH_MAX + 300]; snprintf(p, sizeof p, "%s/%s/00.login-signup/current_login.txt", house, e->d_name); if (kv_bar(p, "current_user_uuid", uuid, sizeof uuid) && uuid[0]) break; }
        if (d) closedir(d);
        if (uuid[0] && id_ok(uuid)) { snprintf(sess_root, sizeof sess_root, "%s/xyzfs/users/%s/home/livedesk/sessions/session.pdl", house, uuid); kv_bar(sess_root, "active_session", book, sizeof book); }
        if (book[0] && !id_ok(book)) book[0] = 0;
    }
    char layer[PATH_MAX + 64] = "", trunk[PATH_MAX + 64]; snprintf(trunk, sizeof trunk, "%s/&.widgits/db-hq/data", house);
    if (book[0]) { snprintf(layer, sizeof layer, "%s/@.apps/%s/db", house, book); if (!is_dir(layer)) layer[0] = 0; }
    int up = 0, same = 0, skip = 0;
    for (size_t t = 0; t < sizeof TABS / sizeof *TABS; t++) {
        char lp[PATH_MAX + 128], tp[PATH_MAX + 128], dst[PATH_MAX + 128]; const char *src = NULL, *which = "trunk";
        snprintf(lp, sizeof lp, "%s/%s.pdl", layer, TABS[t]); snprintf(tp, sizeof tp, "%s/%s.pdl", trunk, TABS[t]); snprintf(dst, sizeof dst, "%s/#.desktop/db_hq_%s.state.txt", house, TABS[t]);
        if (layer[0] && exists(lp)) { src = lp; which = "layer"; } else if (exists(tp)) src = tp;
        if (!src) { printf("%s: skipped (no data file)\n", TABS[t]); skip++; continue; }
        size_t sn = 0, dn = 0; char *sb = slurp(src, &sn), *db = exists(dst) ? slurp(dst, &dn) : NULL;
        if (!sb) { printf("%s: skipped (unreadable)\n", TABS[t]); skip++; free(db); continue; }
        if (db && dn == sn && !memcmp(sb, db, sn)) { printf("%s: same (%s)\n", TABS[t], which); same++; }
        else if (dry) { printf("%s: would update (%s)\n", TABS[t], which); up++; }
        else { char tmp[PATH_MAX + 140]; snprintf(tmp, sizeof tmp, "%s.tmp", dst); FILE *o = fopen(tmp, "wb"); int ok = o && fwrite(sb, 1, sn, o) == sn; if (o && fclose(o)) ok = 0; if (ok && !rename(tmp, dst)) { printf("%s: updated (%s)\n", TABS[t], which); up++; } else { remove(tmp); printf("%s: skipped (cannot write)\n", TABS[t]); skip++; } }
        free(sb); free(db);
    }
    printf("book=%s layer=%s updated=%d same=%d skipped=%d\n", book[0] ? book : "-", layer[0] ? "yes" : "no", up, same, skip);
    return 0;
usage:
    fprintf(stderr, "Usage: db_publish <house> [--book <id>] [--dry]\n"); return 2;
}
