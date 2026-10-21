#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/* bp_book_op - compile a BOOK of events into prisc programs, one per trigger (B4b of BOOK 08-roadmap/design-docs/BLUEPRINTZ-DESIGN.md section 11;
 * guide: 02-architecture/BLUEPRINT-EVENT-PAGES-GUIDE.md section 12).
 *
 *   bp_book_op <house> <sources.pdl|-> <out_dir> [--no-common] [--stub]      compile
 *   bp_book_op <house> --run <out_dir> <trigger> [<caller_entity_dir>]         run the compiled programs of one trigger in a prisc VM
 *
 * THE BOOK is the EVENT rows of <sources.pdl> (EVENT | name | <event dir>, relative to <house> or absolute, the file blueprintz keeps) plus, unless
 * --no-common, every <house>/common_events/<name>. THE ORDER OF THE LOOP (the same as play_event.sh, written down here because it is the contract):
 * for a trigger T, first every ordinary event in book order, then every common event by name; inside ONE event only its highest-numbered page whose
 * condition.pdl trigger is exactly T runs (RPG Maker "highest page wins"); pages without an event.pal are skipped. Switch conditions are NOT evaluated
 * (play_event.sh does not either); a page's switch is only listed in the index.
 * OUTPUT <out_dir>: book.<trigger>[.<n>].pal (ordinary events), book.common.<trigger>[.<n>].pal (common events)   programs per trigger (cut into parts at a segment boundary when it nears the VM's 1024 instructions
 *                                              or 128 labels), book.index.pdl, shims/s<k>.sh   one tiny script per command.
 * A page's event.pal is inlined: `exec cmd_N.sh` becomes `exec shims/sK.sh` (exec resolves a relative target against the pal's own folder and is limited to
 * 255 characters, house paths are longer, so the shim holds the absolute path), a `halt` before the end of a page becomes a jump to that page's end,
 * labels are prefixed per page, and a page that uses registers gets x1..x15 zeroed first (separate VMs started with zeroed registers before). The shim of a
 * COMMON event's command exports MUCHI_TARGET_ENT = $MUCHI_CALLER_PKG (when set and not already set), exactly what play_event.sh does for a common event, so a
 * common event acts on the played entity and an ordinary event on itself. --stub writes shims that only append "s<k> <event> <page>" to <out_dir>/stub.log, to
 * check order and structure without running any game command. book.pal is generated, never the source: the IR stays the one source of truth. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <ctype.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAXEV 512
#define MAXPG 48
#define MAXTRIG 64
#define P PATH_MAX
#define INST_CAP 900
#define LABEL_CAP 100

typedef struct { char name[64], dir[P]; int common, npg; struct { char name[32], trig[64], sw[64]; int num; } pg[MAXPG]; } Ev;
static Ev EV[MAXEV]; static int nEV = 0;
static char TRIG[MAXTRIG][64]; static int nTRIG = 0;
static int g_stub = 0, g_shim = 0, g_labels = 0, g_ninst = 0;
static char g_out[P], g_house[P];

static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int cmp_ev(const void *a, const void *b) { return strcmp(((const Ev *)a)->name, ((const Ev *)b)->name); }
static int exists(const char *p) { return access(p, F_OK) == 0; }
static void add_trig(const char *t) { for (int i = 0; i < nTRIG; i++) if (!strcmp(TRIG[i], t)) return; if (nTRIG < MAXTRIG) snprintf(TRIG[nTRIG++], 64, "%s", t); }

