/* layout_check.c - validator + catalog for .xhtpm / .chtpm layouts (layout studio, layer 1+2:
 * LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md).
 *
 *   layout_check <file> [--ui <ui.txt>]...   check one layout; prints "ERROR/WARN line N: what | fix: how"; exit 1 if any ERROR
 *   layout_check --catalog                   print the allowed tags/attributes as markdown (generated from the same tables the checker uses)
 *   layout_check --drift <khtpm_core_render.c>  compare the renderer's attribute/tag names with the tables; exit 1 on drift
 *
 * The tables below are the ONE list. The corpus they were seeded from: every .xhtpm/.chtpm in the house (233 files, 2026-10-09) plus the renderer's
 * attribute handler. Run --drift after touching the renderer so the catalog cannot rot.
 * What it checks: unknown tag, unknown attribute, unbalanced tags, duplicate id, interactive element without a label, item/tab without an action,
 * cli_io without id/target_id, duplicate cli_io target_id (they are state keys), repeat without count+bind, and (with --ui) ${key} the manager
 * never publishes. Warnings never fail the run; errors do. Pure C, no X11. */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *name; int interactive; int void_tag; const char *what; const char *example; } TagDef;
static const TagDef TAGS[] = {
    {"window",     0, 0, "top-level window; label= is the title, vars= lists ui files whose keys ${...} read", "<window label=\"my window\" class=\"database-window managed\" vars=\"state/ui.txt\">"},
    {"page",       0, 0, "a page of a window; name= picks it", "<page name=\"main\">"},
    {"panel",      0, 0, "container; pairs with a sidebar", "<panel id=\"p1\">"},
    {"sidebar",    0, 0, "left rail / list container", "<sidebar id=\"rail\">"},
    {"scrolllist", 0, 0, "vertically scrolling list of items", "<scrolllist id=\"list\">"},
    {"tabbar",     0, 0, "horizontal row of tabs", "<tabbar id=\"toolbar\">"},
    {"tab",        1, 0, "clickable toolbar entry; onclick= runs a command", "<tab id=\"t1\" label=\"File\" onclick=\"sh '${HOUSE}/x.sh'\"/>"},
    {"row",        0, 0, "horizontal group; class canvas-overlay-* places it over the canvas", "<row id=\"r1\" class=\"canvas-overlay-right\">"},
    {"item",       1, 0, "clickable row; action= runs a command; gets a nav number", "<item id=\"i1\" label=\"Open\" action=\"sh '${HOUSE}/x.sh' open\"/>"},
    {"button",     1, 0, "clickable button (same as item for actions)", "<button id=\"b1\" label=\"OK\" action=\"sh '${HOUSE}/x.sh'\"/>"},
    {"text",       0, 0, "static text; label= is the text", "<text label=\"Hello\"/>"},
    {"text_area",  1, 0, "multi-line editable field", "<text_area id=\"notes\" target_id=\"notes\" rows=\"6\"/>"},
    {"cli_io",     1, 0, "one-line typed field; target_id= is the state key; action= runs on Enter", "<cli_io id=\"s1\" target_id=\"search\" label=\"search: \" action=\"sh x.sh\"/>"},
    {"canvas",     0, 0, "the 2D/3D blit region", "<canvas id=\"view\"/>"},
    {"grid",       0, 0, "tile / swatch grid", "<grid id=\"g1\">"},
    {"bar",        0, 0, "progress/status bar", "<bar id=\"hp\" value=\"${hp}\" max=\"100\"/>"},
    {"title",      0, 0, "window title strip", "<title label=\"My window\"/>"},
    {"footer",     0, 0, "bottom strip", "<footer id=\"status\">"},
    {"module",     0, 0, "starts a manager process whose ui.txt feeds ${...} (self-closed with src=), or a plain container in old .chtpm", "<module src=\"@.apps/x/ops/+x/x_manager.+x\" args=\"x\"/>"},
    {"repeat",     0, 0, "repeats its children count= times, binding ${key} from bind=", "<repeat count=\"${n_rows}\" bind=\"r_\">"},
    {"overlay",    0, 1, "splices another layout fragment in", "<overlay src=\"@.apps/x/menu.xhtpm\"/>"},
    {"br",         0, 1, "line break", "<br/>"},
    {"interact",   0, 1, "interaction hook (relay)", "<interact relay=\"...\"/>"},
};
typedef struct { const char *name; const char *what; } AttrDef;
static const AttrDef ATTRS[] = {
    {"id", "unique name in the window"}, {"name", "page name / alias of id"}, {"class", "CSS classes (space separated)"},
    {"label", "visible text; REQUIRED on item/tab/button"}, {"action", "shell command run on activate/Enter"},
    {"onclick", "same as action (tabs)"}, {"onClick", "same as action (tabs)"}, {"show", "${key} - hidden when empty/0"},
    {"src", "file for module/overlay"}, {"args", "arguments for a module"}, {"target_id", "cli_io state key (saved in cli_io_state.txt)"},
    {"count", "repeat count, usually ${n_x}"}, {"bind", "repeat key prefix"}, {"href", "link target"},
    {"time_reactive", "re-render on the clock"}, {"vars", "ui files whose keys ${...} read"}, {"sprite", "sprite image for an item"},
    {"rows", "text_area rows"}, {"backspace_action", "command run on Backspace in an empty cli_io"}, {"content", "seeds a cli_io input"},
    {"confirm", "ask before running action"}, {"drop_action", "XDND drop command"}, {"drop_highlight", "drop colour"},
    {"relay", "nav/relay key list"}, {"bg", "background colour"}, {"visibility", "hidden|visible"}, {"inner", "inner layout hint"},
    {"input_mode", "cli_io mode"}, {"value", "bar value / initial value"}, {"max", "bar max"}, {"width", "rare, prefer CSS"}, {"height", "rare, prefer CSS"},
};
#define NTAGS ((int)(sizeof(TAGS) / sizeof(TAGS[0])))
#define NATTRS ((int)(sizeof(ATTRS) / sizeof(ATTRS[0])))

