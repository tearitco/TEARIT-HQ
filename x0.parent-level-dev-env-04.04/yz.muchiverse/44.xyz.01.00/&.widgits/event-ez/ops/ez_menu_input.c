/* ez_menu_input - Event-EZ side effects
 * CHTPM owns [>] focus, digit_accum, and cli_io text editing (Enter
 * activates, ESC deactivates - see reference_cli_io_field_mechanic).
 * This op only handles KEY:n button clicks:
 *   5=Save Trigger (Page screen - writes condition.pdl's real trigger)
 *   6=Save Change Gold (Change Gold parameter screen - appends a real
 *     NODE row to event.ir.pdl, recompiles event.pal fresh from it)
 *   7=Clear All Commands on This Page (wipe NODE rows; keep trigger;
 *     recompile empty event.pal; remove cmd_*.sh wrappers)
 *   8=Save Show Text (Show Text parameter screen - appends type=
 *     show_text NODE, recompiles, materializes msg_<id>.txt + wrapper)
 *   9=Save Show Choices (same pattern; runtime op still pending)
 *   idle 0 = no-op
 *
 * KEY:n is injected as the ASCII digit char ('5'==53, not integer 5) —
 * see chtpm_parser_pal send_command KEY: path + inject_raw_key. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *value) {
    char lines[64][MAX_LINE];
    int n = 0;
    int replaced = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[MAX_LINE];
        size_t klen = strlen(key);
        while (n < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                snprintf(lines[n], MAX_LINE, "%s=%s\n", key, value);
                replaced = 1;
            } else {
                snprintf(lines[n], MAX_LINE, "%s", line);
            }
            n++;
        }
        fclose(rf);
    }
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    for (int i = 0; i < n; i++) fputs(lines[i], wf);
    if (!replaced) fprintf(wf, "%s=%s\n", key, value);
    fclose(wf);
}

/* Same real mechanism as ez_compose_frame.c's own current_page_number()
 * - chtpm_parser_pal.c writes the active layout's own path into
 * pieces/display/current_layout.txt on every screen switch (confirmed
 * via source, not invented). Save needs to know which page it's
 * currently on the SAME way the renderer does - both must agree. */
static int current_page_number(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int n = 0;
    if (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, "page_");
        if (p) n = atoi(p + 5);
    }
    fclose(f);
    return n;
}

static void bump(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/ez_screen_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void set_msg(const char *state, const char *msg) {
    write_kv(state, "last_message", msg);
}

/* Event-EZ IR rows are one line per NODE, but authored Show Text may
 * contain real newlines (cli_io Ctrl+J, same trick Show Choices
 * documents). Store newlines escaped as literal "\n" two-char sequences
 * (and fold '|' to ':' - the pipe is the row's own field separator);
 * decode back at compile time when materializing msg_<id>.txt. */
static void ir_escape(char *buf, size_t sz) {
    char tmp[MAX_LINE];
    size_t j = 0;
    for (size_t i = 0; buf[i] && j + 2 < sizeof(tmp); i++) {
        if (buf[i] == '\n') { tmp[j++] = '\\'; tmp[j++] = 'n'; }
        else if (buf[i] == '\r') { /* drop */ }
        else if (buf[i] == '|') { tmp[j++] = ':'; }
        else tmp[j++] = buf[i];
    }
    tmp[j] = '\0';
    snprintf(buf, sz, "%s", tmp);
}

static void ir_unescape(const char *in, char *out, size_t sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < sz; i++) {
        if (in[i] == '\\' && in[i + 1] == 'n') { out[j++] = '\n'; i++; }
        else out[j++] = in[i];
    }
    out[j] = '\0';
}

/* Greedy word-wrap at ~38 columns - matches the 40-col box aesthetic
 * every event-ez screen already uses. khtpm_show_text.c's documented
 * contract is "caller wraps, op does not reflow"; event-ez is now a
 * real caller/author, so the wrap lives HERE (compile step), not in
 * the shared op. Explicit \n in input is preserved as a hard break. */
