/* evhq_projector.c - the <module> UI projector for events-hq.xhtpm.
 *
 * Reads what khtpm_events_hq_manager.+x publishes for ONE entity's
 * event_pkg and writes <event_pkg>/.hq_manager/ui.txt (key=value) for
 * the static template. No compile logic here - the IR->pal->cmd_N.sh
 * chain stays entirely in the manager; this only projects state to UI.
 *
 * A C projector (not .pal) because command-row pretty-printing needs
 * the data-driven registry (#.ref/menu/event_commands.registry.pdl):
 * type -> LABEL + ordered PARAMS names, paired with the manager's
 * pipe-separated key=val params. EVENTS-HQ-XHTPM-PORT.md §6 sanctions
 * a small C projector where string-ops.md is not enough.
 *
 * Inputs  ($ARG3/.hq_manager/ , published by the manager + evhq_action.sh)
 *   label.txt          entity label            (button-pal.sh)
 *   pages.state.txt     one page-dir per line   (manager)
 *   selected_page.txt   open page's dir name    (evhq_action.sh)
 *   page.state.txt      TRIGGER| / CMD|id|type|params / SWITCH| / SCRATCHBLOCK|
 *   picker.txt          "1" when the Add Command overlay is open (evhq_action.sh)
 * Registry ($HOUSE/#.ref/menu/event_commands.registry.pdl)
 * Output   ($ARG3/.hq_manager/ui.txt)  - written only when it changes.
 *
 * argv: <house_root> <xhtpm_pkg_dir>   (launch_module passes these)
 * env : KHTPM_ARG3 = the event_pkg dir (renderer's argv[3] hook)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAXCMDS 128
#define MAXTYPES 128
#define LINEBUF 1024
#define UIBUF   65536

static char g_house[PATH_MAX];
static char g_pkg[PATH_MAX];      /* the event_pkg dir (KHTPM_ARG3) */

/* ---- registry: type -> label + up to 4 param names ---- */
typedef struct { char type[48]; char label[64]; char pnames[4][32]; int npn; } TypeDef;
static TypeDef g_types[MAXTYPES];
static int g_ntypes = 0;
static time_t g_reg_mtime = 0;

static void reg_path(char *o, size_t n) {
    snprintf(o, n, "%s/#.ref/menu/event_commands.registry.pdl", g_house);
}

static void load_registry(void) {
    char p[PATH_MAX]; reg_path(p, sizeof(p));
    struct stat st;
    if (stat(p, &st) != 0) return;
    if (st.st_mtime == g_reg_mtime && g_ntypes > 0) return;
    g_reg_mtime = st.st_mtime;
    g_ntypes = 0;
    FILE *f = fopen(p, "r");
    if (!f) return;
    char l[LINEBUF];
    TypeDef *cur = NULL;
    while (fgets(l, sizeof(l), f)) {
        l[strcspn(l, "\r\n")] = 0;
        if (strncmp(l, "COMMAND ", 8) == 0) {
            if (g_ntypes < MAXTYPES) {
                cur = &g_types[g_ntypes++];
                memset(cur, 0, sizeof(*cur));
                snprintf(cur->type, sizeof(cur->type), "%s", l + 8);
            } else cur = NULL;
        } else if (cur && strncmp(l, "  LABEL ", 8) == 0) {
            snprintf(cur->label, sizeof(cur->label), "%s", l + 8);
        } else if (cur && strncmp(l, "  PARAMS ", 9) == 0) {
            char *s = l + 9, *tok = strtok(s, ",");
            while (tok && cur->npn < 4) {
                while (*tok == ' ') tok++;
                snprintf(cur->pnames[cur->npn++], 32, "%s", tok);
                tok = strtok(NULL, ",");
            }
        } else if (strcmp(l, "END") == 0) {
            cur = NULL;
        }
    }
    fclose(f);
}