static void load_pages(Ev *e) {
    char root[P]; snprintf(root, P, "%s/event_pkg/pages", e->dir); DIR *d = opendir(root); if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && e->npg < MAXPG) {
        if (strncmp(de->d_name, "page_", 5) || strlen(de->d_name) >= 32) continue;
        int i = e->npg++; snprintf(e->pg[i].name, 32, "%s", de->d_name); e->pg[i].num = atoi(de->d_name + 5); e->pg[i].trig[0] = 0; e->pg[i].sw[0] = 0;
        char c[P], l[400]; snprintf(c, P, "%s/%s/condition.pdl", root, de->d_name); FILE *f = fopen(c, "r");
        if (f) { while (fgets(l, sizeof l, f)) if (!strncmp(l, "COND", 4)) {
                char *t = strstr(l, "trigger"), *w = strstr(l, "switch"), *k = t ? t : w; if (!k) continue; char *bar = strchr(k, '|'); if (!bar) continue;
                char v[64]; snprintf(v, 64, "%s", trim(bar + 1)); char *o = v, *q = v; while (*q) { if (*q != ' ') *o++ = *q; q++; } *o = 0;      /* play_event.sh drops spaces too */
                snprintf(t ? e->pg[i].trig : e->pg[i].sw, 64, "%s", v); }
            fclose(f); }
        if (e->pg[i].trig[0]) add_trig(e->pg[i].trig);
    }
    closedir(d);
    for (int i = 1; i < e->npg; i++) { typeof(e->pg[0]) t = e->pg[i]; int j = i - 1; while (j >= 0 && e->pg[j].num > t.num) { e->pg[j + 1] = e->pg[j]; j--; } e->pg[j + 1] = t; }
}
static int have_ev(const char *name) { for (int i = 0; i < nEV; i++) if (!strcmp(EV[i].name, name)) return 1; return 0; }

static void load_book(const char *src, int with_common) {
    if (strcmp(src, "-")) {
        FILE *f = fopen(src, "r"); char l[P + 200];
        while (f && fgets(l, sizeof l, f) && nEV < MAXEV) {
            if (strncmp(l, "EVENT |", 7)) continue; l[strcspn(l, "\r\n")] = 0;
            char *a = strchr(l + 7, '|'); if (!a) continue; *a = 0; char *name = trim(l + 7), *path = trim(a + 1);
            Ev *e = &EV[nEV]; memset(e, 0, sizeof *e); snprintf(e->name, 64, "%s", name);
            if (path[0] == '/') snprintf(e->dir, P, "%s", path); else snprintf(e->dir, P, "%s/%s", g_house, path);
            e->common = strstr(e->dir, "/common_events/") != NULL; nEV++;
        }
        if (f) fclose(f);
    }
    if (with_common) {
        char dir[P]; snprintf(dir, P, "%s/common_events", g_house); DIR *d = opendir(dir); struct dirent *de;
        while (d && (de = readdir(d)) && nEV < MAXEV) {
            if (de->d_name[0] == '.' || have_ev(de->d_name)) continue; char t[P]; snprintf(t, P, "%s/%s/event_pkg/pages", dir, de->d_name); if (!exists(t)) continue;
            Ev *e = &EV[nEV]; memset(e, 0, sizeof *e); snprintf(e->name, 64, "%s", de->d_name); snprintf(e->dir, P, "%s/%s", dir, de->d_name); e->common = 1; nEV++;
        }
        if (d) closedir(d);
    }
    /* ordinary events keep book order; common events are sorted by name and come after */
    Ev *tmp = malloc(sizeof(Ev) * (size_t)(nEV ? nEV : 1)); int k = 0, nc = 0;
    for (int i = 0; i < nEV; i++) if (!EV[i].common) tmp[k++] = EV[i];
    for (int i = 0; i < nEV; i++) if (EV[i].common) { tmp[k + nc] = EV[i]; nc++; }
    qsort(tmp + k, (size_t)nc, sizeof(Ev), cmp_ev);
    memcpy(EV, tmp, sizeof(Ev) * (size_t)nEV); free(tmp);
    for (int i = 0; i < nEV; i++) load_pages(&EV[i]);
}

static int ident_ok(const char *s) { if (!*s) return 0; for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '_') return 0; return 1; }
static void fsafe(char *o, size_t n, const char *t) { size_t k = 0; for (; *t && k + 1 < n; t++) o[k++] = (isalnum((unsigned char)*t) || *t == '_' || *t == '-' || *t == '.') ? *t : '_'; o[k] = 0; }