static const TagDef *tag_def(const char *t) { for (int i = 0; i < NTAGS; i++) if (!strcmp(TAGS[i].name, t)) return &TAGS[i]; return NULL; }
static int attr_known(const char *a) { for (int i = 0; i < NATTRS; i++) if (!strcmp(ATTRS[i].name, a)) return 1; return 0; }

#define MAXA 24
typedef struct { char name[48]; char val[1024]; } Attr;
static int g_errors = 0, g_warns = 0;
static void report(int err, int line, const char *msg, const char *fix) {
    printf("%s line %d: %s | fix: %s\n", err ? "ERROR" : "WARN", line, msg, fix);
    if (err) g_errors++; else g_warns++;
}
static const char *aget(Attr *a, int n, const char *k) { for (int i = 0; i < n; i++) if (!strcmp(a[i].name, k)) return a[i].val; return NULL; }

static char (*g_uikeys)[64]; static int g_nui = 0, g_uicap = 0, g_have_ui = 0;
static void ui_load(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "layout_check: cannot read ui file %s\n", path); return; }
    char line[2048]; g_have_ui = 1;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='); if (!eq || eq == line) continue;
        size_t n = (size_t)(eq - line); if (n >= 64) continue;
        if (g_nui == g_uicap) { g_uicap = g_uicap ? g_uicap * 2 : 256; g_uikeys = realloc(g_uikeys, (size_t)g_uicap * 64); }
        memcpy(g_uikeys[g_nui], line, n); g_uikeys[g_nui][n] = 0; g_nui++;
    }
    fclose(f);
}
static int ui_has(const char *k) { for (int i = 0; i < g_nui; i++) if (!strcmp(g_uikeys[i], k)) return 1; return 0; }
/* a repeat bind="d_" publishes d_0_label ... so ${d_0_x} style keys with a numeric piece are matched by prefix */
static int ui_has_prefixed(const char *k) { for (int i = 0; i < g_nui; i++) if (!strncmp(g_uikeys[i], k, strlen(k)) && strlen(k) > 1) return 1; return 0; }