static TypeDef *find_type(const char *t) {
    for (int i = 0; i < g_ntypes; i++)
        if (strcmp(g_types[i].type, t) == 0) return &g_types[i];
    return NULL;
}

/* read a whole small file into buf (NUL-terminated); returns length or -1 */
static long slurp(const char *path, char *buf, size_t cap) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = 0; return -1; }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = 0;
    return (long)n;
}

/* one line, newline stripped */
static void read_line1(const char *path, char *out, size_t cap) {
    FILE *f = fopen(path, "r");
    out[0] = 0;
    if (!f) return;
    if (fgets(out, (int)cap, f)) out[strcspn(out, "\r\n")] = 0;
    fclose(f);
}

static void trim(char *s) {
    char *p = s; while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t l = strlen(s);
    while (l && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = 0;
}

/* look up value for key in a pipe-separated "k=v|k=v" params line */
static void param_val(const char *params, const char *key, char *out, size_t cap) {
    out[0] = 0;
    size_t klen = strlen(key);
    const char *p = params;
    while (p && *p) {
        const char *bar = strchr(p, '|');
        size_t seglen = bar ? (size_t)(bar - p) : strlen(p);
        if (seglen > klen + 1 && strncmp(p, key, klen) == 0 && p[klen] == '=') {
            size_t vlen = seglen - klen - 1;
            if (vlen >= cap) vlen = cap - 1;
            memcpy(out, p + klen + 1, vlen);
            out[vlen] = 0;
            return;
        }
        p = bar ? bar + 1 : NULL;
    }
}

/* "Change Gold (amount: 100)" - mirrors evhq_describe_command() */
static void describe_cmd(const char *type, const char *params, char *out, size_t cap) {
    TypeDef *d = find_type(type);
    if (!d || !d->label[0]) { snprintf(out, cap, "%s  %s", type, params); return; }
    if (d->npn == 0) { snprintf(out, cap, "%s", d->label); return; }
    char body[512] = "";
    for (int i = 0; i < d->npn; i++) {
        char v[256]; param_val(params, d->pnames[i], v, sizeof(v));
        char seg[300];
        snprintf(seg, sizeof(seg), "%s%s: %s", i ? ", " : "", d->pnames[i], v[0] ? v : "(empty)");
        strncat(body, seg, sizeof(body) - strlen(body) - 1);
    }
    snprintf(out, cap, "%s (%s)", d->label, body);
}

/* append "key=value\n" to ui buffer */
static void put(char *ui, size_t *off, const char *fmt, ...) {
    if (*off >= UIBUF) return;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(ui + *off, UIBUF - *off, fmt, ap);
    va_end(ap);
    if (n > 0) *off += (size_t)n;
}

static void project(char *ui, size_t *off) {
    char mgr[PATH_MAX];
    snprintf(mgr, sizeof(mgr), "%s/.hq_manager", g_pkg);

    char path[PATH_MAX], line[LINEBUF], buf[UIBUF];

    /* entity label */
    snprintf(path, sizeof(path), "%s/label.txt", mgr);
    read_line1(path, line, sizeof(line));
    if (!line[0]) snprintf(line, sizeof(line), "events");
    put(ui, off, "entity_label=%s\n", line);

    /* selected page */
    char sel[128];
    snprintf(path, sizeof(path), "%s/selected_page.txt", mgr);
    read_line1(path, sel, sizeof(sel));
    trim(sel);

    /* page tabs */
    snprintf(path, sizeof(path), "%s/pages.state.txt", mgr);
    slurp(path, buf, sizeof(buf));
    int npages = 0;
    for (char *ln = strtok(buf, "\n"); ln; ln = strtok(NULL, "\n")) {
        trim(ln);
        if (!ln[0]) continue;
        put(ui, off, "pg_%d_name=%s\n", npages, ln);
        int active = strcmp(ln, sel) == 0 || (!sel[0] && npages == 0);
        put(ui, off, "pg_%d_active_class=%s\n", npages, active ? "active" : "");
        npages++;
    }
    put(ui, off, "n_pages=%d\n", npages);

    /* trigger + command list from page.state.txt */
    snprintf(path, sizeof(path), "%s/page.state.txt", mgr);
    slurp(path, buf, sizeof(buf));
    char trigger[256] = "";
    int nrows = 0;
    for (char *ln = strtok(buf, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "TRIGGER|", 8) == 0) {
            snprintf(trigger, sizeof(trigger), "%s", ln + 8);
            trim(trigger);
        } else if (strncmp(ln, "CMD|", 4) == 0) {
            /* CMD|<id>|<type>|<params...> */
            char *p = ln + 4;
            char *b1 = strchr(p, '|'); if (!b1) continue;
            char *b2 = strchr(b1 + 1, '|'); if (!b2) continue;
            char type[48];
            size_t tl = (size_t)(b2 - (b1 + 1));
            if (tl >= sizeof(type)) tl = sizeof(type) - 1;
            memcpy(type, b1 + 1, tl); type[tl] = 0;
            const char *params = b2 + 1;
            char desc[600];
            describe_cmd(type, params, desc, sizeof(desc));
            /* '|' is the frame-dump field separator - never emit it in a label */
            for (char *c = desc; *c; c++) if (*c == '|') *c = '/';
            put(ui, off, "row_%d_text=%s\n", nrows, desc);
            nrows++;
        }
    }
    put(ui, off, "trigger=%s\n", trigger[0] ? trigger : "(unknown)");
    put(ui, off, "rows_count=%d\n", nrows);
    put(ui, off, "list_title=Commands (%d)\n", nrows);
    put(ui, off, "empty_list=%d\n", nrows == 0 ? 1 : 0);

    /* Add Command picker overlay */
    char pk[16];
    snprintf(path, sizeof(path), "%s/picker.txt", mgr);
    read_line1(path, pk, sizeof(pk));
    int picker_open = (pk[0] == '1');
    put(ui, off, "picker_open=%d\n", picker_open);
    put(ui, off, "list_open=%d\n", picker_open ? 0 : 1);
    if (picker_open) {
        for (int i = 0; i < g_ntypes; i++) {
            put(ui, off, "pk_%d_label=%s\n", i, g_types[i].label[0] ? g_types[i].label : g_types[i].type);
            put(ui, off, "pk_%d_type=%s\n", i, g_types[i].type);
        }
        put(ui, off, "n_picker=%d\n", g_ntypes);
    } else {
        put(ui, off, "n_picker=0\n");
    }

    put(ui, off, "detail_hint=Add Command opens the picker; Play runs this page's event\n");
}

int main(int argc, char **argv) {
    snprintf(g_house, sizeof(g_house), "%s", argc > 1 ? argv[1] : (getenv("KHTPM_HOUSE") ? getenv("KHTPM_HOUSE") : "."));
    const char *a3 = getenv("KHTPM_ARG3");
    if (!a3 || !a3[0]) { fprintf(stderr, "evhq_projector: KHTPM_ARG3 (event_pkg dir) unset\n"); return 1; }
    snprintf(g_pkg, sizeof(g_pkg), "%s", a3);

    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/.hq_manager/ui.txt", g_pkg);

    static char ui[UIBUF], last[UIBUF];
    last[0] = 0;

    for (;;) {
        load_registry();
        size_t off = 0;
        ui[0] = 0;
        project(ui, &off);
        if (strcmp(ui, last) != 0) {            /* content-gated write */
            char tmp[PATH_MAX];
            snprintf(tmp, sizeof(tmp), "%s.tmp", out);
            FILE *f = fopen(tmp, "w");
            if (f) { fputs(ui, f); fclose(f); rename(tmp, out); }
            snprintf(last, sizeof(last), "%s", ui);
        }
        usleep(400000);
    }
    return 0;
}
