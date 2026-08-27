/* khtpm_events_hq_manager.c — events-hq's MANAGER binary (Stage 2d
 * shell/manager split, same real mechanism proven on db-hq - see
 * local-2do-15.txt's own "Stage 2d shell/manager split CORRECTED TO THE
 * REAL MECHANISM" entry). Launched by khtpm_events_hq_render.c's own
 * main() via a real fork()+execv() reading dashboard.chtpm's <module
 * src="..."/> tag - not an external launcher script.
 *
 * Real per-entity scoping (events-hq is LEGITIMATELY multi-instance -
 * one process pair per entity, see button.sh's own same_entity_pids()
 * guard): this manager takes the SAME 3 args the shell already does
 * (house_root, pkg_dir, entity_label) and keeps ALL its state files
 * INSIDE pkg_dir/.hq_manager/ - since pkg_dir is already per-entity
 * (each entity has its own event_pkg/), this gives natural isolation
 * with zero extra namespacing work, unlike db-hq's #.desktop/ files
 * (db-hq is single-instance, so a flat shared location was fine there;
 * events-hq is not, so it can't reuse that shape as-is).
 *
 * Owns EVERYTHING that used to be khtpm_events_hq_render.c's own
 * business logic (load_pages/load_condition/load_commands/compile_page/
 * append_node_and_compile, moved here near-verbatim):
 *   - scans pkg_dir/pages/ every poll, publishes sorted page list to
 *     pkg_dir/.hq_manager/pages.state.txt.
 *   - reads pkg_dir/.hq_manager/selected_page.txt (the shell writes
 *     this whenever the user switches page tabs) to know which page's
 *     condition.pdl + event.ir.pdl to read; publishes trigger + command
 *     list to pkg_dir/.hq_manager/page.state.txt.
 *   - polls pkg_dir/.hq_manager/action.txt for a pending
 *     "append:<type>|<params_line>" request (the shell writes this when
 *     the user submits the "add command" picker); on seeing one, does
 *     the real event.ir.pdl append + event.pal recompile (the exact
 *     compile_page() logic, ported line-for-line from ez_menu_input.c
 *     originally, now living here instead of the shell), then clears
 *     the request and republishes page.state.txt with the new command.
 *
 * All file I/O uses atomic tmp-write-then-rename for state publishes,
 * same convention khtpm_taskbar_manager.c's own registry writes use. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PATH_BUF 4096
#define MAX_PAGES 16
#define MAX_CMDS 64
#define MAX_REGISTRY_CMDS 32
#define MAX_FIELDS 4

static char g_house_root[PATH_BUF];
static char g_pkg_dir[PATH_BUF];
static char g_entity_label[128];

static char g_pages_state_path[PATH_BUF];
static char g_selected_page_path[PATH_BUF];
static char g_page_state_path[PATH_BUF];
static char g_action_path[PATH_BUF];
static char g_mgr_dir[PATH_BUF];

static char g_pages[MAX_PAGES][64];
static int g_n_pages = 0;
static int g_current_page = 0;

static void page_dir(char *out, size_t outsz, int page_idx) {
    snprintf(out, outsz, "%s/pages/%s", g_pkg_dir, g_pages[page_idx]);
}

static void publish_pages(void) {
    char pages_root[PATH_BUF];
    snprintf(pages_root, sizeof(pages_root), "%s/pages", g_pkg_dir);
    if (access(pages_root, F_OK) != 0) mkdir(pages_root, 0755);
    DIR *d = opendir(pages_root);
    int n = 0;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && n < MAX_PAGES) {
            if (de->d_name[0] == '.') continue;
            if (strncmp(de->d_name, "page_", 5) != 0) continue;
            snprintf(g_pages[n], sizeof(g_pages[0]), "%s", de->d_name);
            n++;
        }
        closedir(d);
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (atoi(g_pages[j] + 5) < atoi(g_pages[i] + 5)) {
                    char t[64]; snprintf(t, sizeof(t), "%s", g_pages[i]);
                    snprintf(g_pages[i], sizeof(g_pages[i]), "%s", g_pages[j]);
                    snprintf(g_pages[j], sizeof(g_pages[j]), "%s", t);
                }
    }
    if (n == 0) {
        /* real, not a stub - matches the shell's own old lazy-scaffold
         * behavior (event-ez creates pages lazily too). */
        char p1[PATH_BUF]; snprintf(p1, sizeof(p1), "%s/page_1", pages_root);
        mkdir(p1, 0755);
        snprintf(g_pages[0], sizeof(g_pages[0]), "page_1");
        n = 1;
    }
    g_n_pages = n;
    if (g_current_page >= g_n_pages) g_current_page = 0;

    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_pages_state_path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (int i = 0; i < g_n_pages; i++) fprintf(f, "%s\n", g_pages[i]);
    fclose(f);
    rename(tmp, g_pages_state_path);
}

static void read_selected_page(void) {
    FILE *f = fopen(g_selected_page_path, "r");
    if (!f) return;
    char line[64] = "";
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        for (int i = 0; i < g_n_pages; i++) {
            if (strcmp(g_pages[i], line) == 0) { g_current_page = i; break; }
        }
    }
    fclose(f);
}

