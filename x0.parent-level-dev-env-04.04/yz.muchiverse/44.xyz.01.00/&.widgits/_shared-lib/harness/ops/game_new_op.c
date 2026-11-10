/* game_new_op - the GAME FOUNDATION GENERATOR (2026-11-09; owner: "we should have harnesses for simple stuff like making new GAME FOUNDATIONS where we could just put in args").
 *   game_new_op <house> new <name> --title "<User Name>" --kind ledger|realtime|editor [--players 2-4] [--seed N] [--into <dir>] [--dry-run]
 *   game_new_op <house> list                 the known kinds and their templates
 *   game_new_op <house> check <name>         re-run the generated app's own smoke checks (files, game.pdl, state.pdl, a scripted game on the shared engine, replay, audit, the book is openable)
 *
 * WHAT IT DOES: copies the template files named by harness/templates/game/<kind>/MANIFEST.pdl (and _common/MANIFEST.pdl, which every kind includes) into <house>/@.apps/<name>/ and, for the
 * harness files, into <house>/&.widgits/_shared-lib/harness/, replacing {{name}} {{Title}} {{kind}} {{seed}} {{players}}. It never writes a C string per game: templates are DATA. The app it makes has NO C of its own:
 * its engine, driver and button op are the one shared binary `_shared-lib/ops/+x/game_core.+x` (decision D61). It refuses: an existing target, a bad name (2 to 24 characters of a-z 0-9 - _, starting with a letter or digit,
 * and not an existing @.apps folder), an unknown kind, a bad title / players / seed, a template that uses a placeholder its MANIFEST row does not declare (or declares one it does not use), and any `{{` left in the output.
 * Nothing is written unless every file is built in memory and no target exists. A failure part-way removes what was written. --dry-run lists the files and writes nothing. --into <dir> puts the app in <dir>/<name>
 * (the harness case and pal are then NOT written: they name $HOUSE/@.apps/<name>).
 * MANIFEST.pdl rows:  KIND | name | description      INCLUDE | <path under templates/game>      FILE | <src> | <dest> | <octal mode> | <placeholders used, comma list or -> | text|bin      NOTE | text (printed after success)
 *   A dest starting with `harness/` is under <house>/&.widgits/_shared-lib/; any other dest is under the app folder. A dest may contain {{name}}.
 * Output: lines `created <path>`, then `NOTE ...`; on failure one line `game_new: <reason>` on stderr. Exit 0 ok, 1 a check failed, 2 usage or a refusal.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/game_new_op.+x game_new_op.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define P 4096
#define MAXF 128
#define MAXC 32

static char HOUSE[P], TPL[P], HARN[P];
static int die_(int code, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static int die_(int code, const char *fmt, ...) { va_list ap; va_start(ap, fmt); fprintf(stderr, "game_new: "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap); return code; }

typedef struct { char src[P], dest[P], phs[160]; unsigned mode; int bin; } Ent;
static Ent ENT[MAXF]; static int NENT; static char NOTES[16][400]; static int NNOTES;

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static char *slurp(const char *path, size_t *len) { FILE *f = fopen(path, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); if (n < 0 || n > (32L << 20)) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; } size_t r = fread(b, 1, (size_t)n, f); fclose(f); b[r] = 0; if (len) *len = r; return b; }
static void mkdir_p(const char *path, char created[][P], int *nc) { char t[P]; snprintf(t, sizeof t, "%s", path); for (char *c = t + 1; *c; c++) if (*c == '/') { *c = 0; if (mkdir(t, 0755) == 0 && created && *nc < 64) snprintf(created[(*nc)++], P, "%s", t); *c = '/'; } if (mkdir(t, 0755) == 0 && created && *nc < 64) snprintf(created[(*nc)++], P, "%s", t); }
static int exists(const char *p) { return access(p, F_OK) == 0; }
static int good_name(const char *s) { size_t n = strlen(s); if (n < 2 || n > 24) return 0; if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return 0;
    for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0; } return 1; }
static int good_title(const char *s) { size_t n = strlen(s); if (n < 1 || n > 40) return 0; if (s[0] == ' ' || s[n - 1] == ' ') return 0;
    for (size_t i = 0; i < n; i++) { unsigned char c = (unsigned char)s[i]; if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr(" -_.',:!&()", c))) return 0; } return 1; }

/* ---------- MANIFEST ---------- */
static int read_manifest(const char *rel, int depth, const char *kind_for_desc) {
    if (depth > 3) return die_(2, "MANIFEST includes nest too deep (%s)", rel);
    char p[P], ln[2048]; snprintf(p, sizeof p, "%s/%s", TPL, rel); FILE *f = fopen(p, "r"); if (!f) return die_(2, "cannot read %s", p); int lineno = 0, rc = 0;
    while (fgets(ln, sizeof ln, f)) { lineno++; if (ln[0] == '#' || ln[0] == '\n') continue; char *fl[8]; int n = split(ln, fl, 8);
        if (!strcmp(fl[0], "KIND")) { (void)kind_for_desc; continue; }
        else if (!strcmp(fl[0], "INCLUDE") && n >= 2) { if ((rc = read_manifest(fl[1], depth + 1, kind_for_desc))) break; }
        else if (!strcmp(fl[0], "NOTE") && n >= 2) { if (NNOTES < 16) snprintf(NOTES[NNOTES++], 400, "%s", fl[1]); }
        else if (!strcmp(fl[0], "FILE") && n >= 6) { if (NENT >= MAXF) { rc = die_(2, "%s: too many FILE rows", rel); break; } Ent *e = &ENT[NENT++]; snprintf(e->src, sizeof e->src, "%s", fl[1]); snprintf(e->dest, sizeof e->dest, "%s", fl[2]); char *end; e->mode = (unsigned)strtoul(fl[3], &end, 8); if (*end || e->mode > 0777) { rc = die_(2, "%s:%d: bad mode %s", rel, lineno, fl[3]); break; }
            snprintf(e->phs, sizeof e->phs, "%s", strcmp(fl[4], "-") ? fl[4] : ""); e->bin = !strcmp(fl[5], "bin"); if (!e->bin && strcmp(fl[5], "text")) { rc = die_(2, "%s:%d: the last field is text or bin", rel, lineno); break; } }
        else { rc = die_(2, "%s:%d: unknown or short row `%s`", rel, lineno, fl[0]); break; } }
    fclose(f); return rc;
}
static int kind_exists(const char *k) { char p[P]; snprintf(p, sizeof p, "%s/%s/MANIFEST.pdl", TPL, k); return strcmp(k, "_common") && good_name(k) && exists(p); }
static int kind_desc(const char *k, char *o, size_t cap) { char p[P], ln[1024]; snprintf(p, sizeof p, "%s/%s/MANIFEST.pdl", TPL, k); FILE *f = fopen(p, "r"); o[0] = 0; if (!f) return -1; while (fgets(ln, sizeof ln, f)) { char *fl[4]; if (ln[0] == '#') continue; int n = split(ln, fl, 4); if (n >= 3 && !strcmp(fl[0], "KIND")) { snprintf(o, cap, "%s", fl[2]); break; } } fclose(f); return 0; }

