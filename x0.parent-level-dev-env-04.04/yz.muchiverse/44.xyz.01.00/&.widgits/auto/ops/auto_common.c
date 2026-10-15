/* auto_common.c - text-include for the auto ops (design: BOOK 17.ai/AUTO-AND-AUTO-STUDIO-DESIGN.md). Pure helpers, no state: pipe-row parsing, auto.pdl keys, the per-user ledger, STOP file, action budget.
 * A user dir (--user DIR) holds  auto.pdl  (AUTO | key | value rows)  and  auto/ledger.txt (append-only)  and  auto/STOP (kill switch). Missing or malformed auto.pdl = everything off. */
#pragma GCC diagnostic ignored "-Wunused-function"      /* each op uses a subset of these helpers */
#pragma GCC diagnostic ignored "-Wformat-truncation"    /* paths are PL-sized; a longer path fails to open, never overflows */
#pragma GCC diagnostic ignored "-Wmisleading-indentation" /* house style packs short statements on one line */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#define PL 4096
static int ac_split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; if (n == max) break; char *q = strchr(p, '|'); if (!q) break; *q = 0; p = q + 1; } for (int i = 0; i < n; i++) { char *s = f[i]; while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; f[i] = s; } return n; }
static int ac_get(const char *user, const char *key, char *out, size_t cap) { char p[PL], ln[PL]; snprintf(p, sizeof p, "%s/auto.pdl", user); FILE *f = fopen(p, "r"); if (!f) return 0; int found = 0; while (!found && fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; char *fd[4]; int n = ac_split(ln, fd, 4); if (n >= 3 && !strcmp(fd[0], "AUTO") && !strcmp(fd[1], key)) { snprintf(out, cap, "%s", fd[2]); found = 1; } } fclose(f); return found; }
static int ac_is_on(const char *user, const char *key) { char v[64]; return ac_get(user, key, v, sizeof v) && !strcmp(v, "on"); }   /* anything but exactly "on" is off */
static void ac_ledger(const char *user, const char *fmt, ...) { char d[PL], p[PL]; snprintf(d, sizeof d, "%s/auto", user); mkdir(d, 0700); snprintf(p, sizeof p, "%s/ledger.txt", d); FILE *f = fopen(p, "a"); if (!f) return; va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fputc('\n', f); fclose(f); }
static int ac_stop_present(const char *user) { char p[PL]; snprintf(p, sizeof p, "%s/auto/STOP", user); return access(p, F_OK) == 0; }
static int ac_recent_acts(const char *user, int secs) { char p[PL], ln[PL]; snprintf(p, sizeof p, "%s/auto/ledger.txt", user); FILE *f = fopen(p, "r"); if (!f) return 0; long now = (long)time(NULL); int n = 0; while (fgets(ln, sizeof ln, f)) { if (strncmp(ln, "ACT | ", 6)) continue; long ts = atol(ln + 6); if (ts >= now - secs) n++; } fclose(f); return n; }
static long ac_size(const char *path) { struct stat st; return stat(path, &st) ? -1 : (long)st.st_size; }
static int ac_last_line(const char *path, char *out, size_t cap) { FILE *f = fopen(path, "r"); if (!f) return 0; char ln[PL]; out[0] = 0; int got = 0; while (fgets(ln, sizeof ln, f)) { if (ln[0] != '\n') { snprintf(out, cap, "%s", ln); got = 1; } } fclose(f); out[strcspn(out, "\r\n")] = 0; return got; }
static void ac_abs(const char *house, const char *rel, char *out, size_t cap) { if (rel[0] == '/') snprintf(out, cap, "%s", rel); else snprintf(out, cap, "%s/%s", house, rel); }
static int ac_in_list(const char *list, const char *item) { char buf[PL]; snprintf(buf, sizeof buf, "%s", list); for (char *t = strtok(buf, ", "); t; t = strtok(NULL, ", ")) if (!strcmp(t, item)) return 1; return 0; }
