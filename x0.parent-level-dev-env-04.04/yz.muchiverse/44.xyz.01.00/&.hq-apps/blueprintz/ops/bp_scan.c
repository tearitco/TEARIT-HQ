/* bp_scan.c - blueprintz page view, read-only (BOOK BLUEPRINTZ-DESIGN.md 12.3, 12.8, 12.9).
 * Usage: bp_scan <house> --dir <pkg_dir> [--dir <pkg_dir> ...]
 *        bp_scan <house> --sources <bp_sources.pdl>      rows:  EVENT | <name> | <pkg_dir (absolute, or relative to the house)>
 * <pkg_dir> is a folder that holds event_pkg/ (an entity or a common event). For each event prints one row
 *   EV | name | trigger(s) | state | flags
 * and one FLAG row per problem:  FLAG | name | page_N | code | detail
 * State comes from the debugger's append-only files (queue.txt / trace.txt in the state dir): waiting (default),
 * queued, running (START without END), ran (last run ended). It never writes anything.
 * Flags (each is a rule that reads files only):
 *   dup-id         the same id= twice in one event.ir.pdl
 *   empty-params   a command whose registry entry lists PARAMS has an empty parameter field
 *   unknown-type   command type not in #.ref/menu/event_commands.registry.pdl
 *   no-condition   page has no condition.pdl with a trigger: play_event.sh skips it, it can never run
 *   no-pal         page has an event.ir.pdl but no event.pal
 *   missing-cmd    event.pal has "exec cmd_N.sh" but that file is missing
 *   dead-target    a cmd wrapper execs a program that does not exist (house-rooted $D/... or ../../...)
 *   abs-path       event.pal or a wrapper contains an absolute path that is not under this house
 *   over-128       more than 128 NODE rows (compile_page stops reading at 128, silently)
 * Test override: BLUEPRINTZ_STATE_DIR (same as bp_ctl/play_event.sh). Exit: 0 no flags, 1 flags found, 2 usage. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define P 4400
static char house[4096], state[P];
static int nflags_total = 0;

static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static int is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static char *trim(char *s) { while (isspace((unsigned char)*s)) s++; char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; return s; }

/* ---- registry: command name -> needs params ---- */
typedef struct { char name[64]; int params; } Cmd;
static Cmd reg[1024]; static int nreg = 0;
static void load_registry(void) {
    char p[P]; snprintf(p, P, "%s/#.ref/menu/event_commands.registry.pdl", house);
    FILE *f = fopen(p, "r"); if (!f) return;
    char l[1024]; int cur = -1;
    while (fgets(l, sizeof l, f)) {
        char *t = trim(l);
        if (strncmp(t, "COMMAND ", 8) == 0 && nreg < 1024) { cur = nreg++; snprintf(reg[cur].name, 64, "%s", trim(t + 8)); reg[cur].params = 0; }
        else if (cur >= 0 && strncmp(t, "PARAMS", 6) == 0 && trim(t + 6)[0]) reg[cur].params = 1;
        else if (strncmp(t, "END", 3) == 0) cur = -1;
    }
    fclose(f);
}
static Cmd *find_cmd(const char *n) { for (int i = 0; i < nreg; i++) if (!strcmp(reg[i].name, n)) return &reg[i]; return NULL; }

static void flag(const char *ev, const char *page, const char *code, const char *detail) {
    printf("FLAG | %s | %s | %s | %s\n", ev, page, code, detail); nflags_total++;
}

static int lines_with(const char *path, const char *prefix, char out[][512], int max) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    char l[2048]; int n = 0;
    while (fgets(l, sizeof l, f)) if (strncmp(l, prefix, strlen(prefix)) == 0 && n < max) { l[strcspn(l, "\r\n")] = 0; snprintf(out[n++], 512, "%s", l); }
    fclose(f); return n;
}

/* an absolute path in text that is not under the house */
static int has_foreign_abs(const char *path, char *found, size_t fn) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char l[4096]; int hit = 0; size_t hl = strlen(house);
    while (!hit && fgets(l, sizeof l, f)) {
        char *p = l;
        while ((p = strstr(p, "\"/home/"))) {
            if (strncmp(p + 1, house, hl) != 0) { snprintf(found, fn, "%.120s", p + 1); found[strcspn(found, "\"\r\n")] = 0; hit = 1; break; }
            p++;
        }
    }
    fclose(f); return hit;
}