static void check_vars(int line, const char *val, const char *attr, char binds[][48], int nbinds) {
    if (!g_have_ui) return;
    for (const char *p = val; (p = strstr(p, "${")); ) {
        const char *e = strchr(p, '}'); if (!e) break;
        char key[64]; size_t n = (size_t)(e - p - 2); if (n >= sizeof key) { p = e; continue; } memcpy(key, p + 2, n); key[n] = 0; p = e;
        char *d = strstr(key, ":-"); if (d) *d = 0;
        if (!strcmp(key, "HOUSE") || !strcmp(key, "ROOT") || !key[0]) continue;
        if (ui_has(key)) continue;
        int bound = 0;   /* inside a repeat: ${d_label} resolves to d_<n>_label */
        for (int i = 0; i < nbinds; i++) if (!strncmp(key, binds[i], strlen(binds[i])) || ui_has_prefixed(binds[i])) bound = 1;
        if (bound) continue;
        char msg[200], fix[200];
        snprintf(msg, sizeof msg, "${%s} in %s is not a key the manager publishes", key, attr);
        snprintf(fix, sizeof fix, "publish %s= in the ui.txt (or fix the spelling); keys seen: %d", key, g_nui);
        report(1, line, msg, fix);
    }
}

typedef struct { char tag[32]; int line; int bind_pushed; } Frame;
#define MAXIDS 2048
static char (*g_ids)[96]; static int g_nids = 0;
static char (*g_tids)[96]; static int g_ntids = 0;