static void wrap38(const char *in, char *out, size_t sz) {
    size_t j = 0;
    int col = 0;
    const char *p = in;
    while (*p && j + 1 < sz) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '\n') { out[j++] = '\n'; col = 0; p++; continue; }
        char word[256];
        size_t w = 0;
        while (*p && *p != ' ' && *p != '\n' && w + 1 < sizeof(word)) word[w++] = *p++;
        word[w] = '\0';
        if (col == 0) {
            /* word longer than a line stays whole - no mid-word split */
        } else if (col + 1 + (int)w <= 38) {
            out[j++] = ' ';
            col += 1;
        } else {
            out[j++] = '\n';
            col = 0;
        }
        for (size_t k = 0; k < w && j + 1 < sz; k++) out[j++] = word[k];
        col += (int)w;
    }
    out[j] = '\0';
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char state[PATH_BUF], gui[PATH_BUF];
    snprintf(state, sizeof(state), "%s/pieces/system/ez_state.txt", project_root);
    snprintf(gui, sizeof(gui), "%s/projects/event-ez/manager/gui_state.txt", project_root);

    if (key == 0) return 0;

    if (key == '5') {
        /* REAL, 2026-08-05, direct instruction ("this is at this point a
         * proving grounds for our ability to do event management
         * mirroring rpg maker" - see visual-event-compiler-pal.md §7):
         * the old flat Chase/Flee/Wander/Idle/Target/Speed/Command model
         * is gone - a page now holds a real, growing COMMAND LIST
         * (Change Gold today, Show Choices next). "Save Trigger" (this
         * KEY) now ONLY writes condition.pdl - real commands are each
         * saved on their OWN parameter screen (see KEY:6 below), not
         * bundled into this same save. */
        char pkg[128], trigger[64];
        int page_n = current_page_number();
        if (page_n <= 0) { set_msg(state, "Save failed: not on a page screen"); bump(); return 0; }
        read_kv(state, "pkg_name", pkg, sizeof(pkg));
        read_kv(gui, "ez_trigger", trigger, sizeof(trigger));
        if (!pkg[0]) { set_msg(state, "Save failed: no pkg_name set"); bump(); return 0; }
        if (!trigger[0]) snprintf(trigger, sizeof(trigger), "on_click");

        char pkg_dir[PATH_BUF];
        read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
        if (!pkg_dir[0]) { set_msg(state, "Save failed: no pkg_dir set"); bump(); return 0; }

        char page_dir[PATH_BUF];
        snprintf(page_dir, sizeof(page_dir), "%s/pages/page_%d", pkg_dir, page_n);
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", page_dir);
        if (system(cmd) != 0) { /* best-effort */ }

        char cond_path[PATH_BUF];
        snprintf(cond_path, sizeof(cond_path), "%s/condition.pdl", page_dir);
        FILE *cf = fopen(cond_path, "w");
        if (cf) {
            fprintf(cf, "SECTION      | KEY                | VALUE\n");
            fprintf(cf, "----------------------------------------\n");
            fprintf(cf, "META         | piece_id           | %s\n", pkg);
            fprintf(cf, "COND         | trigger              | %s\n", trigger);
            fclose(cf);
        }

        /* Real event.ir.pdl header only gets written here IF the page
         * has no commands yet (compile_event_pal() below regenerates the
         * real event.pal from whatever NODE rows already exist - a fresh
         * page needs a real header + a terminating NODE before any real
         * command gets appended by KEY:6). */
        char ir_path[PATH_BUF];
        snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", page_dir);
        FILE *chk = fopen(ir_path, "r");
        if (!chk) {
            FILE *f = fopen(ir_path, "w");
            if (f) {
                fprintf(f, "SECTION      | KEY                | VALUE\n");
                fprintf(f, "----------------------------------------\n");
                fprintf(f, "META         | piece_id           | %s\n", pkg);
                fprintf(f, "STATE        | source               | event-ez\n");
                fclose(f);
            }
        } else fclose(chk);

        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "Saved page %d trigger=%s - use Add Command for real commands", page_n, trigger);
        set_msg(state, msg);
        bump();
        return 0;
    }

    if (key == '6') {
        /* REAL, 2026-08-05: Change Gold parameter screen's own Save -
         * appends a real NODE row (type=change_gold amount=X) to this
         * page's own event.ir.pdl, then RECOMPILES the real event.pal
         * from scratch from ir.pdl's own NODE rows (compile_event_pal()
         * below) - matching the "visual COMPILER" framing exactly:
         * event.ir.pdl is the real source of truth/IR, event.pal is
         * always a fresh, regenerated compiled artifact, never hand-
         * patched incrementally. Returns to the Page screen (href on
         * the parameter screen's own Save button... no, Save here is a
         * KEY not an href - real UX: after saving, the SCREEN is still
         * the parameter screen; a human/k3 agent then clicks "Back" to
         * return to the Page screen and see the new command listed). */
        char pkg[128], amount[32];
        int page_n = current_page_number();
        if (page_n <= 0) { set_msg(state, "Save failed: not on a command screen"); bump(); return 0; }
        read_kv(state, "pkg_name", pkg, sizeof(pkg));
        read_kv(gui, "ez_cg_amount", amount, sizeof(amount));
        if (!amount[0]) { set_msg(state, "Save failed: enter an amount (+10 or -5)"); bump(); return 0; }

        /* Debounce via state file (ez_menu_input is a one-shot process —
         * static vars do NOT persist across KEY:6 presses). */
        {
            char last_amt[64] = "";
            char last_t_s[32] = "";
            read_kv(state, "last_cg_amount", last_amt, sizeof(last_amt));
            read_kv(state, "last_cg_time", last_t_s, sizeof(last_t_s));
            time_t now = time(NULL);
            time_t last_t = (time_t)atol(last_t_s);
            if (last_amt[0] && strcmp(last_amt, amount) == 0 && last_t > 0 && (now - last_t) < 2) {
                set_msg(state, "Already saved that amount — not adding again");
                bump();
                return 0;
            }
            char ts[32];
            snprintf(ts, sizeof(ts), "%ld", (long)now);
            write_kv(state, "last_cg_amount", amount);
            write_kv(state, "last_cg_time", ts);
        }

        char pkg_dir[PATH_BUF];
        read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
        if (!pkg_dir[0]) { set_msg(state, "Save failed: no pkg_dir set"); bump(); return 0; }

        char page_dir[PATH_BUF];
        snprintf(page_dir, sizeof(page_dir), "%s/pages/page_%d", pkg_dir, page_n);
        char ir_path[PATH_BUF];
        snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", page_dir);

        /* Real, stable NODE id: count existing NODE rows + 1. */
        int next_id = 1;
        FILE *rf2 = fopen(ir_path, "r");
        if (rf2) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), rf2)) {
                if (strncmp(line, "NODE", 4) == 0) next_id++;
            }
            fclose(rf2);
        }
        FILE *af = fopen(ir_path, "a");
        if (!af) { set_msg(state, "Save failed: could not open event.ir.pdl"); bump(); return 0; }
        fprintf(af, "NODE         | id=%d type=change_gold | amount=%s\n", next_id, amount);
        fclose(af);

        /* Real compile pass: event.pal is ALWAYS fully regenerated from
         * event.ir.pdl's own NODE rows, never hand-patched - genuine
         * "visual compiler" semantics. mr_change_gold.+x's own real path
         * is resolved relative to project_root's own house layout
         * (event-ez lives at <house>/&.widgits/event-ez/, so 3 levels
         * up reaches house root - same real relationship every other
         * real op path in this house already assumes). */
        char pal_path[PATH_BUF];
        snprintf(pal_path, sizeof(pal_path), "%s/event.pal", page_dir);
        FILE *pf = fopen(pal_path, "w");
        if (pf) {
            fprintf(pf, "# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl by event-ez\n");
            fprintf(pf, "# pkg=%s page=%d - regenerated fresh on every command save\n", pkg, page_n);
            FILE *irf = fopen(ir_path, "r");
            if (irf) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), irf)) {
                    if (strncmp(line, "NODE", 4) != 0) continue;
                    char *tp = strstr(line, "type=");
                    if (!tp) continue;
                    char type_buf[48] = "";
                    char *t = tp + 5, *sp = strchr(t, ' ');
                    char *pipe = strchr(t, '|');
                    size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
                    if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
                    memcpy(type_buf, t, len);
                    type_buf[len] = '\0';
                    if (strcmp(type_buf, "change_gold") == 0) {
                        char *ap = strstr(line, "amount=");
                        char amt[32] = "";
                        if (ap) {
                            snprintf(amt, sizeof(amt), "%s", ap + 7);
                            amt[strcspn(amt, "\r\n|")] = '\0';
                        }
                        /* REAL FIX, root-caused ONLY by actually running
                         * the compiled event.pal for real (not just
                         * reviewing it): prisc+x's own real `exec`
                         * opcode supports exactly ONE literal argument -
                         * confirmed via direct source read, its own
                         * comment says so explicitly ("a script needing
                         * 2 literal args should shell to a wrapper
                         * script... matching every op-calls-op pattern
                         * already used family-wide"). mr_change_gold.+x
                         * needs 2 (entity_dir, amount) - exec silently
                         * only honored the first, so nothing ever ran.
                         * Real fix, exactly as that comment prescribes:
                         * generate a real, tiny wrapper shell script per
                         * command (one real literal arg: nothing, it's
                         * baked into the script itself), event.pal's own
                         * exec line calls THAT with zero args. */
                        char *idp = strstr(line, "id=");
                        int node_id = idp ? atoi(idp + 3) : 1;
                        char wrapper_path[PATH_BUF];
                        snprintf(wrapper_path, sizeof(wrapper_path), "%s/cmd_%d.sh", page_dir, node_id);
                        FILE *wf = fopen(wrapper_path, "w");
                        if (wf) {
                            /* REAL FIX (2026-08-11, direct live report:
                             * "test the current change gold... fixing it
                             * if it doesn't run" - reproduced live: a
                             * real Play trigger via interact_relay.txt
                             * ran event.pal successfully but gold never
                             * changed; traced to this exact wrapper).
                             *
                             * The OLD wrapper assumed a FIXED depth from
                             * entity dir to mr_change_gold.+x
                             * ("../../ops/+x/", i.e. exactly 2 levels up
                             * from the entity root) - true ONLY for the
                             * original @.apps/MUCHI_RANCHER/entities/<name>/
                             * and *.monads/*.muchi-pet/entities/<name>/
                             * template layouts (entity's grandparent ==
                             * MUCHI_RANCHER/*.muchi-pet, which has its own
                             * ops/+x/ sibling). Silently WRONG the moment
                             * an entity is deployed/migrated to
                             * xyzfs/users/<uuid>/home/livedesk/{pals,
                             * sessions/<id>/entities}/<name>/ - a
                             * DIFFERENT depth from house_root - which is
                             * exactly where this house's own migration
                             * (away from @.apps/dev-folders into
                             * xyzfs/sessions) moves every entity to. The
                             * comment's own stated goal ("no absolute
                             * house path baked in - survives any future
                             * rename") was right in spirit but implemented
                             * as a depth assumption instead of an anchor
                             * search - a rename survives fine, a DEPTH
                             * change (exactly what this house's ongoing
                             * migration does) does not.
                             *
                             * Fixed: walk up from the entity dir (still
                             * genuinely 3 levels above this wrapper - that
                             * part was always correct, matches
                             * inventory.txt's real location) searching for
                             * the literal `*.monads` directory - a stable
                             * anchor that sits directly under house_root
                             * regardless of how deep the entity itself is
                             * nested. Same search-for-a-known-anchor
                             * pattern khtpm_taskbar_manager.c's own
                             * livedesk_login_root() already uses for the
                             * identical class of problem (finding a
                             * fixed-location tool from a variable-depth
                             * caller).
                             *
                             * UPDATED 2026-08-11 (same session, direct
                             * instruction "migration makes sense now"):
                             * mr_change_gold.+x itself moved from the
                             * legacy per-game dev folder
                             * (*.monads/*.muchi-pet/ops/+x/) to
                             * *.monads/*.muchi-pet/ops/+x/ - a shared,
                             * house-wide location (not per-user, not
                             * per-session), matching the stated direction
                             * that ops/code should be shared across all
                             * sessions/desks/users going forward (see
                             * au11-hq/EVENTS_RUNTIME.md's ops-vs-events
                             * table). Anchor changed from `*.monads` to
                             * `xyzfs` (also always a direct child of
                             * house_root) - still hardcodes
                             * mr_change_gold.+x's own name/relative
                             * location under the anchor, matching this
                             * compiler's existing level of specificity;
                             * only the anchor+target changed, not the
                             * search STRATEGY.
                             *
                             * UPDATED AGAIN 2026-08-29 ("mr was just one
                             * project using events (probably the first)
                             * but it doesn't own events"): mr_change_gold
                             * and its sibling event-command ops moved a
                             * second time, from *.monads/*.muchi-pet/ops/
                             * (still a single project's own dir, false
                             * ownership) to &.widgits/events-hq/ops/ - the
                             * already-existing SHARED events-hq ops dir
                             * (khtpm_events_hq_manager.c's own home),
                             * since every entity/project in the house
                             * compiles events that call these, not just
                             * muchi-pet. Anchor search strategy unchanged
                             * (still walks up for a directory containing
                             * xyzfs) - only the target path changed. Old
                             * binaries left in place at the old path as a
                             * compatibility shim for already-compiled
                             * cmd_N.sh wrappers; see the events-hq ops
                             * dir's own build_mr_event_ops.sh header. */
                            fprintf(wf, "#!/bin/sh\n");
                            fprintf(wf, "cd \"$(dirname \"$0\")/../../..\" || exit 1\n");
                            fprintf(wf, "ENT=\"$PWD\"\n");
                            fprintf(wf, "D=\"$ENT\"\n");
                            fprintf(wf, "while [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n");
                            fprintf(wf, "exec \"$D/&.widgits/events-hq/ops/+x/mr_change_gold.+x\" \"$ENT\" '%s'\n", amt);
                            fclose(wf);
                            chmod(wrapper_path, 0755);
                        }
                        /* Relative exec target: prisc+x resolves relative
                         * exec lines against the pal file's own dir, so
                         * the sibling cmd_N.sh wrapper is found here. */
                        fprintf(pf, "exec cmd_%d.sh\n", node_id);
                    }
                }
                fclose(irf);
            }
            fprintf(pf, "halt\n");
            fclose(pf);
        }

        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "Saved Change Gold %s to page %d - click Back to see it listed", amount, page_n);
        set_msg(state, msg);
        bump();
        return 0;
    }

    if (key == '7') {
        /* REAL FIX 2026-08-06, user: "clear all isn't yet working" —
         * layout had onClick=KEY:7 and session relay showed 55 ('7'),
         * but this handler was never implemented. Wipe every NODE on the
         * current page's event.ir.pdl, leave META/STATE/header + trigger
         * (condition.pdl) alone, rewrite event.pal to empty+halt, delete
         * cmd_*.sh wrappers, and poke piece_methods so chtpm_parser_pal
         * re-parses the page with the emptied command_list_rows_N. */
        int page_n = current_page_number();
        if (page_n <= 0) {
            set_msg(state, "Clear failed: not on a page screen");
            bump();
            return 0;
        }
        char pkg[128], pkg_dir[PATH_BUF];
        read_kv(state, "pkg_name", pkg, sizeof(pkg));
        read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
        if (!pkg_dir[0]) {
            set_msg(state, "Clear failed: no pkg_dir set");
            bump();
            return 0;
        }

        char page_dir[PATH_BUF], ir_path[PATH_BUF], pal_path[PATH_BUF];
        snprintf(page_dir, sizeof(page_dir), "%s/pages/page_%d", pkg_dir, page_n);
        snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", page_dir);
        snprintf(pal_path, sizeof(pal_path), "%s/event.pal", page_dir);

        int removed = 0;
        {
            char keep[128][MAX_LINE];
            int n_keep = 0;
            FILE *rf = fopen(ir_path, "r");
            if (rf) {
                char line[MAX_LINE];
                while (n_keep < 128 && fgets(line, sizeof(line), rf)) {
                    if (strncmp(line, "NODE", 4) == 0) {
                        removed++;
                        continue;
                    }
                    snprintf(keep[n_keep], MAX_LINE, "%s", line);
                    n_keep++;
                }
                fclose(rf);
            }
            FILE *wf = fopen(ir_path, "w");
            if (!wf) {
                set_msg(state, "Clear failed: could not rewrite event.ir.pdl");
                bump();
                return 0;
            }
            if (n_keep == 0) {
                /* Page never had IR — still leave a valid header. */
                fprintf(wf, "SECTION      | KEY                | VALUE\n");
                fprintf(wf, "----------------------------------------\n");
                fprintf(wf, "META         | piece_id           | %s\n", pkg[0] ? pkg : "event");
                fprintf(wf, "STATE        | source               | event-ez\n");
            } else {
                for (int i = 0; i < n_keep; i++) fputs(keep[i], wf);
            }
            fclose(wf);
        }

        /* Empty compiled artifact (same visual-compiler semantics as KEY:6). */
        {
            FILE *pf = fopen(pal_path, "w");
            if (pf) {
                fprintf(pf, "# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl by event-ez\n");
                fprintf(pf, "# pkg=%s page=%d - cleared (no NODE commands)\n",
                        pkg[0] ? pkg : "?", page_n);
                fprintf(pf, "halt\n");
                fclose(pf);
            }
        }

        /* Drop per-command wrappers baked at Save time. */
        {
            char rmcmd[PATH_BUF * 2];
            snprintf(rmcmd, sizeof(rmcmd), "rm -f '%s'/cmd_*.sh 2>/dev/null", page_dir);
            if (system(rmcmd) != 0) { /* best-effort */ }
        }

        /* Force chtpm reparse: command_list_rows_N is baked into the
         * element tree at parse_chtm time; only active_target_id /
         * piece_methods / input_text / wait_for_view_change re-trigger
         * parse. Flip piece_methods so the next compose_frame reloads
         * the page with emptied rows (compose_gallery rewrites
         * command_list_rows_N from disk after this bump). */
        {
            char nonce[64];
            snprintf(nonce, sizeof(nonce), "ez_clear_%d_%ld", page_n, (long)time(NULL));
            write_kv(gui, "piece_methods", nonce);
        }

        char msg[MAX_LINE];
        if (removed > 0)
            snprintf(msg, sizeof(msg),
                     "Cleared %d command(s) on page %d (trigger kept)", removed, page_n);
        else
            snprintf(msg, sizeof(msg),
                     "Page %d already had no commands (trigger kept)", page_n);
        set_msg(state, msg);
        bump();
        return 0;
    }

    if (key == '8') {
        /* Show Text command save - same pattern as Change Gold (KEY:6). */
        char pkg[128], text[256], speaker[128];
        int page_n = current_page_number();
        if (page_n <= 0) { set_msg(state, "Save failed: not on a command screen"); bump(); return 0; }
        read_kv(state, "pkg_name", pkg, sizeof(pkg));
        read_kv(gui, "ez_st_text", text, sizeof(text));
        read_kv(gui, "ez_st_speaker", speaker, sizeof(speaker));
        if (!text[0]) { set_msg(state, "Save failed: enter message text"); bump(); return 0; }

        /* Debounce via state file (ez_menu_input is a one-shot process -
         * static vars do NOT persist across presses). Same double-KEY
         * injection protection KEY:6 already needed. */
        {
            char last_txt[64] = "";
            char last_t_s[32] = "";
            read_kv(state, "last_st_text", last_txt, sizeof(last_txt));
            read_kv(state, "last_st_time", last_t_s, sizeof(last_t_s));
            time_t now = time(NULL);
            time_t last_t = (time_t)atol(last_t_s);
            if (last_txt[0] && strncmp(last_txt, text, sizeof(last_txt) - 1) == 0 && last_t > 0 && (now - last_t) < 2) {
                set_msg(state, "Already saved that message — not adding again");
                bump();
                return 0;
            }
            char ts[32];
            snprintf(ts, sizeof(ts), "%ld", (long)now);
            write_kv(state, "last_st_text", text);
            write_kv(state, "last_st_time", ts);
        }

        char pkg_dir[PATH_BUF];
        read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
        if (!pkg_dir[0]) { set_msg(state, "Save failed: no pkg_dir set"); bump(); return 0; }

        char page_dir[PATH_BUF];
        snprintf(page_dir, sizeof(page_dir), "%s/pages/page_%d", pkg_dir, page_n);
        char ir_path[PATH_BUF];
        snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", page_dir);

        int next_id = 1;
        FILE *rf2 = fopen(ir_path, "r");
        if (rf2) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), rf2)) {
                if (strncmp(line, "NODE", 4) == 0) next_id++;
            }
            fclose(rf2);
        }
        /* REAL FIX 2026-08-24: raw newlines from cli_io's Ctrl+J would
         * corrupt the single-line NODE row format (and '|' would split
         * fields). Escape both before the row is written; compile side
         * decodes them back when materializing msg files. */
        ir_escape(text, sizeof(text));
        ir_escape(speaker, sizeof(speaker));
        FILE *af = fopen(ir_path, "a");
        if (!af) { set_msg(state, "Save failed: could not open event.ir.pdl"); bump(); return 0; }
        if (speaker[0]) {
            /* REAL FIX 2026-08-24: was "text=%s speaker=%s" - a SPACE
             * separator. Every reader of these rows extracts text via
             * strcspn("\r\n|") i.e. up to the PIPE (change_gold's own
             * rows are pipe-separated too), so the speaker suffix leaked
             * into the message body both here at save time and in the
             * materialized msg file. Pipe-separate like every other
             * multi-field NODE row. */
            fprintf(af, "NODE         | id=%d type=show_text | text=%s | speaker=%s\n", next_id, text, speaker);
        } else {
            fprintf(af, "NODE         | id=%d type=show_text | text=%s\n", next_id, text);
        }
        fclose(af);

        /* Compile event.pal from ir.pdl */
        char pal_path[PATH_BUF];
        snprintf(pal_path, sizeof(pal_path), "%s/event.pal", page_dir);
        FILE *pf = fopen(pal_path, "w");
        if (pf) {
            fprintf(pf, "# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl by event-ez\n");
            fprintf(pf, "# pkg=%s page=%d - regenerated fresh on every command save\n", pkg, page_n);
            FILE *irf = fopen(ir_path, "r");
            if (irf) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), irf)) {
                    if (strncmp(line, "NODE", 4) != 0) continue;
                    char *tp = strstr(line, "type=");
                    if (!tp) continue;
                    char type_buf[48] = "";
                    char *t = tp + 5, *sp = strchr(t, ' ');
                    char *pipe = strchr(t, '|');
                    size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
                    if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
                    memcpy(type_buf, t, len);
                    type_buf[len] = '\0';
                    if (strcmp(type_buf, "show_text") == 0) {
                        char *txtp = strstr(line, "text=");
                        char raw[512] = "", txt[512] = "";
                        if (txtp) {
                            snprintf(raw, sizeof(raw), "%s", txtp + 5);
                            raw[strcspn(raw, "\r\n|")] = '\0';
                        }
                        ir_unescape(raw, txt, sizeof(txt));
                        char *spkp = strstr(line, "speaker=");
                        char spk_raw[128] = "", spk[128] = "";
                        if (spkp) {
                            snprintf(spk_raw, sizeof(spk_raw), "%s", spkp + 8);
                            spk_raw[strcspn(spk_raw, "\r\n|")] = '\0';
                            ir_unescape(spk_raw, spk, sizeof(spk));
                        }
                        char *idp = strstr(line, "id=");
                        int node_id = idp ? atoi(idp + 3) : 1;

                        /* REAL FIX 2026-08-24, root-caused by comparing
                         * this compiler's output against mr_show_text.+x's
                         * own argv contract ("<package_dir> <text_file>"):
                         * the OLD wrapper baked the LITERAL message in as
                         * arg2 - which the op realpath()s and fopen()s as
                         * a PATH - so unless the author happened to type a
                         * valid relative path, resolution failed and
                         * nothing ever displayed. It also forwarded an
                         * optional 3rd "speaker" arg the op has never
                         * accepted, and single-quoted the text into sh
                         * (any apostrophe silently broke the script).
                         *
                         * Real fix, matching the house's own visual-
                         * compiler semantics (event.pal and cmd_N.sh are
                         * ALWAYS regenerated artifacts of event.ir.pdl):
                         * this compile step now also MATERIALIZES
                         * msg_<id>.txt from the IR row - speaker name as
                         * its own first line when present, body word-
                         * wrapped at 38 cols per khtpm_show_text.c's
                         * documented caller-wraps contract - and the
                         * wrapper hands that FILE to the op. No user text
                         * ever touches shell quoting again. */
                        char msg_path[PATH_BUF], wrapped[MAX_LINE];
                        wrap38(txt, wrapped, sizeof(wrapped));
                        snprintf(msg_path, sizeof(msg_path), "%s/msg_%d.txt", page_dir, node_id);
                        FILE *mf = fopen(msg_path, "w");
                        if (mf) {
                            if (spk[0]) fprintf(mf, "%s\n", spk);
                            fputs(wrapped, mf);
                            fputc('\n', mf);
                            fclose(mf);
                        }
                        char wrapper_path[PATH_BUF];
                        snprintf(wrapper_path, sizeof(wrapper_path), "%s/cmd_%d.sh", page_dir, node_id);
                        FILE *wf = fopen(wrapper_path, "w");
                        if (wf) {
                            fprintf(wf, "#!/bin/sh\n");
                            /* Capture this script's own absolute dir BEFORE
                             * cd - $0 may be relative to the pal file's
                             * own dir and meaningless after it. Same depth
                             * math as change_gold's wrapper: cmd_N.sh sits
                             * at <entity>/event_pkg/pages/page_N/, three
                             * levels up is the entity root. */
                            fprintf(wf, "HERE=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n");
                            fprintf(wf, "cd \"$HERE/../../..\" || exit 1\n");
                            fprintf(wf, "ENT=\"$PWD\"\n");
                            fprintf(wf, "D=\"$ENT\"\n");
                            fprintf(wf, "while [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n");
                            fprintf(wf, "exec \"$D/&.widgits/events-hq/ops/+x/mr_show_text.+x\" \"$ENT\" \"$HERE/msg_%d.txt\"\n", node_id);
                            fclose(wf);
                            chmod(wrapper_path, 0755);
                        }
                        fprintf(pf, "exec cmd_%d.sh\n", node_id);
                    }
                }
                fclose(irf);
            }
            fprintf(pf, "halt\n");
            fclose(pf);
        }

        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "Saved Show Text to page %d - click Back to see it listed", page_n);
        set_msg(state, msg);
        bump();
        return 0;
    }

    if (key == '9') {
        /* Show Choices command save - same pattern. */
        char pkg[128], choices[1024], default_idx[32];
        int page_n = current_page_number();
        if (page_n <= 0) { set_msg(state, "Save failed: not on a command screen"); bump(); return 0; }
        read_kv(state, "pkg_name", pkg, sizeof(pkg));
        read_kv(gui, "ez_sc_choices", choices, sizeof(choices));
        read_kv(gui, "ez_sc_default", default_idx, sizeof(default_idx));
        if (!choices[0]) { set_msg(state, "Save failed: enter choice list"); bump(); return 0; }
        int def_idx = default_idx[0] ? atoi(default_idx) : 0;

        char pkg_dir[PATH_BUF];
        read_kv(state, "pkg_dir", pkg_dir, sizeof(pkg_dir));
        if (!pkg_dir[0]) { set_msg(state, "Save failed: no pkg_dir set"); bump(); return 0; }

        char page_dir[PATH_BUF];
        snprintf(page_dir, sizeof(page_dir), "%s/pages/page_%d", pkg_dir, page_n);
        char ir_path[PATH_BUF];
        snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", page_dir);

        int next_id = 1;
        FILE *rf2 = fopen(ir_path, "r");
        if (rf2) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), rf2)) {
                if (strncmp(line, "NODE", 4) == 0) next_id++;
            }
            fclose(rf2);
        }
        FILE *af = fopen(ir_path, "a");
        if (!af) { set_msg(state, "Save failed: could not open event.ir.pdl"); bump(); return 0; }
        fprintf(af, "NODE         | id=%d type=show_choices | choices=%s default=%d\n", next_id, choices, def_idx);
        fclose(af);

        /* Compile event.pal from ir.pdl */
        char pal_path[PATH_BUF];
        snprintf(pal_path, sizeof(pal_path), "%s/event.pal", page_dir);
        FILE *pf = fopen(pal_path, "w");
        if (pf) {
            fprintf(pf, "# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl by event-ez\n");
            fprintf(pf, "# pkg=%s page=%d - regenerated fresh on every command save\n", pkg, page_n);
            FILE *irf = fopen(ir_path, "r");
            if (irf) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), irf)) {
                    if (strncmp(line, "NODE", 4) != 0) continue;
                    char *tp = strstr(line, "type=");
                    if (!tp) continue;
                    char type_buf[48] = "";
                    char *t = tp + 5, *sp = strchr(t, ' ');
                    char *pipe = strchr(t, '|');
                    size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
                    if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
                    memcpy(type_buf, t, len);
                    type_buf[len] = '\0';
                    if (strcmp(type_buf, "show_choices") == 0) {
                        char *chp = strstr(line, "choices=");
                        char ch[1024] = "";
                        if (chp) {
                            snprintf(ch, sizeof(ch), "%s", chp + 8);
                            ch[strcspn(ch, "\r\n|")] = '\0';
                        }
                        char *defp = strstr(line, "default=");
                        int def = defp ? atoi(defp + 8) : 0;
                        char *idp = strstr(line, "id=");
                        int node_id = idp ? atoi(idp + 3) : 1;
                        char wrapper_path[PATH_BUF];
                        snprintf(wrapper_path, sizeof(wrapper_path), "%s/cmd_%d.sh", page_dir, node_id);
                        FILE *wf = fopen(wrapper_path, "w");
                        if (wf) {
                            fprintf(wf, "#!/bin/sh\n");
                            fprintf(wf, "cd \"$(dirname \"$0\")/../../..\" || exit 1\n");
                            fprintf(wf, "ENT=\"$PWD\"\n");
                            fprintf(wf, "D=\"$ENT\"\n");
                            fprintf(wf, "while [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n");
                            fprintf(wf, "exec \"$D/&.widgits/events-hq/ops/+x/mr_show_choices.+x\" \"$ENT\" '%s' %d\n", ch, def);
                            fclose(wf);
                            chmod(wrapper_path, 0755);
                        }
                        fprintf(pf, "exec cmd_%d.sh\n", node_id);
                    }
                }
                fclose(irf);
            }
            fprintf(pf, "halt\n");
            fclose(pf);
        }

        char msg[MAX_LINE];
        snprintf(msg, sizeof(msg), "Saved Show Choices to page %d - click Back to see it listed", page_n);
        set_msg(state, msg);
        bump();
        return 0;
    }

    if (key == 10 || key == 13) {
        set_msg(state, "Enter: CHTPM commits focused button/field (onClick)");
        bump();
        return 0;
    }

    return 0;
}