/* dead-target / missing-cmd checks for one page */
static void check_wrappers(const char *ev, const char *page, const char *pagedir) {
    char pal[P]; snprintf(pal, P, "%s/event.pal", pagedir);
    FILE *f = fopen(pal, "r"); if (!f) return;
    char l[1024];
    while (fgets(l, sizeof l, f)) {
        char *t = trim(l);
        if (strncmp(t, "exec ", 5) != 0) continue;
        char *arg = trim(t + 5); char cmd[256]; snprintf(cmd, sizeof cmd, "%s", arg);
        cmd[strcspn(cmd, " \t\"")] = 0;
        if (strncmp(cmd, "cmd_", 4) != 0) continue;
        char cp[P]; snprintf(cp, P, "%s/%s", pagedir, cmd);
        if (!exists(cp)) { flag(ev, page, "missing-cmd", cmd); continue; }
        FILE *cf = fopen(cp, "r"); if (!cf) continue;
        char cl[2048], target[P] = "";
        while (fgets(cl, sizeof cl, cf)) {
            char *c = trim(cl); if (strncmp(c, "exec ", 5) != 0) continue;
            char *a = trim(c + 5);
            if (!strncmp(a, "sh ", 3)) a = trim(a + 3);
            if (*a == '"') { a++; char *q = strchr(a, '"'); if (q) *q = 0; } else a[strcspn(a, " \t")] = 0;
            snprintf(target, P, "%s", a);
        }
        fclose(cf);
        if (!target[0]) continue;
        char real[P];
        if (!strncmp(target, "$D/", 3)) snprintf(real, P, "%s/%s", house, target + 3);
        else if (!strncmp(target, "../../", 6)) snprintf(real, P, "%s/../../../%s", pagedir, target); /* the wrapper cd's to the entity dir first */
        else if (target[0] == '/') snprintf(real, P, "%s", target);
        else continue;                                    /* sh, sleep, other variables: not a file we can judge */
        if (!exists(real)) { char dd[P]; snprintf(dd, P, "%s calls %.200s", cmd, target); flag(ev, page, "dead-target", dd); }
        if (target[0] == '/' && strncmp(target, house, strlen(house)) != 0) { char fa2[300]; snprintf(fa2, sizeof fa2, "%s targets %.150s", cmd, target); flag(ev, page, "abs-path", fa2); }
    }
    fclose(f);
    char fa[256];
    if (has_foreign_abs(pal, fa, sizeof fa)) flag(ev, page, "abs-path", fa);
}

static void scan_page(const char *ev, const char *pagedir, const char *page, char *trigs, size_t tn) {
    char ir[P], cond[P], pal[P];
    snprintf(ir, P, "%s/event.ir.pdl", pagedir); snprintf(cond, P, "%s/condition.pdl", pagedir); snprintf(pal, P, "%s/event.pal", pagedir);
    /* trigger */
    char rows[8][512]; int n = lines_with(cond, "COND", rows, 8); char trig[128] = "";
    for (int i = 0; i < n; i++) {
        char *a = strchr(rows[i], '|'); if (!a) continue; char *b = strchr(a + 1, '|'); if (!b) continue;
        char key[64]; snprintf(key, sizeof key, "%.*s", (int)(b - a - 1), a + 1);
        if (strstr(key, "trigger")) { snprintf(trig, sizeof trig, "%s", trim(b + 1)); }
    }
    if (n < 0 || !trig[0]) flag(ev, page, "no-condition", "no condition.pdl trigger: this page can never run");
    else { size_t l = strlen(trigs); snprintf(trigs + l, tn - l, "%s%s:%s", l ? "," : "", page, trig); }
    if (!exists(pal)) flag(ev, page, "no-pal", "event.ir.pdl has no compiled event.pal");
    /* IR rows */
    FILE *f = fopen(ir, "r"); if (!f) return;
    char l[2048]; char seen[512][16]; int nseen = 0, nodes = 0;
    while (fgets(l, sizeof l, f)) {
        if (strncmp(l, "NODE", 4) != 0) continue;
        nodes++;
        char *idp = strstr(l, "id="), *tp = strstr(l, "type=");
        if (!idp || !tp) continue;
        char id[16], type[64]; int k = 0;
        for (char *c = idp + 3; *c && isdigit((unsigned char)*c) && k < 15; c++) { id[k++] = *c; }
        id[k] = 0;
        k = 0;
        for (char *c = tp + 5; *c && !isspace((unsigned char)*c) && *c != '|' && k < 63; c++) { type[k++] = *c; }
        type[k] = 0;
        for (int i = 0; i < nseen; i++) if (!strcmp(seen[i], id)) { char d[64]; snprintf(d, sizeof d, "id=%s appears twice", id); flag(ev, page, "dup-id", d); break; }
        if (nseen < 512) snprintf(seen[nseen++], 16, "%s", id);
        Cmd *c = find_cmd(type);
        if (!c) { char d[96]; snprintf(d, sizeof d, "id=%s type=%s not in the registry", id, type); flag(ev, page, "unknown-type", d); }
        else if (c->params) {
            char *b1 = strchr(l, '|'); char *b2 = b1 ? strchr(b1 + 1, '|') : NULL;
            if (b2 && !trim(b2 + 1)[0]) { char d[96]; snprintf(d, sizeof d, "id=%s type=%s has empty params", id, type); flag(ev, page, "empty-params", d); }
        }
    }
    fclose(f);
    if (nodes > 128) { char d[64]; snprintf(d, sizeof d, "%d NODE rows; compile stops at 128", nodes); flag(ev, page, "over-128", d); }
    check_wrappers(ev, page, pagedir);
}