/* REAL, 2026-08-26 (direct instruction: "we never hardcode stuff,
 * always keeping things super modular and abstract" - full rationale
 * in #.ref/menu/EVENT-COMMAND-REGISTRY-ARCHITECTURE.md) - a data-driven
 * command registry, replacing the hardcoded per-type if/else chain
 * compile_page() used to have. Modeled directly on #.haiku+/tpmos-re-
 * dox/fo-menu-sys.md's own real, documented pattern (a METHOD row's
 * VALUE is a dispatch template, substituted and exec'd generically -
 * the manager never hardcodes what "feed" or "play" means). Adding a
 * new SIMPLE command (wraps one real op with substituted string params
 * - true of almost every event command) now means editing ONLY
 * #.ref/menu/event_commands.registry.pdl - zero recompile.
 *
 * Genuinely compiler-shaped commands (Conditional Branch - needs real
 * branch/jump target computation, not string substitution) are the
 * deliberate, documented exception and stay hardcoded below this
 * engine, same as prisc+x's own opcode set is unavoidably C - see the
 * architecture doc for the full reasoning on where that line is. */
#define MAX_PAL_LINES 8
typedef struct {
    char type[48];
    char label[64];
    char field1[64];
    char field2[64];
    char param_names[MAX_FIELDS][32];
    int n_params;
    char tmpl[512];
    char pal_lines[MAX_PAL_LINES][256];
    int n_pal;
} CommandDef;

static CommandDef g_registry[MAX_REGISTRY_CMDS];
static int g_n_registry = 0;
static time_t g_registry_mtime = 0;

static void registry_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.ref/menu/event_commands.registry.pdl", g_house_root);
}

/* Re-reads the registry only when its mtime changes - a live edit to
 * the .pdl (adding a new command type) takes effect on the very next
 * poll tick, no manager restart needed either. */
static void load_command_registry(void) {
    char path[PATH_BUF];
    registry_path(path, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (st.st_mtime == g_registry_mtime && g_n_registry > 0) return;
    g_registry_mtime = st.st_mtime;
    g_n_registry = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[600];
    CommandDef *cur = NULL;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ') p++;
        if (strncmp(p, "COMMAND ", 8) == 0) {
            if (g_n_registry >= MAX_REGISTRY_CMDS) break;
            cur = &g_registry[g_n_registry++];
            memset(cur, 0, sizeof(*cur));
            snprintf(cur->type, sizeof(cur->type), "%s", p + 8);
        } else if (!cur) {
            continue;
        } else if (strncmp(p, "LABEL ", 6) == 0) {
            snprintf(cur->label, sizeof(cur->label), "%s", p + 6);
        } else if (strncmp(p, "FIELD1 ", 7) == 0) {
            snprintf(cur->field1, sizeof(cur->field1), "%s", p + 7);
        } else if (strncmp(p, "FIELD2 ", 7) == 0) {
            snprintf(cur->field2, sizeof(cur->field2), "%s", p + 7);
        } else if (strncmp(p, "PARAMS ", 7) == 0) {
            cur->n_params = 0;
            char *tok = p + 7, *comma;
            while (tok && *tok && cur->n_params < MAX_FIELDS) {
                comma = strchr(tok, ',');
                size_t l = comma ? (size_t)(comma - tok) : strlen(tok);
                if (l >= sizeof(cur->param_names[0])) l = sizeof(cur->param_names[0]) - 1;
                memcpy(cur->param_names[cur->n_params], tok, l);
                cur->param_names[cur->n_params][l] = '\0';
                cur->n_params++;
                tok = comma ? comma + 1 : NULL;
            }
        } else if (strncmp(p, "TEMPLATE ", 9) == 0) {
            snprintf(cur->tmpl, sizeof(cur->tmpl), "%s", p + 9);
        } else if (strncmp(p, "PAL ", 4) == 0) {
            if (cur->n_pal < MAX_PAL_LINES) {
                snprintf(cur->pal_lines[cur->n_pal], sizeof(cur->pal_lines[0]), "%s", p + 4);
                cur->n_pal++;
            }
        } else if (strcmp(p, "END") == 0) {
            cur = NULL;
        }
    }
    fclose(f);
}

static CommandDef *find_command_def(const char *type) {
    load_command_registry();
    for (int i = 0; i < g_n_registry; i++)
        if (strcmp(g_registry[i].type, type) == 0) return &g_registry[i];
    return NULL;
}

/* Splits a NODE line's params blob ("text=Hello there|speaker=Bob" or
 * a bare "amount=5") into key/value pairs. '|' is the real field
 * separator (values may contain spaces/commas freely - only a literal
 * '|' is disallowed, same restriction this house's other text fields
 * already carry, e.g. onClick quoting). */