static int check_file(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) { fprintf(stderr, "layout_check: cannot read %s\n", path); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)sz + 1); if (fread(s, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return 2; } s[sz] = 0; fclose(f);
    g_ids = malloc(MAXIDS * 96); g_tids = malloc(MAXIDS * 96);
    Frame st[256]; int sp = 0; char binds[16][48]; int nbinds = 0;
    int line = 1;
    for (char *p = s; *p; ) {
        if (*p == '\n') { line++; p++; continue; }
        if (!strncmp(p, "<!--", 4)) { char *e = strstr(p, "-->"); char *q = p; if (!e) { report(1, line, "unterminated comment", "close it with -->"); break; } for (; q < e; q++) if (*q == '\n') line++; p = e + 3; continue; }
        if (*p != '<') { p++; continue; }
        int tl = line; p++;
        if (*p == '/') {   /* closing tag */
            p++; char name[32]; int n = 0; while (*p && *p != '>' && !isspace((unsigned char)*p) && n < 31) name[n++] = *p++; name[n] = 0; while (*p && *p != '>') p++; if (*p) p++;
            if (sp == 0 || strcmp(st[sp - 1].tag, name)) {
                char msg[200], fix[200]; snprintf(msg, sizeof msg, "closing </%s> does not match the open <%s>", name, sp ? st[sp - 1].tag : "(none)");
                snprintf(fix, sizeof fix, "close tags in reverse order; <%s> was opened at line %d", sp ? st[sp - 1].tag : "?", sp ? st[sp - 1].line : 0); report(1, tl, msg, fix);
                if (sp) sp--;
            } else { if (st[sp - 1].bind_pushed && nbinds) nbinds--; sp--; }
            continue;
        }
        if (!isalpha((unsigned char)*p) && *p != '_') continue;   /* a stray '<' in text */
        char tag[32]; int n = 0; while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/' && n < 31) tag[n++] = *p++; tag[n] = 0;
        Attr a[MAXA]; int na = 0; int selfclose = 0;
        for (;;) {
            while (*p && isspace((unsigned char)*p)) { if (*p == '\n') line++; p++; }
            if (!*p) break;
            if (*p == '/') { selfclose = 1; p++; continue; }
            if (*p == '>') { p++; break; }
            char an[48]; int m = 0; while (*p && *p != '=' && *p != '>' && *p != '/' && !isspace((unsigned char)*p) && m < 47) an[m++] = *p++; an[m] = 0;
            while (*p && isspace((unsigned char)*p)) p++;
            char val[1024]; val[0] = 0;
            if (*p == '=') {
                p++; while (*p && isspace((unsigned char)*p)) p++;
                if (*p == '"' || *p == '\'') { char q = *p++; int k = 0; while (*p && *p != q) { if (*p == '\n') line++; if (k < 1023) val[k++] = *p; p++; } val[k] = 0; if (*p) p++; }
                else { int k = 0; while (*p && !isspace((unsigned char)*p) && *p != '>' && k < 1023) val[k++] = *p++; val[k] = 0;
                       char msg[160]; snprintf(msg, sizeof msg, "attribute %s=%s has no quotes", an, val); report(1, tl, msg, "wrap the value in double quotes"); }
            }
            if (na < MAXA) { snprintf(a[na].name, sizeof a[na].name, "%s", an); snprintf(a[na].val, sizeof a[na].val, "%s", val); na++; }
        }
        const TagDef *td = tag_def(tag);
        if (!td) {
            char msg[200], fix[240]; snprintf(msg, sizeof msg, "unknown tag <%s>", tag);
            snprintf(fix, sizeof fix, "use one of: window page panel sidebar scrolllist tabbar tab row item button text text_area cli_io canvas grid bar title footer module repeat overlay (run --catalog)");
            report(1, tl, msg, fix);
        }
        for (int i = 0; i < na; i++) {
            if (!attr_known(a[i].name)) { char msg[200], fix[200]; snprintf(msg, sizeof msg, "unknown attribute %s= on <%s>", a[i].name, tag); snprintf(fix, sizeof fix, "remove it or check the spelling (run --catalog for the allowed list)"); report(1, tl, msg, fix); }
            check_vars(tl, a[i].val, a[i].name, binds, nbinds);
        }
        const char *id = aget(a, na, "id"), *label = aget(a, na, "label"), *act = aget(a, na, "action");
        if (!act) act = aget(a, na, "onclick");
        if (!act) act = aget(a, na, "onClick");
        if (id && *id) {
            if (strchr(id, ' ')) { char msg[160]; snprintf(msg, sizeof msg, "id=\"%s\" has a space", id); report(1, tl, msg, "ids are one word: use - or _"); }
            int dup = 0; for (int i = 0; i < g_nids; i++) if (!strcmp(g_ids[i], id)) dup = 1;
            if (dup) { char msg[160], fix[160]; snprintf(msg, sizeof msg, "duplicate id \"%s\"", id); snprintf(fix, sizeof fix, "ids must be unique in the window; rename one (e.g. %s-2)", id); report(1, tl, msg, fix); }
            else if (g_nids < MAXIDS) snprintf(g_ids[g_nids++], 96, "%s", id);
        }
        if (td && td->interactive) {
            int has_sprite = aget(a, na, "sprite") != NULL;
            if (strcmp(tag, "cli_io") && strcmp(tag, "text_area") && (!label || !*label) && !has_sprite) {
                char msg[160]; snprintf(msg, sizeof msg, "<%s> has no label", tag); report(1, tl, msg, "add label=\"...\" - every interactive element needs visible text (it also gets a nav number)");
            }
            if ((!strcmp(tag, "item") || !strcmp(tag, "tab") || !strcmp(tag, "button")) && (!act || !*act) && !aget(a, na, "href") && !aget(a, na, "relay")) {
                char msg[160]; snprintf(msg, sizeof msg, "<%s label=\"%s\"> does nothing when clicked", tag, label ? label : ""); report(0, tl, msg, "add action=\"sh '${HOUSE}/path/script.sh' verb\" (or relay=/href=)");
            }
            if (!strcmp(tag, "cli_io") || !strcmp(tag, "text_area")) {
                const char *tid = aget(a, na, "target_id");
                if (!id || !*id) report(1, tl, "cli_io/text_area without id", "add id=\"unique-name\"");
                if (!tid || !*tid) report(0, tl, "cli_io/text_area without target_id (its saved-state key; the id is used instead)", "add target_id=\"my-field\" so the saved text is stored as my-field=<text> in cli_io_state.txt");
                else {
                    int dup = 0; for (int i = 0; i < g_ntids; i++) if (!strcmp(g_tids[i], tid)) dup = 1;
                    if (dup) { char msg[160]; snprintf(msg, sizeof msg, "target_id \"%s\" used twice (it is a state key, the fields would share text)", tid); report(1, tl, msg, "give every field its own target_id"); }
                    else if (g_ntids < MAXIDS) snprintf(g_tids[g_ntids++], 96, "%s", tid);
                }
                if (label && strlen(label) > 30) report(0, tl, "cli_io label is long; label is only a short prefix", "keep it like \"search: \"; seed the text with content=");
            }
        }
        if (!strcmp(tag, "repeat")) {
            if (!aget(a, na, "count") || !aget(a, na, "bind")) report(1, tl, "<repeat> needs count= and bind=", "e.g. <repeat count=\"${n_rows}\" bind=\"r_\"> and publish n_rows= plus r_0_label=...");
        }
        if (((!strcmp(tag, "module") && selfclose) || !strcmp(tag, "overlay")) && !aget(a, na, "src")) report(1, tl, "self-closed <module>/<overlay> without src=", "add src=\"path\"");
        int pushed = 0;
        if (!strcmp(tag, "repeat") && aget(a, na, "bind") && nbinds < 16) { snprintf(binds[nbinds++], 48, "%s", aget(a, na, "bind")); pushed = 1; }
        if (!selfclose && !(td && td->void_tag)) {
            if (sp < 256) { snprintf(st[sp].tag, sizeof st[sp].tag, "%s", tag); st[sp].line = tl; st[sp].bind_pushed = pushed; sp++; }
        } else if (pushed) nbinds--;
    }
    while (sp > 0) { char msg[160]; snprintf(msg, sizeof msg, "<%s> opened here is never closed", st[sp - 1].tag); report(1, st[sp - 1].line, msg, "add the closing tag or make it self-closing with />"); sp--; }
    free(s);
    return 0;
}

