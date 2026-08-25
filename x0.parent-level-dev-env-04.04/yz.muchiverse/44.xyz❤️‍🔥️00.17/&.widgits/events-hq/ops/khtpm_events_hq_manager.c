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

/* Real compile pass, ported line-for-line from the shell's own old
 * compile_page() (itself ported from ez_menu_input.c originally) -
 * event.pal is ALWAYS fully regenerated from event.ir.pdl, never
 * hand-patched, matching event-ez's own "visual compiler" semantics. */
static void compile_page(int page_idx) {
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), page_idx);
    char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);
    char pal_path[PATH_BUF]; snprintf(pal_path, sizeof(pal_path), "%s/event.pal", pd);

    FILE *pf = fopen(pal_path, "w");
    if (!pf) return;
    fprintf(pf, "# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl by khtpm_events_hq_manager.c\n");
    fprintf(pf, "# pkg=%s page=%s - regenerated fresh on every command save\n", g_entity_label, g_pages[page_idx]);
    FILE *irf = fopen(ir_path, "r");
    if (irf) {
        char line[512];
        while (fgets(line, sizeof(line), irf)) {
            if (strncmp(line, "NODE", 4) != 0) continue;
            char *tp = strstr(line, "type=");
            if (!tp) continue;
            char type_buf[48] = "";
            char *t = tp + 5, *sp = strchr(t, ' ');
            char *pipe = strchr(t, '|');
            size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
            if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
            memcpy(type_buf, t, len); type_buf[len] = '\0';
            char *idp = strstr(line, "id=");
            int node_id = idp ? atoi(idp + 3) : 1;
            char wrapper_path[PATH_BUF];
            snprintf(wrapper_path, sizeof(wrapper_path), "%s/cmd_%d.sh", pd, node_id);
            FILE *wf = fopen(wrapper_path, "w");
            if (wf) {
                fprintf(wf, "#!/bin/sh\n");
                fprintf(wf, "cd \"$(dirname \"$0\")/../../..\" || exit 1\n");
                fprintf(wf, "ENT=\"$PWD\"\n");
                fprintf(wf, "D=\"$ENT\"\n");
                fprintf(wf, "while [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n");
                if (strcmp(type_buf, "change_gold") == 0) {
                    char *amtp = strstr(line, "amount=");
                    char amt[32] = "0";
                    if (amtp) { snprintf(amt, sizeof(amt), "%s", amtp + 7); amt[strcspn(amt, "\r\n| ")] = '\0'; }
                    fprintf(wf, "exec \"$D/*.monads/*.muchi-pet/ops/+x/mr_change_gold.+x\" \"$ENT\" '%s'\n", amt);
                } else if (strcmp(type_buf, "show_text") == 0) {
                    char *txtp = strstr(line, "text=");
                    char txt[256] = "";
                    if (txtp) { snprintf(txt, sizeof(txt), "%s", txtp + 5); char *bar = strchr(txt, '|'); if (bar) *bar = '\0'; txt[strcspn(txt, "\r\n")] = '\0'; }
                    char *spkp = strstr(line, "speaker=");
                    char spk[64] = "";
                    if (spkp) { snprintf(spk, sizeof(spk), "%s", spkp + 8); spk[strcspn(spk, "\r\n")] = '\0'; }
                    if (spk[0]) fprintf(wf, "exec \"$D/*.monads/*.muchi-pet/ops/+x/mr_show_text.+x\" \"$ENT\" '%s' '%s'\n", txt, spk);
                    else fprintf(wf, "exec \"$D/*.monads/*.muchi-pet/ops/+x/mr_show_text.+x\" \"$ENT\" '%s'\n", txt);
                } else if (strcmp(type_buf, "show_choices") == 0) {
                    char *chp = strstr(line, "choices=");
                    char ch[256] = "";
                    if (chp) { snprintf(ch, sizeof(ch), "%s", chp + 8); char *d = strstr(ch, " default="); if (d) *d = '\0'; ch[strcspn(ch, "\r\n")] = '\0'; }
                    char *defp = strstr(line, "default=");
                    int def = defp ? atoi(defp + 8) : 0;
                    fprintf(wf, "exec \"$D/*.monads/*.muchi-pet/ops/+x/mr_show_choices.+x\" \"$ENT\" '%s' %d\n", ch, def);
                }
                fclose(wf);
                chmod(wrapper_path, 0755);
            }
            fprintf(pf, "exec cmd_%d.sh\n", node_id);
        }
        fclose(irf);
    }
    fprintf(pf, "halt\n");
    fclose(pf);
}

/* Publishes the currently-selected page's trigger + command list.
 * Format: first line "TRIGGER|<value>", then one "CMD|<id>|<type>|<params>"
 * line per command - simple enough for the shell to parse without a
 * real structured-data library, matching this house's existing plain-
 * pipe-delimited convention elsewhere. */
static void publish_page_state(void) {
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);

    char trigger[64] = "(unset)";
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
            char val[64]; snprintf(val, sizeof(val), "%s", pipe2 + 1);
            char *nl = strpbrk(val, "\r\n"); if (nl) *nl = '\0';
            char *s = val; while (*s == ' ') s++;
            snprintf(trigger, sizeof(trigger), "%s", s);
            break;
        }
        fclose(cf);
    }

    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_page_state_path);
    FILE *wf = fopen(tmp, "w");
    if (!wf) return;
    fprintf(wf, "TRIGGER|%s\n", trigger);

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
            char *pipe2 = pipe ? strchr(pipe + 1, '|') : NULL;
            char params[512] = "";
            if (pipe2) {
                const char *ps = pipe2 + 1;
                while (*ps == ' ') ps++;
                snprintf(params, sizeof(params), "%s", ps);
                params[strcspn(params, "\r\n")] = '\0';
            }
            fprintf(wf, "CMD|%d|%s|%s\n", id, type_buf, params);
        }
        fclose(irf);
    }
    fclose(wf);
    rename(tmp, g_page_state_path);
}

static void handle_action_request(void) {
    FILE *f = fopen(g_action_path, "r");
    if (!f) return;
    char line[600] = "";
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0' || strncmp(line, "append:", 7) != 0) return;

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