static void parse_params(const char *params_line, char keys[MAX_FIELDS][32], char vals[MAX_FIELDS][256], int *n_out) {
    int n = 0;
    const char *p = params_line;
    while (*p && n < MAX_FIELDS) {
        const char *bar = strchr(p, '|');
        size_t seglen = bar ? (size_t)(bar - p) : strlen(p);
        char seg[300];
        if (seglen >= sizeof(seg)) seglen = sizeof(seg) - 1;
        memcpy(seg, p, seglen); seg[seglen] = '\0';
        char *eq = strchr(seg, '=');
        if (eq) {
            *eq = '\0';
            snprintf(keys[n], 32, "%s", seg);
            snprintf(vals[n], 256, "%s", eq + 1);
        } else {
            snprintf(keys[n], 32, "%s", seg);
            vals[n][0] = '\0';
        }
        n++;
        if (!bar) break;
        p = bar + 1;
    }
    *n_out = n;
}

static const char *lookup_value(char keys[MAX_FIELDS][32], char vals[MAX_FIELDS][256], int n, const char *key) {
    for (int i = 0; i < n; i++) if (strcmp(keys[i], key) == 0) return vals[i];
    return "";
}

/* Convert old space-separated format to new pipe-separated format.
 * OLD: "choices=Say hello,Wave,Ignore default=0"
 * NEW: "choices=Say hello,Wave,Ignore|default=0"
 * Uses the registry to know which param names to expect, so we can find
 * value boundaries even when values contain spaces. */
static void convert_params_to_new_format(const char *old_params, const char *cmd_type, char *out, size_t outsz) {
    CommandDef *def = find_command_def(cmd_type);
    if (!def) {
        snprintf(out, outsz, "%s", old_params);
        return;
    }
    out[0] = '\0';
    for (int i = 0; i < def->n_params && i < MAX_FIELDS; i++) {
        const char *key = def->param_names[i];
        char search[64]; snprintf(search, sizeof(search), "%s=", key);
        const char *start = strstr(old_params, search);
        if (!start) continue;
        start += strlen(search);
        const char *end = start;
        if (i + 1 < def->n_params) {
            const char *next_key = def->param_names[i + 1];
            char next_search[64]; snprintf(next_search, sizeof(next_search), " %s=", next_key);
            const char *next_pos = strstr(end, next_search);
            if (next_pos) end = next_pos;
            else end = old_params + strlen(old_params);
        } else {
            end = old_params + strlen(old_params);
        }
        size_t val_len = (size_t)(end - start);
        while (val_len > 0 && start[val_len - 1] == ' ') val_len--;
        char seg[300]; snprintf(seg, sizeof(seg), "%s=%.*s", key, (int)val_len, start);
        if (i > 0) strncat(out, "|", outsz - strlen(out) - 1);
        strncat(out, seg, outsz - strlen(out) - 1);
    }
}

/* Expands {key} substitutions and [optional {key} segments] - see
 * event_commands.registry.pdl's own header comment for the exact
 * syntax contract. No nesting; that's a deliberate simplicity limit,
 * not an oversight - every real command so far needs at most one
 * optional trailing field. */
static void expand_template(const char *tmpl, char keys[MAX_FIELDS][32], char vals[MAX_FIELDS][256], int n, char *out, size_t outsz) {
    size_t oi = 0;
    const char *p = tmpl;
    while (*p && oi + 1 < outsz) {
        if (*p == '[') {
            const char *close = strchr(p, ']');
            if (!close) { out[oi++] = *p++; continue; }
            char seg[128]; size_t seglen = (size_t)(close - p - 1);
            if (seglen >= sizeof(seg)) seglen = sizeof(seg) - 1;
            memcpy(seg, p + 1, seglen); seg[seglen] = '\0';
            char *ob = strchr(seg, '{'), *cb = ob ? strchr(ob, '}') : NULL;
            int keep = 1;
            if (ob && cb) {
                char kk[32]; size_t kl = (size_t)(cb - ob - 1);
                if (kl >= sizeof(kk)) kl = sizeof(kk) - 1;
                memcpy(kk, ob + 1, kl); kk[kl] = '\0';
                keep = (lookup_value(keys, vals, n, kk)[0] != '\0');
            }
            if (keep) {
                char expanded[256];
                expand_template(seg, keys, vals, n, expanded, sizeof(expanded));
                size_t el = strlen(expanded);
                if (oi + el + 1 < outsz) { memcpy(out + oi, expanded, el); oi += el; }
            }
            p = close + 1;
        } else if (*p == '{') {
            const char *close = strchr(p, '}');
            if (!close) { out[oi++] = *p++; continue; }
            char kk[32]; size_t kl = (size_t)(close - p - 1);
            if (kl >= sizeof(kk)) kl = sizeof(kk) - 1;
            memcpy(kk, p + 1, kl); kk[kl] = '\0';
            const char *v = lookup_value(keys, vals, n, kk);
            size_t vl = strlen(v);
            if (oi + vl + 1 < outsz) { memcpy(out + oi, v, vl); oi += vl; }
            p = close + 1;
        } else {
            out[oi++] = *p++;
        }
    }
    out[oi] = '\0';
}

/* Resolves the session root (or entity root fallback) for PAL-mode
 * commands that need absolute paths to switches.txt/variables.txt.
 * Walks up from g_pkg_dir looking for a sessions/<id>/ ancestor.
 * Falls back to the entity root (parent of event_pkg) for non-session
 * entities under pals/ — per A6 in the handoff doc. Writes the result
 * to `out`, returns 1 on success, 0 on failure. */
