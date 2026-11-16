/* hostlist_op.c - the MASTER HOST LIST of the +phone (2026-11-15; design: BOOK 16.game/PLUS-PHONE-DESIGN-2026-11-15.md section 31). A plain text list of open hosts a phone app can join (kind = the app, for example tsc), kept locally and refreshed from a master list
 * (a file, or a static https file such as a GitHub raw file or release asset, or a Supabase-style REST view that returns the same rows). Rows only ever say WHERE a host is; the app's own handshake decides everything else.
 *   hostlist_op publish <list> <kind> <id> <addr> <port> [--ttl S] [--note T] [--now N]   add or replace the row of <id> (expires now+ttl, default 600)
 *   hostlist_op list    <list> [<kind>] [--now N]                                           print the rows that have not expired: kind id addr port seconds_left note
 *   hostlist_op seeds   <list> <kind> [--now N]                                             print addr:port,addr:port (the PALNET_SEEDS form) of the live rows of a kind
 *   hostlist_op remove  <list> <id>        hostlist_op prune <list> [--now N]                remove one row / every expired row
 *   hostlist_op fetch   <source> <list> [--now N]                                           merge a master list into the local list. <source> = a file path, or http(s)://... fetched by curl (10 s, 64 KB cap, no cookies, generic user agent, NO identity sent).
 * Row file:  HOST | kind | id | addr | port | expires_epoch | note        Every row is VALIDATED on the way in (a fetched row that fails is dropped and counted, never stored): kind [a-z0-9_-]{1,16}, id [A-Za-z0-9_.-]{1,48}, addr an IPv4 or hostname [A-Za-z0-9.-]{1,80}, port 1024-65535, expires an integer, note printable without a bar, max 60.
 * The list is a runtime file under a cap: at most 256 rows, rewritten atomically (temp + rename). Exit 0 ok, 1 a refused value, 2 usage or I/O.
 * Build: gcc -std=gnu11 -Wall -O2 -o +x/hostlist_op.+x hostlist_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define P 4096
#define MAXROWS 256
typedef struct { char kind[20], id[52], addr[84], note[64]; int port; long exp; } Row;
static Row R[MAXROWS]; static int NR; static long NOW;
static int okchars(const char *s, size_t lo, size_t hi, const char *extra, int lower) { size_t n = strlen(s); if (n < lo || n > hi) return 0; for (; *s; s++) { unsigned char c = (unsigned char)*s; if (isalnum(c) && !(lower && isupper(c))) continue; if (c && strchr(extra, c)) continue; return 0; } return 1; }
static int row_ok(const Row *r) { return okchars(r->kind, 1, 16, "_-", 1) && okchars(r->id, 1, 48, "_.-", 0) && okchars(r->addr, 1, 80, ".-", 0) && r->port >= 1024 && r->port <= 65535 && r->exp > 0; }
static int note_ok(const char *s) { if (strlen(s) > 60) return 0; for (; *s; s++) if ((unsigned char)*s < 32 || *s == '|' || *s == 127) return 0; return 1; }
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int parse_line(char *ln, Row *r) { char *fl[8]; int n = 0; char *p = ln; while (n < 7) { fl[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) fl[i] = trim(fl[i]);
    if (n < 6 || strcmp(fl[0], "HOST")) return 0; memset(r, 0, sizeof *r); snprintf(r->kind, sizeof r->kind, "%s", fl[1]); snprintf(r->id, sizeof r->id, "%s", fl[2]); snprintf(r->addr, sizeof r->addr, "%s", fl[3]); char *e; long port = strtol(fl[4], &e, 10); if (*e) return 0; r->port = (int)port; r->exp = strtol(fl[5], &e, 10); if (*e) return 0; if (n >= 7) { if (!note_ok(fl[6])) return 0; snprintf(r->note, sizeof r->note, "%s", fl[6]); } return row_ok(r); }
static int load(const char *f) { NR = 0; FILE *fp = fopen(f, "r"); if (!fp) return 1; char ln[512]; while (fgets(ln, sizeof ln, fp) && NR < MAXROWS) { Row r; if (ln[0] == '#') continue; if (parse_line(ln, &r)) R[NR++] = r; } fclose(fp); return 1; }
static int save(const char *f) { char t[P + 8]; snprintf(t, sizeof t, "%s.tmp", f); FILE *w = fopen(t, "w"); if (!w) return 0; fprintf(w, "# hostlist.pdl - open hosts (kind, id, address, port, expiry epoch, note); written by hostlist_op, rows validated on the way in\n"); for (int i = 0; i < NR; i++) fprintf(w, "HOST | %s | %s | %s | %d | %ld | %s\n", R[i].kind, R[i].id, R[i].addr, R[i].port, R[i].exp, R[i].note); fclose(w); return rename(t, f) == 0; }
static void put(const Row *r) { for (int i = 0; i < NR; i++) if (!strcmp(R[i].id, r->id) && !strcmp(R[i].kind, r->kind)) { if (r->exp >= R[i].exp) R[i] = *r; return; } if (NR < MAXROWS) R[NR++] = *r; }
static int prune(void) { int k = 0, gone = 0; for (int i = 0; i < NR; i++) { if (R[i].exp > NOW) R[k++] = R[i]; else gone++; } NR = k; return gone; }
static int fetch_url(const char *url, const char *out) {      /* curl, no shell; a plain GET: no cookies, no identity, generic user agent, 10 s, 64 KB */
    pid_t c = fork(); if (c < 0) return 0; if (c == 0) { int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); } int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) dup2(fd, 1);
        char *av[] = { "curl", "-fsS", "--max-time", "10", "--max-filesize", "65536", "-A", "phone-hostlist/1", "--proto", "=https,http", url, NULL }; execvp("curl", av); _exit(127); } int st; waitpid(c, &st, 0); return WIFEXITED(st) && WEXITSTATUS(st) == 0; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: hostlist_op publish|list|seeds|remove|prune|fetch ...\n"); return 2; } NOW = time(NULL); long ttl = 600; const char *note = "";
    for (int i = 3; i + 1 < argc; i++) { if (!strcmp(argv[i], "--now")) NOW = atol(argv[i + 1]); else if (!strcmp(argv[i], "--ttl")) ttl = atol(argv[i + 1]); else if (!strcmp(argv[i], "--note")) note = argv[i + 1]; }
    const char *v = argv[1];
    if (!strcmp(v, "fetch")) { if (argc < 4) return 2; const char *src = argv[2], *list = argv[3]; char tmp[P]; const char *file = src; int remote = !strncmp(src, "http://", 7) || !strncmp(src, "https://", 8);
        if (remote) { snprintf(tmp, sizeof tmp, "%s.fetch", list); if (!fetch_url(src, tmp)) { printf("FETCH-FAILED %s (nothing changed)\n", src); unlink(tmp); return 1; } file = tmp; }
        struct stat sb; if (stat(file, &sb) || sb.st_size > 65536) { printf("FETCH-REFUSED %s is missing or larger than 64 KB\n", file); if (remote) unlink(tmp); return 1; }
        load(list); FILE *fp = fopen(file, "r"); int added = 0, dropped = 0; char ln[512]; while (fp && fgets(ln, sizeof ln, fp)) { if (ln[0] == '#' || !trim(ln)[0]) continue; Row r; if (parse_line(ln, &r) && r.exp > NOW) { put(&r); added++; } else dropped++; } if (fp) fclose(fp); if (remote) unlink(tmp);
        prune(); if (!save(list)) return 2; printf("FETCHED added=%d dropped=%d rows=%d\n", added, dropped, NR); return 0; }
    const char *list = argv[2]; load(list);
    if (!strcmp(v, "publish")) { if (argc < 7) return 2; Row r; memset(&r, 0, sizeof r); snprintf(r.kind, sizeof r.kind, "%s", argv[3]); snprintf(r.id, sizeof r.id, "%s", argv[4]); snprintf(r.addr, sizeof r.addr, "%s", argv[5]); r.port = atoi(argv[6]); r.exp = NOW + ttl; if (!note_ok(note)) { printf("HOST-REFUSED the note has a bar, a control character or is over 60\n"); return 1; } snprintf(r.note, sizeof r.note, "%s", note);
        if (ttl < 10 || ttl > 86400 || !row_ok(&r)) { printf("HOST-REFUSED a value is not valid (kind, id, address, port 1024-65535, ttl 10-86400)\n"); return 1; } put(&r); prune(); if (!save(list)) return 2; printf("HOST-PUBLISHED %s %s:%d\n", r.id, r.addr, r.port); return 0; }
    if (!strcmp(v, "list")) { const char *kind = argc >= 4 && strncmp(argv[3], "--", 2) ? argv[3] : NULL; int n = 0; for (int i = 0; i < NR; i++) { if (R[i].exp <= NOW || (kind && strcmp(kind, R[i].kind))) continue; printf("%s %s %s %d %ld %s\n", R[i].kind, R[i].id, R[i].addr, R[i].port, R[i].exp - NOW, R[i].note); n++; } printf("HOSTS %d\n", n); return 0; }
    if (!strcmp(v, "seeds")) { if (argc < 4) return 2; int first = 1; for (int i = 0; i < NR; i++) { if (R[i].exp <= NOW || strcmp(argv[3], R[i].kind)) continue; printf("%s%s:%d", first ? "" : ",", R[i].addr, R[i].port); first = 0; } printf("\n"); return 0; }
    if (!strcmp(v, "remove")) { if (argc < 4) return 2; int k = 0; for (int i = 0; i < NR; i++) if (strcmp(R[i].id, argv[3])) R[k++] = R[i]; int gone = NR - k; NR = k; if (!save(list)) return 2; printf("HOST-REMOVED %d\n", gone); return 0; }
    if (!strcmp(v, "prune")) { int gone = prune(); if (!save(list)) return 2; printf("PRUNED %d\n", gone); return 0; }
    fprintf(stderr, "hostlist_op: unknown verb %s\n", v); return 2;
}