/* ---------- substitution ---------- */
typedef struct { const char *key; const char *val; } Var;
/* returns a malloc'd output or NULL (reason printed). used = the keys found (comma list). */
static char *subst(const char *in, size_t n, const Var *vars, int nv, char *used, size_t ucap, const char *what) {
    size_t cap = n * 2 + 256, o = 0; char *out = malloc(cap); if (!out) return NULL; used[0] = 0;
    for (size_t i = 0; i < n; ) {
        if (in[i] == '{' && i + 1 < n && in[i + 1] == '{') {
            size_t j = i + 2; while (j < n && in[j] != '}' && in[j] != '\n' && j - i < 40) j++;
            if (j + 1 >= n || in[j] != '}' || in[j + 1] != '}') { die_(2, "%s: an unclosed placeholder near byte %zu (`{{` with no `}}`)", what, i); free(out); return NULL; }
            char key[40]; size_t kl = j - (i + 2); if (kl == 0 || kl >= sizeof key) { die_(2, "%s: an empty or long placeholder near byte %zu", what, i); free(out); return NULL; } memcpy(key, in + i + 2, kl); key[kl] = 0;
            const char *val = NULL; for (int v = 0; v < nv; v++) if (!strcmp(vars[v].key, key)) val = vars[v].val;
            if (!val) { die_(2, "%s: unknown placeholder {{%s}} (known: name Title kind seed players)", what, key); free(out); return NULL; }
            { int have = 0; char cp2[160]; snprintf(cp2, sizeof cp2, "%s", used); for (char *u = strtok(cp2, ","); u; u = strtok(NULL, ",")) if (!strcmp(u, key)) have = 1; if (!have) { if (used[0]) strncat(used, ",", ucap - strlen(used) - 1); strncat(used, key, ucap - strlen(used) - 1); } }
            size_t vl = strlen(val); if (o + vl + 2 > cap) { cap = (o + vl) * 2 + 256; out = realloc(out, cap); if (!out) return NULL; } memcpy(out + o, val, vl); o += vl; i = j + 2; continue; }
        if (o + 2 > cap) { cap *= 2; out = realloc(out, cap); if (!out) return NULL; } out[o++] = in[i++]; }
    out[o] = 0; if (strstr(out, "{{")) { die_(2, "%s: an unreplaced placeholder is left in the output", what); free(out); return NULL; } return out;
}
static int list_has(const char *list, const char *item) { size_t L = strlen(item); for (const char *q = list; *q; ) { const char *e = strchr(q, ','); size_t n = e ? (size_t)(e - q) : strlen(q); if (n == L && !strncmp(q, item, L)) return 1; if (!e) break; q = e + 1; } return 0; }
static int list_n(const char *list) { if (!*list) return 0; int n = 1; for (const char *q = list; *q; q++) if (*q == ',') n++; return n; }
static int same_set(const char *a, const char *b) {   /* comma lists, any order, no repeats */
    if (list_n(a) != list_n(b)) return 0;
    for (const char *q = a; *q; ) { const char *e = strchr(q, ','); char it[48]; size_t n = e ? (size_t)(e - q) : strlen(q); if (n >= sizeof it) return 0; memcpy(it, q, n); it[n] = 0; if (!list_has(b, it)) return 0; if (!e) break; q = e + 1; }
    return 1;
}
/* ---------- running a program (no shell) ---------- */
static int run(char *const av[], char *out, size_t cap) {
    int fd[2]; if (pipe(fd)) return -1; if (out) out[0] = 0; pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { alarm(60); close(fd[0]); dup2(fd[1], 1); dup2(fd[1], 2); close(fd[1]); execv(av[0], av); _exit(127); }
    close(fd[1]); size_t n = 0; char c[512]; ssize_t r; while ((r = read(fd[0], c, sizeof c)) > 0) { if (out && n + (size_t)r < cap) { memcpy(out + n, c, (size_t)r); n += (size_t)r; out[n] = 0; } }
    close(fd[0]); int st = 0; waitpid(pid, &st, 0); if (out) { size_t ol = strlen(out); while (ol && out[ol - 1] == 10) out[--ol] = 0; } return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* ---------- new ---------- */
static int cmd_new(int argc, char **argv) {
    if (argc < 1) return die_(2, "usage: game_new_op <house> new <name> --title \"<User Name>\" --kind ledger|realtime|editor [--players 2-4] [--seed N] [--into <dir>] [--dry-run]");
    const char *name = argv[0], *title = NULL, *kind = NULL, *into = NULL; long players = 3, seed = 7; int dry = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dry-run")) dry = 1;
        else if (i + 1 < argc && !strcmp(argv[i], "--title")) title = argv[++i];
        else if (i + 1 < argc && !strcmp(argv[i], "--kind")) kind = argv[++i];
        else if (i + 1 < argc && !strcmp(argv[i], "--into")) into = argv[++i];
        else if (i + 1 < argc && !strcmp(argv[i], "--players")) { char *e; players = strtol(argv[++i], &e, 10); if (*e || players < 2 || players > 4) return die_(2, "--players is a number 2 to 4"); }
        else if (i + 1 < argc && !strcmp(argv[i], "--seed")) { char *e; seed = strtol(argv[++i], &e, 10); if (*e || seed < 0 || seed > 999999999) return die_(2, "--seed is a number 0 to 999999999"); }
        else return die_(2, "unknown or incomplete argument `%s`", argv[i]); }
    if (!good_name(name)) return die_(2, "bad name `%s`: 2 to 24 characters of a-z 0-9 - _, starting with a letter or digit", name);
    if (!title) return die_(2, "--title is required"); if (!good_title(title)) return die_(2, "bad --title: 1 to 40 characters of letters, digits, space and - _ . ' , : ! & ( ) (no bar, no brace)");
    if (!kind) return die_(2, "--kind is required (ledger, realtime or editor)"); if (!kind_exists(kind)) return die_(2, "unknown kind `%s` (try: game_new_op <house> list)", kind);
    char appsdir[P], app[P]; snprintf(appsdir, sizeof appsdir, "%s/@.apps", HOUSE);
    { char clash[P]; snprintf(clash, sizeof clash, "%s/@.apps/%s", HOUSE, name); if (exists(clash)) return die_(2, "`%s` already exists in @.apps (%s): pick another name", name, clash); }
    if (into) { if (into[0] != '/') return die_(2, "--into must be an absolute path"); snprintf(app, sizeof app, "%s/%s", into, name); } else snprintf(app, sizeof app, "%s/%s", appsdir, name);
    if (exists(app)) return die_(2, "the target %s already exists", app);
    NENT = 0; NNOTES = 0; char km[P]; snprintf(km, sizeof km, "%s/MANIFEST.pdl", kind); if (read_manifest(km, 0, kind)) return 2;
    char ss[16], pp[16]; snprintf(ss, sizeof ss, "%ld", seed); snprintf(pp, sizeof pp, "%ld", players);
    Var vars[5] = { { "name", name }, { "Title", title }, { "kind", kind }, { "seed", ss }, { "players", pp } };
    char *outb[MAXF]; size_t outl[MAXF]; char dest[MAXF][P]; unsigned modes[MAXF]; int nd = 0, rc = 0, skipped = 0; memset(outb, 0, sizeof outb);
    for (int i = 0; i < NENT && !rc; i++) {
        Ent *e = &ENT[i]; char sp[P]; snprintf(sp, sizeof sp, "%s/%s", TPL, e->src); size_t n = 0; char *raw = slurp(sp, &n); if (!raw) { rc = die_(2, "cannot read the template %s", sp); break; }
        char dd[P], used[160]; char *d2 = subst(e->dest, strlen(e->dest), vars, 5, used, sizeof used, e->dest); if (!d2) { free(raw); rc = 2; break; } snprintf(dd, sizeof dd, "%s", d2); free(d2);
        if (strstr(dd, "..") || dd[0] == '/') { free(raw); rc = die_(2, "bad destination %s in the MANIFEST", dd); break; }
        char full[P]; int is_h = !strncmp(dd, "harness/", 8); if (is_h && into) { skipped++; free(raw); continue; }
        if (is_h) snprintf(full, sizeof full, "%s/%s", HARN, dd); else snprintf(full, sizeof full, "%s/%s", app, dd);
        if (e->bin) { if (e->phs[0]) { free(raw); rc = die_(2, "%s: a bin file declares placeholders", e->src); break; } outb[nd] = raw; outl[nd] = n; }
        else { char *o = subst(raw, n, vars, 5, used, sizeof used, e->src); free(raw); if (!o) { rc = 2; break; }
            if (!same_set(used, e->phs)) { free(o); rc = die_(2, "%s: the file uses placeholders [%s] but its MANIFEST row declares [%s]", e->src, used, e->phs); break; } outb[nd] = o; outl[nd] = strlen(o); }
        snprintf(dest[nd], P, "%s", full); modes[nd] = e->mode; nd++; }
    if (!rc) for (int i = 0; i < nd; i++) if (exists(dest[i])) { rc = die_(2, "the target file %s already exists", dest[i]); break; }
    if (rc) { for (int i = 0; i < nd; i++) free(outb[i]); return 2; }
    if (dry) { printf("DRY-RUN: %s (%s) would write %d files into %s%s:\n", name, kind, nd, app, skipped ? " (harness files skipped by --into)" : ""); for (int i = 0; i < nd; i++) printf("would create %s (%zu bytes)\n", dest[i], outl[i]); for (int i = 0; i < nd; i++) free(outb[i]); return 0; }
    char created_dirs[64][P]; int ncd = 0; char created_files[MAXF][P]; int ncf = 0;
    for (int i = 0; i < nd && !rc; i++) { char dir[P]; snprintf(dir, sizeof dir, "%s", dest[i]); char *sl = strrchr(dir, '/'); if (sl) { *sl = 0; mkdir_p(dir, created_dirs, &ncd); }
        int fd = open(dest[i], O_WRONLY | O_CREAT | O_EXCL, modes[i] ? modes[i] : 0644); if (fd < 0) { rc = die_(2, "cannot create %s: %s", dest[i], strerror(errno)); break; }
        snprintf(created_files[ncf++], P, "%s", dest[i]); size_t off = 0; while (off < outl[i]) { ssize_t w = write(fd, outb[i] + off, outl[i] - off); if (w <= 0) { rc = die_(2, "cannot write %s", dest[i]); break; } off += (size_t)w; }
        if (close(fd) && !rc) rc = die_(2, "cannot close %s", dest[i]); if (!rc && fchmodat(AT_FDCWD, dest[i], modes[i] ? modes[i] : 0644, 0)) rc = die_(2, "cannot set the mode of %s", dest[i]); }
    if (rc) { for (int i = ncf - 1; i >= 0; i--) unlink(created_files[i]); for (int i = ncd - 1; i >= 0; i--) rmdir(created_dirs[i]); for (int i = 0; i < nd; i++) free(outb[i]); return 2; }
    printf("created %s (%s) in %s: %d files%s\n", name, kind, app, nd, skipped ? " (harness case and pal skipped by --into)" : "");
    for (int i = 0; i < nd; i++) printf("created %s\n", dest[i]);
    for (int i = 0; i < NNOTES; i++) printf("NOTE %s\n", NOTES[i]);
    printf("NOTE the shared engine is %s/&.widgits/_shared-lib/ops/+x/game_core.+x (build_harness_ops.sh builds it); check: game_new_op %s check %s\n", HOUSE, HOUSE, name);
    printf("NOTE README row to add by hand (harness/README.md is shared): | `%s.pal` | `cases/%s.pdl` | N | the generated %s game %s: replay, tamper refused, audit, projections |\n", name, name, kind, title);
    for (int i = 0; i < nd; i++) free(outb[i]); return 0;
}