static int resolve_session_root(char *out, size_t outsz) {
    /* First: entity root = parent of g_pkg_dir (strip "event_pkg") */
    char entity_root[PATH_BUF];
    char *last_slash = strrchr(g_pkg_dir, '/');
    if (last_slash && strncmp(last_slash + 1, "event_pkg", 9) == 0) {
        size_t len = (size_t)(last_slash - g_pkg_dir);
        if (len > 0 && len < sizeof(entity_root) - 1) {
            memcpy(entity_root, g_pkg_dir, len);
            entity_root[len] = '\0';
        } else {
            snprintf(entity_root, sizeof(entity_root), "%s", g_pkg_dir);
        }
    } else {
        snprintf(entity_root, sizeof(entity_root), "%s", g_pkg_dir);
    }
    /* Walk up from entity_root looking for sessions/<id>/ */
    char walk[PATH_BUF];
    snprintf(walk, sizeof(walk), "%s", entity_root);
    while (walk[0] && strcmp(walk, "/") != 0) {
        char *bs = strrchr(walk, '/');
        if (!bs || bs == walk) break;
        /* Check if parent of walk is "sessions" */
        char parent[PATH_BUF];
        size_t plen = (size_t)(bs - walk);
        if (plen >= sizeof(parent)) break;
        memcpy(parent, walk, plen);
        parent[plen] = '\0';
        char *pp = strrchr(parent, '/');
        if (pp) {
            if (strcmp(pp + 1, "sessions") == 0) {
                /* walk IS the session dir (parent/sessions/<id>) */
                snprintf(out, outsz, "%s", walk);
                return 1;
            }
        } else if (strcmp(parent, "sessions") == 0) {
            snprintf(out, outsz, "%s", walk);
            return 1;
        }
        *bs = '\0';
    }
    /* Fallback: use entity root (per A6 - non-session entities get
     * per-entity switches/variables, an acceptable degradation) */
    snprintf(out, outsz, "%s", entity_root);
    return 1;
}

/* Real compile pass, ported line-for-line from the shell's own old
 * compile_page() (itself ported from ez_menu_input.c originally) -
 * event.pal is ALWAYS fully regenerated from event.ir.pdl, never
 * hand-patched, matching event-ez's own "visual compiler" semantics.
 *
 * TASK 3 (2026-08-26): Two-pass compilation with IfFrame nesting stack.
 * Pass 1 reads all IR nodes into an array. Pass 2 generates PAL with
 * label resolution for if/else/end blocks. This is the deliberate,
 * documented exception to the registry-driven approach — conditional
 * branching requires real compiler work (block structure, forward
 * labels), not string template expansion. */
