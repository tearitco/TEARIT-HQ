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

/* ---- registry: type -> label + up to 4 param names + their prompts ---- */
typedef struct { char type[48]; char label[64]; char pnames[4][32]; char prompts[4][48]; int npn; } TypeDef;
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
        } else if (cur && strncmp(l, "  FIELD1 ", 9) == 0) {
            if (strcmp(l + 9, "-") != 0) snprintf(cur->prompts[0], 48, "%s", l + 9);
        } else if (cur && strncmp(l, "  FIELD2 ", 9) == 0) {
            if (strcmp(l + 9, "-") != 0) snprintf(cur->prompts[1], 48, "%s", l + 9);
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

/* look up value for key in newline-separated "k=v\n" text (non-mutating) */
static void kv_val(const char *buf, const char *key, char *out, size_t cap) {
    out[0] = 0;
    size_t kl = strlen(key);
    const char *p = buf;
    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
        if (linelen > kl && strncmp(p, key, kl) == 0 && p[kl] == '=') {
            size_t vl = linelen - kl - 1;
            if (vl >= cap) vl = cap - 1;
            memcpy(out, p + kl + 1, vl);
            out[vl] = 0;
            return;
        }
        p = nl ? nl + 1 : NULL;
    }
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
    int nrows = 0, nscratch = 0;
    for (char *ln = strtok(buf, "\n"); ln; ln = strtok(NULL, "\n")) {
        if (strncmp(ln, "TRIGGER|", 8) == 0) {
            snprintf(trigger, sizeof(trigger), "%s", ln + 8);
            trim(trigger);
        } else if (strncmp(ln, "SCRATCHBLOCK|", 13) == 0) {
            char s[400]; snprintf(s, sizeof(s), "%s", ln + 13);
            for (char *c = s; *c; c++) if (*c == '|') *c = ':';
            put(ui, off, "scratch_%d_text=%s\n", nscratch++, s);
        } else if (strncmp(ln, "CMD|", 4) == 0) {
            /* CMD|<id>|<type>|<params...> */
            char *p = ln + 4;
            char *b1 = strchr(p, '|'); if (!b1) continue;
            char *b2 = strchr(b1 + 1, '|'); if (!b2) continue;
            char idbuf[16], type[48];
            size_t il = (size_t)(b1 - p);
            if (il >= sizeof(idbuf)) il = sizeof(idbuf) - 1;
            memcpy(idbuf, p, il); idbuf[il] = 0;
            size_t tl = (size_t)(b2 - (b1 + 1));
            if (tl >= sizeof(type)) tl = sizeof(type) - 1;
            memcpy(type, b1 + 1, tl); type[tl] = 0;
            const char *params = b2 + 1;
            char desc[600];
            describe_cmd(type, params, desc, sizeof(desc));
            /* '|' is the frame-dump field separator - never emit it in a label */
            for (char *c = desc; *c; c++) if (*c == '|') *c = '/';
            put(ui, off, "row_%d_text=%s\n", nrows, desc);
            put(ui, off, "row_%d_id=%s\n", nrows, idbuf);
            put(ui, off, "row_%d_type=%s\n", nrows, type);
            nrows++;
        }
    }
    put(ui, off, "trigger=%s\n", trigger[0] ? trigger : "(unknown)");
    put(ui, off, "rows_count=%d\n", nrows);
    put(ui, off, "list_title=Commands (%d)\n", nrows);
    put(ui, off, "empty_list=%d\n", nrows == 0 ? 1 : 0);

    /* ---- editor.txt: which sub-view of the right panel is up ---- */
    char ed[512];
    snprintf(path, sizeof(path), "%s/editor.txt", mgr);
    slurp(path, ed, sizeof(ed));
    char ed_mode[16] = "", ed_type[48] = "", ed_id[16] = "-1";
    for (char *ln = strtok(ed, "\n"); ln; ln = strtok(NULL, "\n")) {
        if      (strncmp(ln, "mode=", 5) == 0)   snprintf(ed_mode, sizeof(ed_mode), "%s", ln + 5);
        else if (strncmp(ln, "type=", 5) == 0)   snprintf(ed_type, sizeof(ed_type), "%s", ln + 5);
        else if (strncmp(ln, "edit_id=", 8) == 0) snprintf(ed_id, sizeof(ed_id), "%s", ln + 8);
    }
    int fields_open = (strcmp(ed_mode, "fields") == 0) && ed_type[0];

    char pk[16];
    snprintf(path, sizeof(path), "%s/picker.txt", mgr);
    read_line1(path, pk, sizeof(pk));
    int picker_open = (pk[0] == '1') && !fields_open;

    /* view mode: 0 Scripting / 1 Scratch / 2 Blueprints (evhq_action.sh view N) */
    char vw[8];
    snprintf(path, sizeof(path), "%s/view.txt", mgr);
    read_line1(path, vw, sizeof(vw));
    int view = atoi(vw);
    if (view < 0 || view > 2) view = 0;
    int scripting = (view == 0);
    if (!scripting) { picker_open = 0; fields_open = 0; }   /* Scratch/Blueprints own the panel */
    put(ui, off, "view=%d\n", view);
    put(ui, off, "is_scratch=%d\n", view == 1 ? 1 : 0);
    put(ui, off, "is_blueprints=%d\n", view == 2 ? 1 : 0);
    put(ui, off, "scratch_count=%d\n", nscratch);
    put(ui, off, "scratch_empty=%d\n", nscratch == 0 ? 1 : 0);

    put(ui, off, "picker_open=%d\n", picker_open);
    put(ui, off, "fields_open=%d\n", fields_open);
    put(ui, off, "list_open=%d\n", (scripting && !picker_open && !fields_open) ? 1 : 0);

    /* Add Command type picker */
    if (picker_open) {
        for (int i = 0; i < g_ntypes; i++) {
            put(ui, off, "pk_%d_label=%s\n", i, g_types[i].label[0] ? g_types[i].label : g_types[i].type);
            put(ui, off, "pk_%d_type=%s\n", i, g_types[i].type);
        }
        put(ui, off, "n_picker=%d\n", g_ntypes);
    } else {
        put(ui, off, "n_picker=0\n");
    }

    /* Field editor: one <cli_io> per registry PARAM, pre-filled from
     * pending_fields.txt (values typed so far) or, when editing an
     * existing command, from that CMD's current params. */
    if (fields_open) {
        TypeDef *d = find_type(ed_type);
        int nf = d ? d->npn : 0;
        put(ui, off, "editor_type=%s\n", ed_type);
        put(ui, off, "editor_id=%s\n", ed_id);
        put(ui, off, "editor_title=%s\n", (d && d->label[0]) ? d->label : ed_type);

        /* current params for an edit: re-scan page.state.txt for CMD|<ed_id>| */
        char cur_params[512] = "";
        if (strcmp(ed_id, "-1") != 0) {
            char buf2[UIBUF];
            snprintf(path, sizeof(path), "%s/page.state.txt", mgr);
            slurp(path, buf2, sizeof(buf2));
            char want[32]; snprintf(want, sizeof(want), "CMD|%s|", ed_id);
            for (char *ln = strtok(buf2, "\n"); ln; ln = strtok(NULL, "\n")) {
                if (strncmp(ln, want, strlen(want)) != 0) continue;
                char *b1 = strchr(ln + 4, '|');
                char *b2 = b1 ? strchr(b1 + 1, '|') : NULL;
                if (b2) snprintf(cur_params, sizeof(cur_params), "%s", b2 + 1);
                break;
            }
        }
        /* pending_fields.txt overrides current params, key by key */
        char pend[1024];
        snprintf(path, sizeof(path), "%s/pending_fields.txt", mgr);
        slurp(path, pend, sizeof(pend));

        for (int i = 0; i < nf; i++) {
            const char *name = d->pnames[i];
            const char *prompt = d->prompts[i][0] ? d->prompts[i] : name;
            char v[256] = "";
            kv_val(pend, name, v, sizeof(v));                      /* what's typed so far */
            if (!v[0] && cur_params[0]) param_val(cur_params, name, v, sizeof(v));  /* else existing value */
            for (char *c = v; *c; c++) if (*c == '|') *c = '/';
            put(ui, off, "f_%d_name=%s\n", i, name);
            put(ui, off, "f_%d_prompt=%s\n", i, prompt);
            put(ui, off, "f_%d_value=%s\n", i, v);
        }
        put(ui, off, "n_fields=%d\n", nf);
    } else {
        put(ui, off, "n_fields=0\n");
        put(ui, off, "editor_type=\neditor_id=-1\neditor_title=\n");
    }

    put(ui, off, "detail_hint=Add Command / Play - click a command to edit its fields\n");
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
