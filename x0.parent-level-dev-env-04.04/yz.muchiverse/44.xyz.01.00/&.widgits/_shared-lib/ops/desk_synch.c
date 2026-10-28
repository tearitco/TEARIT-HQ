/* desk_synch - Synch between a GAME (a book of rooms) and the LyfeDesk (the desk's open book and page), the two directions of the standard (18.pc-hq/PAGE-FILE.md, @.apps/piececraft-hq/ops/pc_synch_request.sh):
 *   desk_synch <house> from-desk <book> <room_id,room_id,...>      the DESK is the sender, the game follows: prints `GOTO <room>` when the desk's open book is <book> and its open page is one of the rooms
 *   desk_synch <house> to-desk   <book> <room_id>                   the GAME is the sender, the desk follows: only when <book> is already a session on the desk and that session has a page file desks/<room>.pdl
 *                                                                   (the same rule as pc_synch_request.sh: "a different book is taken only when that session directory is already on disk"); sets the open book (root session.pdl active_session) and the open page (STATE | active_desk)
 * The desk = the first user under <house>/xyzfs/users/<uuid>/home/livedesk/sessions whose session.pdl has `active_session | <id>` and whose <id>/session.pdl exists (the same lookup as pc_synch_request.sh).
 * Output: `DESK book=<b> page=<p>` (what the desk had), then one result line: `GOTO <room>` | `OK desk-follows-game book=<b> page=<p>` | `NOGO <reason>`. Reasons: no-desk, other-book, no-such-page, book-not-on-desk, page-not-in-book, bad-name.
 * Exit 0 = GOTO or OK, 1 = NOGO, 2 = usage. from-desk never writes; to-desk writes only the two STATE lines, atomically (file.tmp then rename).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/desk_synch.+x desk_synch.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int is_file(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISREG(s.st_mode); }
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static int name_ok(const char *s) { size_t n = strlen(s); if (!n || n > 48) return 0; for (size_t i = 0; i < n; i++) if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_' || s[i] == '-')) return 0; return 1; }
/* value of `STATE | key | value` (or `key | value`) in a pdl file */
static int pdl_value(const char *file, const char *key, char *out, size_t cap) {
    FILE *f = fopen(file, "r"); char l[512]; out[0] = 0; if (!f) return 0;
    while (fgets(l, sizeof l, f)) { char *f1 = l, *b1 = strchr(l, '|'); if (!b1) continue; *b1 = 0; char *f2 = b1 + 1, *b2 = strchr(f2, '|'); char *f3 = NULL; if (b2) { *b2 = 0; f3 = b2 + 1; }
        f1 = trim(f1); f2 = trim(f2); if (f3) f3 = trim(f3);
        if (f3 && !strcmp(f1, "STATE") && !strcmp(f2, key)) { snprintf(out, cap, "%s", f3); fclose(f); return 1; }
        if (!f3 && !strcmp(f1, key)) { snprintf(out, cap, "%s", f2); fclose(f); return 1; }
        if (f3 && !strcmp(f1, key)) { snprintf(out, cap, "%s", f2); fclose(f); return 1; } }
    fclose(f); return 0;
}
static int set_state(const char *file, const char *key, const char *val) {      /* replace `STATE | key | ...` or append it; atomically */
    char tmp[PATH_MAX + 16]; snprintf(tmp, sizeof tmp, "%s.synch-tmp", file); FILE *in = fopen(file, "r"), *o = fopen(tmp, "w"); if (!o) { if (in) fclose(in); return -1; }
    char l[1024]; int done = 0; char pat[80]; snprintf(pat, sizeof pat, "| %s |", key);
    if (in) while (fgets(l, sizeof l, in)) { if (!done && strstr(l, pat) && !strncmp(l, "STATE", 5)) { fprintf(o, "STATE | %s | %s\n", key, val); done = 1; } else fputs(l, o); }
    if (!done) fprintf(o, "STATE | %s | %s\n", key, val);
    if (in) { fclose(in); }
    if (fclose(o)) return -1;
    return rename(tmp, file);
}
int main(int argc, char **argv) {
    if (argc < 5) goto usage;
    const char *house = argv[1], *dir = argv[2], *book = argv[3], *arg = argv[4]; int from = !strcmp(dir, "from-desk"), to = !strcmp(dir, "to-desk"); if (!from && !to) goto usage;
    if (!name_ok(book)) { printf("NOGO bad-name\n"); return 1; }
    char users[PATH_MAX]; snprintf(users, sizeof users, "%s/xyzfs/users", house); DIR *d = opendir(users); struct dirent *e;
    char sroot[PATH_MAX] = "", active[96] = "";
    while (d && (e = readdir(d))) { if (e->d_name[0] == '.') continue; char sr[PATH_MAX + 80], rp[PATH_MAX + 120], a[96], ap[PATH_MAX + 220];
        snprintf(sr, sizeof sr, "%s/%s/home/livedesk/sessions", users, e->d_name); snprintf(rp, sizeof rp, "%s/session.pdl", sr);
        if (!pdl_value(rp, "active_session", a, sizeof a) || !a[0]) { continue; }
        snprintf(ap, sizeof ap, "%s/%s/session.pdl", sr, a);
        if (!is_file(ap)) { continue; }
        snprintf(sroot, sizeof sroot, "%s", sr); snprintf(active, sizeof active, "%s", a); break; }
    if (d) closedir(d);
    if (!sroot[0]) { printf("NOGO no-desk\n"); return 1; }
    char sp[PATH_MAX + 200], dbook[96], dpage[96]; snprintf(sp, sizeof sp, "%s/%s/session.pdl", sroot, active);
    if (!pdl_value(sp, "name", dbook, sizeof dbook) || !dbook[0]) snprintf(dbook, sizeof dbook, "%s", active);
    pdl_value(sp, "active_desk", dpage, sizeof dpage);
    printf("DESK book=%s page=%s\n", dbook, dpage);
    if (from) {
        if (strcmp(dbook, book)) { printf("NOGO other-book\n"); return 1; }
        char rooms[1024]; snprintf(rooms, sizeof rooms, "%s", arg); for (char *t = strtok(rooms, ","); t; t = strtok(NULL, ",")) if (!strcmp(trim(t), dpage) && dpage[0]) { printf("GOTO %s\n", dpage); return 0; }
        printf("NOGO no-such-page\n"); return 1;
    }
    if (!name_ok(arg)) { printf("NOGO bad-name\n"); return 1; }
    char bdir[PATH_MAX + 200], pg[PATH_MAX + 260]; snprintf(bdir, sizeof bdir, "%s/%s", sroot, book);
    if (!is_dir(bdir) || !is_file((snprintf(sp, sizeof sp, "%s/session.pdl", bdir), sp))) { printf("NOGO book-not-on-desk\n"); return 1; }
    snprintf(pg, sizeof pg, "%s/desks/%s.pdl", bdir, arg); if (!is_file(pg)) { printf("NOGO page-not-in-book\n"); return 1; }
    char rp[PATH_MAX + 120]; snprintf(rp, sizeof rp, "%s/session.pdl", sroot);
    if (strcmp(active, book) && set_state(rp, "active_session", book)) { printf("NOGO write-failed\n"); return 1; }
    if (set_state(sp, "name", book) || set_state(sp, "active_desk", arg)) { printf("NOGO write-failed\n"); return 1; }
    printf("OK desk-follows-game book=%s page=%s\n", book, arg); return 0;
usage:
    fprintf(stderr, "Usage: desk_synch <house> from-desk <book> <room_id,...> | to-desk <book> <room_id>\n"); return 2;
}