#define MAX_IR_NODES 128
#define MAX_IF_NEST 16
typedef struct {
    char type[48];
    int id;
    char keys[MAX_FIELDS][32];
    char vals[MAX_FIELDS][256];
    int n_params;
} IrNode;
typedef struct {
    char end_label[32];
    char else_label[32];
    int has_else;
} IfFrame;
static void compile_page(int page_idx) {
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), page_idx);
    char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);
    char pal_path[PATH_BUF]; snprintf(pal_path, sizeof(pal_path), "%s/event.pal", pd);

    /* Pass 1: Read all IR nodes into array */
    IrNode nodes[MAX_IR_NODES];
    int n_nodes = 0;
    FILE *irf = fopen(ir_path, "r");
    if (irf) {
        char line[512];
        while (fgets(line, sizeof(line), irf) && n_nodes < MAX_IR_NODES) {
            if (strncmp(line, "NODE", 4) != 0) continue;
            IrNode *nd = &nodes[n_nodes];
            nd->n_params = 0;
            char *tp = strstr(line, "type=");
            if (!tp) continue;
            char *t = tp + 5, *sp = strchr(t, ' ');
            char *pipe = strchr(t, '|');
            size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
            if (len >= sizeof(nd->type)) len = sizeof(nd->type) - 1;
            memcpy(nd->type, t, len); nd->type[len] = '\0';
            char *idp = strstr(line, "id=");
            nd->id = idp ? atoi(idp + 3) : 1;
            char *first_bar = strchr(line, '|');
            char *second_bar = first_bar ? strchr(first_bar + 1, '|') : NULL;
            if (second_bar) {
                char *pp = second_bar + 1;
                while (*pp == ' ') pp++;
                pp[strcspn(pp, "\r\n")] = '\0';
                parse_params(pp, nd->keys, nd->vals, &nd->n_params);
            }
            n_nodes++;
        }
        fclose(irf);
    }

    /* Pass 2: Generate PAL with label resolution */
    FILE *pf = fopen(pal_path, "w");
    if (!pf) return;
    fprintf(pf, "# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl by khtpm_events_hq_manager.c\n");
    fprintf(pf, "# pkg=%s page=%s - regenerated fresh on every command save\n", g_entity_label, g_pages[page_idx]);

    IfFrame if_stack[MAX_IF_NEST];
    int if_top = 0;
    int label_counter = 0;

    for (int ni = 0; ni < n_nodes; ni++) {
        IrNode *nd = &nodes[ni];

        if (strcmp(nd->type, "if") == 0) {
            if (if_top >= MAX_IF_NEST) continue;
            label_counter++;
            IfFrame *fr = &if_stack[if_top];
            snprintf(fr->end_label, sizeof(fr->end_label), "_endif_%d", label_counter);
            snprintf(fr->else_label, sizeof(fr->else_label), "_else_%d", label_counter);
            fr->has_else = 0;
            if_top++;
            /* Evaluate condition: read switch/variable, compare, branch */
            char state_dir[PATH_BUF];
            resolve_session_root(state_dir, sizeof(state_dir));
            char switch_name[128] = "", compare[32] = "";
            for (int pi = 0; pi < nd->n_params; pi++) {
                if (strcmp(nd->keys[pi], "switch_name") == 0) snprintf(switch_name, sizeof(switch_name), "%s", nd->vals[pi]);
                else if (strcmp(nd->keys[pi], "compare") == 0) snprintf(compare, sizeof(compare), "%s", nd->vals[pi]);
            }
            if (switch_name[0]) {
                fprintf(pf, "li x15, 6\n");
                fprintf(pf, "ecall \"%s/switches.txt\" \"%s\"\n", state_dir, switch_name);
                fprintf(pf, "li x2, %s\n", strcmp(compare, "on") == 0 ? "1" : "0");
                fprintf(pf, "bne x12, x2, %s\n", fr->else_label);
            }
            continue;
        }
        if (strcmp(nd->type, "else") == 0) {
            if (if_top > 0) {
                IfFrame *fr = &if_stack[if_top - 1];
                fprintf(pf, "j %s\n", fr->end_label);
                fprintf(pf, "%s:\n", fr->else_label);
                fr->has_else = 1;
            }
            continue;
        }
        if (strcmp(nd->type, "end") == 0) {
            if (if_top > 0) {
                if_top--;
                IfFrame *fr = &if_stack[if_top];
                if (!fr->has_else) fprintf(pf, "%s:\n", fr->else_label);
                fprintf(pf, "%s:\n", fr->end_label);
            }
            continue;
        }
        /* Normal registry-driven compilation (existing code) */
        CommandDef *def = find_command_def(nd->type);
        char keys[MAX_FIELDS][32], vals[MAX_FIELDS][256]; int n = nd->n_params;
        for (int pi = 0; pi < n; pi++) {
            snprintf(keys[pi], sizeof(keys[0]), "%s", nd->keys[pi]);
            snprintf(vals[pi], sizeof(vals[0]), "%s", nd->vals[pi]);
        }
        if (def && def->n_pal > 0) {
            char state_dir[PATH_BUF];
            resolve_session_root(state_dir, sizeof(state_dir));
            if (n < MAX_FIELDS) {
                snprintf(keys[n], sizeof(keys[0]), "STATE_DIR");
                snprintf(vals[n], sizeof(vals[0]), "%s", state_dir);
                n++;
            }
            for (int pi = 0; pi < def->n_pal; pi++) {
                char expanded[512];
                expand_template(def->pal_lines[pi], keys, vals, n, expanded, sizeof(expanded));
                fprintf(pf, "%s\n", expanded);
            }
        } else {
            char wrapper_path[PATH_BUF];
            snprintf(wrapper_path, sizeof(wrapper_path), "%s/cmd_%d.sh", pd, nd->id);
            FILE *wf = fopen(wrapper_path, "w");
            if (wf) {
                fprintf(wf, "#!/bin/sh\n");
                fprintf(wf, "cd \"$(dirname \"$0\")/../../..\" || exit 1\n");
                fprintf(wf, "ENT=\"$PWD\"\n");
                fprintf(wf, "D=\"$ENT\"\n");
                fprintf(wf, "while [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n");
                if (def) {
                    char expanded[512];
                    expand_template(def->tmpl, keys, vals, n, expanded, sizeof(expanded));
                    fprintf(wf, "%s\n", expanded);
                }
                fclose(wf);
                chmod(wrapper_path, 0755);
            }
            fprintf(pf, "exec cmd_%d.sh\n", nd->id);
        }
    }
    fprintf(pf, "halt\n");
    fclose(pf);
}

/* Publishes the currently-selected page's trigger + switch + command list.
 * Format: first line "TRIGGER|<value>", second line "SWITCH|<value>" (if set),
 * then one "CMD|<id>|<type>|<params>" line per command - simple enough for
 * the shell to parse without a real structured-data library, matching this
 * house's existing plain-pipe-delimited convention elsewhere. */