static int line_count_pending(const char *name_pkg) {   /* queue rows after the cursor that mention this pkg */
    char q[P], c[P]; snprintf(q, P, "%s/queue.txt", state); snprintf(c, P, "%s/queue.cursor", state);
    int cur = 0; FILE *cf = fopen(c, "r"); if (cf) { if (fscanf(cf, "%d", &cur) != 1) cur = 0; fclose(cf); }
    FILE *f = fopen(q, "r"); if (!f) return 0;
    char l[P]; int i = 0, hit = 0;
    while (fgets(l, sizeof l, f)) { if (strncmp(l, "QUEUED", 6)) continue; if (i++ >= cur && strstr(l, name_pkg)) hit++; }
    fclose(f); return hit;
}
static const char *run_state(const char *name, const char *pkg) {
    if (line_count_pending(pkg) > 0) return "queued";
    char t[P]; snprintf(t, P, "%s/trace.txt", state);
    FILE *f = fopen(t, "r"); if (!f) return "waiting";
    char l[P]; const char *st = "waiting"; char want[256]; snprintf(want, sizeof want, "| %s |", name);
    while (fgets(l, sizeof l, f)) {
        if (!strstr(l, want)) continue;
        if (!strncmp(l, "START", 5)) st = "running"; else if (!strncmp(l, "END", 3)) st = "ran";
    }
    fclose(f); return st;
}

static void scan_event(const char *name, const char *pkg) {
    char pages[P]; snprintf(pages, P, "%s/event_pkg/pages", pkg);
    char trigs[512] = ""; int npages = 0;
    int before = nflags_total;
    printf("EV_BEGIN | %s\n", name);
    DIR *d = opendir(pages);
    if (!d) { flag(name, "-", "no-pages", "no event_pkg/pages folder"); }
    else {
        struct dirent *e; char names[64][64]; int nn = 0;
        while ((e = readdir(d))) if (strncmp(e->d_name, "page_", 5) == 0 && nn < 64) snprintf(names[nn++], 64, "%s", e->d_name);
        closedir(d);
        for (int i = 0; i < nn; i++)               /* simple numeric order */
            for (int j = i + 1; j < nn; j++) if (atoi(names[j] + 5) < atoi(names[i] + 5)) { char t[64]; strcpy(t, names[i]); strcpy(names[i], names[j]); strcpy(names[j], t); }
        for (int i = 0; i < nn; i++) {
            char pd[P]; snprintf(pd, P, "%s/%s", pages, names[i]);
            if (is_dir(pd)) { scan_page(name, pd, names[i], trigs, sizeof trigs); npages++; }
        }
    }
    printf("EV | %s | %s | %s | %d flags\n", name, trigs[0] ? trigs : "-", run_state(name, pkg), nflags_total - before);
    (void)npages;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: bp_scan <house> --dir <pkg_dir> [...] | --sources <file>\n"); return 2; }
    snprintf(house, sizeof house, "%s", argv[1]);
    const char *e = getenv("BLUEPRINTZ_STATE_DIR");
    if (e && *e) snprintf(state, P, "%s", e); else snprintf(state, P, "%s/&.hq-apps/blueprintz/state", house);
    load_registry();
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--dir") && i + 1 < argc) {
            char *p = argv[++i]; char *base = strrchr(p, '/'); scan_event(base ? base + 1 : p, p);
        } else if (!strcmp(argv[i], "--sources") && i + 1 < argc) {
            FILE *f = fopen(argv[++i], "r"); if (!f) { perror("sources"); return 2; }
            char l[P];
            while (fgets(l, sizeof l, f)) {
                if (strncmp(l, "EVENT |", 7) != 0) continue;
                l[strcspn(l, "\r\n")] = 0;
                char *a = strchr(l + 7, '|'); if (!a) continue; *a = 0;
                char *name = trim(l + 7), *path = trim(a + 1), full[P];
                if (path[0] == '/') snprintf(full, P, "%s", path); else snprintf(full, P, "%s/%s", house, path);
                scan_event(name, full);
            }
            fclose(f);
        }
    }
    printf("SUMMARY | flags=%d\n", nflags_total);
    return nflags_total ? 1 : 0;
}