/* ---------- list ---------- */
static int cmd_list(void) {
    DIR *d = opendir(TPL); if (!d) return die_(2, "no templates at %s", TPL); struct dirent *e; char names[MAXC][64]; int n = 0;
    while ((e = readdir(d)) && n < MAXC) if (e->d_name[0] != '.' && kind_exists(e->d_name)) snprintf(names[n++], 64, "%s", e->d_name); closedir(d);
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (strcmp(names[i], names[j]) > 0) { char t[64]; memcpy(t, names[i], 64); memcpy(names[i], names[j], 64); memcpy(names[j], t, 64); }
    for (int i = 0; i < n; i++) { char ds[200]; kind_desc(names[i], ds, sizeof ds); NENT = 0; NNOTES = 0; char km[P]; snprintf(km, sizeof km, "%s/MANIFEST.pdl", names[i]); if (read_manifest(km, 0, names[i])) return 2; int nh = 0; for (int k = 0; k < NENT; k++) if (!strncmp(ENT[k].dest, "harness/", 8)) nh++; printf("kind %s: %s (templates/game/%s/MANIFEST.pdl, %d files, %d of them harness)\n", names[i], ds, names[i], NENT, nh); }
    if (!n) return die_(2, "no kinds found under %s", TPL); return 0;
}