/* one shim per command; returns its number */
static int make_shim(const Ev *e, const char *page, const char *cmd_abs) {
    int k = ++g_shim; char p[P]; snprintf(p, P, "%s/shims/s%d.sh", g_out, k); FILE *f = fopen(p, "w"); if (!f) return k;
    fprintf(f, "#!/bin/sh\n");
    if (g_stub) fprintf(f, "echo 's%d %s %s' >> \"$(dirname \"$0\")/../stub.log\"\n", k, e->name, page);
    else {
        if (e->common) fprintf(f, "if [ -n \"$MUCHI_CALLER_PKG\" ]; then MUCHI_TARGET_ENT=\"${MUCHI_TARGET_ENT:-$MUCHI_CALLER_PKG}\"; export MUCHI_TARGET_ENT; fi\n");
        fputs("exec sh '", f); for (const char *c = cmd_abs; *c; c++) { if (*c == '\'') fputs("'\\''", f); else fputc(*c, f); } fputs("' \"$@\"\n", f);
    }
    fclose(f); chmod(p, 0755); return k;
}

/* does the instruction text use a register? (outside quotes) */
static int uses_regs(const char *l) { int q = 0; for (const char *c = l; *c; c++) { if (*c == '"') q = !q; else if (!q && *c == 'x' && isdigit((unsigned char)c[1]) && (c == l || !isalnum((unsigned char)c[-1]))) return 1; } return 0; }

/* inline one page's event.pal; segment number seg (for label prefixes). Returns the instructions written. */
static int emit_page(FILE *out, const Ev *e, const char *page, int seg) {
    char pagedir[P], palp[P]; snprintf(pagedir, P, "%s/event_pkg/pages/%s", e->dir, page); snprintf(palp, P, "%s/event.pal", pagedir);
    FILE *f = fopen(palp, "r"); if (!f) return 0;
    static char lines[600][600]; int n = 0; char raw[600];
    while (n < 600 && fgets(raw, sizeof raw, f)) { char *t = trim(raw); if (!*t || *t == '#') continue; snprintf(lines[n++], 600, "%s", t); }
    fclose(f);
    static char labs[64][40]; int nl = 0; int regs = 0;
    for (int i = 0; i < n; i++) { size_t L = strlen(lines[i]); if (L > 1 && lines[i][L - 1] == ':' && !strchr(lines[i], ' ') && nl < 64) { snprintf(labs[nl], 40, "%.*s", (int)L - 1, lines[i]); nl++; } else if (strncmp(lines[i], "exec", 4) && strcmp(lines[i], "halt") && uses_regs(lines[i])) regs = 1; }
    int ninst = 0, used_end = 0; int last_inst = -1; for (int i = n - 1; i >= 0; i--) { size_t L = strlen(lines[i]); if (!(L > 1 && lines[i][L - 1] == ':' && !strchr(lines[i], ' '))) { last_inst = i; break; } }
    fprintf(out, "# --- %s %s (%s, trigger %s) ---\n", e->name, page, e->common ? "common event" : "event", "");
    if (regs) for (int r = 1; r <= 15; r++) { fprintf(out, "li x%d, 0\n", r); ninst++; }
    for (int i = 0; i < n; i++) {
        char *l = lines[i]; size_t L = strlen(l);
        if (L > 1 && l[L - 1] == ':' && !strchr(l, ' ')) { fprintf(out, "p%d_%.*s:\n", seg, (int)(L - 1 > 22 ? 22 : L - 1), l); g_labels++; continue; }
        char tok[64] = ""; sscanf(l, "%63s", tok);
        if (!strcmp(tok, "halt")) { if (i == last_inst) continue; fprintf(out, "j p%d_end\n", seg); used_end = 1; ninst++; continue; }
        if (!strcmp(tok, "exec")) {
            char t2[300] = "", rest[300] = ""; sscanf(l + 4, " %299s %299[^\n]", t2, rest); size_t T = strlen(t2);
            if (T > 3 && !strcmp(t2 + T - 3, ".sh") && !strchr(t2, '/')) {
                char cmd[P]; snprintf(cmd, P, "%s/%s", pagedir, t2);
                if (!exists(cmd)) { fprintf(out, "# MISSING %s (no exec emitted)\n", t2); continue; }
                int k = make_shim(e, page, cmd); fprintf(out, "exec shims/s%d.sh%s%s\n", k, rest[0] ? " " : "", rest); ninst++; continue; }
        }
        if (!strcmp(tok, "j") || !strcmp(tok, "beq") || !strcmp(tok, "bne") || !strcmp(tok, "jal") || !strcmp(tok, "blt") || !strcmp(tok, "bge")) {
            char *lastsp = strrchr(l, ' '); char *lastcm = strrchr(l, ','); char *cut = lastsp > lastcm ? lastsp : lastcm;
            if (cut) { char lab[40]; snprintf(lab, 40, "%s", cut + 1); int known = 0; for (int q = 0; q < nl; q++) if (!strcmp(labs[q], lab)) known = 1;
                if (known) { fprintf(out, "%.*s p%d_%.22s\n", (int)(cut - l + (*cut == ',' ? 1 : 0)), l, seg, lab); ninst++; continue; } }
        }
        fprintf(out, "%s\n", l); ninst++;
    }
    if (used_end) { fprintf(out, "p%d_end:\n", seg); g_labels++; }
    return ninst;
}