static void publish_page_state(void) {
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);

    char trigger[64] = "(unset)";
    char switch_name[128] = "";  /* empty string means not configured */
    char cpath[PATH_BUF]; snprintf(cpath, sizeof(cpath), "%s/condition.pdl", pd);
    FILE *cf = fopen(cpath, "r");
    if (cf) {
        char line[512];
        while (fgets(line, sizeof(line), cf)) {
            if (strncmp(line, "COND", 4) != 0) continue;
            char *pipe1 = strchr(line, '|');
            if (!pipe1) continue;
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (!pipe2) continue;

            /* Extract key (between first two pipes) */
            char key[64] = "";
            sscanf(pipe1 + 1, "%[^|]", key);

            /* Extract value (after second pipe) */
            char val[128];
            snprintf(val, sizeof(val), "%s", pipe2 + 1);
            char *nl = strpbrk(val, "\r\n"); if (nl) *nl = '\0';
            char *s = val; while (*s == ' ') s++;

            if (strstr(key, "trigger")) {
                snprintf(trigger, sizeof(trigger), "%s", s);
            } else if (strstr(key, "switch")) {
                snprintf(switch_name, sizeof(switch_name), "%s", s);
            }
        }
        fclose(cf);
    }

    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_page_state_path);
    FILE *wf = fopen(tmp, "w");
    if (!wf) return;
    fprintf(wf, "TRIGGER|%s\n", trigger);
    if (switch_name[0]) {
        fprintf(wf, "SWITCH|%s\n", switch_name);
    }

    char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);
    FILE *irf = fopen(ir_path, "r");
    if (irf) {
        char line[512];
        while (fgets(line, sizeof(line), irf)) {
            if (strncmp(line, "NODE", 4) != 0) continue;
            char *tp = strstr(line, "type=");
            if (!tp) continue;
            char type_buf[32] = "";
            char *t = tp + 5, *sp = strchr(t, ' ');
            char *pipe = strchr(t, '|');
            size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
            if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
            memcpy(type_buf, t, len); type_buf[len] = '\0';
            char *idp = strstr(line, "id=");
            int id = idp ? atoi(idp + 3) : 0;
            /* REAL BUG FIX (2026-08-26, found live via Task 7 - command
             * descriptions came out with every value "(empty)"): a NODE
             * line ("NODE | id=N type=T | params") has exactly TWO pipes
             * total. `pipe` (found above, searching from just after
             * "type=") already lands on the SECOND one, the real
             * delimiter before params. This used to search for a THIRD
             * pipe after that (pipe2 = strchr(pipe+1,'|')), which never
             * exists in a real NODE line, so `params` was unconditionally
             * empty for every single command that was ever published -
             * a real, pre-existing bug nothing had surfaced/looked at the
             * actual value of before now. */
            char params[512] = "";
            if (pipe) {
                const char *ps = pipe + 1;
                while (*ps == ' ') ps++;
                snprintf(params, sizeof(params), "%s", ps);
                params[strcspn(params, "\r\n")] = '\0';
                /* REAL BUG FIX (2026-08-26, Task 7): convert old space-separated
                 * params format from event.ir.pdl to new pipe-separated format that
                 * the render code expects. Without this, multi-param commands (like
                 * show_choices with choices and default) get all fields concatenated
                 * into field1 with field2 staying empty. */
                char converted[512] = "";
                convert_params_to_new_format(params, type_buf, converted, sizeof(converted));
                if (converted[0]) snprintf(params, sizeof(params), "%s", converted);
            }
            fprintf(wf, "CMD|%d|%s|%s\n", id, type_buf, params);
        }
        fclose(irf);
    }
    fclose(wf);
    rename(tmp, g_page_state_path);
}

static void handle_new_page_request(void) {
    /* Create a new page directory with the next available page number.
     * Matches event-ez's format: pages/page_N/ with condition.pdl and event.ir.pdl. */
    char pages_root[PATH_BUF];
    snprintf(pages_root, sizeof(pages_root), "%s/pages", g_pkg_dir);
    if (access(pages_root, F_OK) != 0) mkdir(pages_root, 0755);

    /* Find the next available page number */
    int next_page_num = 1;
    DIR *d = opendir(pages_root);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            if (strncmp(de->d_name, "page_", 5) != 0) continue;
            int num = atoi(de->d_name + 5);
            if (num >= next_page_num) next_page_num = num + 1;
        }
        closedir(d);
    }

    /* Create the new page directory */
    char new_page_dir[PATH_BUF];
    snprintf(new_page_dir, sizeof(new_page_dir), "%s/page_%d", pages_root, next_page_num);
    if (mkdir(new_page_dir, 0755) != 0) return;

    /* Create condition.pdl with default trigger. REAL FIX (2026-08-25,
     * found verifying H6 live): must be "on-click" (hyphen), NOT
     * "on_click" (underscore) - play_event.sh's own default TRIGGER and
     * every real pre-existing page (e.g. page_1 here) use the hyphenated
     * form with an EXACT string match (no normalization anywhere in the
     * runtime) - a page created with the underscore form is silently
     * unplayable via any trigger, forever, with no error. */
    char cond_path[PATH_BUF];
    snprintf(cond_path, sizeof(cond_path), "%s/condition.pdl", new_page_dir);
    FILE *cf = fopen(cond_path, "w");
    if (cf) {
        fprintf(cf, "SECTION      | KEY                | VALUE\n");
        fprintf(cf, "----------------------------------------\n");
        fprintf(cf, "META         | piece_id           | %s\n", g_entity_label);
        fprintf(cf, "COND         | trigger              | on-click\n");
        fclose(cf);
    }

    /* Create empty event.ir.pdl with header only */
    char ir_path[PATH_BUF];
    snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", new_page_dir);
    FILE *irf = fopen(ir_path, "w");
    if (irf) {
        fprintf(irf, "SECTION      | KEY                | VALUE\n");
        fprintf(irf, "----------------------------------------\n");
        fprintf(irf, "META         | piece_id           | %s\n", g_entity_label);
        fprintf(irf, "STATE        | source               | events-hq\n");
        fclose(irf);
    }

    /* Compile the empty page (generates empty event.pal) */
    compile_page(next_page_num - 1);
}