/* ---------- check ---------- */
static int NCHK, NPASS, NFAIL;
static void res(int ok, const char *code, const char *fmt, ...) { char b[600]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); NCHK++; if (ok) NPASS++; else NFAIL++; printf("%s %s %s\n", ok ? "PASS" : "FAIL", code, b); }
static int file_has(const char *p, const char *needle) { size_t n; char *b = slurp(p, &n); if (!b) return 0; int r = strstr(b, needle) != NULL; free(b); return r; }
static int cmd_check(int argc, char **argv) {
    if (argc < 1) return die_(2, "usage: game_new_op <house> check <name>"); const char *name = argv[0]; if (!good_name(name)) return die_(2, "bad name `%s`", name);
    char app[P], p[P], core[P], b[8192], out[4096]; snprintf(app, sizeof app, "%s/@.apps/%s", HOUSE, name); if (argc > 1) snprintf(app, sizeof app, "%s", argv[1]); snprintf(core, sizeof core, "%s/&.widgits/_shared-lib/ops/+x/game_core.+x", HOUSE);
    if (!exists(app)) return die_(2, "no app at %s", app);
    static const char *must[] = { "game.pdl", "state.pdl", "verbs.pdl", "rooms.pdl", "layouts.pdl", "harness.pdl", "nav.pdl", "tuning.pdl", "rulesets/default/board.pdl", "rulesets/default/rules.pdl", NULL };
    int miss = 0; for (int i = 0; must[i]; i++) { snprintf(p, sizeof p, "%s/%s", app, must[i]); if (!exists(p)) { res(0, "files", "missing %s", must[i]); miss++; } } if (!miss) res(1, "files", "%d app files present", 10);
    char kind[24] = "", gname[64] = "", driver[600] = "", title[100] = ""; snprintf(p, sizeof p, "%s/game.pdl", app); { FILE *f = fopen(p, "r"); char ln[1024]; if (f) { while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; char *fl[6]; int n = split(ln, fl, 6); if (n >= 3 && !strcmp(fl[0], "GAME")) { if (!strcmp(fl[1], "kind")) snprintf(kind, sizeof kind, "%s", fl[2]); else if (!strcmp(fl[1], "name")) snprintf(gname, sizeof gname, "%s", fl[2]); else if (!strcmp(fl[1], "driver")) snprintf(driver, sizeof driver, "%s", fl[2]); else if (!strcmp(fl[1], "title")) snprintf(title, sizeof title, "%s", fl[2]); } } fclose(f); } }
    res((!strcmp(kind, "ledger") || !strcmp(kind, "realtime") || !strcmp(kind, "editor")) && !strcmp(gname, name) && title[0], "game.pdl", "GAME rows: name=%s kind=%s title=%s", gname, kind, title);
    { char exp[P + 700]; size_t eo = 0; const char *s = driver; while (*s && eo + 8 < sizeof exp) { if (!strncmp(s, "{house}", 7)) { eo += (size_t)snprintf(exp + eo, sizeof exp - eo, "%s", HOUSE); s += 7; } else { exp[eo++] = *s++; } } exp[eo] = 0; char *sp = strchr(exp, ' '); if (sp) *sp = 0;
      res(driver[0] && !access(exp, X_OK), "driver", "GAME | driver names an executable (%s)", exp[0] ? exp : "none"); }
    snprintf(p, sizeof p, "%s/state.pdl", app); res(file_has(p, "FILE | ledger.txt | LEDGER") || file_has(p, "FILE | ledger.txt  | LEDGER"), "state.pdl", "declares ledger.txt as LEDGER");
    snprintf(p, sizeof p, "%s/%s-pchq/pieces/system/maps/%s/game.pdl", app, name, name); int pc = exists(p) && file_has(p, "desk_1_id");
    snprintf(b, sizeof b, "%s/%s-pchq/pieces/system/maps/%s/board/map.txt", app, name, name); res(pc && exists(b), "pchq", "the book package: maps/%s/game.pdl with desk_1_id and board/map.txt", name);
    snprintf(p, sizeof p, "%s/@.apps/rpg-edit-hq/ops/edit_open.sh", HOUSE);
    if (exists(p)) { snprintf(b, sizeof b, "%s", p); char *av[] = { (char *)"/bin/sh", b, HOUSE, (char *)name, (char *)"board", NULL }; int rc = run(av, out, sizeof out); res(rc == 0 && strstr(out, "book=") != NULL, "openable", "the editor's own read-only edit_open.sh opens %s:board (%.80s)", name, rc == 0 ? out : "refused"); }
    else printf("SKIP openable the editor's edit_open.sh is not in this house\n");
    if (access(core, X_OK)) { res(0, "engine", "%s is not built (build_harness_ops.sh builds it)", core); }
    else {   /* a scripted game on the shared engine in a scratch folder, then the tamper controls */
        char tmpl[] = "/tmp/game_check_XXXXXX"; if (!mkdtemp(tmpl)) return die_(2, "cannot make a scratch folder"); char sd[P]; snprintf(sd, sizeof sd, "%s/g", tmpl); mkdir(sd, 0755);
        struct { const char *a, *b, *c, *d; } steps_l[] = { { "new", "7", NULL, NULL }, { "join", "A", "pet", NULL }, { "join", "B", "pet", NULL }, { "start", NULL, NULL, NULL }, { "autoplay", NULL, NULL, NULL }, { "check", NULL, NULL, NULL }, { "replay", NULL, NULL, NULL }, { "audit", NULL, NULL, NULL }, { NULL, NULL, NULL, NULL } };
        struct { const char *a, *b, *c, *d; } steps_e[] = { { "new", NULL, NULL, NULL }, { "add", "start", "the start", NULL }, { "add", "goal", "the goal", NULL }, { "publish", NULL, NULL, NULL }, { "export", NULL, NULL, NULL }, { "check", NULL, NULL, NULL }, { "replay", NULL, NULL, NULL }, { "audit", NULL, NULL, NULL }, { NULL, NULL, NULL, NULL } };
        int ed = !strcmp(kind, "editor"); int ok = 1; char last_replay[200] = "";
        for (int i = 0; ed ? steps_e[i].a : steps_l[i].a; i++) { const char *a = ed ? steps_e[i].a : steps_l[i].a, *b2 = ed ? steps_e[i].b : steps_l[i].b, *c2 = ed ? steps_e[i].c : steps_l[i].c; char *av[10]; int k = 0; av[k++] = core; av[k++] = app; av[k++] = sd; av[k++] = (char *)a; if (b2) av[k++] = (char *)b2; if (c2) av[k++] = (char *)c2; av[k] = NULL;
            int rc = run(av, out, sizeof out); { size_t ol = strlen(out); while (ol && out[ol - 1] == 10) out[--ol] = 0; } if (rc != 0) { res(0, "engine", "`%s` exited %d: %.150s", a, rc, out); ok = 0; break; } if (!strcmp(a, "replay")) snprintf(last_replay, sizeof last_replay, "%.150s", out); }
        if (ok) { res(1, "engine", "a scripted %s game ran: new ... %s ... check replay audit all exit 0 (%.60s)", kind, ed ? "add publish export" : "join start autoplay", last_replay);
            char *av[] = { core, app, sd, (char *)"replay", NULL }; run(av, out, sizeof out); res(!strcmp(out, last_replay), "replay", "a second replay prints the same state hash");
            snprintf(p, sizeof p, "%s/stray.txt", sd); FILE *f = fopen(p, "w"); if (f) { fputs("x\n", f); fclose(f); } char *av2[] = { core, app, sd, (char *)"audit", NULL }; int rc = run(av2, out, sizeof out); res(rc == 1 && strstr(out, "stray.txt"), "audit", "a stray file fails the audit and is named (control: it passed before)"); unlink(p);
            snprintf(p, sizeof p, "%s/ledger.txt", sd); size_t n; char *lb = slurp(p, &n); const char *from = ed ? "the start" : "|new|7|", *to = ed ? "the stort" : "|new|8|"; char *q = lb ? strstr(lb, from) : NULL;
            res(q != NULL, "tamper-setup", "the tamper target `%s` is in the ledger (vacuity)", from); if (q) { memcpy(q, to, strlen(to)); FILE *w = fopen(p, "w"); if (w) { fwrite(lb, 1, n, w); fclose(w); } } free(lb);
            char *av3[] = { core, app, sd, (char *)"replay", NULL }; rc = run(av3, out, sizeof out); res(rc == 1 && strstr(out, "ledger line"), "tamper", "a hand-edited ledger is refused (exit %d, %.70s)", rc, out); }
        char *rmv[] = { (char *)"/bin/rm", (char *)"-rf", tmpl, NULL }; run(rmv, out, sizeof out); }
    printf("checks=%d pass=%d fail=%d\n", NCHK, NPASS, NFAIL); return NFAIL ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc < 3) return die_(2, "usage: game_new_op <house> new|list|check ...   (new <name> --title T --kind ledger|realtime|editor [--players 2-4] [--seed N] [--into DIR] [--dry-run])");
    snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); size_t L = strlen(HOUSE); while (L > 1 && HOUSE[L - 1] == '/') HOUSE[--L] = 0;
    if (HOUSE[0] != '/') return die_(2, "the house must be an absolute path");
    snprintf(HARN, sizeof HARN, "%s/&.widgits/_shared-lib", HOUSE); snprintf(TPL, sizeof TPL, "%s/&.widgits/_shared-lib/harness/templates/game", HOUSE);
    if (!strcmp(argv[2], "new")) return cmd_new(argc - 3, argv + 3);
    if (!strcmp(argv[2], "list")) return cmd_list();
    if (!strcmp(argv[2], "check")) return cmd_check(argc - 3, argv + 3);
    return die_(2, "unknown command `%s` (new, list or check)", argv[2]);
}