typedef struct { char name[64]; } Name;
static void write_index_row(FILE *ix, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void write_index_row(FILE *ix, const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(ix, fmt, ap); va_end(ap); fputc('\n', ix); }

static int compile(const char *src, int with_common) {
    char sh[P]; snprintf(sh, P, "%s/shims", g_out); mkdir(g_out, 0755); mkdir(sh, 0755);
    load_book(src, with_common);
    char ip[P]; snprintf(ip, P, "%s/book.index.pdl", g_out); FILE *ix = fopen(ip, "w"); if (!ix) { fprintf(stderr, "bp_book: cannot write %s\n", ip); return 2; }
    int npages = 0; for (int i = 0; i < nEV; i++) npages += EV[i].npg;
    write_index_row(ix, "BOOK | %s | events=%d | pages=%d | triggers=%d | order=ordinary events in book order, then common events by name; highest matching page per event; switches not evaluated | stub=%d", g_house, nEV, npages, nTRIG, g_stub);
    for (int i = 0; i < nEV; i++) for (int j = 0; j < EV[i].npg; j++)
        write_index_row(ix, "PAGE | %s | %s | %s | trigger=%s | switch=%s", EV[i].common ? "common" : "event", EV[i].name, EV[i].pg[j].name, EV[i].pg[j].trig, EV[i].pg[j].sw[0] ? EV[i].pg[j].sw : "-");
    int nseg_total = 0, nfiles = 0;
    for (int t = 0; t < nTRIG; t++) {
        char tf[96]; fsafe(tf, sizeof tf, TRIG[t]);
        for (int pass = 0; pass < 2; pass++) {
            const char *base = pass ? "common." : "";      /* ordinary events and common events are SEPARATE programs: the runner gives the common one the played entity as target, as play_event.sh does */
            int part = 1, ninst = 0, nseg = 0; FILE *out = NULL; char names[2000] = ""; g_labels = 0;
            for (int i = 0; i < nEV; i++) {
                if (EV[i].common != pass) continue;
                int pick = -1; for (int j = 0; j < EV[i].npg; j++) if (!strcmp(EV[i].pg[j].trig, TRIG[t])) { char pp[P]; snprintf(pp, P, "%s/event_pkg/pages/%s/event.pal", EV[i].dir, EV[i].pg[j].name); if (exists(pp)) pick = j; }
                if (pick < 0) continue;
                if (out && (ninst > INST_CAP || g_labels > LABEL_CAP)) { fprintf(out, "halt\n"); fclose(out); out = NULL; part++; ninst = 0; g_labels = 0; }
                if (!out) { char fp[P];
                    if (part == 1) snprintf(fp, P, "%s/book.%s%s.pal", g_out, base, tf); else snprintf(fp, P, "%s/book.%s%s.%d.pal", g_out, base, tf, part);
                    out = fopen(fp, "w"); if (!out) return 2; nfiles++;
                    fprintf(out, "# book.%s%s.pal part %d - GENERATED by bp_book_op from the IR/event.pal of the book (never edit; rebuild). trigger: %s\n", base, tf, part, TRIG[t]); }
                nseg_total++; nseg++; ninst += emit_page(out, &EV[i], EV[i].pg[pick].name, nseg_total);
                size_t L = strlen(names); snprintf(names + L, sizeof names - L, "%s%s:%s", L ? "," : "", EV[i].name, EV[i].pg[pick].name);
            }
            if (out) { fprintf(out, "halt\n"); fclose(out); }
            if (nseg) write_index_row(ix, "TRIGGER | %s | %s | file=book.%s%s.pal | parts=%d | segments=%d | %s", TRIG[t], pass ? "common" : "event", base, tf, part, nseg, names);
        }
    }
    fclose(ix);
    printf("bp_book: ok  events=%d pages=%d triggers=%d segments=%d shims=%d files=%d  -> %s\n", nEV, npages, nTRIG, nseg_total, g_shim, nfiles, g_out);
    return 0;
}

static int run_book(const char *out_dir, const char *trigger, const char *caller) {
    char tf[96]; fsafe(tf, sizeof tf, trigger);
    glob_t g; char pat[P]; snprintf(pat, P, "%s/101.mutaclsym*/system/prisc+x", g_house); char prisc[P] = "";
    if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc >= 1) snprintf(prisc, P, "%s", g.gl_pathv[0]); globfree(&g);
    if (!prisc[0]) { fprintf(stderr, "bp_book: no prisc+x under %s\n", g_house); return 2; }
    char sysdir[P]; snprintf(sysdir, P, "%s", prisc); char *sl = strrchr(sysdir, '/'); if (sl) *sl = 0;
    int ran = 0;
    for (int pass = 0; pass < 2; pass++) for (int part = 1; part < 100; part++) {
        const char *base = pass ? "common." : "";
        char fp[P]; if (part == 1) snprintf(fp, P, "%s/book.%s%s.pal", out_dir, base, tf); else snprintf(fp, P, "%s/book.%s%s.%d.pal", out_dir, base, tf, part);
        if (!exists(fp)) break;
        pid_t pid = fork(); if (pid < 0) return 2;
        if (pid == 0) { if (chdir(sysdir)) _exit(126); unsetenv("MUCHI_TARGET_ENT"); unsetenv("MUCHI_CALLER_PKG");
            if (caller && caller[0]) { if (pass) { setenv("MUCHI_CALLER_PKG", caller, 1); setenv("MUCHI_TARGET_ENT", caller, 1); } else setenv("MUCHI_CALLER_PKG", caller, 1); }
            execl(prisc, prisc, fp, (char *)NULL); _exit(127); }
        int st; waitpid(pid, &st, 0); ran++; if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { fprintf(stderr, "bp_book: %s part %d exited abnormally\n", pass ? "common" : "event", part); return 1; }
    }
    printf("bp_book: ran %d part(s) of trigger %s\n", ran, trigger);
    return ran ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: bp_book_op <house> <sources.pdl|-> <out_dir> [--no-common] [--stub]\n       bp_book_op <house> --run <out_dir> <trigger> [<caller_entity_dir>]\n"); return 2; }
    snprintf(g_house, P, "%s", argv[1]);
    if (!strcmp(argv[2], "--run")) { if (argc < 5) return 2; return run_book(argv[3], argv[4], argc > 5 ? argv[5] : ""); }
    snprintf(g_out, P, "%s", argv[3]); int with_common = 1;
    for (int i = 4; i < argc; i++) { if (!strcmp(argv[i], "--no-common")) with_common = 0; else if (!strcmp(argv[i], "--stub")) g_stub = 1; }
    (void)ident_ok; (void)sizeof(Name);
    return compile(argv[2], with_common);
}