static void handle_action_request(void) {
    FILE *f = fopen(g_action_path, "r");
    if (!f) return;
    char line[600] = "";
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') return;

    if (strncmp(line, "new_page", 8) == 0) {
        handle_new_page_request();
        FILE *cw = fopen(g_action_path, "w");
        if (cw) fclose(cw);
        return;
    }

    /* Handle trigger update: "trigger:<new_trigger_value>"
     * Task H7 (2026-08-25) - events-hq trigger/condition editing */
    if (strncmp(line, "trigger:", 8) == 0) {
        char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);
        char cpath[PATH_BUF]; snprintf(cpath, sizeof(cpath), "%s/condition.pdl", pd);

        char new_trigger[64];
        snprintf(new_trigger, sizeof(new_trigger), "%s", line + 8);
        new_trigger[strcspn(new_trigger, "\r\n")] = '\0';

        FILE *cf = fopen(cpath, "r");
        char old_content[2048] = "";
        if (cf) {
            char lbuf[512];
            while (fgets(lbuf, sizeof(lbuf), cf)) {
                /* REAL BUG FIX (2026-08-27, found live) - this used to
                 * skip EVERY "COND" line unconditionally, silently
                 * wiping the switch-name COND line (added same day)
                 * every time the trigger was updated, and vice versa in
                 * the switch: handler below. Only skip the line THIS
                 * handler owns ("COND | trigger | ..."), preserve any
                 * other COND line (e.g. "COND | switch | ...")
                 * unchanged - confirmed live: setting trigger no longer
                 * destroys an already-set switch name. */
                if (strncmp(lbuf, "COND", 4) == 0 && strstr(lbuf, "| trigger |")) continue;
                strncat(old_content, lbuf, sizeof(old_content) - strlen(old_content) - 1);
            }
            fclose(cf);
        }

        FILE *wf = fopen(cpath, "w");
        if (wf) {
            fprintf(wf, "%s", old_content);
            fprintf(wf, "COND | trigger | %s\n", new_trigger);
            fclose(wf);
        }

        publish_page_state();
        FILE *cw = fopen(g_action_path, "w");
        if (cw) fclose(cw);
        return;
    }

    /* Handle switch name update: "switch:<new_switch_name>"
     * For Autorun/Parallel common events, stores the real switch name to watch */
    if (strncmp(line, "switch:", 7) == 0) {
        char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);
        char cpath[PATH_BUF]; snprintf(cpath, sizeof(cpath), "%s/condition.pdl", pd);

        char new_switch[128];
        snprintf(new_switch, sizeof(new_switch), "%s", line + 7);
        new_switch[strcspn(new_switch, "\r\n")] = '\0';

        FILE *cf = fopen(cpath, "r");
        char old_content[2048] = "";
        if (cf) {
            char lbuf[512];
            while (fgets(lbuf, sizeof(lbuf), cf)) {
                /* REAL BUG FIX (2026-08-27) - see the trigger: handler's
                 * own comment above, same real bug, same fix: only skip
                 * this handler's own COND line, preserve the trigger's. */
                if (strncmp(lbuf, "COND", 4) == 0 && strstr(lbuf, "| switch |")) continue;
                strncat(old_content, lbuf, sizeof(old_content) - strlen(old_content) - 1);
            }
            fclose(cf);
        }

        FILE *wf = fopen(cpath, "w");
        if (wf) {
            fprintf(wf, "%s", old_content);
            fprintf(wf, "COND | switch | %s\n", new_switch);
            fclose(wf);
        }

        publish_page_state();
        FILE *cw = fopen(g_action_path, "w");
        if (cw) fclose(cw);
        return;
    }

    /* Handle play action - run the current event using play_event.sh.
     * Task H8: Play button in events-hq footer (2026-08-25) */
    if (strncmp(line, "play", 4) == 0) {
        /* play_event.sh expects the ENTITY directory (parent of event_pkg),
         * not the event_pkg directory itself. Get the parent directory. */
        char entity_dir[PATH_BUF];
        char *last_slash = strrchr(g_pkg_dir, '/');
        if (last_slash && strncmp(last_slash + 1, "event_pkg", 9) == 0) {
            size_t len = (size_t)(last_slash - g_pkg_dir);
            if (len > 0 && len < sizeof(entity_dir) - 1) {
                memcpy(entity_dir, g_pkg_dir, len);
                entity_dir[len] = '\0';
            } else {
                snprintf(entity_dir, sizeof(entity_dir), "%s", g_pkg_dir);
            }
        } else {
            snprintf(entity_dir, sizeof(entity_dir), "%s", g_pkg_dir);
        }

        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "sh -c 'exec sh \"%s/*.monads/*.muchi-pet/ops/play_event.sh\" \"%s\"' >/dev/null 2>&1 &",
                 g_house_root, entity_dir);
        system(cmd);
        FILE *cw = fopen(g_action_path, "w");
        if (cw) fclose(cw);
        return;
    }

    /* Task 7 (2026-08-26, direct instruction: command rows need to be
     * real, editable, not just data-driven-added) - "edit:<id>|<type>|
     * <params_line>" rewrites the matching NODE line's type+params in
     * place (id and position both preserved - only append: knows how to
     * compute a next_id; editing never needs one), same real recompile
     * afterward as append:. */
    if (strncmp(line, "edit:", 5) == 0) {
        char *rest = line + 5;
        char *bar1 = strchr(rest, '|');
        if (!bar1) return;
        int edit_id = atoi(rest);
        char *type_start = bar1 + 1;
        char *bar2 = strchr(type_start, '|');
        if (!bar2) return;
        char type[32]; size_t tlen = (size_t)(bar2 - type_start);
        if (tlen >= sizeof(type)) tlen = sizeof(type) - 1;
        memcpy(type, type_start, tlen); type[tlen] = '\0';
        const char *params_line = bar2 + 1;

        char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);
        char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);

        char all[8192] = ""; size_t all_len = 0;
        FILE *rf = fopen(ir_path, "r");
        if (rf) {
            char l[512];
            while (fgets(l, sizeof(l), rf)) {
                if (strncmp(l, "NODE", 4) == 0) {
                    char *idp = strstr(l, "id=");
                    int this_id = idp ? atoi(idp + 3) : -1;
                    if (this_id == edit_id) {
                        char newl[600];
                        snprintf(newl, sizeof(newl), "NODE         | id=%d type=%s | %s\n", edit_id, type, params_line);
                        size_t nl = strlen(newl);
                        if (all_len + nl < sizeof(all)) { memcpy(all + all_len, newl, nl); all_len += nl; }
                        continue;
                    }
                }
                size_t ll = strlen(l);
                if (all_len + ll < sizeof(all)) { memcpy(all + all_len, l, ll); all_len += ll; }
            }
            fclose(rf);
        }
        FILE *wf = fopen(ir_path, "w");
        if (wf) { fwrite(all, 1, all_len, wf); fclose(wf); }

        compile_page(g_current_page);
        publish_page_state();

        FILE *cw = fopen(g_action_path, "w");
        if (cw) fclose(cw);
        return;
    }

    if (strncmp(line, "append:", 7) != 0) return;

    char *rest = line + 7;
    char *bar = strchr(rest, '|');
    if (!bar) return;
    char type[32]; size_t tlen = (size_t)(bar - rest);
    if (tlen >= sizeof(type)) tlen = sizeof(type) - 1;
    memcpy(type, rest, tlen); type[tlen] = '\0';
    const char *params_line = bar + 1;

    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);
    char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);

    int next_id = 1;
    FILE *rf = fopen(ir_path, "r");
    if (rf) {
        char l[512];
        while (fgets(l, sizeof(l), rf)) if (strncmp(l, "NODE", 4) == 0) next_id++;
        fclose(rf);
    } else {
        FILE *hf = fopen(ir_path, "w");
        if (hf) {
            fprintf(hf, "SECTION      | KEY                | VALUE\n");
            fprintf(hf, "----------------------------------------\n");
            fprintf(hf, "META         | piece_id           | %s\n", g_entity_label);
            fprintf(hf, "STATE        | source               | events-hq\n");
            fclose(hf);
        }
    }
    FILE *af = fopen(ir_path, "a");
    if (af) {
        fprintf(af, "NODE         | id=%d type=%s | %s\n", next_id, type, params_line);
        fclose(af);
    }

    compile_page(g_current_page);
    publish_page_state();

    FILE *cw = fopen(g_action_path, "w");
    if (cw) fclose(cw);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "khtpm_events_hq_manager: usage: <house_root> <pkg_dir> <entity_label>\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_pkg_dir, sizeof(g_pkg_dir), "%s", argv[2]);
    snprintf(g_entity_label, sizeof(g_entity_label), "%s", argv[3]);

    snprintf(g_mgr_dir, sizeof(g_mgr_dir), "%s/.hq_manager", g_pkg_dir);
    if (access(g_mgr_dir, F_OK) != 0) mkdir(g_mgr_dir, 0755);
    snprintf(g_pages_state_path, sizeof(g_pages_state_path), "%s/pages.state.txt", g_mgr_dir);
    snprintf(g_selected_page_path, sizeof(g_selected_page_path), "%s/selected_page.txt", g_mgr_dir);
    snprintf(g_page_state_path, sizeof(g_page_state_path), "%s/page.state.txt", g_mgr_dir);
    snprintf(g_action_path, sizeof(g_action_path), "%s/action.txt", g_mgr_dir);

    FILE *w = fopen(g_action_path, "w"); /* start clean, no stale action */
    if (w) fclose(w);

    int last_page = -1;
    for (;;) {
        publish_pages();
        read_selected_page();
        if (g_current_page != last_page) {
            publish_page_state();
            last_page = g_current_page;
        }
        handle_action_request();
        usleep(400000);
    }
    return 0;
}