static void catalog(void) {
    printf("# Layout catalog (generated by layout_check --catalog; do not edit)\n\n## Tags\n\n| tag | interactive | what | example |\n|---|---|---|---|\n");
    for (int i = 0; i < NTAGS; i++) printf("| `<%s>` | %s | %s | `%s` |\n", TAGS[i].name, TAGS[i].interactive ? "yes (nav number)" : "no", TAGS[i].what, TAGS[i].example);
    printf("\n## Attributes\n\n| attribute | meaning |\n|---|---|\n");
    for (int i = 0; i < NATTRS; i++) printf("| `%s` | %s |\n", ATTRS[i].name, ATTRS[i].what);
    printf("\n## Rules the checker enforces\n\n- ids unique in the window, one word\n- item/tab/button/cli_io need a label (cli_io: a short prefix); item/tab/button need action=/onclick=/href=/relay=\n"
           "- cli_io/text_area need id and a unique target_id (a state key in cli_io_state.txt); a field must open empty (the manager must not trust stale state)\n"
           "- repeat needs count= and bind=; ${key} must be published by a manager (check with --ui state/ui.txt)\n- every tag closed or self-closed; attribute values quoted\n");
}

static int drift(const char *renderer) {
    FILE *f = fopen(renderer, "r"); if (!f) { fprintf(stderr, "layout_check: cannot read %s\n", renderer); return 2; }
    char line[4096]; int bad = 0, ln = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        for (char *p = line; (p = strstr(p, "strcmp(name, \"")); ) {
            p += 14; char *e = strchr(p, '"'); if (!e) break; char nm[64]; size_t n = (size_t)(e - p); if (n >= sizeof nm) continue; memcpy(nm, p, n); nm[n] = 0;
            if (isupper((unsigned char)nm[0]) && strcmp(nm, "HOUSE") != 0 && 0) continue;
            if (isupper((unsigned char)nm[0])) continue;   /* env-var names (HOUSE, PKG, PID ...) are not attributes */
            if (!attr_known(nm)) { printf("DRIFT line %d: renderer reads attribute \"%s\" that layout_check does not list | fix: add it to ATTRS in layout_check.c\n", ln, nm); bad++; }
        }
        for (char *p = line; (p = strstr(p, "->tag, \"")); ) {
            p += 8; char *e = strchr(p, '"'); if (!e) break; char nm[64]; size_t n = (size_t)(e - p); if (n >= sizeof nm) continue; memcpy(nm, p, n); nm[n] = 0;
            if (!tag_def(nm)) { printf("DRIFT line %d: renderer handles tag \"%s\" that layout_check does not list | fix: add it to TAGS in layout_check.c\n", ln, nm); bad++; }
        }
    }
    fclose(f);
    if (!bad) printf("no drift: every attribute/tag the renderer compares is in the tables (%d tags, %d attributes)\n", NTAGS, NATTRS);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    const char *file = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--catalog")) { catalog(); return 0; }
        if (!strcmp(argv[i], "--drift") && i + 1 < argc) return drift(argv[i + 1]);
        if (!strcmp(argv[i], "--ui") && i + 1 < argc) { ui_load(argv[++i]); continue; }
        file = argv[i];
    }
    if (!file) { fprintf(stderr, "usage: layout_check <file.xhtpm> [--ui ui.txt]... | --catalog | --drift khtpm_core_render.c\n"); return 2; }
    int rc = check_file(file); if (rc) return rc;
    printf("%s: %d error(s), %d warning(s)\n", file, g_errors, g_warns);
    return g_errors ? 1 : 0;
}
